#include "computed_dynamic_array_audit_expectations.hpp"

#include "orison/lowering/llvm_object_emitter.hpp"
#include "orison/link/host_linker.hpp"
#include "orison/pipeline/compile_pipeline.hpp"
#include "orison/pipeline/dynamic_array_cleanup_metadata.hpp"

#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>

namespace {

namespace smoke = orison::tests::smoke;

void assert_line_contains(
    std::vector<std::string> const& lines,
    std::size_t index,
    std::string_view expected_fragment
) {
    assert(index < lines.size());
    if (lines[index].find(expected_fragment) == std::string::npos) {
        std::cerr << "expected report line " << index << " to contain '" << expected_fragment
                  << "', actual line: '" << lines[index] << "'\n";
    }
    assert(lines[index].find(expected_fragment) != std::string::npos);
}

void assert_any_line_contains(
    std::vector<std::string> const& lines,
    std::string_view expected_fragment
) {
    for (auto const& line : lines) {
        if (line.find(expected_fragment) != std::string::npos) {
            return;
        }
    }
    std::cerr << "expected any report line to contain '" << expected_fragment << "'\n";
    for (auto const& line : lines) {
        std::cerr << "actual line: '" << line << "'\n";
    }
    assert(false);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_pipeline_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::pipeline::CompilePipeline pipeline;
    auto source_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "minimal.or";

    auto analysis = pipeline.analyze(source_path);
    assert(!analysis.has_errors());
    assert(analysis.source_file.has_value());
    assert(analysis.parse_result.module.package_name == "demo.minimal");
    assert(analysis.parse_result.module.functions.size() == 1);
    assert(analysis.semantic_planned_drop_report.empty());
    assert(analysis.semantic_drop_implementation_report.empty());
    assert(analysis.semantic_drop_resolution_report.empty());
    assert(analysis.semantic_drop_diagnostic_report.empty());
    assert(analysis.semantic_drop_lowering_authorizations.empty());
    assert(analysis.semantic_drop_lowering_authorization_report.empty());
    assert(analysis.semantic_drop_resolution_summary_report.empty());
    assert(analysis.dynamic_array_descriptor_cleanup_plan_report.empty());

    auto ir = pipeline.emit_llvm(source_path);
    assert(!ir.has_errors());
    assert(ir.ir_text.find("define i32 @main()") != std::string::npos);
    assert(ir.ir_text.find("ret i32 0") != std::string::npos);
    assert(ir.semantic_drop_lowering_authorizations.empty());
    assert(ir.planned_drop_report.empty());
    assert(ir.emitted_drop_declaration_report.empty());
    assert(ir.drop_readiness_snapshot.semantic_authorizations.empty());
    assert(ir.drop_readiness_snapshot.emitted_declarations.empty());
    assert(ir.drop_readiness_snapshot.cleanup_authorizations.empty());
    assert(ir.drop_readiness_snapshot_report.size() == 1);
    assert(
        ir.drop_readiness_snapshot_report.front().find("semantic authorizations 0") != std::string::npos
    );
    assert(ir.drop_readiness_summary.semantic_authorized == 0);
    assert(ir.drop_readiness_summary.semantic_blocked == 0);
    assert(ir.drop_readiness_summary.emitted_declarations == 0);
    assert(ir.drop_readiness_summary.cleanup_authorized == 0);
    assert(ir.drop_readiness_summary.cleanup_blocked == 0);
    assert(ir.drop_readiness_summary_report.size() == 1);
    assert(
        ir.drop_readiness_summary_report.front().find("semantic authorized 0 blocked 0") != std::string::npos
    );
    assert(ir.drop_readiness_relation_report.empty());
    assert(ir.drop_readiness_blocker_summary.blocked_cleanups == 0);
    assert(ir.drop_readiness_blocker_summary.semantic_lowering_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.semantic_unresolved_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.missing_declarations.empty());
    assert(ir.drop_readiness_blocker_report.size() == 1);
    assert(
        ir.drop_readiness_blocker_report.front() ==
        "drop readiness blockers cleanups 0 semantic blockers 0 semantic unresolved 0 "
        "source lowering blocked 0 missing declarations 0"
    );
    assert(ir.drop_readiness_source_correlation_report.size() == 1);
    assert(
        ir.drop_readiness_source_correlation_report.front() ==
        "drop readiness source correlations actions 0 semantic sites 0"
    );
    assert(ir.dynamic_array_descriptor_cleanup_plan_report.empty());
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        ir.dynamic_array_cleanup_production_readiness
    ));
    assert(ir.dynamic_array_cleanup_production_readiness_report.size() == 1);
    assert_line_contains(
        ir.dynamic_array_cleanup_production_readiness_report,
        0,
        "production readiness blocked"
    );
    assert_line_contains(
        ir.dynamic_array_cleanup_production_readiness_report,
        0,
        "[descriptor origins missing]"
    );

    auto drop_readiness_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness.or";
    auto drop_readiness = pipeline.emit_llvm(drop_readiness_path);
    assert(!drop_readiness.has_errors());
    assert(drop_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 1);
    assert(drop_readiness.drop_readiness_snapshot.emitted_declarations.empty());
    assert(drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(drop_readiness.drop_readiness_snapshot_report.size() == 3);
    assert(
        drop_readiness.drop_readiness_snapshot_report[0].find("semantic authorizations 1") !=
        std::string::npos
    );
    assert(
        drop_readiness.drop_readiness_snapshot_report[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        drop_readiness.drop_readiness_snapshot_report[2].find("__orison_thread_cleanup.launch.12.0 blocked") !=
        std::string::npos
    );
    assert(drop_readiness.drop_readiness_summary.semantic_authorized == 0);
    assert(drop_readiness.drop_readiness_summary.semantic_blocked == 1);
    assert(drop_readiness.drop_readiness_summary.emitted_declarations == 0);
    assert(drop_readiness.drop_readiness_summary.cleanup_authorized == 0);
    assert(drop_readiness.drop_readiness_summary.cleanup_blocked == 1);
    assert(drop_readiness.drop_readiness_summary_report.size() == 1);
    assert(
        drop_readiness.drop_readiness_summary_report.front().find("semantic authorized 0 blocked 1") !=
        std::string::npos
    );
    assert(drop_readiness.drop_readiness_relation_report.size() == 3);
    assert(
        drop_readiness.drop_readiness_relation_report[0].find(
            "__orison_thread_cleanup.launch.12.0 blocked"
        ) != std::string::npos
    );
    assert(
        drop_readiness.drop_readiness_relation_report[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        drop_readiness.drop_readiness_relation_report[2].find("missing declaration __orison_drop.Payload") !=
        std::string::npos
    );
    assert(drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_report.size() == 4);
    assert(
        drop_readiness.drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 1 "
        "source lowering blocked 0 missing declarations 1"
    );
    assert(
        drop_readiness.drop_readiness_blocker_report[1].find("__orison_drop.Payload") != std::string::npos
    );
    assert(drop_readiness.drop_readiness_source_correlation_report.size() == 2);
    assert(
        drop_readiness.drop_readiness_source_correlation_report[0] ==
        "drop readiness source correlations actions 1 semantic sites 1"
    );
    assert(
        drop_readiness.drop_readiness_source_correlation_report[1].find(
            "__orison_thread_cleanup.launch.12.0 __orison_drop.Payload"
        ) != std::string::npos
    );

    auto dynamic_array_drop_report_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_drop_report.or";
    {
        auto dynamic_array_drop_report_source = std::ofstream(dynamic_array_drop_report_path);
        dynamic_array_drop_report_source
            << "package demo.pipeline.dynamicarray\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    1 as UInt32\n";
    }
    auto dynamic_array_drop_readiness = pipeline.emit_llvm(
        dynamic_array_drop_report_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .initial_capacity = 2,
                },
            },
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );
    assert(!dynamic_array_drop_readiness.has_errors());
    assert(dynamic_array_drop_readiness.planned_drop_action_report.size() == 1);
    assert_line_contains(
        dynamic_array_drop_readiness.planned_drop_action_report,
        0,
        "dynamic_array0.element: Payload"
    );
    assert(dynamic_array_drop_readiness.drop_cleanup_authorization_report.size() == 4);
    assert_line_contains(
        dynamic_array_drop_readiness.drop_cleanup_authorization_report,
        0,
        "drop cleanup authorization __orison_dynamic_array_cleanup.0 blocked"
    );
    assert_line_contains(
        dynamic_array_drop_readiness.drop_cleanup_authorization_report,
        1,
        "semantic drop lowering blocked __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness.drop_cleanup_authorization_report,
        2,
        "semantic drop unresolved __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness.drop_cleanup_authorization_report,
        3,
        "missing drop declaration __orison_drop.Payload"
    );
    assert(dynamic_array_drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_relation_report.size() == 3);
    assert_line_contains(
        dynamic_array_drop_readiness.drop_readiness_relation_report,
        0,
        "__orison_dynamic_array_cleanup.0 blocked"
    );
    assert_line_contains(
        dynamic_array_drop_readiness.drop_readiness_relation_report,
        1,
        "semantic blocker __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness.drop_readiness_relation_report,
        2,
        "missing declaration __orison_drop.Payload"
    );
    assert(dynamic_array_drop_readiness.drop_readiness_source_correlation_report.size() == 2);
    assert(
        dynamic_array_drop_readiness.drop_readiness_source_correlation_report[0] ==
        "drop readiness source correlations actions 1 semantic sites 0"
    );
    assert_line_contains(
        dynamic_array_drop_readiness.drop_readiness_source_correlation_report,
        1,
        "__orison_dynamic_array_cleanup.0 __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness.drop_readiness_source_correlation_report,
        1,
        "semantic absent source lowering absent declaration missing"
    );
    assert(dynamic_array_drop_readiness.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto dynamic_array_source_owner_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_source_owner.or";
    {
        auto dynamic_array_source_owner_source = std::ofstream(dynamic_array_source_owner_path);
        dynamic_array_source_owner_source
            << "package demo.pipeline.dynamicarraysource\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    1 as UInt32\n";
    }
    auto dynamic_array_source_owner = pipeline.analyze(dynamic_array_source_owner_path);
    assert(!dynamic_array_source_owner.has_errors());
    assert(dynamic_array_source_owner.semantic_result.dynamic_array_descriptor_origins.size() == 1);
    assert(dynamic_array_source_owner.semantic_dynamic_array_descriptor_origin_report.size() == 1);
    assert(
        dynamic_array_source_owner.semantic_dynamic_array_descriptor_origin_report.front() ==
        "dynamic array descriptor origin DynamicArray<Payload> owner items element Payload at line 6 (metadata only)"
    );
    assert(dynamic_array_source_owner.semantic_planned_drop_report.size() == 2);
    assert_line_contains(
        dynamic_array_source_owner.semantic_planned_drop_report,
        0,
        "DynamicArray<Payload> owner items"
    );
    assert_line_contains(
        dynamic_array_source_owner.semantic_planned_drop_report,
        1,
        "Payload owner items.element"
    );

    auto dynamic_array_bound_descriptor = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_bound_descriptor.has_errors());
    assert(dynamic_array_bound_descriptor.dynamic_array_descriptor_cleanup_plan_report.size() == 1);
    assert_line_contains(
        dynamic_array_bound_descriptor.dynamic_array_descriptor_cleanup_plan_report,
        0,
        "descriptor %items.addr bound"
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_obligation_report.size() == 1);
    assert_line_contains(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_obligation_report,
        0,
        "owner items source DynamicArray<Payload> element Payload descriptor %items.addr origin line 6 actions 1"
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_plan_report.size() == 1);
    assert_line_contains(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_plan_report,
        0,
        "[load descriptor] [drop initialized elements] [deallocate descriptor storage]"
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_verification_report.size() == 1);
    assert_line_contains(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_verification_report,
        0,
        "__orison_dynamic_array_cleanup.0 passed"
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_emission_gate_report.size() == 1);
    assert_line_contains(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_emission_gate_report,
        0,
        "__orison_dynamic_array_cleanup.0 allowed"
    );
    assert(
        dynamic_array_bound_descriptor.ir_text.find("define i32 @use_items({ ptr, i64, i64 } %items)") !=
        std::string::npos
    );
    assert(
        dynamic_array_bound_descriptor.ir_text.find("store { ptr, i64, i64 } %items, ptr %items.addr") !=
        std::string::npos
    );

    auto dynamic_array_owned_production_signature_descriptor = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .dynamic_array_parameter_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_production_signature_descriptor.has_errors());
    assert(
        dynamic_array_owned_production_signature_descriptor.error_text.find(
            "lowering DynamicArray parameter 'items' with owned element type Payload requires ownership/drop proof "
            "before production lowering"
        ) != std::string::npos
    );

    auto dynamic_array_source_correlated_cleanup = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_source_correlated_cleanup.has_errors());
    assert(dynamic_array_source_correlated_cleanup.drop_readiness_source_correlation_report.size() == 2);
    assert_line_contains(
        dynamic_array_source_correlated_cleanup.drop_readiness_source_correlation_report,
        0,
        "drop readiness source correlations actions 1 semantic sites"
    );
    assert_line_contains(
        dynamic_array_source_correlated_cleanup.drop_readiness_source_correlation_report,
        1,
        "__orison_dynamic_array_cleanup.0 __orison_drop.Payload for Payload capture items.element field 0 action line 6"
    );
    assert_line_contains(
        dynamic_array_source_correlated_cleanup.drop_readiness_source_correlation_report,
        1,
        "semantic owner items.element site line 6"
    );
    assert_line_contains(
        dynamic_array_source_correlated_cleanup.drop_readiness_source_correlation_report,
        1,
        "declaration missing"
    );

    auto cleanup_metadata_options = orison::pipeline::CompilePipelineOptions {
        .source_drop_lowering_enabled = true,
        .dynamic_array_descriptor_cleanup_planning_enabled = true,
        .dynamic_array_parameter_descriptor_audit_bindings_enabled = true,
        .dynamic_array_production_cleanup_emission_enabled = true,
    };
    auto cleanup_metadata_facade = pipeline.collect_dynamic_array_cleanup_metadata(
        dynamic_array_source_owner_path,
        cleanup_metadata_options
    );
    auto cleanup_metadata_collector =
        orison::pipeline::DynamicArrayCleanupMetadataCollector(pipeline).collect(
            dynamic_array_source_owner_path,
            cleanup_metadata_options
        );
    assert(!cleanup_metadata_facade.has_errors());
    assert(!cleanup_metadata_collector.has_errors());
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_obligation_report ==
        cleanup_metadata_facade.dynamic_array_cleanup_obligation_report
    );
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_sequence_verification_report ==
        cleanup_metadata_facade.dynamic_array_cleanup_sequence_verification_report
    );
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_emission_capability_report ==
        cleanup_metadata_facade.dynamic_array_cleanup_emission_capability_report
    );
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_production_readiness_report ==
        cleanup_metadata_facade.dynamic_array_cleanup_production_readiness_report
    );

    auto scalar_dynamic_array_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_descriptor.or";
    {
        auto scalar_dynamic_array_source = std::ofstream(scalar_dynamic_array_path);
        scalar_dynamic_array_source
            << "package demo.pipeline.dynamicarrayscalar\n"
            << "\n"
            << "function use_words(words: DynamicArray<UInt32>) -> UInt32\n"
            << "    1 as UInt32\n";
    }
    auto scalar_dynamic_array_cleanup = pipeline.emit_llvm(
        scalar_dynamic_array_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_cleanup.has_errors());
    assert(scalar_dynamic_array_cleanup.dynamic_array_descriptor_cleanup_plan_report.size() == 1);
    assert_line_contains(
        scalar_dynamic_array_cleanup.dynamic_array_descriptor_cleanup_plan_report,
        0,
        "descriptor %words.addr bound"
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_plan_report.size() == 1);
    assert_line_contains(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_plan_report,
        0,
        "[load descriptor] [deallocate descriptor storage]"
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_verification_report.size() == 1);
    assert_line_contains(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_verification_report,
        0,
        "__orison_dynamic_array_cleanup.0 passed"
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_gate_report.size() == 1);
    assert_line_contains(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_gate_report,
        0,
        "__orison_dynamic_array_cleanup.0 allowed"
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_report.size() == 1);
    assert_line_contains(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_report,
        0,
        "capability proven"
    );
    assert_line_contains(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_report,
        0,
        "[element cleanup ok]"
    );
    assert(
        scalar_dynamic_array_cleanup.ir_text.find("declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)") !=
        std::string::npos
    );
    assert(
        scalar_dynamic_array_cleanup.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %words.dynamic_array_cleanup0.cleanup.data, i64 4, "
            "i64 %words.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_production_signature = pipeline.emit_llvm(
        scalar_dynamic_array_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_production_signature.has_errors());
    assert(
        scalar_dynamic_array_production_signature.ir_text.find(
            "define i32 @use_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_production_signature.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %words.dynamic_array_cleanup0.cleanup.data, i64 4, "
            "i64 %words.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_length_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_length.or";
    {
        auto parameter_length_source = std::ofstream(scalar_dynamic_array_parameter_length_path);
        parameter_length_source
            << "package demo.pipeline.dynamicarrayparameterlength\n"
            << "\n"
            << "function use_words(words: DynamicArray<UInt32>) -> IntSize\n"
            << "    words.length()\n";
    }
    auto scalar_dynamic_array_parameter_length = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_length_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_parameter_length.has_errors());
    assert(
        scalar_dynamic_array_parameter_length.ir_text.find(
            "define i64 @use_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_length.ir_text.find(
            "%words.dynamic_array_length0.value = extractvalue { ptr, i64, i64 } "
            "%words.dynamic_array_length0.descriptor, 1"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_length.ir_text.find(
            "ret i64 %words.dynamic_array_length0.value"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_index_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_index.or";
    {
        auto parameter_index_source = std::ofstream(scalar_dynamic_array_parameter_index_path);
        parameter_index_source
            << "package demo.pipeline.dynamicarrayparameterindex\n"
            << "\n"
            << "function use_words(words: DynamicArray<UInt32>) -> UInt32\n"
            << "    words[0]\n";
    }
    auto scalar_dynamic_array_parameter_index = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_index_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_parameter_index.has_errors());
    assert(scalar_dynamic_array_parameter_index.dynamic_array_runtime_request_report.size() == 2);
    assert_line_contains(
        scalar_dynamic_array_parameter_index.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        scalar_dynamic_array_parameter_index.ir_text.find(
            "define i32 @use_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_index.ir_text.find(
            "%words.dynamic_array_index0.in_bounds = icmp ult i64 0, %words.dynamic_array_index0.length"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_index.ir_text.find(
            "ret i32 %words.dynamic_array_index0.value"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_for_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_for.or";
    {
        auto parameter_for_source = std::ofstream(scalar_dynamic_array_parameter_for_path);
        parameter_for_source
            << "package demo.pipeline.dynamicarrayparameterfor\n"
            << "\n"
            << "function sum_words(words: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in words\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto scalar_dynamic_array_parameter_for_without_gate = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_for_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_signature_lowering_enabled = true,
        }
    );
    assert(scalar_dynamic_array_parameter_for_without_gate.has_errors());
    assert(
        scalar_dynamic_array_parameter_for_without_gate.error_text.find(
            "lowering DynamicArray for statements currently requires explicit production enablement"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_for = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_for_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_parameter_for.has_errors());
    assert(scalar_dynamic_array_parameter_for.dynamic_array_runtime_request_report.size() == 1);
    assert_line_contains(
        scalar_dynamic_array_parameter_for.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_deallocate"
    );
    assert(
        scalar_dynamic_array_parameter_for.ir_text.find(
            "define i32 @sum_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_for.ir_text.find(
            "%words.sequence_for0.value = load i32, ptr %words.sequence_for0.element.addr"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_for.ir_text.find(
            "ret i32 %tmp"
        ) != std::string::npos
    );
    auto scalar_dynamic_array_parameter_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(scalar_dynamic_array_parameter_for.ir_text);
    assert(!scalar_dynamic_array_parameter_for_object.has_errors());

    auto computed_dynamic_array_parameter_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_parameter_for_rejected.or";
    {
        auto computed_parameter_for_source = std::ofstream(computed_dynamic_array_parameter_for_path);
        computed_parameter_for_source
            << "package demo.pipeline.computeddynamicarrayparameterfor\n"
            << "\n"
            << "function sum_words(flag: Bool, left: DynamicArray<UInt32>, right: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? left : right\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_parameter_for = pipeline.emit_llvm(
        computed_dynamic_array_parameter_for_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(computed_dynamic_array_parameter_for.has_errors());
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "lowering DynamicArray for statements currently requires a named descriptor iterable"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray iterable of type 'DynamicArray<UInt32>' requires a proven single descriptor owner"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "cleanup owner proof missing"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<UInt32> "
            "element UInt32 owners left right [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray cleanup sequence plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [loop cleanup blocked] [function cleanup blocked] "
            "[cleanup sequence disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray descriptor render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor load blocked] [data projection blocked] "
            "[length projection blocked] [capacity projection blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop control render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [entry branch blocked] [index phi blocked] [bounds check blocked] "
            "[conditional branch blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray element address render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [data pointer blocked] [index blocked] [element address blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray element load render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [element address blocked] [item value blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop continue render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [continue block blocked] [next index blocked] [backedge branch blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop render sequence plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor render blocked] [loop control blocked] [body block blocked] "
            "[element address blocked] [element load blocked] [loop continue blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop exit cleanup plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [exit block blocked] [cleanup blocked] [cleanup sequence disabled] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray production emission gate plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [ownership blocked] [loop render blocked] [loop cleanup ownership blocked] "
            "[function cleanup resumption blocked] [exit cleanup blocked] "
            "[production sequence blocked] [production emission disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_dynamic_array_same_owner_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_same_owner_for_rejected.or";
    {
        auto same_owner_for_source = std::ofstream(computed_dynamic_array_same_owner_for_path);
        same_owner_for_source
            << "package demo.pipeline.computeddynamicarraysameownerfor\n"
            << "\n"
            << "function sum_words(flag: Bool, items: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_same_owner_for = pipeline.emit_llvm(
        computed_dynamic_array_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(computed_dynamic_array_same_owner_for.has_errors());
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray ownership plan ternary single owner unproven source DynamicArray<UInt32> "
            "element UInt32 owners items items [ownership join ok] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray descriptor handoff plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items handoff items [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray cleanup sequence plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [loop cleanup blocked] [function cleanup blocked] "
            "[cleanup sequence disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray descriptor render plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [descriptor load blocked] [data projection blocked] "
            "[length projection blocked] [capacity projection blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray loop control render plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [entry branch blocked] [index phi blocked] [bounds check blocked] "
            "[conditional branch blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray element address render plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [data pointer blocked] [index blocked] [element address blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray element load render plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [element address blocked] [item value blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray loop continue render plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [continue block blocked] [next index blocked] "
            "[backedge branch blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray loop render sequence plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [descriptor render blocked] [loop control blocked] "
            "[body block blocked] [element address blocked] [element load blocked] "
            "[loop continue blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray loop exit cleanup plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [exit block blocked] [cleanup blocked] "
            "[cleanup sequence disabled] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.error_text.find(
            "computed DynamicArray production emission gate plan cleanup owner unproven source DynamicArray<UInt32> "
            "element UInt32 owner items [ownership blocked] [loop render blocked] "
            "[loop cleanup ownership blocked] [function cleanup resumption blocked] [exit cleanup blocked] "
            "[production sequence blocked] [production emission disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_dynamic_array_local_same_owner_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_for_rejected.or";
    {
        auto local_same_owner_for_source = std::ofstream(computed_dynamic_array_local_same_owner_for_path);
        local_same_owner_for_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerfor\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_collect_computed_dynamic_array_for_descriptor_renders = true,
            .test_only_collect_computed_dynamic_array_for_loop_control_renders = true,
            .test_only_collect_computed_dynamic_array_for_element_address_renders = true,
            .test_only_collect_computed_dynamic_array_for_element_load_renders = true,
            .test_only_collect_computed_dynamic_array_for_loop_continue_renders = true,
            .test_only_collect_computed_dynamic_array_for_loop_render_sequences = true,
            .test_only_collect_computed_dynamic_array_for_loop_exit_cleanups = true,
            .test_only_collect_computed_dynamic_array_for_cleanup_transitions = true,
            .test_only_collect_computed_dynamic_array_for_production_emission_gates = true,
            .test_only_collect_computed_dynamic_array_for_production_sequences = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(computed_dynamic_array_local_same_owner_for.has_errors());
    assert(computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_descriptor_render_report.size() == 1);
    assert(
        computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_descriptor_render_report.front() ==
        smoke::computed_dynamic_array_descriptor_render_report
    );
    assert(computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_loop_control_render_report.size() == 1);
    assert(
        computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_loop_control_render_report.front() ==
        smoke::computed_dynamic_array_loop_control_render_report
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_report.front() ==
        smoke::computed_dynamic_array_element_address_render_report
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_report.front() ==
        smoke::computed_dynamic_array_element_load_render_report
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_report.front() ==
        smoke::computed_dynamic_array_loop_continue_render_report
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_report.front() ==
        smoke::computed_dynamic_array_loop_render_sequence_report
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_report.front() ==
        smoke::computed_dynamic_array_loop_exit_cleanup_report
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_report.front() ==
        smoke::computed_dynamic_array_cleanup_transition_report
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_transition_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_plan_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_render_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_call_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_report.front() ==
        smoke::computed_dynamic_array_production_emission_gate_report
    );
    assert(computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_production_sequence_report.size() == 1);
    assert(
        computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_production_sequence_report.front() ==
        smoke::computed_dynamic_array_production_sequence_report
    );
    auto dynamic_array_metadata_collector =
        orison::pipeline::DynamicArrayCleanupMetadataCollector {pipeline};
    auto computed_dynamic_array_local_same_owner_metadata_without_comments =
        dynamic_array_metadata_collector.collect(
            computed_dynamic_array_local_same_owner_for_path,
            orison::pipeline::CompilePipelineOptions {
                .test_only_collect_computed_dynamic_array_for_production_sequences = true,
                .dynamic_array_production_construction_lowering_enabled = true,
                .dynamic_array_production_for_lowering_enabled = true,
            }
        );
    assert(!computed_dynamic_array_local_same_owner_metadata_without_comments.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_metadata_without_comments
            .test_only_computed_dynamic_array_for_production_sequence_module_ir.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_without_comments.ir_text.find(
            "; computed DynamicArray for production sequence"
        ) == std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_metadata_with_comments =
        dynamic_array_metadata_collector.collect(
            computed_dynamic_array_local_same_owner_for_path,
            orison::pipeline::CompilePipelineOptions {
                .test_only_emit_computed_dynamic_array_for_production_sequence_comments = true,
                .dynamic_array_production_construction_lowering_enabled = true,
                .dynamic_array_production_for_lowering_enabled = true,
            }
        );
    assert(!computed_dynamic_array_local_same_owner_metadata_with_comments.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments
            .test_only_computed_dynamic_array_for_production_sequence_module_ir.size() == 18
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments.ir_text.find(
            "; computed DynamicArray for production sequence function sum_words line 6 "
            "source DynamicArray<UInt32> element UInt32 owner items snippets 17 (metadata only)\n"
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_lowered_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_enable_computed_dynamic_array_for_lowering = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_lowered_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_report.front() ==
        smoke::computed_dynamic_array_inserted_cleanup_transition_report
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.front() ==
        smoke::computed_dynamic_array_inserted_cleanup_state_verification_report
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_emission_gate_report
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_plan_report
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_render_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_render_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_render_report
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data"
        ) == std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_insertion_gate_report
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_call_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls disabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "  %items.computed_for.0.item = load i32, ptr %items.computed_for.0.element.addr\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_authorized_cleanup_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_enable_computed_dynamic_array_for_lowering = true,
            .test_only_authorize_computed_dynamic_array_cleanup_calls = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_authorized_cleanup_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for.ir_text.find(
            "  ; cleanup state handoff resume operation items.computed_for.0.cleanup.resume "
            "from items.loop.entry to items [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data"
        ) == std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.front() ==
        smoke::computed_dynamic_array_inserted_cleanup_state_verification_enabled_report
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_emission_gate_ready_report
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_report.front().find(
                "[cleanup calls enabled]"
            ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_render_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_render_report
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_insertion_gate_ready_report
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_report.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_report.empty()
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_enable_computed_dynamic_array_for_lowering = true,
            .test_only_authorize_computed_dynamic_array_cleanup_calls = true,
            .test_only_insert_computed_dynamic_array_cleanup_calls = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data, "
            "i64 4, i64 %items.computed_for.0.capacity)\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text.find(
            "  store { ptr, i64, i64 } zeroinitializer, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text.find(
            "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .dynamic_array_runtime_request_report.size() == 2
    );
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_allocate"
    );
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.dynamic_array_runtime_request_report,
        1,
        "__orison_dynamic_array_deallocate"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_insertion_gate_ready_report
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_report.front() ==
        smoke::computed_dynamic_array_inserted_cleanup_call_report
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_report.front() ==
        smoke::computed_dynamic_array_consumed_cleanup_descriptor_report
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_object =
        orison::lowering::LlvmObjectEmitter {}.emit(
            computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text
        );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_object.has_errors());
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_inserted_cleanup_run.or";
    {
        auto inserted_cleanup_run_source =
            std::ofstream(computed_dynamic_array_local_same_owner_inserted_cleanup_run_path);
        inserted_cleanup_run_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerinsertedcleanuprun\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    items.push(7 as UInt32)\n"
            << "    for word in true ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_enable_computed_dynamic_array_for_lowering = true,
            .test_only_authorize_computed_dynamic_array_cleanup_calls = true,
            .test_only_insert_computed_dynamic_array_cleanup_calls = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_run.has_errors());
    assert(computed_dynamic_array_local_same_owner_inserted_cleanup_run.dynamic_array_runtime_request_report.size() == 3);
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.dynamic_array_runtime_request_report,
        1,
        "__orison_dynamic_array_grow"
    );
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.dynamic_array_runtime_request_report,
        2,
        "__orison_dynamic_array_deallocate"
    );
    auto computed_inserted_deallocate =
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for."
        );
    auto computed_inserted_clear =
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find(
            "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr"
        );
    auto computed_final_cleanup =
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup"
        );
    auto computed_return =
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find("ret i32 %tmp");
    assert(computed_inserted_deallocate != std::string::npos);
    assert(computed_inserted_clear != std::string::npos);
    assert(computed_final_cleanup != std::string::npos);
    assert(computed_return != std::string::npos);
    assert(computed_inserted_deallocate < computed_inserted_clear);
    assert(computed_inserted_clear < computed_final_cleanup);
    assert(computed_final_cleanup < computed_return);
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_report.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_report.front().find(
                "owner items descriptor %items.addr [inserted cleanup call proven] [descriptor finalized]"
            ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_object =
        orison::lowering::LlvmObjectEmitter {}.emit(
            computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text
        );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_run_object.has_errors());
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_executable =
        smoke_temp_root / "computed_dynamic_array_local_same_owner_inserted_cleanup_run";
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_link = orison::link::HostLinker {}.link(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_object.object_bytes,
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_executable
    );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_run_link.has_errors());
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_status = std::system(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_executable.string().c_str()
    );
    assert(WIFEXITED(computed_dynamic_array_local_same_owner_inserted_cleanup_run_status));
    assert(WEXITSTATUS(computed_dynamic_array_local_same_owner_inserted_cleanup_run_status) == 7);
    auto computed_dynamic_array_local_same_owner_two_loops_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_two_loops.or";
    {
        auto local_same_owner_two_loops_source = std::ofstream(computed_dynamic_array_local_same_owner_two_loops_path);
        local_same_owner_two_loops_source
            << "package demo.pipeline.computeddynamicarraylocalsameownertwoloops\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_two_loops = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_two_loops_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_enable_computed_dynamic_array_for_lowering = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_two_loops.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_transition_report.size() == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_transition_report.front() ==
        smoke::computed_dynamic_array_inserted_cleanup_transition_report
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_transition_report.back().find(
                "items.computed_for.1.cleanup.acquire"
            ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.size() == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.front() ==
        smoke::computed_dynamic_array_inserted_cleanup_state_verification_report
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_state_verification_report.back().find(
                "items.computed_for.1.cleanup.resume"
            ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.size() == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_emission_gate_report
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_report.back().find(
                "[cleanup call emission blocked]"
            ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_report.size() == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_plan_report
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_report.back().find(
                "items.computed_for.1.cleanup.resume.call"
            ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_render_report.size() == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_render_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_render_report
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_render_report.back().find(
                "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.1.data, i64 4, "
                "i64 %items.computed_for.1.capacity)"
            ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.size() == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.front() ==
        smoke::computed_dynamic_array_cleanup_call_insertion_gate_report
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_report.back().find(
                "items.computed_for.1.cleanup.resume.call [inserted state verified] "
                "[cleanup operands proven] [cleanup calls unauthorized]"
            ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.0.cleanup.acquire"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.1.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.1.cleanup.acquire"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "  %items.computed_for.1.index = phi i64 [ 0, %items.computed_for.0.exit ], "
            "[ %items.computed_for.1.next.index, %items.computed_for.1.continue ]\n"
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_after_if_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_after_if.or";
    {
        auto local_same_owner_after_if_source = std::ofstream(computed_dynamic_array_local_same_owner_after_if_path);
        local_same_owner_after_if_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerafterif\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    if flag\n"
            << "        total = 1 as UInt32\n"
            << "    else\n"
            << "        total = 2 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_after_if = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_after_if_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_enable_computed_dynamic_array_for_lowering = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_after_if.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_after_if.ir_text.find(
            "  %items.computed_for.1.index = phi i64 [ 0, %if.merge."
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_after_if.ir_text.find(
            "items.computed_for.1.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_after_if.ir_text.find(
            "items.computed_for.1.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            "computed DynamicArray ownership plan ternary single owner proven source DynamicArray<UInt32> "
            "element UInt32 owners items items [ownership join ok] [cleanup owner proven] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            "computed DynamicArray descriptor handoff plan single cleanup owner handoff planned source "
            "DynamicArray<UInt32> element UInt32 owner items handoff items descriptor %items.addr "
            "[descriptor storage available] [cleanup owner proven] [lowering disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            "computed DynamicArray cleanup sequence plan loop cleanup sequence planned source "
            "DynamicArray<UInt32> element UInt32 owner items descriptor %items.addr "
            "loop-entry items.loop.entry loop-exit items operation items.computed_for.cleanup.acquire "
            "[loop cleanup owns descriptor] "
            "[function cleanup resumes] [cleanup sequence disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_descriptor_render_plan
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_loop_control_render_plan
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_element_address_render_plan
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_element_load_render_plan
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_loop_continue_render_plan
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_loop_render_sequence_plan
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_loop_exit_cleanup_plan
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for.error_text.find(
            smoke::computed_dynamic_array_production_emission_gate_plan
        ) != std::string::npos
    );

    auto view_parameter_length_path = smoke_temp_root / "orison_pipeline_view_parameter_length.or";
    {
        auto view_length_source = std::ofstream(view_parameter_length_path);
        view_length_source
            << "package demo.pipeline.viewlength\n"
            << "\n"
            << "function count(values: View<UInt32>) -> IntSize\n"
            << "    values.length()\n";
    }
    auto view_parameter_length = pipeline.emit_llvm(view_parameter_length_path);
    assert(!view_parameter_length.has_errors());
    assert(view_parameter_length.dynamic_array_runtime_request_report.empty());
    assert(
        view_parameter_length.ir_text.find("define i64 @count({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        view_parameter_length.ir_text.find("%view_length0.value = extractvalue { ptr, i64 } %values, 1") !=
        std::string::npos
    );

    auto view_parameter_index_path = smoke_temp_root / "orison_pipeline_view_parameter_index.or";
    {
        auto view_index_source = std::ofstream(view_parameter_index_path);
        view_index_source
            << "package demo.pipeline.viewindex\n"
            << "\n"
            << "function first(values: View<UInt32>) -> UInt32\n"
            << "    values[0]\n";
    }
    auto view_parameter_index = pipeline.emit_llvm(view_parameter_index_path);
    assert(!view_parameter_index.has_errors());
    assert(view_parameter_index.dynamic_array_runtime_request_report.size() == 1);
    assert_line_contains(
        view_parameter_index.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        view_parameter_index.ir_text.find("define i32 @first({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        view_parameter_index.ir_text.find("%view_index1.in_bounds = icmp ult i64 0, %view_index1.length") !=
        std::string::npos
    );
    auto view_parameter_index_object =
        orison::lowering::LlvmObjectEmitter {}.emit(view_parameter_index.ir_text);
    assert(!view_parameter_index_object.has_errors());

    auto view_parameter_for_path = smoke_temp_root / "orison_pipeline_view_parameter_for.or";
    {
        auto view_for_source = std::ofstream(view_parameter_for_path);
        view_for_source
            << "package demo.pipeline.viewfor\n"
            << "\n"
            << "function sum(values: View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in values\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto view_parameter_for = pipeline.emit_llvm(view_parameter_for_path);
    assert(!view_parameter_for.has_errors());
    assert(view_parameter_for.dynamic_array_runtime_request_report.empty());
    assert(
        view_parameter_for.ir_text.find("define i32 @sum({ ptr, i64 } %values)") != std::string::npos
    );
    assert(
        view_parameter_for.ir_text.find(
            "%values.sequence_for0.value = load i32, ptr %values.sequence_for0.element.addr"
        ) != std::string::npos
    );
    auto view_parameter_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(view_parameter_for.ir_text);
    assert(!view_parameter_for_object.has_errors());

    auto shared_view_parameter_length_path =
        smoke_temp_root / "orison_pipeline_shared_view_parameter_length.or";
    {
        auto shared_view_length_source = std::ofstream(shared_view_parameter_length_path);
        shared_view_length_source
            << "package demo.pipeline.sharedviewlength\n"
            << "\n"
            << "function count(values: shared.View<UInt32>) -> IntSize\n"
            << "    values.length()\n";
    }
    auto shared_view_parameter_length = pipeline.emit_llvm(shared_view_parameter_length_path);
    assert(!shared_view_parameter_length.has_errors());
    assert(shared_view_parameter_length.dynamic_array_runtime_request_report.empty());
    assert(
        shared_view_parameter_length.ir_text.find("define i64 @count({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        shared_view_parameter_length.ir_text.find(
            "%view_length0.value = extractvalue { ptr, i64 } %values, 1"
        ) != std::string::npos
    );

    auto exclusive_view_parameter_index_path =
        smoke_temp_root / "orison_pipeline_exclusive_view_parameter_index.or";
    {
        auto exclusive_view_index_source = std::ofstream(exclusive_view_parameter_index_path);
        exclusive_view_index_source
            << "package demo.pipeline.exclusiveviewindex\n"
            << "\n"
            << "function first(values: exclusive.View<UInt32>) -> UInt32\n"
            << "    values[0]\n";
    }
    auto exclusive_view_parameter_index = pipeline.emit_llvm(exclusive_view_parameter_index_path);
    assert(!exclusive_view_parameter_index.has_errors());
    assert(exclusive_view_parameter_index.dynamic_array_runtime_request_report.size() == 1);
    assert_line_contains(
        exclusive_view_parameter_index.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        exclusive_view_parameter_index.ir_text.find("define i32 @first({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        exclusive_view_parameter_index.ir_text.find(
            "%view_index1.value = load i32, ptr %view_index1.element.addr"
        ) != std::string::npos
    );

    auto exclusive_view_parameter_assignment_path =
        smoke_temp_root / "orison_pipeline_exclusive_view_parameter_assignment.or";
    {
        auto exclusive_view_assignment_source = std::ofstream(exclusive_view_parameter_assignment_path);
        exclusive_view_assignment_source
            << "package demo.pipeline.exclusiveviewassign\n"
            << "\n"
            << "function write_first(values: exclusive.View<UInt32>) -> Unit\n"
            << "    values[0] = 7 as UInt32\n";
    }
    auto exclusive_view_parameter_assignment = pipeline.emit_llvm(exclusive_view_parameter_assignment_path);
    assert(!exclusive_view_parameter_assignment.has_errors());
    assert(exclusive_view_parameter_assignment.dynamic_array_runtime_request_report.size() == 1);
    assert_line_contains(
        exclusive_view_parameter_assignment.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        exclusive_view_parameter_assignment.ir_text.find(
            "define void @write_first({ ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        exclusive_view_parameter_assignment.ir_text.find(
            "%values.view_assign0.in_bounds = icmp ult i64 0, %values.view_assign0.length"
        ) != std::string::npos
    );
    assert(
        exclusive_view_parameter_assignment.ir_text.find(
            "store i32 7, ptr %values.view_assign0.element.addr"
        ) != std::string::npos
    );
    auto exclusive_view_parameter_assignment_object =
        orison::lowering::LlvmObjectEmitter {}.emit(exclusive_view_parameter_assignment.ir_text);
    assert(!exclusive_view_parameter_assignment_object.has_errors());

    auto shared_view_parameter_for_path =
        smoke_temp_root / "orison_pipeline_shared_view_parameter_for.or";
    {
        auto shared_view_for_source = std::ofstream(shared_view_parameter_for_path);
        shared_view_for_source
            << "package demo.pipeline.sharedviewfor\n"
            << "\n"
            << "function sum(values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in values\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto shared_view_parameter_for = pipeline.emit_llvm(shared_view_parameter_for_path);
    assert(!shared_view_parameter_for.has_errors());
    assert(shared_view_parameter_for.dynamic_array_runtime_request_report.empty());
    assert(
        shared_view_parameter_for.ir_text.find(
            "%values.sequence_for0.value = load i32, ptr %values.sequence_for0.element.addr"
        ) != std::string::npos
    );

    auto computed_shared_view_for_path =
        smoke_temp_root / "orison_pipeline_computed_shared_view_for.or";
    {
        auto computed_shared_view_for_source = std::ofstream(computed_shared_view_for_path);
        computed_shared_view_for_source
            << "package demo.pipeline.computedsharedviewfor\n"
            << "\n"
            << "function forward(values: shared.View<UInt32>) -> shared.View<UInt32>\n"
            << "    values\n"
            << "\n"
            << "function sum(values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in forward(values)\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto computed_shared_view_for = pipeline.emit_llvm(computed_shared_view_for_path);
    assert(!computed_shared_view_for.has_errors());
    assert(computed_shared_view_for.dynamic_array_runtime_request_report.empty());
    assert(
        computed_shared_view_for.ir_text.find(
            " = call { ptr, i64 } @forward({ ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        computed_shared_view_for.ir_text.find("%computed.sequence_for") != std::string::npos
    );
    auto computed_shared_view_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(computed_shared_view_for.ir_text);
    assert(!computed_shared_view_for_object.has_errors());

    auto method_returned_shared_view_for_path =
        smoke_temp_root / "orison_pipeline_method_returned_shared_view_for.or";
    {
        auto method_returned_shared_view_for_source =
            std::ofstream(method_returned_shared_view_for_path);
        method_returned_shared_view_for_source
            << "package demo.pipeline.methodreturnedsharedviewfor\n"
            << "\n"
            << "extend UInt32\n"
            << "    public function forward_view(this: shared This, values: shared.View<UInt32>) -> shared.View<UInt32>\n"
            << "        values\n"
            << "\n"
            << "function sum(seed: UInt32, values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in seed.forward_view(values)\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto method_returned_shared_view_for = pipeline.emit_llvm(method_returned_shared_view_for_path);
    assert(!method_returned_shared_view_for.has_errors());
    assert(method_returned_shared_view_for.dynamic_array_runtime_request_report.empty());
    assert(
        method_returned_shared_view_for.ir_text.find(
            " = call { ptr, i64 } @method.UInt32.forward_view(i32 %seed, { ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        method_returned_shared_view_for.ir_text.find("%computed.sequence_for") != std::string::npos
    );
    auto method_returned_shared_view_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(method_returned_shared_view_for.ir_text);
    assert(!method_returned_shared_view_for_object.has_errors());

    auto member_receiver_method_returned_shared_view_for_path =
        smoke_temp_root / "orison_pipeline_member_receiver_method_returned_shared_view_for.or";
    {
        auto member_receiver_method_returned_shared_view_for_source =
            std::ofstream(member_receiver_method_returned_shared_view_for_path);
        member_receiver_method_returned_shared_view_for_source
            << "package demo.pipeline.memberreceiverreturnedsharedviewfor\n"
            << "\n"
            << "record Bucket\n"
            << "    marker: UInt32\n"
            << "\n"
            << "record Wrapper\n"
            << "    bucket: Bucket\n"
            << "\n"
            << "record Shelf\n"
            << "    buckets: Array<Bucket, 2>\n"
            << "\n"
            << "extend Bucket\n"
            << "    public function forward_view(this: shared This, values: shared.View<UInt32>) -> shared.View<UInt32>\n"
            << "        values\n"
            << "\n"
            << "function sum_wrapper(wrapper: Wrapper, values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in wrapper.bucket.forward_view(values)\n"
            << "        total = total + item\n"
            << "    total\n"
            << "\n"
            << "function sum_shelf(shelf: Shelf, values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in shelf.buckets[0].forward_view(values)\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto member_receiver_method_returned_shared_view_for =
        pipeline.emit_llvm(member_receiver_method_returned_shared_view_for_path);
    assert(!member_receiver_method_returned_shared_view_for.has_errors());
    assert(member_receiver_method_returned_shared_view_for.dynamic_array_runtime_request_report.empty());
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find(
            " = call { ptr, i64 } @method.Bucket.forward_view(%record.Bucket"
        ) != std::string::npos
    );
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find(
            " = getelementptr %record.Wrapper, ptr %wrapper.addr"
        ) != std::string::npos
    );
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find(
            " = getelementptr [2 x %record.Bucket], ptr"
        ) != std::string::npos
    );
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find("%computed.sequence_for") !=
        std::string::npos
    );
    auto member_receiver_method_returned_shared_view_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(
            member_receiver_method_returned_shared_view_for.ir_text
        );
    assert(!member_receiver_method_returned_shared_view_for_object.has_errors());

    auto dynamic_array_blocked_owned_cleanup = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_blocked_owned_cleanup.has_errors());
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_report.size() == 1);
    assert_line_contains(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_report,
        0,
        "capability blocked"
    );
    assert_line_contains(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_report,
        0,
        "[element cleanup missing]"
    );
    assert(dynamic_array_blocked_owned_cleanup.drop_readiness_summary.cleanup_authorized == 0);
    assert(dynamic_array_blocked_owned_cleanup.drop_readiness_summary.cleanup_blocked == 1);
    assert(
        dynamic_array_blocked_owned_cleanup.ir_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_production_readiness
    ));
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_production_readiness_report.size() == 1);
    assert_line_contains(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_production_readiness_report,
        0,
        "production readiness blocked"
    );
    assert_line_contains(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_production_readiness_report,
        0,
        "[cleanup capability missing]"
    );

    auto dynamic_array_owned_production_signature_rejected = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .dynamic_array_parameter_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(dynamic_array_owned_production_signature_rejected.has_errors());
    assert(
        dynamic_array_owned_production_signature_rejected.error_text.find(
            "lowering DynamicArray parameter 'items' with owned element type Payload requires ownership/drop proof "
            "before production lowering"
        ) != std::string::npos
    );

    auto dynamic_array_owned_cleanup = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_owned_cleanup.has_errors());
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_report.size() == 1);
    assert_line_contains(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_report,
        0,
        "capability proven"
    );
    assert_line_contains(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_report,
        0,
        "[element cleanup ok]"
    );
    assert(dynamic_array_owned_cleanup.drop_readiness_summary.cleanup_authorized == 1);
    assert(dynamic_array_owned_cleanup.drop_readiness_summary.cleanup_blocked == 0);
    assert(
        dynamic_array_owned_cleanup.ir_text.find("declare void @__orison_drop.Payload(ptr)") !=
        std::string::npos
    );
    assert(
        dynamic_array_owned_cleanup.ir_text.find(
            "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_cleanup.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_production_readiness
    ));
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_production_readiness_report.size() == 1);
    assert_line_contains(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_production_readiness_report,
        0,
        "production readiness blocked"
    );
    assert_line_contains(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_production_readiness_report,
        0,
        "[cleanup capability ok]"
    );
    assert_line_contains(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_production_readiness_report,
        0,
        "[production signatures missing]"
    );

    auto dynamic_array_owned_signature_gate_only = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_signature_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_signature_gate_only.has_errors());
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_owned_signature_gate_only.dynamic_array_cleanup_production_readiness
    ));
    assert_line_contains(
        dynamic_array_owned_signature_gate_only.dynamic_array_cleanup_production_readiness_report,
        0,
        "[production signatures ok]"
    );
    assert_line_contains(
        dynamic_array_owned_signature_gate_only.dynamic_array_cleanup_production_readiness_report,
        0,
        "[production construction missing]"
    );

    auto dynamic_array_owned_construction_gate = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .owner_name = "items",
                    .initial_capacity = 2,
                },
            },
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_signature_lowering_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_construction_gate.has_errors());
    assert(dynamic_array_owned_construction_gate.dynamic_array_construction_plan_report.size() == 1);
    assert_line_contains(
        dynamic_array_owned_construction_gate.dynamic_array_construction_plan_report,
        0,
        "requests __orison_dynamic_array_allocate"
    );
    assert(dynamic_array_owned_construction_gate.dynamic_array_runtime_request_report.size() == 2);
    assert_any_line_contains(
        dynamic_array_owned_construction_gate.dynamic_array_runtime_request_report,
        "__orison_dynamic_array_allocate"
    );
    assert(dynamic_array_owned_construction_gate.dynamic_array_allocation_call_ir.size() == 1);
    assert(
        dynamic_array_owned_construction_gate.dynamic_array_allocation_call_ir.front() ==
        "  %dynamic_array_alloc0.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array_alloc0.addr, i64 8, i64 2)\n"
        "  %dynamic_array_alloc0 = load { ptr, i64, i64 }, ptr %dynamic_array_alloc0.addr\n"
    );
    assert(
        dynamic_array_owned_construction_gate.ir_text.find(
            "call void @__orison_dynamic_array_allocate"
        ) == std::string::npos
    );
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_owned_construction_gate.dynamic_array_cleanup_production_readiness
    ));
    assert_line_contains(
        dynamic_array_owned_construction_gate.dynamic_array_cleanup_production_readiness_report,
        0,
        "[production construction ok]"
    );
    assert_line_contains(
        dynamic_array_owned_construction_gate.dynamic_array_cleanup_production_readiness_report,
        0,
        "[production cleanup emission missing]"
    );

    auto dynamic_array_source_construction_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_source_construction.or";
    {
        auto source_construction_source = std::ofstream(dynamic_array_source_construction_path);
        source_construction_source
            << "package demo.pipeline.dynamicarraysourceconstruction\n"
            << "\n"
            << "function build_items<T>() -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    1 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    0 as UInt32\n";
    }
    auto dynamic_array_source_construction = pipeline.emit_llvm(
        dynamic_array_source_construction_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_construction_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_source_construction.has_errors());
    assert(dynamic_array_source_construction.dynamic_array_construction_plan_report.size() == 1);
    assert(
        dynamic_array_source_construction.dynamic_array_construction_plan_report.front() ==
        "dynamic array construction DynamicArray<UInt32> owner items element UInt32 lowers to i32 "
        "element_size 4 initial_capacity 0 requests __orison_dynamic_array_allocate (metadata only)"
    );
    assert(dynamic_array_source_construction.dynamic_array_runtime_request_report.size() == 1);
    assert_line_contains(
        dynamic_array_source_construction.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_allocate"
    );
    assert(dynamic_array_source_construction.dynamic_array_allocation_call_ir.size() == 1);
    assert(
        dynamic_array_source_construction.dynamic_array_allocation_call_ir.front() ==
        "  %dynamic_array_alloc0.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array_alloc0.addr, i64 4, i64 0)\n"
        "  %dynamic_array_alloc0 = load { ptr, i64, i64 }, ptr %dynamic_array_alloc0.addr\n"
    );
    assert(
        dynamic_array_source_construction.ir_text.find(
            "call void @__orison_dynamic_array_allocate"
        ) == std::string::npos
    );

    auto dynamic_array_placed_construction_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_placed_construction.or";
    {
        auto placed_construction_source = std::ofstream(dynamic_array_placed_construction_path);
        placed_construction_source
            << "package demo.pipeline.dynamicarrayplacedconstruction\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    0 as UInt32\n";
    }
    auto dynamic_array_placed_construction = pipeline.emit_llvm(
        dynamic_array_placed_construction_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_construction_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_placed_construction.has_errors());
    assert(dynamic_array_placed_construction.dynamic_array_construction_plan_report.size() == 1);
    assert_line_contains(
        dynamic_array_placed_construction.dynamic_array_construction_plan_report,
        0,
        "owner items"
    );
    assert(
        dynamic_array_placed_construction.ir_text.find(
            "  %items.dynamic_array_alloc.addr = alloca { ptr, i64, i64 }\n"
            "  call void @__orison_dynamic_array_allocate("
            "ptr sret({ ptr, i64, i64 }) %items.dynamic_array_alloc.addr, i64 4, i64 0)\n"
            "  %items.dynamic_array_alloc = load { ptr, i64, i64 }, ptr %items.dynamic_array_alloc.addr\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_placed_construction.ir_text.find(
            "  %items.addr = alloca { ptr, i64, i64 }\n"
            "  store { ptr, i64, i64 } %items.dynamic_array_alloc, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(dynamic_array_placed_construction.ir_text.find("__orison_dynamic_array_grow") == std::string::npos);
    assert(dynamic_array_placed_construction.ir_text.find("__orison_dynamic_array_deallocate") == std::string::npos);

    auto dynamic_array_local_cleanup = pipeline.emit_llvm(
        dynamic_array_placed_construction_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_local_cleanup.has_errors());
    assert(dynamic_array_local_cleanup.dynamic_array_runtime_request_report.size() == 2);
    assert_line_contains(
        dynamic_array_local_cleanup.dynamic_array_runtime_request_report,
        0,
        "__orison_dynamic_array_allocate"
    );
    assert_line_contains(
        dynamic_array_local_cleanup.dynamic_array_runtime_request_report,
        1,
        "__orison_dynamic_array_deallocate"
    );
    assert(
        dynamic_array_local_cleanup.ir_text.find(
            "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_cleanup.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 4, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)\n"
        ) != std::string::npos
    );
    auto local_cleanup = dynamic_array_local_cleanup.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto local_return = dynamic_array_local_cleanup.ir_text.find("ret i32 0");
    assert(local_cleanup != std::string::npos);
    assert(local_return != std::string::npos);
    assert(local_cleanup < local_return);

    auto dynamic_array_local_index_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_index.or";
    {
        auto local_index_source = std::ofstream(dynamic_array_local_index_path);
        local_index_source
            << "package demo.pipeline.dynamicarraylocalindex\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items[0]\n";
    }
    auto dynamic_array_local_index = pipeline.emit_llvm(
        dynamic_array_local_index_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_local_index.has_errors());
    assert(dynamic_array_local_index.dynamic_array_runtime_request_report.size() == 3);
    assert_line_contains(
        dynamic_array_local_index.dynamic_array_runtime_request_report,
        1,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        dynamic_array_local_index.ir_text.find("declare void @__orison_dynamic_array_bounds_failed()") !=
        std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "  %items.dynamic_array_index0.in_bounds = icmp ult i64 0, %items.dynamic_array_index0.length\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "  br i1 %items.dynamic_array_index0.in_bounds, label %dynamic_array.index.in_bounds.0, "
            "label %dynamic_array.index.out_of_bounds.0\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "dynamic_array.index.out_of_bounds.0:\n"
            "  call void @__orison_dynamic_array_bounds_failed()\n"
            "  unreachable\n"
            "dynamic_array.index.in_bounds.0:\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "  %items.dynamic_array_index0.value = load i32, ptr %items.dynamic_array_index0.element.addr\n"
        ) != std::string::npos
    );
    auto pipeline_index_load =
        dynamic_array_local_index.ir_text.find("%items.dynamic_array_index0.value = load i32");
    auto pipeline_index_cleanup =
        dynamic_array_local_index.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto pipeline_index_return =
        dynamic_array_local_index.ir_text.find("ret i32 %items.dynamic_array_index0.value");
    assert(pipeline_index_load != std::string::npos);
    assert(pipeline_index_cleanup != std::string::npos);
    assert(pipeline_index_return != std::string::npos);
    assert(pipeline_index_load < pipeline_index_cleanup);
    assert(pipeline_index_cleanup < pipeline_index_return);

    auto dynamic_array_local_append_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_append.or";
    {
        auto local_append_source = std::ofstream(dynamic_array_local_append_path);
        local_append_source
            << "package demo.pipeline.dynamicarraylocalappend\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items.push(7 as UInt32)\n"
            << "    0 as UInt32\n";
    }
    auto dynamic_array_local_append = pipeline.emit_llvm(dynamic_array_local_append_path);
    assert(!dynamic_array_local_append.has_errors());
    assert(dynamic_array_local_append.dynamic_array_runtime_request_report.size() == 3);
    assert_line_contains(
        dynamic_array_local_append.dynamic_array_runtime_request_report,
        1,
        "__orison_dynamic_array_grow"
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  %items.dynamic_array_append0.has_capacity = icmp ult i64 %items.dynamic_array_append0.length, "
            "%items.dynamic_array_append0.capacity\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  %items.dynamic_array_append0.next.capacity = select i1 %items.dynamic_array_append0.capacity.is_zero, "
            "i64 1, i64 %items.dynamic_array_append0.doubled.capacity\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  call void @__orison_dynamic_array_grow("
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  store i32 7, ptr %items.dynamic_array_append0.element.addr\n"
        ) != std::string::npos
    );
    assert(dynamic_array_local_append.ir_text.find("__orison_dynamic_array_capacity_failed") == std::string::npos);
    auto dynamic_array_local_append_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_local_append.ir_text);
    assert(!dynamic_array_local_append_object.has_errors());
    auto dynamic_array_local_append_executable = smoke_temp_root / "dynamic_array_local_append";
    auto dynamic_array_local_append_link = orison::link::HostLinker {}.link(
        dynamic_array_local_append_object.object_bytes,
        dynamic_array_local_append_executable
    );
    assert(!dynamic_array_local_append_link.has_errors());
    auto dynamic_array_local_append_status = std::system(dynamic_array_local_append_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_local_append_status));
    assert(WEXITSTATUS(dynamic_array_local_append_status) == 0);

    auto dynamic_array_append_index_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_append_index.or";
    {
        auto append_index_source = std::ofstream(dynamic_array_append_index_path);
        append_index_source
            << "package demo.pipeline.dynamicarrayappendindex\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items.push(7 as UInt32)\n"
            << "    items[0]\n";
    }
    auto dynamic_array_append_index = pipeline.emit_llvm(dynamic_array_append_index_path);
    assert(!dynamic_array_append_index.has_errors());
    assert(dynamic_array_append_index.dynamic_array_runtime_request_report.size() == 4);
    assert_line_contains(
        dynamic_array_append_index.dynamic_array_runtime_request_report,
        1,
        "__orison_dynamic_array_bounds_failed"
    );
    assert_line_contains(
        dynamic_array_append_index.dynamic_array_runtime_request_report,
        2,
        "__orison_dynamic_array_grow"
    );
    auto append_store = dynamic_array_append_index.ir_text.find(
        "  store i32 7, ptr %items.dynamic_array_append0.element.addr\n"
    );
    auto index_load = dynamic_array_append_index.ir_text.find(
        "  %items.dynamic_array_index1.value = load i32, ptr %items.dynamic_array_index1.element.addr\n"
    );
    auto append_index_cleanup = dynamic_array_append_index.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto append_index_return = dynamic_array_append_index.ir_text.find(
        "ret i32 %items.dynamic_array_index1.value"
    );
    assert(append_store != std::string::npos);
    assert(index_load != std::string::npos);
    assert(append_index_cleanup != std::string::npos);
    assert(append_index_return != std::string::npos);
    assert(append_store < index_load);
    assert(index_load < append_index_cleanup);
    assert(append_index_cleanup < append_index_return);
    auto dynamic_array_append_index_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_append_index.ir_text);
    assert(!dynamic_array_append_index_object.has_errors());
    auto dynamic_array_append_index_executable = smoke_temp_root / "dynamic_array_append_index";
    auto dynamic_array_append_index_link = orison::link::HostLinker {}.link(
        dynamic_array_append_index_object.object_bytes,
        dynamic_array_append_index_executable
    );
    assert(!dynamic_array_append_index_link.has_errors());
    auto dynamic_array_append_index_status = std::system(dynamic_array_append_index_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_append_index_status));
    assert(WEXITSTATUS(dynamic_array_append_index_status) == 7);

    auto dynamic_array_append_length_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_append_length.or";
    {
        auto append_length_source = std::ofstream(dynamic_array_append_length_path);
        append_length_source
            << "package demo.pipeline.dynamicarrayappendlength\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items.push(7 as UInt32)\n"
            << "    items.length()\n";
    }
    auto dynamic_array_append_length = pipeline.emit_llvm(dynamic_array_append_length_path);
    assert(!dynamic_array_append_length.has_errors());
    assert(dynamic_array_append_length.dynamic_array_runtime_request_report.size() == 3);
    assert_line_contains(
        dynamic_array_append_length.dynamic_array_runtime_request_report,
        1,
        "__orison_dynamic_array_grow"
    );
    assert(
        dynamic_array_append_length.ir_text.find(
            "  %items.dynamic_array_length1.value = extractvalue { ptr, i64, i64 } "
            "%items.dynamic_array_length1.descriptor, 1\n"
        ) != std::string::npos
    );
    assert(dynamic_array_append_length.ir_text.find("__orison_dynamic_array_bounds_failed") == std::string::npos);
    auto length_load = dynamic_array_append_length.ir_text.find(
        "%items.dynamic_array_length1.value = extractvalue { ptr, i64, i64 }"
    );
    auto append_length_cleanup = dynamic_array_append_length.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto append_length_return = dynamic_array_append_length.ir_text.find(
        "ret i64 %items.dynamic_array_length1.value"
    );
    assert(length_load != std::string::npos);
    assert(append_length_cleanup != std::string::npos);
    assert(append_length_return != std::string::npos);
    assert(length_load < append_length_cleanup);
    assert(append_length_cleanup < append_length_return);
    auto dynamic_array_append_length_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_append_length.ir_text);
    assert(!dynamic_array_append_length_object.has_errors());
    auto dynamic_array_append_length_executable = smoke_temp_root / "dynamic_array_append_length";
    auto dynamic_array_append_length_link = orison::link::HostLinker {}.link(
        dynamic_array_append_length_object.object_bytes,
        dynamic_array_append_length_executable
    );
    assert(!dynamic_array_append_length_link.has_errors());
    auto dynamic_array_append_length_status = std::system(dynamic_array_append_length_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_append_length_status));
    assert(WEXITSTATUS(dynamic_array_append_length_status) == 1);

    auto dynamic_array_append_for_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_append_for.or";
    {
        auto append_for_source = std::ofstream(dynamic_array_append_for_path);
        append_for_source
            << "package demo.pipeline.dynamicarrayappendfor\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    items.push(7 as UInt32)\n"
            << "    items.push(9 as UInt32)\n"
            << "    for item in items\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto dynamic_array_append_for = pipeline.emit_llvm(dynamic_array_append_for_path);
    assert(!dynamic_array_append_for.has_errors());
    assert(dynamic_array_append_for.dynamic_array_runtime_request_report.size() == 3);
    assert(dynamic_array_append_for.ir_text.find("for.condition.") != std::string::npos);
    assert(dynamic_array_append_for.ir_text.find("for.continue.") != std::string::npos);
    assert(
        dynamic_array_append_for.ir_text.find(
            "getelementptr i32, ptr %items.sequence_for2.data, i64 %items.sequence_for2.index"
        ) != std::string::npos
    );
    assert(dynamic_array_append_for.ir_text.find("__orison_dynamic_array_bounds_failed") == std::string::npos);
    auto dynamic_array_append_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_append_for.ir_text);
    assert(!dynamic_array_append_for_object.has_errors());
    auto dynamic_array_append_for_executable = smoke_temp_root / "dynamic_array_append_for";
    auto dynamic_array_append_for_link = orison::link::HostLinker {}.link(
        dynamic_array_append_for_object.object_bytes,
        dynamic_array_append_for_executable
    );
    assert(!dynamic_array_append_for_link.has_errors());
    auto dynamic_array_append_for_status = std::system(dynamic_array_append_for_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_append_for_status));
    assert(WEXITSTATUS(dynamic_array_append_for_status) == 16);

    auto dynamic_array_local_owned_cleanup_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_owned_cleanup.or";
    {
        auto local_owned_cleanup_source = std::ofstream(dynamic_array_local_owned_cleanup_path);
        local_owned_cleanup_source
            << "package demo.pipeline.dynamicarraylocalownedcleanup\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    let items: DynamicArray<Payload> = DynamicArray()\n"
            << "    1 as UInt32\n";
    }
    auto dynamic_array_local_owned_cleanup = pipeline.emit_llvm(
        dynamic_array_local_owned_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_local_owned_cleanup.has_errors());
    assert(dynamic_array_local_owned_cleanup.semantic_drop_lowering_authorizations.size() == 2);
    assert(dynamic_array_local_owned_cleanup.dynamic_array_runtime_request_report.size() == 2);
    assert(
        dynamic_array_local_owned_cleanup.ir_text.find("define void @__orison_drop.Payload(ptr %value)") !=
        std::string::npos
    );
    assert(
        dynamic_array_local_owned_cleanup.ir_text.find(
            "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_owned_cleanup.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );
    auto local_owned_drop = dynamic_array_local_owned_cleanup.ir_text.find("call void @__orison_drop.Payload");
    auto local_owned_deallocate =
        dynamic_array_local_owned_cleanup.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto local_owned_return = dynamic_array_local_owned_cleanup.ir_text.find("ret i32 1");
    assert(local_owned_drop != std::string::npos);
    assert(local_owned_deallocate != std::string::npos);
    assert(local_owned_return != std::string::npos);
    assert(local_owned_drop < local_owned_deallocate);
    assert(local_owned_deallocate < local_owned_return);

    auto dynamic_array_owned_parameter_initialized_run_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_initialized_run.or";
    {
        auto owned_parameter_initialized_run_source = std::ofstream(dynamic_array_owned_parameter_initialized_run_path);
        owned_parameter_initialized_run_source
            << "package demo.pipeline.dynamicarrayownedparameterinitializedrun\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    use_items(items)\n";
    }
    auto dynamic_array_owned_parameter_initialized_ir = pipeline.emit_llvm(
        dynamic_array_owned_parameter_initialized_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_initialized_ir.has_errors());
    auto initialized_transfer_deallocate =
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(initialized_transfer_deallocate != std::string::npos);
    assert(
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            initialized_transfer_deallocate + 1
        ) == std::string::npos
    );
    auto dynamic_array_owned_parameter_initialized_run = pipeline.emit_object(
        dynamic_array_owned_parameter_initialized_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_initialized_run.has_errors());
    assert(!dynamic_array_owned_parameter_initialized_run.object_bytes.empty());
    auto dynamic_array_owned_parameter_initialized_run_executable =
        smoke_temp_root / "dynamic_array_owned_parameter_initialized_run";
    auto dynamic_array_owned_parameter_initialized_run_link = orison::link::HostLinker {}.link(
        dynamic_array_owned_parameter_initialized_run.object_bytes,
        dynamic_array_owned_parameter_initialized_run_executable
    );
    assert(!dynamic_array_owned_parameter_initialized_run_link.has_errors());
    auto dynamic_array_owned_parameter_initialized_run_status =
        std::system(dynamic_array_owned_parameter_initialized_run_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_owned_parameter_initialized_run_status));
    assert(WEXITSTATUS(dynamic_array_owned_parameter_initialized_run_status) == 0);

    auto dynamic_array_owned_parameter_forwarding_run_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_forwarding_run.or";
    {
        auto forwarding_run_source = std::ofstream(dynamic_array_owned_parameter_forwarding_run_path);
        forwarding_run_source
            << "package demo.pipeline.dynamicarrayownedparameterforwardingrun\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function forward_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    use_items(items)\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    forward_items(items)\n";
    }
    auto dynamic_array_owned_parameter_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_owned_parameter_forwarding_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_forwarding_ir.has_errors());
    auto forwarding_deallocate =
        dynamic_array_owned_parameter_forwarding_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(forwarding_deallocate != std::string::npos);
    assert(
        dynamic_array_owned_parameter_forwarding_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            forwarding_deallocate + 1
        ) == std::string::npos
    );
    auto dynamic_array_owned_parameter_forwarding_run = pipeline.emit_object(
        dynamic_array_owned_parameter_forwarding_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_forwarding_run.has_errors());
    assert(!dynamic_array_owned_parameter_forwarding_run.object_bytes.empty());
    auto dynamic_array_owned_parameter_forwarding_run_executable =
        smoke_temp_root / "dynamic_array_owned_parameter_forwarding_run";
    auto dynamic_array_owned_parameter_forwarding_run_link = orison::link::HostLinker {}.link(
        dynamic_array_owned_parameter_forwarding_run.object_bytes,
        dynamic_array_owned_parameter_forwarding_run_executable
    );
    assert(!dynamic_array_owned_parameter_forwarding_run_link.has_errors());
    auto dynamic_array_owned_parameter_forwarding_run_status =
        std::system(dynamic_array_owned_parameter_forwarding_run_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_owned_parameter_forwarding_run_status));
    assert(WEXITSTATUS(dynamic_array_owned_parameter_forwarding_run_status) == 0);

    auto dynamic_array_owned_parameter_forwarding_reuse_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_forwarding_reuse.or";
    {
        auto forwarding_reuse_source = std::ofstream(dynamic_array_owned_parameter_forwarding_reuse_path);
        forwarding_reuse_source
            << "package demo.pipeline.dynamicarrayownedparameterforwardingreuse\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function forward_items(items: DynamicArray<Payload>) -> IntSize\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    items.length()\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    forward_items(items)\n";
    }
    auto dynamic_array_owned_parameter_forwarding_reuse = pipeline.emit_llvm(
        dynamic_array_owned_parameter_forwarding_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_forwarding_reuse.has_errors());
    assert(
        dynamic_array_owned_parameter_forwarding_reuse.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_parameter_branch_join_run_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_branch_join_run.or";
    {
        auto branch_join_source = std::ofstream(dynamic_array_owned_parameter_branch_join_run_path);
        branch_join_source
            << "package demo.pipeline.dynamicarrayownedparameterbranchjoinrun\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function choose(flag: Bool, items: DynamicArray<Payload>) -> UInt32\n"
            << "    if flag\n"
            << "        use_items(items)\n"
            << "    else\n"
            << "        use_items(items)\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    choose(true, items)\n";
    }
    auto dynamic_array_owned_parameter_branch_join_ir = pipeline.emit_llvm(
        dynamic_array_owned_parameter_branch_join_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_branch_join_ir.has_errors());
    auto branch_join_deallocate =
        dynamic_array_owned_parameter_branch_join_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(branch_join_deallocate != std::string::npos);
    assert(
        dynamic_array_owned_parameter_branch_join_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            branch_join_deallocate + 1
        ) == std::string::npos
    );
    auto dynamic_array_owned_parameter_branch_join_run = pipeline.emit_object(
        dynamic_array_owned_parameter_branch_join_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_branch_join_run.has_errors());
    assert(!dynamic_array_owned_parameter_branch_join_run.object_bytes.empty());
    auto dynamic_array_owned_parameter_branch_join_run_executable =
        smoke_temp_root / "dynamic_array_owned_parameter_branch_join_run";
    auto dynamic_array_owned_parameter_branch_join_run_link = orison::link::HostLinker {}.link(
        dynamic_array_owned_parameter_branch_join_run.object_bytes,
        dynamic_array_owned_parameter_branch_join_run_executable
    );
    assert(!dynamic_array_owned_parameter_branch_join_run_link.has_errors());
    auto dynamic_array_owned_parameter_branch_join_run_status =
        std::system(dynamic_array_owned_parameter_branch_join_run_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_owned_parameter_branch_join_run_status));
    assert(WEXITSTATUS(dynamic_array_owned_parameter_branch_join_run_status) == 0);

    auto dynamic_array_owned_parameter_branch_mismatch_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_branch_mismatch.or";
    {
        auto branch_mismatch_source = std::ofstream(dynamic_array_owned_parameter_branch_mismatch_path);
        branch_mismatch_source
            << "package demo.pipeline.dynamicarrayownedparameterbranchmismatch\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function choose(flag: Bool, items: DynamicArray<Payload>) -> UInt32\n"
            << "    if flag\n"
            << "        use_items(items)\n"
            << "    else\n"
            << "        0 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    choose(true, items)\n";
    }
    auto dynamic_array_owned_parameter_branch_mismatch = pipeline.emit_llvm(
        dynamic_array_owned_parameter_branch_mismatch_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_branch_mismatch.has_errors());
    assert(
        dynamic_array_owned_parameter_branch_mismatch.error_text.find(
            "if branch ownership mismatch: owned transfers must match across all continuing branches"
        ) != std::string::npos
    );

    auto dynamic_array_owned_parameter_statement_branch_mismatch_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_statement_branch_mismatch.or";
    {
        auto statement_branch_mismatch_source =
            std::ofstream(dynamic_array_owned_parameter_statement_branch_mismatch_path);
        statement_branch_mismatch_source
            << "package demo.pipeline.dynamicarrayownedparameterstatementbranchmismatch\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    if true\n"
            << "        let result: UInt32 = use_items(items)\n"
            << "    else\n"
            << "        let result: UInt32 = 0 as UInt32\n"
            << "    items.length()\n";
    }
    auto dynamic_array_owned_parameter_statement_branch_mismatch = pipeline.emit_llvm(
        dynamic_array_owned_parameter_statement_branch_mismatch_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_statement_branch_mismatch.has_errors());
    assert(
        dynamic_array_owned_parameter_statement_branch_mismatch.error_text.find(
            "if branch ownership mismatch: owned transfers must match across all continuing branches"
        ) != std::string::npos
    );

    auto dynamic_array_owned_parameter_second_use_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_second_use.or";
    {
        auto second_use_source = std::ofstream(dynamic_array_owned_parameter_second_use_path);
        second_use_source
            << "package demo.pipeline.dynamicarrayownedparameterseconduse\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    use_items(items)\n";
    }
    auto dynamic_array_owned_parameter_second_use = pipeline.emit_llvm(
        dynamic_array_owned_parameter_second_use_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_second_use.has_errors());
    assert(
        dynamic_array_owned_parameter_second_use.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_parameter_length_after_move_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_length_after_move.or";
    {
        auto length_after_move_source = std::ofstream(dynamic_array_owned_parameter_length_after_move_path);
        length_after_move_source
            << "package demo.pipeline.dynamicarrayownedparameterlengthaftermove\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    items.length()\n";
    }
    auto dynamic_array_owned_parameter_length_after_move = pipeline.emit_llvm(
        dynamic_array_owned_parameter_length_after_move_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_length_after_move.has_errors());
    assert(
        dynamic_array_owned_parameter_length_after_move.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_parameter_push_after_move_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_push_after_move.or";
    {
        auto push_after_move_source = std::ofstream(dynamic_array_owned_parameter_push_after_move_path);
        push_after_move_source
            << "package demo.pipeline.dynamicarrayownedparameterpushaftermove\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    items.push(Payload(8))\n"
            << "    result\n";
    }
    auto dynamic_array_owned_parameter_push_after_move = pipeline.emit_llvm(
        dynamic_array_owned_parameter_push_after_move_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_push_after_move.has_errors());
    assert(
        dynamic_array_owned_parameter_push_after_move.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_production_ready = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .dynamic_array_parameter_lowering_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_owned_production_ready.has_errors());
    assert(dynamic_array_owned_production_ready.drop_readiness_summary.cleanup_authorized == 1);
    assert(dynamic_array_owned_production_ready.drop_readiness_summary.cleanup_blocked == 0);
    assert(dynamic_array_owned_production_ready.dynamic_array_runtime_request_report.size() == 1);
    assert_line_contains(
        dynamic_array_owned_production_ready.dynamic_array_runtime_request_report,
        0,
        "dynamic array runtime __orison_dynamic_array_deallocate"
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find("define i32 @use_items({ ptr, i64, i64 } %items)") !=
        std::string::npos
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find("declare void @__orison_drop.Payload(ptr)") !=
        std::string::npos
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find(
            "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );
    auto parameter_owned_drop = dynamic_array_owned_production_ready.ir_text.find(
        "call void @__orison_drop.Payload"
    );
    auto parameter_owned_deallocate = dynamic_array_owned_production_ready.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto parameter_owned_return = dynamic_array_owned_production_ready.ir_text.find("ret i32 1");
    assert(parameter_owned_drop != std::string::npos);
    assert(parameter_owned_deallocate != std::string::npos);
    assert(parameter_owned_return != std::string::npos);
    assert(parameter_owned_drop < parameter_owned_deallocate);
    assert(parameter_owned_deallocate < parameter_owned_return);

    auto dynamic_array_authorized_readiness = pipeline.emit_llvm(
        dynamic_array_drop_report_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = dynamic_array_source_owner.semantic_result.planned_drop_sites[1],
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .owner_name = "items",
                    .initial_capacity = 2,
                },
            },
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );
    assert(!dynamic_array_authorized_readiness.has_errors());
    assert(dynamic_array_authorized_readiness.emitted_drop_declaration_report.size() == 1);
    assert_line_contains(
        dynamic_array_authorized_readiness.emitted_drop_declaration_report,
        0,
        "__orison_drop.Payload"
    );
    assert(dynamic_array_authorized_readiness.drop_cleanup_authorization_report.empty());
    assert(dynamic_array_authorized_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 1);
    assert(
        dynamic_array_authorized_readiness.drop_readiness_snapshot.semantic_authorizations.front().site.owner_name ==
        "items.element"
    );
    assert(dynamic_array_authorized_readiness.drop_readiness_snapshot.emitted_declarations.size() == 1);
    assert(dynamic_array_authorized_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(dynamic_array_authorized_readiness.planned_drop_action_report.size() == 1);
    assert_line_contains(dynamic_array_authorized_readiness.planned_drop_action_report, 0, "capture items.element");
    assert(dynamic_array_authorized_readiness.drop_readiness_summary.semantic_authorized == 1);
    assert(dynamic_array_authorized_readiness.drop_readiness_summary.cleanup_authorized == 1);
    assert(dynamic_array_authorized_readiness.drop_readiness_summary.cleanup_blocked == 0);
    assert(dynamic_array_authorized_readiness.drop_readiness_snapshot_report.size() == 4);
    assert_line_contains(
        dynamic_array_authorized_readiness.drop_readiness_snapshot_report,
        3,
        "__orison_dynamic_array_cleanup.0 authorized"
    );
    assert(dynamic_array_authorized_readiness.drop_readiness_relation_report.size() == 1);
    assert_line_contains(
        dynamic_array_authorized_readiness.drop_readiness_relation_report,
        0,
        "__orison_dynamic_array_cleanup.0 authorized"
    );
    assert(dynamic_array_authorized_readiness.drop_readiness_blocker_summary.blocked_cleanups == 0);
    assert(dynamic_array_authorized_readiness.drop_readiness_source_correlation_report.size() == 1);
    assert(
        dynamic_array_authorized_readiness.drop_readiness_source_correlation_report.front() ==
        "drop readiness source correlations actions 0 semantic sites 1"
    );
    assert(dynamic_array_authorized_readiness.ir_text.find("declare void @__orison_drop.Payload") != std::string::npos);
    assert(dynamic_array_authorized_readiness.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto multi_drop_readiness_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness_multi.or";
    auto multi_drop_readiness = pipeline.emit_llvm(multi_drop_readiness_path);
    assert(!multi_drop_readiness.has_errors());
    assert(multi_drop_readiness.planned_drop_report.size() == 2);
    assert_line_contains(multi_drop_readiness.planned_drop_report, 0, "__orison_drop.Payload");
    assert_line_contains(multi_drop_readiness.planned_drop_report, 1, "__orison_drop.OtherPayload");
    assert(multi_drop_readiness.planned_drop_action_report.size() == 2);
    assert_line_contains(multi_drop_readiness.planned_drop_action_report, 0, "capture payload: Payload");
    assert_line_contains(multi_drop_readiness.planned_drop_action_report, 1, "capture other: OtherPayload");
    assert(multi_drop_readiness.drop_cleanup_authorization_report.size() == 7);
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[0].find(
            "__orison_thread_cleanup.launch.20.0 blocked"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[1].find(
            "semantic drop lowering blocked __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[2].find(
            "semantic drop lowering blocked __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[3].find(
            "semantic drop unresolved __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[4].find(
            "semantic drop unresolved __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[5].find(
            "missing drop declaration __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[6].find(
            "missing drop declaration __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(multi_drop_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 2);
    assert(multi_drop_readiness.drop_readiness_snapshot.emitted_declarations.empty());
    assert(multi_drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(multi_drop_readiness.drop_readiness_snapshot_report.size() == 4);
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[0].find("semantic authorizations 2") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[2].find("__orison_drop.OtherPayload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[3].find(
            "__orison_thread_cleanup.launch.20.0 blocked"
        ) != std::string::npos
    );
    assert(multi_drop_readiness.drop_readiness_summary.semantic_authorized == 0);
    assert(multi_drop_readiness.drop_readiness_summary.semantic_blocked == 2);
    assert(multi_drop_readiness.drop_readiness_summary.emitted_declarations == 0);
    assert(multi_drop_readiness.drop_readiness_summary.cleanup_authorized == 0);
    assert(multi_drop_readiness.drop_readiness_summary.cleanup_blocked == 1);
    assert(multi_drop_readiness.drop_readiness_summary_report.size() == 1);
    assert(
        multi_drop_readiness.drop_readiness_summary_report.front().find("semantic authorized 0 blocked 2") !=
        std::string::npos
    );
    assert(multi_drop_readiness.drop_readiness_relation_report.size() == 5);
    assert(
        multi_drop_readiness.drop_readiness_relation_report[0].find(
            "__orison_thread_cleanup.launch.20.0 blocked"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[2].find("__orison_drop.OtherPayload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[3].find(
            "missing declaration __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[4].find(
            "missing declaration __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(multi_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(multi_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_report.size() == 7);
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 2 semantic unresolved 2 "
        "source lowering blocked 0 missing declarations 2"
    );
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[2].find("__orison_drop.OtherPayload") !=
        std::string::npos
    );

    auto failed_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_summary_failure.or";
    {
        std::ofstream source(failed_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function same(left: Bool, right: Bool) -> Bool\n";
        source << "    left < right\n";
    }
    auto failed_lowering = pipeline.emit_llvm(failed_lowering_path);
    assert(failed_lowering.has_errors());
    assert(
        failed_lowering.error_text.find(
            "lowering does not yet support this return expression: unsupported operator: <"
        ) != std::string::npos
    );
    assert(failed_lowering.drop_readiness_snapshot_report.empty());
    assert(failed_lowering.drop_readiness_summary_report.empty());
    assert(failed_lowering.drop_readiness_relation_report.empty());
    assert(failed_lowering.drop_readiness_blocker_report.empty());

    auto failed_unary_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_unary_failure.or";
    {
        std::ofstream source(failed_unary_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function negate(value: UInt32) -> UInt32\n";
        source << "    -value\n";
    }
    auto failed_unary_lowering = pipeline.emit_llvm(failed_unary_lowering_path);
    assert(failed_unary_lowering.has_errors());
    assert(
        failed_unary_lowering.error_text.find(
            "lowering does not yet support this return expression: unsupported operator: -"
        ) != std::string::npos
    );
    assert(failed_unary_lowering.drop_readiness_snapshot_report.empty());
    assert(failed_unary_lowering.drop_readiness_summary_report.empty());
    assert(failed_unary_lowering.drop_readiness_relation_report.empty());
    assert(failed_unary_lowering.drop_readiness_blocker_report.empty());

    auto failed_cast_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_cast_failure.or";
    {
        std::ofstream source(failed_cast_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function main() -> UInt32\n";
        source << "    -1 as UInt32\n";
    }
    auto failed_cast_lowering = pipeline.emit_llvm(failed_cast_lowering_path);
    assert(failed_cast_lowering.has_errors());
    assert(
        failed_cast_lowering.error_text.find(
            "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
        ) != std::string::npos
    );
    assert(failed_cast_lowering.drop_readiness_snapshot_report.empty());
    assert(failed_cast_lowering.drop_readiness_summary_report.empty());
    assert(failed_cast_lowering.drop_readiness_relation_report.empty());
    assert(failed_cast_lowering.drop_readiness_blocker_report.empty());

    auto failed_final_if_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_final_if_failure.or";
    {
        std::ofstream source(failed_final_if_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n";
        source << "    if flag\n";
        source << "        left < right\n";
        source << "    else\n";
        source << "        false\n";
    }
    auto failed_final_if_lowering = pipeline.emit_llvm(failed_final_if_lowering_path);
    assert(failed_final_if_lowering.has_errors());
    assert(
        failed_final_if_lowering.error_text.find(
            "lowering does not yet support this final control-flow statement: "
            "if then arm lowering failed: unsupported operator: <"
        ) != std::string::npos
    );
    assert(failed_final_if_lowering.drop_readiness_snapshot_report.empty());
    assert(failed_final_if_lowering.drop_readiness_summary_report.empty());
    assert(failed_final_if_lowering.drop_readiness_relation_report.empty());
    assert(failed_final_if_lowering.drop_readiness_blocker_report.empty());

    auto failed_final_switch_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_final_switch_failure.or";
    {
        std::ofstream source(failed_final_switch_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n";
        source << "    switch flag\n";
        source << "        true => left < right\n";
        source << "        false => false\n";
    }
    auto failed_final_switch_lowering = pipeline.emit_llvm(failed_final_switch_lowering_path);
    assert(failed_final_switch_lowering.has_errors());
    assert(
        failed_final_switch_lowering.error_text.find(
            "lowering does not yet support this final control-flow statement: "
            "switch case lowering failed: unsupported operator: <"
        ) != std::string::npos
    );
    assert(failed_final_switch_lowering.drop_readiness_snapshot_report.empty());
    assert(failed_final_switch_lowering.drop_readiness_summary_report.empty());
    assert(failed_final_switch_lowering.drop_readiness_relation_report.empty());
    assert(failed_final_switch_lowering.drop_readiness_blocker_report.empty());

    auto object = pipeline.emit_object(source_path);
    assert(!object.has_errors());
    assert(!object.object_bytes.empty());

    auto missing = pipeline.analyze(source_path.parent_path() / "missing.or");
    assert(missing.has_errors());
    assert(missing.error_text == "error: unable to read source file\n");

    auto library_path = std::filesystem::temp_directory_path() / "orison_pipeline_libraries.or";
    {
        std::ofstream source(library_path);
        source << "package demo.libraries\n";
        source << "package foreign \"c\" library \"m\"\n";
        source << "    function first(value: Int32) -> Int32\n";
        source << "package foreign \"c\" library \"m\"\n";
        source << "    function second(value: Int32) -> Int32\n";
        source << "function main() -> Int32\n";
        source << "    0 as Int32\n";
    }
    auto libraries = pipeline.analyze(library_path);
    assert(!libraries.has_errors());
    assert(libraries.link_libraries.size() == 1);
    assert(libraries.link_libraries.front() == "m");

    auto semantic_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "semantic_planned_drop.or";
    auto semantic_drops = pipeline.analyze(semantic_drop_path);
    assert(!semantic_drops.has_errors());
    assert(semantic_drops.semantic_planned_drop_report.size() == 2);
    assert_line_contains(semantic_drops.semantic_planned_drop_report, 0, "owner input");
    assert_line_contains(semantic_drops.semantic_planned_drop_report, 1, "owner local");
    assert(semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert_line_contains(semantic_drops.semantic_drop_resolution_report, 0, "missing drop site");
    assert_line_contains(semantic_drops.semantic_drop_resolution_report, 1, "owner local");
    assert(semantic_drops.semantic_drop_diagnostic_report.size() == 2);
    assert_line_contains(semantic_drops.semantic_drop_diagnostic_report, 0, "blocked no implementation discovered");
    assert_line_contains(semantic_drops.semantic_drop_diagnostic_report, 1, "owner local");
    assert(semantic_drops.semantic_drop_lowering_authorization_report.size() == 2);
    assert(semantic_drops.semantic_drop_lowering_authorizations.size() == 2);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].semantic_resolved);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].source_drop_lowering_enabled);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].authorized);
    assert_line_contains(
        semantic_drops.semantic_drop_lowering_authorization_report,
        0,
        "semantic-unresolved lowering-blocked"
    );
    assert(semantic_drops.semantic_drop_resolution_summary_report.size() == 1);
    assert_line_contains(semantic_drops.semantic_drop_resolution_summary_report, 0, "resolved 0 missing 2");

    auto parsed_drop_path = std::filesystem::temp_directory_path() / "orison_pipeline_parsed_drop_candidate.or";
    {
        std::ofstream source(parsed_drop_path);
        source << "package demo.parseddrop\n";
        source << "record Payload\n";
        source << "    public value: Int64\n";
        source << "interface Drop\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "implements Drop for Payload\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "        return\n";
        source << "function read(input: Payload) -> Int64\n";
        source << "    input.value\n";
    }
    auto parsed_drop = pipeline.analyze(parsed_drop_path);
    assert(!parsed_drop.has_errors());
    assert(parsed_drop.semantic_planned_drop_report.size() == 1);
    assert_line_contains(parsed_drop.semantic_planned_drop_report, 0, "owner input");
    assert(parsed_drop.semantic_drop_implementation_report.size() == 1);
    assert_line_contains(parsed_drop.semantic_drop_implementation_report, 0, "parsed-candidate-collection");
    assert(parsed_drop.semantic_drop_resolution_report.size() == 1);
    assert_line_contains(parsed_drop.semantic_drop_resolution_report, 0, "resolved drop site");
    assert(parsed_drop.semantic_drop_diagnostic_report.size() == 1);
    assert_line_contains(parsed_drop.semantic_drop_diagnostic_report, 0, "resolved");
    assert(parsed_drop.semantic_drop_lowering_authorization_report.size() == 1);
    assert(parsed_drop.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop.semantic_drop_lowering_authorizations.front().authorized);
    assert_line_contains(
        parsed_drop.semantic_drop_lowering_authorization_report,
        0,
        "semantic-resolved lowering-blocked"
    );
    auto parsed_drop_ir = pipeline.emit_llvm(parsed_drop_path);
    assert(!parsed_drop_ir.has_errors());
    assert(parsed_drop_ir.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_ir.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop_ir.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop_ir.semantic_drop_lowering_authorizations.front().authorized);
    assert(parsed_drop_ir.ir_text.find("__orison_drop.Payload") == std::string::npos);
    auto parsed_drop_source_lowering_ir = pipeline.emit_llvm(
        parsed_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!parsed_drop_source_lowering_ir.has_errors());
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.front().authorized);
    assert(
        parsed_drop_source_lowering_ir.ir_text.find("define void @__orison_drop.Payload(ptr %value)") !=
        std::string::npos
    );

    auto parsed_drop_readiness_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_parsed_drop_readiness.or";
    {
        std::ofstream source(parsed_drop_readiness_path);
        source << "package demo.parseddropreadiness\n";
        source << "record Payload\n";
        source << "    public value: Int64\n";
        source << "interface Drop\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "implements Transferable for Payload\n";
        source << "    function placeholder(this: shared This) -> Unit\n";
        source << "        return\n";
        source << "implements Drop for Payload\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "        return\n";
        source << "function launch(value: Int64) -> Int64\n";
        source << "    let payload: Payload = Payload(value)\n";
        source << "    let worker = thread\n";
        source << "        payload.value\n";
        source << "\n";
        source << "    worker.join()\n";
    }
    auto parsed_drop_readiness = pipeline.emit_llvm(parsed_drop_readiness_path);
    assert(!parsed_drop_readiness.has_errors());
    assert(parsed_drop_readiness.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_readiness.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop_readiness.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop_readiness.semantic_drop_lowering_authorizations.front().authorized);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.empty());
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_report.size() == 4);
    assert(
        parsed_drop_readiness.drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 0 "
        "source lowering blocked 1 missing declarations 1"
    );
    assert(
        parsed_drop_readiness.drop_readiness_blocker_report[2].find("source lowering not accepted") !=
        std::string::npos
    );
    assert(parsed_drop_readiness.drop_readiness_source_correlation_report.size() == 2);
    assert(
        parsed_drop_readiness.drop_readiness_source_correlation_report[0] ==
        "drop readiness source correlations actions 1 semantic sites 1"
    );
    assert(
        parsed_drop_readiness.drop_readiness_source_correlation_report[1].find("semantic resolved") !=
        std::string::npos
    );
    assert(parsed_drop_readiness.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto resolved_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    }
                ),
            },
        }
    );
    assert(!resolved_semantic_drops.has_errors());
    assert(resolved_semantic_drops.semantic_drop_implementation_report.size() == 1);
    assert_line_contains(resolved_semantic_drops.semantic_drop_implementation_report, 0, "discovery test-injection");
    assert(resolved_semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert_line_contains(resolved_semantic_drops.semantic_drop_resolution_report, 0, "resolved drop site");
    assert_line_contains(resolved_semantic_drops.semantic_drop_resolution_report, 1, "owner local");
    assert(resolved_semantic_drops.semantic_drop_diagnostic_report.size() == 2);
    assert_line_contains(resolved_semantic_drops.semantic_drop_diagnostic_report, 0, "resolved");
    assert_line_contains(resolved_semantic_drops.semantic_drop_diagnostic_report, 1, "owner local");
    assert(resolved_semantic_drops.semantic_drop_lowering_authorization_report.size() == 2);
    assert(resolved_semantic_drops.semantic_drop_lowering_authorizations.size() == 2);
    assert(resolved_semantic_drops.semantic_drop_lowering_authorizations[0].semantic_resolved);
    assert(!resolved_semantic_drops.semantic_drop_lowering_authorizations[0].source_drop_lowering_enabled);
    assert(!resolved_semantic_drops.semantic_drop_lowering_authorizations[0].authorized);
    assert_line_contains(
        resolved_semantic_drops.semantic_drop_lowering_authorization_report,
        0,
        "semantic-resolved lowering-blocked"
    );
    assert(resolved_semantic_drops.semantic_drop_resolution_summary_report.size() == 1);
    assert_line_contains(resolved_semantic_drops.semantic_drop_resolution_summary_report, 0, "resolved 2 missing 0");

    auto candidate_resolved_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementation_candidates = {
                orison::semantics::DropImplementationCandidate {
                    .source_type_name = "Payload",
                    .declaration_line = 3,
                    .body = orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    },
                },
                orison::semantics::DropImplementationCandidate {
                    .source_type_name = "Payload",
                    .declaration_line = 4,
                    .body = orison::semantics::DropImplementationBodySummary {},
                },
            },
        }
    );
    assert(!candidate_resolved_semantic_drops.has_errors());
    assert(candidate_resolved_semantic_drops.semantic_drop_implementation_report.size() == 1);
    assert_line_contains(
        candidate_resolved_semantic_drops.semantic_drop_implementation_report,
        0,
        "discovery candidate-collection"
    );
    assert(candidate_resolved_semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert_line_contains(candidate_resolved_semantic_drops.semantic_drop_resolution_report, 0, "resolved drop site");
    assert_line_contains(candidate_resolved_semantic_drops.semantic_drop_resolution_report, 1, "owner local");
    assert(candidate_resolved_semantic_drops.semantic_drop_resolution_summary_report.size() == 1);
    assert_line_contains(
        candidate_resolved_semantic_drops.semantic_drop_resolution_summary_report,
        0,
        "resolved 2 missing 0"
    );

    auto unproven_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {}
                ),
            },
        }
    );
    assert(!unproven_semantic_drops.has_errors());
    assert(unproven_semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert_line_contains(unproven_semantic_drops.semantic_drop_resolution_report, 0, "missing drop site");
    assert_line_contains(unproven_semantic_drops.semantic_drop_resolution_report, 1, "owner local");
    assert(unproven_semantic_drops.semantic_drop_diagnostic_report.size() == 2);
    assert_line_contains(unproven_semantic_drops.semantic_drop_diagnostic_report, 0, "discovered but unproven");
    assert_line_contains(unproven_semantic_drops.semantic_drop_diagnostic_report, 1, "owner local");

    auto partial_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "semantic_partial_drop_resolution.or";
    auto partial_semantic_drops = pipeline.analyze(
        partial_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    }
                ),
            },
        }
    );
    assert(!partial_semantic_drops.has_errors());
    assert(partial_semantic_drops.semantic_drop_resolution_report.size() == 4);
    assert_line_contains(partial_semantic_drops.semantic_drop_resolution_report, 0, "resolved drop site");
    assert_line_contains(partial_semantic_drops.semantic_drop_resolution_report, 1, "__orison_drop.Resource");
    assert_line_contains(partial_semantic_drops.semantic_drop_resolution_report, 2, "owner local_payload");
    assert_line_contains(partial_semantic_drops.semantic_drop_resolution_report, 3, "owner local_resource");
    assert(partial_semantic_drops.semantic_drop_resolution_summary_report.size() == 2);
    assert_line_contains(partial_semantic_drops.semantic_drop_resolution_summary_report, 0, "resolved 2 missing 0");
    assert_line_contains(partial_semantic_drops.semantic_drop_resolution_summary_report, 1, "resolved 0 missing 2");
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
