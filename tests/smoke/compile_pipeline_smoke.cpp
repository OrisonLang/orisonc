#include "orison/pipeline/compile_pipeline.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>

namespace {

void assert_line_contains(
    std::vector<std::string> const& lines,
    std::size_t index,
    std::string_view expected_fragment
) {
    assert(index < lines.size());
    assert(lines[index].find(expected_fragment) != std::string::npos);
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

    auto dynamic_array_source_correlated_cleanup = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
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
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
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
        failed_lowering.error_text.find("lowering does not yet support this return expression") != std::string::npos
    );
    assert(failed_lowering.drop_readiness_snapshot_report.empty());
    assert(failed_lowering.drop_readiness_summary_report.empty());
    assert(failed_lowering.drop_readiness_relation_report.empty());
    assert(failed_lowering.drop_readiness_blocker_report.empty());

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
