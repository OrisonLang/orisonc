#include "computed_dynamic_array_audit_expectations.hpp"

#include "../../compiler/driver/src/computed_cleanup_reports.hpp"
#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace {

namespace smoke = orison::tests::smoke;

void test_dynamic_array_descriptor_lifetime_plan_origin_blocker_report() {
    auto state = orison::pipeline::DynamicArrayDescriptorLifetimePlanState {
        .summary_blockers = {
            orison::pipeline::DynamicArrayDescriptorSummaryBlocker {
                .owner_name = "items",
                .source_type_name = "DynamicArray<Payload>",
                .element_source_type_name = "Payload",
                .binding_kind = orison::semantics::DynamicArrayDescriptorBindingKind::parameter_binding,
                .reason = "cleanup-plan-missing",
                .source_line = 12,
            },
            orison::pipeline::DynamicArrayDescriptorSummaryBlocker {
                .owner_name = "other",
                .source_type_name = "DynamicArray<Box<UInt32>>",
                .element_source_type_name = "Box<UInt32>",
                .binding_kind = orison::semantics::DynamicArrayDescriptorBindingKind::local_binding,
                .reason = "semantic-origin-mismatched",
                .source_line = 18,
            },
        },
        .all_summaries_have_cleanup_plans = false,
        .all_cleanup_plans_have_summaries = false,
    };

    auto report = orison::driver::dynamic_array_descriptor_lifetime_plan_state_report(state);
    assert(report.size() == 3);
    assert(report[0] == "dynamic array descriptor lifetime plan blocked origins 0 origin-blockers 2");
    assert(
        report[1] ==
        "dynamic array descriptor lifetime blocker cleanup-plan-missing DynamicArray<Payload> owner items "
        "element Payload origin parameter line 12 (metadata only)"
    );
    assert(
        report[2] ==
        "dynamic array descriptor lifetime blocker semantic-origin-mismatched DynamicArray<Box<UInt32>> owner other "
        "element Box<UInt32> origin local line 18 (metadata only)"
    );
}

void write_fixture(
    std::filesystem::path const& path,
    std::string_view package_name,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package " << package_name << "\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

auto run_single_file_command(
    orison::driver::CompilerApp const& app,
    std::string_view command,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        command.data(),
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_emit_llvm(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--emit-llvm", path);
}

auto run_planned_drops(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--planned-drops", path);
}

auto run_semantic_planned_drops(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--semantic-planned-drops", path);
}

auto run_semantic_drop_resolution(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--semantic-drop-resolution", path);
}

auto run_semantic_drop_diagnostics(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--semantic-drop-diagnostics", path);
}

auto run_semantic_drop_lowering_authorization(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--semantic-drop-lowering-authorization", path);
}

auto run_semantic_drop_summary(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--semantic-drop-summary", path);
}

auto run_semantic_dynamic_array_descriptor_origins(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--semantic-dynamic-array-descriptor-origins", path);
}

auto run_planned_drop_actions(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--planned-drop-actions", path);
}

auto run_emitted_drops(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--emitted-drops", path);
}

auto run_drop_cleanup_authorization(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--drop-cleanup-authorization", path);
}

auto run_drop_readiness(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--drop-readiness", path);
}

auto run_drop_readiness_summary(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--drop-readiness-summary", path);
}

auto run_drop_readiness_relations(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--drop-readiness-relations", path);
}

auto run_drop_readiness_blockers(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--drop-readiness-blockers", path);
}

auto run_drop_readiness_source_correlations(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--drop-readiness-source-correlations", path);
}

auto run_dynamic_array_descriptor_cleanup_plan(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-descriptor-cleanup-plan", path);
}

auto run_dynamic_array_descriptor_lifetime_plan(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-descriptor-lifetime-plan", path);
}

auto run_dynamic_array_cleanup_obligations(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-cleanup-obligations", path);
}

auto run_dynamic_array_cleanup_sequence_plan(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-cleanup-sequence-plan", path);
}

auto run_dynamic_array_cleanup_sequence_verification(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-cleanup-sequence-verification", path);
}

auto run_dynamic_array_cleanup_emission_gate(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-cleanup-emission-gate", path);
}

auto run_dynamic_array_cleanup_capability(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-cleanup-capability", path);
}

auto run_computed_dynamic_array_cleanup_call_insertion_capability(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--computed-dynamic-array-cleanup-call-insertion-capability", path);
}

auto run_computed_dynamic_array_cleanup_call_insertion_readiness(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--computed-dynamic-array-cleanup-call-insertion-readiness", path);
}

auto run_computed_dynamic_array_inserted_cleanup_handoffs(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--computed-dynamic-array-inserted-cleanup-handoffs", path);
}

auto run_computed_dynamic_array_inserted_cleanup_calls(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--computed-dynamic-array-inserted-cleanup-calls", path);
}

auto run_computed_dynamic_array_consumed_cleanup_descriptors(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--computed-dynamic-array-consumed-cleanup-descriptors", path);
}

auto run_computed_dynamic_array_cleanup_proof_summary(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--computed-dynamic-array-cleanup-proof-summary", path);
}

auto run_dynamic_array_cleanup_production_readiness(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-cleanup-production-readiness", path);
}

auto run_dynamic_array_cleanup_audit(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--dynamic-array-cleanup-audit", path);
}

void assert_failure_with_no_stdout_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_message) != std::string::npos);
}

void assert_success_with_stdout_contains(
    orison::driver::CompileResult const& result,
    std::initializer_list<std::string_view> expected_fragments
) {
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    for (auto expected_fragment : expected_fragments) {
        assert(result.stdout_text.find(expected_fragment) != std::string::npos);
    }
}

void assert_success_with_stdout_contains_in_order(
    orison::driver::CompileResult const& result,
    std::initializer_list<std::string_view> expected_fragments
) {
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    auto search_offset = std::size_t {0};
    for (auto expected_fragment : expected_fragments) {
        auto found = result.stdout_text.find(expected_fragment, search_offset);
        assert(found != std::string::npos);
        search_offset = found + expected_fragment.size();
    }
}

void assert_computed_dynamic_array_later_owner_use_readiness(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) {
    auto readiness = run_computed_dynamic_array_cleanup_call_insertion_readiness(app, path);
    assert_success_with_stdout_contains(
        readiness,
        {
            "computed DynamicArray cleanup call insertion readiness blocked gates 2 ready 1 blocked 1 "
            "cleanup-blockers 1",
            "cleanup-blocked-reason later owner use",
        }
    );
}

void assert_computed_dynamic_array_final_use_cleanup_reports(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& path
) {
    auto inserted_cleanup_calls = run_computed_dynamic_array_inserted_cleanup_calls(app, path);
    assert_success_with_stdout_contains(
        inserted_cleanup_calls,
        {
            smoke::computed_dynamic_array_inserted_cleanup_call_state_inserted_report,
            "computed DynamicArray inserted cleanup call detail owner items data %items.computed_for.",
        }
    );

    auto consumed_cleanup_descriptors = run_computed_dynamic_array_consumed_cleanup_descriptors(app, path);
    assert_success_with_stdout_contains(
        consumed_cleanup_descriptors,
        {
            smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_finalized_report,
            smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_detail_report,
        }
    );
}

void assert_success_with_empty_stdout(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

}  // namespace

int main() {
    test_dynamic_array_descriptor_lifetime_plan_origin_blocker_report();

    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_drop_report_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::driver::CompilerApp app;
    auto fixture_root = std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures";
    auto dynamic_array_returned_payload_path = fixture_root / "choice_dynamic_array_return_payload_run.or";

    auto clean_emit_path = std::filesystem::temp_directory_path() / "orison_driver_drop_report_clean_emit.or";
    write_fixture(
        clean_emit_path,
        "demo.emit",
        {
            "function main() -> UInt32",
            "    42 as UInt32",
        }
    );

    auto emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_emit_failure.or";
    write_fixture(
        emit_failure_path,
        "demo.emit",
        {
            "function same(left: Bool, right: Bool) -> Bool",
            "    left < right",
        }
    );
    auto drop_readiness_summary_failure = run_drop_readiness_summary(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        drop_readiness_summary_failure,
        "lowering does not yet support this return expression: unsupported operator: <"
    );
    auto emitted_drops_failure = run_emitted_drops(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        emitted_drops_failure,
        "lowering does not yet support this return expression: unsupported operator: <"
    );
    auto dynamic_array_cleanup_sequence_verification_failure =
        run_dynamic_array_cleanup_sequence_verification(app, emit_failure_path);
    assert_success_with_empty_stdout(dynamic_array_cleanup_sequence_verification_failure);
    auto dynamic_array_cleanup_emission_gate_failure =
        run_dynamic_array_cleanup_emission_gate(app, emit_failure_path);
    assert_success_with_empty_stdout(dynamic_array_cleanup_emission_gate_failure);
    auto dynamic_array_descriptor_cleanup_plan_failure =
        run_dynamic_array_descriptor_cleanup_plan(app, emit_failure_path);
    assert_success_with_empty_stdout(dynamic_array_descriptor_cleanup_plan_failure);
    auto dynamic_array_cleanup_obligations_failure =
        run_dynamic_array_cleanup_obligations(app, emit_failure_path);
    assert_success_with_empty_stdout(dynamic_array_cleanup_obligations_failure);
    auto dynamic_array_cleanup_sequence_plan_failure =
        run_dynamic_array_cleanup_sequence_plan(app, emit_failure_path);
    assert_success_with_empty_stdout(dynamic_array_cleanup_sequence_plan_failure);
    auto dynamic_array_cleanup_capability_failure =
        run_dynamic_array_cleanup_capability(app, emit_failure_path);
    assert_success_with_stdout_contains(
        dynamic_array_cleanup_capability_failure,
        {"dynamic array cleanup emission capability proven"}
    );
    auto dynamic_array_cleanup_audit_failure =
        run_dynamic_array_cleanup_audit(app, emit_failure_path);
    assert_success_with_stdout_contains(
        dynamic_array_cleanup_audit_failure,
        {"dynamic array cleanup production readiness blocked"}
    );
    auto dynamic_array_cleanup_production_readiness_failure =
        run_dynamic_array_cleanup_production_readiness(app, emit_failure_path);
    assert_success_with_stdout_contains(
        dynamic_array_cleanup_production_readiness_failure,
        {"dynamic array cleanup production readiness blocked"}
    );
    auto drop_readiness_relations_failure = run_drop_readiness_relations(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        drop_readiness_relations_failure,
        "lowering does not yet support this return expression: unsupported operator: <"
    );

    auto unary_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_unary_failure.or";
    write_fixture(
        unary_emit_failure_path,
        "demo.emit",
        {
            "function negate(value: UInt32) -> UInt32",
            "    -value",
        }
    );
    auto unary_drop_readiness_summary_failure =
        run_drop_readiness_summary(app, unary_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        unary_drop_readiness_summary_failure,
        "lowering does not yet support this return expression: unsupported operator: -"
    );
    auto unary_emitted_drops_failure = run_emitted_drops(app, unary_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        unary_emitted_drops_failure,
        "lowering does not yet support this return expression: unsupported operator: -"
    );
    auto unary_drop_readiness_relations_failure =
        run_drop_readiness_relations(app, unary_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        unary_drop_readiness_relations_failure,
        "lowering does not yet support this return expression: unsupported operator: -"
    );

    auto cast_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_cast_failure.or";
    write_fixture(
        cast_emit_failure_path,
        "demo.emit",
        {
            "function main() -> UInt32",
            "    -1 as UInt32",
        }
    );
    auto cast_drop_readiness_summary_failure =
        run_drop_readiness_summary(app, cast_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        cast_drop_readiness_summary_failure,
        "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
    );
    auto cast_emitted_drops_failure = run_emitted_drops(app, cast_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        cast_emitted_drops_failure,
        "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
    );
    auto cast_drop_readiness_relations_failure =
        run_drop_readiness_relations(app, cast_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        cast_drop_readiness_relations_failure,
        "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
    );

    auto final_if_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_final_if_failure.or";
    write_fixture(
        final_if_emit_failure_path,
        "demo.emit",
        {
            "function same(flag: Bool, left: Bool, right: Bool) -> Bool",
            "    if flag",
            "        left < right",
            "    else",
            "        false",
        }
    );
    auto final_if_drop_readiness_summary_failure =
        run_drop_readiness_summary(app, final_if_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_if_drop_readiness_summary_failure,
        "lowering does not yet support this final control-flow statement: "
        "if then arm lowering failed: unsupported operator: <"
    );
    auto final_if_emitted_drops_failure = run_emitted_drops(app, final_if_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_if_emitted_drops_failure,
        "lowering does not yet support this final control-flow statement: "
        "if then arm lowering failed: unsupported operator: <"
    );
    auto final_if_drop_readiness_relations_failure =
        run_drop_readiness_relations(app, final_if_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_if_drop_readiness_relations_failure,
        "lowering does not yet support this final control-flow statement: "
        "if then arm lowering failed: unsupported operator: <"
    );

    auto final_switch_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_final_switch_failure.or";
    write_fixture(
        final_switch_emit_failure_path,
        "demo.emit",
        {
            "function same(flag: Bool, left: Bool, right: Bool) -> Bool",
            "    switch flag",
            "        true => left < right",
            "        false => false",
        }
    );
    auto final_switch_drop_readiness_summary_failure =
        run_drop_readiness_summary(app, final_switch_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_switch_drop_readiness_summary_failure,
        "lowering does not yet support this final control-flow statement: "
        "switch case lowering failed: unsupported operator: <"
    );
    auto final_switch_emitted_drops_failure =
        run_emitted_drops(app, final_switch_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_switch_emitted_drops_failure,
        "lowering does not yet support this final control-flow statement: "
        "switch case lowering failed: unsupported operator: <"
    );
    auto final_switch_drop_readiness_relations_failure =
        run_drop_readiness_relations(app, final_switch_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_switch_drop_readiness_relations_failure,
        "lowering does not yet support this final control-flow statement: "
        "switch case lowering failed: unsupported operator: <"
    );

    auto planned_drop_report_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness.or";
    auto planned_drop_emit = run_emit_llvm(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        planned_drop_emit,
        {
            "define private void @__orison_thread_cleanup.launch.12.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate payload: Payload field 0 drop __orison_drop.Payload\n"
            "  ret void\n"
            "}",
        }
    );
    assert(planned_drop_emit.stdout_text.find("planned drop __orison_drop.Payload") == std::string::npos);
    assert(planned_drop_emit.stdout_text.find("declare void @__orison_drop.Payload(ptr)") == std::string::npos);
    assert(planned_drop_emit.stdout_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);

    auto planned_drop_report = run_planned_drops(app, planned_drop_report_path);
    assert_success_with_stdout_contains(planned_drop_report, {"planned drop __orison_drop.Payload"});
    auto semantic_planned_drop_report = run_semantic_planned_drops(app, planned_drop_report_path);
    assert_success_with_stdout_contains(semantic_planned_drop_report, {"drop obligation __orison_drop.Payload"});
    auto semantic_drop_resolution = run_semantic_drop_resolution(app, planned_drop_report_path);
    assert_success_with_stdout_contains(semantic_drop_resolution, {"missing drop site __orison_drop.Payload"});
    auto semantic_drop_diagnostics = run_semantic_drop_diagnostics(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        semantic_drop_diagnostics,
        {"drop diagnostic drop site __orison_drop.Payload", "no implementation discovered"}
    );
    auto semantic_drop_lowering_authorization =
        run_semantic_drop_lowering_authorization(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        semantic_drop_lowering_authorization,
        {"drop lowering authorization drop site __orison_drop.Payload", "semantic-unresolved lowering-blocked"}
    );

    auto parsed_drop_candidate_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_parsed_drop_candidate.or";
    auto remove_error = std::error_code {};
    std::filesystem::remove(parsed_drop_candidate_path, remove_error);
    write_fixture(
        parsed_drop_candidate_path,
        "demo.parseddrop",
        {
            "record Payload",
            "    public value: Int64",
            "interface Drop",
            "    function drop(this: exclusive This) -> Unit",
            "implements Drop for Payload",
            "    function drop(this: exclusive This) -> Unit",
            "        return",
            "function read(input: Payload) -> Int64",
            "    input.value",
        }
    );
    auto parsed_drop_candidate_diagnostics = run_semantic_drop_diagnostics(app, parsed_drop_candidate_path);
    assert_success_with_stdout_contains(
        parsed_drop_candidate_diagnostics,
        {"drop diagnostic drop site __orison_drop.Payload", "resolved"}
    );
    auto parsed_drop_candidate_lowering_authorization =
        run_semantic_drop_lowering_authorization(app, parsed_drop_candidate_path);
    assert_success_with_stdout_contains(
        parsed_drop_candidate_lowering_authorization,
        {"drop lowering authorization drop site __orison_drop.Payload", "semantic-resolved lowering-blocked"}
    );
    auto parsed_drop_candidate_emit = run_emit_llvm(app, parsed_drop_candidate_path);
    assert(parsed_drop_candidate_emit.exit_code == 0);
    assert(parsed_drop_candidate_emit.stderr_text.empty());
    assert(parsed_drop_candidate_emit.stdout_text.find("declare void @__orison_drop.Payload(ptr)") == std::string::npos);
    assert(parsed_drop_candidate_emit.stdout_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);

    auto parsed_drop_readiness_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_parsed_drop_readiness.or";
    std::filesystem::remove(parsed_drop_readiness_path, remove_error);
    write_fixture(
        parsed_drop_readiness_path,
        "demo.parseddropreadiness",
        {
            "record Payload",
            "    public value: Int64",
            "interface Drop",
            "    function drop(this: exclusive This) -> Unit",
            "implements Transferable for Payload",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "implements Drop for Payload",
            "    function drop(this: exclusive This) -> Unit",
            "        return",
            "function launch(value: Int64) -> Int64",
            "    let payload: Payload = Payload(value)",
            "    let worker = thread",
            "        payload.value",
            "",
            "    worker.join()",
        }
    );
    auto parsed_drop_readiness_blockers = run_drop_readiness_blockers(app, parsed_drop_readiness_path);
    assert_success_with_stdout_contains(
        parsed_drop_readiness_blockers,
        {
            "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 0",
            "drop readiness blocker source lowering not accepted __orison_drop.Payload",
            "drop readiness blocker missing declaration __orison_drop.Payload",
        }
    );
    auto parsed_drop_readiness_source =
        run_drop_readiness_source_correlations(app, parsed_drop_readiness_path);
    assert_success_with_stdout_contains(
        parsed_drop_readiness_source,
        {
            "drop readiness source correlations actions 1 semantic sites 1",
            "__orison_thread_cleanup.launch.14.0 __orison_drop.Payload",
            "semantic resolved source lowering not accepted declaration missing",
        }
    );

    auto semantic_drop_summary = run_semantic_drop_summary(app, planned_drop_report_path);
    assert_success_with_stdout_contains(semantic_drop_summary, {"drop resolution summary __orison_drop.Payload"});
    auto planned_drop_actions = run_planned_drop_actions(app, planned_drop_report_path);
    assert_success_with_stdout_contains(planned_drop_actions, {"planned drop action __orison_drop.Payload"});
    auto emitted_drops = run_emitted_drops(app, planned_drop_report_path);
    assert_success_with_empty_stdout(emitted_drops);
    auto drop_cleanup_authorization = run_drop_cleanup_authorization(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        drop_cleanup_authorization,
        {
            "drop cleanup authorization __orison_thread_cleanup.launch.12.0 blocked",
            "semantic drop unresolved __orison_drop.Payload",
            "missing drop declaration __orison_drop.Payload",
        }
    );
    auto drop_readiness = run_drop_readiness(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        drop_readiness,
        {
            "drop readiness snapshot semantic authorizations 1",
            "semantic readiness __orison_drop.Payload",
            "cleanup readiness __orison_thread_cleanup.launch.12.0 blocked",
        }
    );
    auto drop_readiness_summary = run_drop_readiness_summary(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        drop_readiness_summary,
        {"drop readiness summary semantic authorized 0 blocked 1"}
    );
    auto drop_readiness_relations = run_drop_readiness_relations(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        drop_readiness_relations,
        {
            "drop readiness relation __orison_thread_cleanup.launch.12.0 blocked",
            "drop readiness relation semantic blocker __orison_drop.Payload",
            "drop readiness relation missing declaration __orison_drop.Payload",
        }
    );
    auto drop_readiness_blockers = run_drop_readiness_blockers(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        drop_readiness_blockers,
        {
            "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 1",
            "drop readiness blocker semantic unresolved __orison_drop.Payload",
            "drop readiness blocker missing declaration __orison_drop.Payload",
        }
    );
    auto drop_readiness_source = run_drop_readiness_source_correlations(app, planned_drop_report_path);
    assert_success_with_stdout_contains(
        drop_readiness_source,
        {
            "drop readiness source correlations actions 1 semantic sites 1",
            "__orison_thread_cleanup.launch.12.0 __orison_drop.Payload",
            "semantic unresolved source lowering not accepted declaration missing",
        }
    );
    auto multi_drop_readiness_fixture_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness_multi.or";
    auto multi_fixture_drop_readiness_relations =
        run_drop_readiness_relations(app, multi_drop_readiness_fixture_path);
    assert_success_with_stdout_contains(
        multi_fixture_drop_readiness_relations,
        {
            "drop readiness relation __orison_thread_cleanup.launch.20.0 blocked",
            "drop readiness relation semantic blocker __orison_drop.Payload",
            "drop readiness relation semantic blocker __orison_drop.OtherPayload",
            "drop readiness relation missing declaration __orison_drop.OtherPayload",
        }
    );
    auto multi_fixture_drop_readiness_blockers =
        run_drop_readiness_blockers(app, multi_drop_readiness_fixture_path);
    assert_success_with_stdout_contains(
        multi_fixture_drop_readiness_blockers,
        {
            "drop readiness blockers cleanups 1 semantic blockers 2 semantic unresolved 2",
            "drop readiness blocker semantic __orison_drop.Payload",
            "drop readiness blocker semantic unresolved __orison_drop.OtherPayload",
            "drop readiness blocker missing declaration __orison_drop.OtherPayload",
        }
    );

    auto empty_planned_drop_report = run_planned_drops(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_planned_drop_report);
    auto empty_semantic_planned_drop_report = run_semantic_planned_drops(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_semantic_planned_drop_report);
    auto empty_semantic_drop_resolution = run_semantic_drop_resolution(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_resolution);
    auto empty_semantic_drop_diagnostics = run_semantic_drop_diagnostics(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_diagnostics);
    auto empty_semantic_drop_lowering_authorization =
        run_semantic_drop_lowering_authorization(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_lowering_authorization);
    auto empty_semantic_drop_summary = run_semantic_drop_summary(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_summary);
    auto empty_semantic_dynamic_array_descriptor_origins =
        run_semantic_dynamic_array_descriptor_origins(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_semantic_dynamic_array_descriptor_origins);
    auto empty_planned_drop_actions = run_planned_drop_actions(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_planned_drop_actions);
    auto empty_emitted_drops = run_emitted_drops(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_emitted_drops);
    auto empty_drop_cleanup_authorization = run_drop_cleanup_authorization(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_drop_cleanup_authorization);
    auto empty_drop_readiness = run_drop_readiness(app, clean_emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness,
        {"drop readiness snapshot semantic authorizations 0"}
    );
    auto empty_drop_readiness_summary = run_drop_readiness_summary(app, clean_emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness_summary,
        {"drop readiness summary semantic authorized 0 blocked 0"}
    );
    auto empty_drop_readiness_relations = run_drop_readiness_relations(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_drop_readiness_relations);
    auto empty_drop_readiness_blockers = run_drop_readiness_blockers(app, clean_emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness_blockers,
        {"drop readiness blockers cleanups 0 semantic blockers 0 semantic unresolved 0"}
    );
    auto empty_drop_readiness_source = run_drop_readiness_source_correlations(app, clean_emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness_source,
        {"drop readiness source correlations actions 0 semantic sites 0"}
    );
    auto empty_dynamic_array_descriptor_cleanup_plan =
        run_dynamic_array_descriptor_cleanup_plan(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_dynamic_array_descriptor_cleanup_plan);
    auto empty_dynamic_array_cleanup_obligations =
        run_dynamic_array_cleanup_obligations(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_dynamic_array_cleanup_obligations);
    auto empty_dynamic_array_cleanup_sequence_plan =
        run_dynamic_array_cleanup_sequence_plan(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_dynamic_array_cleanup_sequence_plan);
    auto empty_dynamic_array_cleanup_sequence_verification =
        run_dynamic_array_cleanup_sequence_verification(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_dynamic_array_cleanup_sequence_verification);
    auto empty_dynamic_array_cleanup_emission_gate =
        run_dynamic_array_cleanup_emission_gate(app, clean_emit_path);
    assert_success_with_empty_stdout(empty_dynamic_array_cleanup_emission_gate);
    auto empty_dynamic_array_cleanup_capability = run_dynamic_array_cleanup_capability(app, clean_emit_path);
    assert_success_with_stdout_contains(
        empty_dynamic_array_cleanup_capability,
        {
            "dynamic array cleanup emission capability proven",
            "[element cleanup ok]",
        }
    );
    auto empty_dynamic_array_cleanup_audit = run_dynamic_array_cleanup_audit(app, clean_emit_path);
    assert_success_with_stdout_contains(
        empty_dynamic_array_cleanup_audit,
        {
            "dynamic array cleanup emission capability proven",
            "[element cleanup ok]",
        }
    );
    auto empty_dynamic_array_cleanup_production_readiness =
        run_dynamic_array_cleanup_production_readiness(app, clean_emit_path);
    assert_success_with_stdout_contains(
        empty_dynamic_array_cleanup_production_readiness,
        {
            "dynamic array cleanup production readiness blocked",
            "[descriptor origins missing]",
            "[production cleanup emission ok]",
        }
    );

    auto dynamic_array_returned_payload_readiness =
        run_dynamic_array_cleanup_production_readiness(app, dynamic_array_returned_payload_path);
    assert_success_with_stdout_contains(
        dynamic_array_returned_payload_readiness,
        {
            "dynamic array cleanup production readiness ready",
            "[descriptor origin blockers absent]",
            "[cleanup capability ok]",
            "[production cleanup emission ok]",
        }
    );

    auto dynamic_array_blocked_cleanup_capability_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_dynamic_array_cleanup_capability.or";
    std::filesystem::remove(dynamic_array_blocked_cleanup_capability_path, remove_error);
    write_fixture(
        dynamic_array_blocked_cleanup_capability_path,
        "demo.dynamicarraycleanupcapability",
        {
            "record Payload",
            "    public value: Int64",
            "function use_items(items: DynamicArray<Payload>) -> UInt32",
            "    1 as UInt32",
        }
    );
    auto dynamic_array_blocked_descriptor_origins =
        run_semantic_dynamic_array_descriptor_origins(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_descriptor_origins,
        {
            "dynamic array descriptor summary DynamicArray<Payload>",
            "owner items",
            "element Payload",
            "(metadata only)",
        }
    );
    auto dynamic_array_blocked_descriptor_cleanup_plan =
        run_dynamic_array_descriptor_cleanup_plan(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_descriptor_cleanup_plan,
        {
            "dynamic array descriptor cleanup DynamicArray<Payload>",
            "owner items",
            "descriptor %items.addr audit",
        }
    );
    auto dynamic_array_blocked_lifetime_plan =
        run_dynamic_array_descriptor_lifetime_plan(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_lifetime_plan,
        {
            "dynamic array descriptor lifetime plan ready origins 1",
            "dynamic array descriptor lifetime DynamicArray<Payload>",
            "owner items",
            "origin parameter",
            "cleanup callee-owned-parameter-cleanup",
            "storage audit",
            "descriptor %items.addr",
            "cleanup-plan available",
        }
    );

    auto dynamic_array_all_origin_lifetime_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_dynamic_array_all_origin_lifetime.or";
    std::filesystem::remove(dynamic_array_all_origin_lifetime_path, remove_error);
    write_fixture(
        dynamic_array_all_origin_lifetime_path,
        "demo.dynamicarrayalloriginlifetime",
        {
            "record Payload",
            "    public value: Int64",
            "function make_items() -> DynamicArray<Payload>",
            "    DynamicArray()",
            "function use_items(items: DynamicArray<Payload>) -> UInt32",
            "    1 as UInt32",
            "function main() -> UInt32",
            "    var local_items: DynamicArray<Payload> = DynamicArray()",
            "    var returned_items: DynamicArray<Payload> = make_items()",
            "    use_items(returned_items)",
        }
    );
    auto dynamic_array_all_origin_lifetime_plan =
        run_dynamic_array_descriptor_lifetime_plan(app, dynamic_array_all_origin_lifetime_path);
    assert_success_with_stdout_contains(
        dynamic_array_all_origin_lifetime_plan,
        {
            "dynamic array descriptor lifetime plan ready origins 3",
            "owner items",
            "origin parameter",
            "cleanup callee-owned-parameter-cleanup",
            "owner local_items",
            "origin local",
            "cleanup caller-owned-local-cleanup",
            "owner returned_items",
            "origin returned",
            "cleanup caller-owned-returned-cleanup",
        }
    );
    auto dynamic_array_blocked_cleanup_obligations =
        run_dynamic_array_cleanup_obligations(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_cleanup_obligations,
        {
            "dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "owner items",
            "element Payload",
        }
    );
    auto dynamic_array_blocked_cleanup_sequence_plan =
        run_dynamic_array_cleanup_sequence_plan(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_cleanup_sequence_plan,
        {
            "dynamic array cleanup sequence __orison_dynamic_array_cleanup.0",
            "[load descriptor] [drop initialized elements] [deallocate descriptor storage]",
        }
    );
    auto dynamic_array_blocked_cleanup_sequence_verification =
        run_dynamic_array_cleanup_sequence_verification(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_cleanup_sequence_verification,
        {"dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed"}
    );
    auto dynamic_array_blocked_cleanup_emission_gate =
        run_dynamic_array_cleanup_emission_gate(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_cleanup_emission_gate,
        {"dynamic array cleanup emission gate __orison_dynamic_array_cleanup.0 allowed"}
    );
    auto dynamic_array_blocked_cleanup_capability =
        run_dynamic_array_cleanup_capability(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_cleanup_capability,
        {
            "dynamic array cleanup emission capability blocked",
            "[element cleanup missing]",
        }
    );
    auto dynamic_array_blocked_cleanup_audit =
        run_dynamic_array_cleanup_audit(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains_in_order(
        dynamic_array_blocked_cleanup_audit,
        {
            "dynamic array descriptor summary DynamicArray<Payload>",
            "dynamic array descriptor cleanup DynamicArray<Payload>",
            "dynamic array descriptor lifetime plan ready origins 1",
            "dynamic array descriptor lifetime DynamicArray<Payload>",
            "dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "dynamic array cleanup sequence __orison_dynamic_array_cleanup.0",
            "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed",
            "dynamic array cleanup emission gate __orison_dynamic_array_cleanup.0 allowed",
            "dynamic array cleanup emission capability blocked",
            "missing-element-drop-pairs [items:items.element:__orison_drop.Payload]",
            "[element cleanup missing]",
            "dynamic array cleanup production readiness blocked",
            "[cleanup capability missing]",
            "[production signatures ok]",
        }
    );
    auto dynamic_array_blocked_production_readiness =
        run_dynamic_array_cleanup_production_readiness(app, dynamic_array_blocked_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_blocked_production_readiness,
        {
            "dynamic array cleanup production readiness blocked",
            "[descriptor origins ok]",
            "[cleanup capability missing]",
            "missing-element-drop-pairs [items:items.element:__orison_drop.Payload]",
            "[production signatures ok]",
        }
    );
    assert(
        dynamic_array_blocked_cleanup_capability.stdout_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_descriptor_cleanup_plan.stdout_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_cleanup_obligations.stdout_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_cleanup_sequence_plan.stdout_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_cleanup_sequence_verification.stdout_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_cleanup_emission_gate.stdout_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_cleanup_audit.stdout_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_cleanup_capability.stdout_text.find(" element-drop-pairs ") ==
        std::string::npos
    );
    assert(
        dynamic_array_blocked_cleanup_audit.stdout_text.find(" element-drop-pairs ") ==
        std::string::npos
    );

    auto dynamic_array_authorized_cleanup_capability_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "dynamic_array_cleanup_audit.or";
    auto dynamic_array_authorized_descriptor_origins =
        run_semantic_dynamic_array_descriptor_origins(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_descriptor_origins,
        {
            "dynamic array descriptor summary DynamicArray<Payload>",
            "owner items",
            "element Payload",
            "(metadata only)",
        }
    );
    auto dynamic_array_authorized_descriptor_cleanup_plan =
        run_dynamic_array_descriptor_cleanup_plan(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_descriptor_cleanup_plan,
        {
            "dynamic array descriptor cleanup DynamicArray<Payload>",
            "owner items",
            "descriptor %items.addr bound",
        }
    );
    auto dynamic_array_authorized_cleanup_obligations =
        run_dynamic_array_cleanup_obligations(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_cleanup_obligations,
        {
            "dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "owner items",
            "element Payload",
        }
    );
    auto dynamic_array_authorized_cleanup_sequence_plan =
        run_dynamic_array_cleanup_sequence_plan(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_cleanup_sequence_plan,
        {
            "dynamic array cleanup sequence __orison_dynamic_array_cleanup.0",
            "[load descriptor] [drop initialized elements] [deallocate descriptor storage]",
        }
    );
    auto dynamic_array_authorized_cleanup_sequence_verification =
        run_dynamic_array_cleanup_sequence_verification(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_cleanup_sequence_verification,
        {"dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed"}
    );
    auto dynamic_array_authorized_cleanup_emission_gate =
        run_dynamic_array_cleanup_emission_gate(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_cleanup_emission_gate,
        {"dynamic array cleanup emission gate __orison_dynamic_array_cleanup.0 allowed"}
    );
    auto dynamic_array_authorized_cleanup_capability =
        run_dynamic_array_cleanup_capability(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_cleanup_capability,
        {
            "dynamic array cleanup emission capability proven",
            "[element cleanup ok]",
        }
    );
    auto dynamic_array_authorized_cleanup_audit =
        run_dynamic_array_cleanup_audit(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains_in_order(
        dynamic_array_authorized_cleanup_audit,
        {
            "dynamic array descriptor summary DynamicArray<Payload>",
            "dynamic array descriptor cleanup DynamicArray<Payload>",
            "dynamic array descriptor lifetime plan ready origins 1",
            "dynamic array descriptor lifetime DynamicArray<Payload>",
            "function use_items dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "function use_items dynamic array cleanup sequence __orison_dynamic_array_cleanup.0",
            "function use_items dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed",
            "function use_items dynamic array cleanup emission gate __orison_dynamic_array_cleanup.0 allowed",
            "function use_items dynamic array cleanup emission capability proven",
            "element-drop-pairs [items:items.element:__orison_drop.Payload]",
            "[element cleanup ok]",
            "computed DynamicArray consumed descriptor finalization plans ready computed-descriptor-plans 0 "
            "emitted-finalization-plans 1 ready 1 blocked 0 (metadata only)",
            "computed DynamicArray consumed descriptor finalization plan detail owner items descriptor %items.addr "
            "(metadata only)",
            "dynamic array cleanup production readiness ready",
            "[production signatures ok]",
        }
    );
    auto dynamic_array_authorized_production_readiness =
        run_dynamic_array_cleanup_production_readiness(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_production_readiness,
        {
            "dynamic array cleanup production readiness ready",
            "[cleanup capability ok]",
            "[production signatures ok]",
            "[production construction ok]",
            "[production cleanup emission ok]",
        }
    );

    auto dynamic_array_computed_local_same_owner_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_computed_local_same_owner_iterable.or";
    auto dynamic_array_computed_local_same_owner_audit =
        run_dynamic_array_cleanup_audit(app, dynamic_array_computed_local_same_owner_path);
    assert_success_with_stdout_contains_in_order(
        dynamic_array_computed_local_same_owner_audit,
        {
            "dynamic array descriptor summary DynamicArray<UInt32>",
            "dynamic array descriptor cleanup DynamicArray<UInt32>",
            "dynamic array descriptor lifetime plan ready origins",
            "dynamic array descriptor lifetime DynamicArray<UInt32>",
            "storage local descriptor %items.addr",
            "dynamic array cleanup emission capability proven",
            "[descriptor storage ok]",
            "computed DynamicArray descriptor render planned renders 1 snippets 4 [metadata available] "
            "[descriptor projections ready] (metadata only)",
            "computed DynamicArray loop control render planned renders 1 snippets 5 [metadata available] "
            "[control flow ready] (metadata only)",
            "computed DynamicArray element address render planned renders 1 snippets 1 [metadata available] "
            "[element address ready] (metadata only)",
            "computed DynamicArray element load render planned renders 1 snippets 1 [metadata available] "
            "[element load ready] (metadata only)",
            "computed DynamicArray loop continue render planned renders 1 snippets 3 [metadata available] "
            "[loop continue ready] (metadata only)",
            "computed DynamicArray loop render sequence planned sequences 1 snippets 15 [metadata available] "
            "[body blocks ready] (metadata only)",
            "computed DynamicArray loop exit cleanup planned cleanups 1 snippets 2 [metadata available] "
            "[cleanup resumptions ready] (metadata only)",
            "computed DynamicArray cleanup transition planned transitions 1 [metadata available] "
            "[transitions paired] (metadata only)",
            smoke::computed_dynamic_array_cleanup_call_insertion_capability_enabled_report,
            "computed DynamicArray production emission gate planned gates 1 snippets 17 [metadata available] "
            "[ownership ready] [loop render ready] [loop cleanup ownership ready] "
            "[function cleanup resumption ready] [exit cleanup ready] [production sequence planned] "
            "[production emission enabled] (metadata only)",
            "computed DynamicArray production sequence planned sequences 1 snippets 17 module-comments 0 "
            "[metadata available] [module comments absent] (metadata only)",
            "dynamic array cleanup production readiness ready",
        }
    );

    auto dynamic_array_computed_later_owner_use_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_computed_later_owner_use.or";
    std::filesystem::remove(dynamic_array_computed_later_owner_use_path, remove_error);
    write_fixture(
        dynamic_array_computed_later_owner_use_path,
        "demo.dynamicarraycomputedlaterowneruse",
        {
            "function sum_words(flag: Bool) -> UInt32",
            "    let items: DynamicArray<UInt32> = DynamicArray()",
            "    var total = 0 as UInt32",
            "    for word in flag ? items : items",
            "        total = total + word",
            "    for word in flag ? items : items",
            "        total = total + word",
            "    total",
        }
    );
    auto dynamic_array_computed_later_owner_use_audit =
        run_dynamic_array_cleanup_audit(app, dynamic_array_computed_later_owner_use_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_later_owner_use_audit,
        {
            "computed DynamicArray inserted cleanup state verification detail owner items acquire "
            "items.computed_for.0.cleanup.acquire",
            "computed DynamicArray cleanup call blockers blocked cleanup-blockers 1 "
            "blocker-reasons [later owner use] (metadata only)",
            "cleanup-blocked-reason later owner use",
            "computed DynamicArray cleanup call emission gate blocked gates 2 ready 1 blocked 1 "
            "[inserted state verified] [cleanup calls disabled] (inserted IR)",
            "computed DynamicArray cleanup call emission gate detail owner items acquire "
            "items.computed_for.0.cleanup.acquire",
            "computed DynamicArray cleanup call plan planned plans 2 planned 2 renderable 2 renders 2 "
            "[inserted state verified] [cleanup operands proven] [cleanup calls disabled] (inserted IR)",
            "computed DynamicArray cleanup call plan detail owner items cleanup-operation "
            "items.computed_for.0.cleanup.resume.call data %items.computed_for.0.data element-size 4 capacity "
            "%items.computed_for.0.capacity",
            "computed DynamicArray inserted cleanup state verification detail owner items acquire "
            "items.computed_for.1.cleanup.acquire",
            "computed DynamicArray cleanup call emission gate detail owner items acquire "
            "items.computed_for.1.cleanup.acquire",
            "computed DynamicArray cleanup call render detail owner items cleanup-operation "
            "items.computed_for.1.cleanup.resume.call data %items.computed_for.1.data element-size 4 capacity "
            "%items.computed_for.1.capacity",
        }
    );
    assert(
        dynamic_array_computed_later_owner_use_audit.stdout_text.find(
            "computed DynamicArray for inserted cleanup call cleanup-operation "
            "items.computed_for.0.cleanup.resume.call"
        ) == std::string::npos
    );
    auto dynamic_array_computed_later_owner_use_insertion_readiness =
        run_computed_dynamic_array_cleanup_call_insertion_readiness(
            app,
            dynamic_array_computed_later_owner_use_path
        );
    assert_success_with_stdout_contains(
        dynamic_array_computed_later_owner_use_insertion_readiness,
        {
            "computed DynamicArray cleanup call insertion readiness blocked gates 2 ready 1 blocked 1 "
            "cleanup-blockers 1",
            "cleanup-operation items.computed_for.0.cleanup.resume.call "
            "cleanup-blocked-reason later owner use",
            "cleanup-operation items.computed_for.1.cleanup.resume.call",
        }
    );
    assert_computed_dynamic_array_final_use_cleanup_reports(app, dynamic_array_computed_later_owner_use_path);

    auto dynamic_array_computed_active_loop_body_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_computed_active_loop_body.or";
    std::filesystem::remove(dynamic_array_computed_active_loop_body_path, remove_error);
    write_fixture(
        dynamic_array_computed_active_loop_body_path,
        "demo.dynamicarraycomputedactiveloopbody",
        {
            "function sum_words(flag: Bool) -> UInt32",
            "    let items: DynamicArray<UInt32> = DynamicArray()",
            "    var total = 0 as UInt32",
            "    while flag",
            "        for word in flag ? items : items",
            "            total = total + word",
            "        break",
            "    total",
        }
    );
    auto dynamic_array_computed_active_loop_body_audit =
        run_dynamic_array_cleanup_audit(app, dynamic_array_computed_active_loop_body_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_active_loop_body_audit,
        {
            "computed DynamicArray inserted cleanup state verification detail owner items acquire "
            "items.computed_for.1.cleanup.acquire",
            "computed DynamicArray cleanup call blockers blocked cleanup-blockers 1 "
            "blocker-reasons [active loop body] (metadata only)",
            "cleanup-blocked-reason active loop body",
            "computed DynamicArray cleanup call emission gate blocked gates 1 ready 0 blocked 1 "
            "[inserted state verified] [cleanup calls disabled] (inserted IR)",
            "computed DynamicArray cleanup call emission gate detail owner items acquire "
            "items.computed_for.1.cleanup.acquire",
            "computed DynamicArray cleanup call plan planned plans 1 planned 1 renderable 1 renders 1 "
            "[inserted state verified] [cleanup operands proven] [cleanup calls disabled] (inserted IR)",
            "computed DynamicArray cleanup call plan detail owner items cleanup-operation "
            "items.computed_for.1.cleanup.resume.call data %items.computed_for.1.data element-size 4 capacity "
            "%items.computed_for.1.capacity",
        }
    );
    assert(dynamic_array_computed_active_loop_body_audit.stdout_text.find("[cleanup calls enabled]") == std::string::npos);
    assert(
        dynamic_array_computed_active_loop_body_audit.stdout_text.find(
            "computed DynamicArray for inserted cleanup call cleanup-operation "
            "items.computed_for.1.cleanup.resume.call"
        ) == std::string::npos
    );
    auto dynamic_array_computed_active_loop_body_insertion_readiness =
        run_computed_dynamic_array_cleanup_call_insertion_readiness(
            app,
            dynamic_array_computed_active_loop_body_path
        );
    assert_success_with_stdout_contains(
        dynamic_array_computed_active_loop_body_insertion_readiness,
        {
            "computed DynamicArray cleanup call insertion readiness blocked gates 1 ready 0 blocked 1 "
            "cleanup-blockers 1",
            "cleanup-operation items.computed_for.1.cleanup.resume.call "
            "cleanup-blocked-reason active loop body",
        }
    );

    auto dynamic_array_computed_after_while_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_computed_after_while.or";
    std::filesystem::remove(dynamic_array_computed_after_while_path, remove_error);
    write_fixture(
        dynamic_array_computed_after_while_path,
        "demo.dynamicarraycomputedafterwhile",
        {
            "function sum_words(flag: Bool) -> UInt32",
            "    let items: DynamicArray<UInt32> = DynamicArray()",
            "    var total = 0 as UInt32",
            "    while flag",
            "        total = total + 1 as UInt32",
            "        break",
            "    for word in flag ? items : items",
            "        total = total + word",
            "    total",
        }
    );
    assert_computed_dynamic_array_final_use_cleanup_reports(app, dynamic_array_computed_after_while_path);

    auto dynamic_array_computed_if_then_later_loop_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_computed_if_then_later_loop.or";
    std::filesystem::remove(dynamic_array_computed_if_then_later_loop_path, remove_error);
    write_fixture(
        dynamic_array_computed_if_then_later_loop_path,
        "demo.dynamicarraycomputedifthenlaterloop",
        {
            "function sum_words(flag: Bool) -> UInt32",
            "    let items: DynamicArray<UInt32> = DynamicArray()",
            "    var total = 0 as UInt32",
            "    if flag",
            "        for word in flag ? items : items",
            "            total = total + word",
            "    for word in flag ? items : items",
            "        total = total + word",
            "    total",
        }
    );
    assert_computed_dynamic_array_later_owner_use_readiness(app, dynamic_array_computed_if_then_later_loop_path);
    assert_computed_dynamic_array_final_use_cleanup_reports(app, dynamic_array_computed_if_then_later_loop_path);

    auto dynamic_array_computed_switch_case_later_loop_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_computed_switch_case_later_loop.or";
    std::filesystem::remove(dynamic_array_computed_switch_case_later_loop_path, remove_error);
    write_fixture(
        dynamic_array_computed_switch_case_later_loop_path,
        "demo.dynamicarraycomputedswitchcaselaterloop",
        {
            "function sum_words(flag: Bool) -> UInt32",
            "    let items: DynamicArray<UInt32> = DynamicArray()",
            "    var total = 0 as UInt32",
            "    switch flag",
            "        true =>",
            "            for word in flag ? items : items",
            "                total = total + word",
            "        default => total = total + 1 as UInt32",
            "    for word in flag ? items : items",
            "        total = total + word",
            "    total",
        }
    );
    assert_computed_dynamic_array_later_owner_use_readiness(app, dynamic_array_computed_switch_case_later_loop_path);
    assert_computed_dynamic_array_final_use_cleanup_reports(app, dynamic_array_computed_switch_case_later_loop_path);

    auto dynamic_array_computed_local_same_owner_insertion_capability =
        run_computed_dynamic_array_cleanup_call_insertion_capability(app, dynamic_array_computed_local_same_owner_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_local_same_owner_insertion_capability,
        {
            smoke::computed_dynamic_array_cleanup_call_insertion_capability_enabled_report,
        }
    );
    auto dynamic_array_computed_local_same_owner_insertion_readiness =
        run_computed_dynamic_array_cleanup_call_insertion_readiness(app, dynamic_array_computed_local_same_owner_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_local_same_owner_insertion_readiness,
        {
            smoke::computed_dynamic_array_cleanup_call_insertion_readiness_ready_report,
            smoke::computed_dynamic_array_cleanup_call_insertion_readiness_detail_report,
        }
    );
    auto dynamic_array_computed_local_same_owner_inserted_cleanup_handoffs =
        run_computed_dynamic_array_inserted_cleanup_handoffs(app, dynamic_array_computed_local_same_owner_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_local_same_owner_inserted_cleanup_handoffs,
        {
            smoke::computed_dynamic_array_inserted_cleanup_handoff_state_paired_production_enabled_report,
            smoke::computed_dynamic_array_inserted_cleanup_handoff_state_detail_report,
        }
    );
    auto dynamic_array_computed_local_same_owner_inserted_cleanup_calls =
        run_computed_dynamic_array_inserted_cleanup_calls(app, dynamic_array_computed_local_same_owner_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_local_same_owner_inserted_cleanup_calls,
        {
            smoke::computed_dynamic_array_inserted_cleanup_call_state_inserted_report,
            smoke::computed_dynamic_array_inserted_cleanup_call_state_detail_report,
        }
    );
    auto dynamic_array_computed_local_same_owner_consumed_cleanup_descriptors =
        run_computed_dynamic_array_consumed_cleanup_descriptors(app, dynamic_array_computed_local_same_owner_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_local_same_owner_consumed_cleanup_descriptors,
        {
            smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_finalized_report,
            smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_detail_report,
        }
    );
    auto dynamic_array_computed_local_same_owner_cleanup_proof_summary =
        run_computed_dynamic_array_cleanup_proof_summary(app, dynamic_array_computed_local_same_owner_path);
    assert_success_with_stdout_contains(
        dynamic_array_computed_local_same_owner_cleanup_proof_summary,
        {
            smoke::computed_dynamic_array_cleanup_proof_summary_inserted_report,
        }
    );
    auto dynamic_array_local_append_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_dynamic_array_append.or";
    auto dynamic_array_local_append_audit =
        run_dynamic_array_cleanup_audit(app, dynamic_array_local_append_path);
    assert_success_with_stdout_contains(
        dynamic_array_local_append_audit,
        {
            "dynamic array descriptor summary DynamicArray<UInt32>",
            "dynamic array descriptor cleanup DynamicArray<UInt32>",
            "descriptor %items.addr local",
            "function first_value dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "function first_value dynamic array cleanup sequence __orison_dynamic_array_cleanup.0",
            "function first_value dynamic array cleanup emission gate __orison_dynamic_array_cleanup.0 allowed",
            "function first_value dynamic array cleanup emission capability proven",
            "function counted_sum dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "[descriptor storage ok]",
            "computed DynamicArray consumed descriptor finalization plans ready computed-descriptor-plans 0 "
            "emitted-finalization-plans 2 ready 2 blocked 0 (metadata only)",
            "computed DynamicArray consumed descriptor finalization plan detail owner items descriptor %items.addr "
            "(metadata only)",
        }
    );

    auto multi_planned_drop_report = run_planned_drops(app, multi_drop_readiness_fixture_path);
    assert_success_with_stdout_contains(
        multi_planned_drop_report,
        {"planned drop __orison_drop.Payload", "planned drop __orison_drop.OtherPayload"}
    );
    auto multi_planned_drop_actions = run_planned_drop_actions(app, multi_drop_readiness_fixture_path);
    assert_success_with_stdout_contains(
        multi_planned_drop_actions,
        {"capture payload: Payload", "capture other: OtherPayload"}
    );
    auto multi_drop_cleanup_authorization =
        run_drop_cleanup_authorization(app, multi_drop_readiness_fixture_path);
    assert_success_with_stdout_contains(
        multi_drop_cleanup_authorization,
        {
            "drop cleanup authorization __orison_thread_cleanup.launch.20.0 blocked",
            "semantic drop lowering blocked __orison_drop.Payload",
            "semantic drop lowering blocked __orison_drop.OtherPayload",
            "missing drop declaration __orison_drop.OtherPayload",
        }
    );

    auto deduped_planned_drop_report_path =
        std::filesystem::temp_directory_path() / "orison_driver_drop_report_deduped.or";
    remove_error = {};
    std::filesystem::remove(deduped_planned_drop_report_path, remove_error);
    write_fixture(
        deduped_planned_drop_report_path,
        "demo.emit",
        {
            "record Payload",
            "    public value: Int64",
            "implements Transferable for Payload",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch(value: Int64) -> Int64",
            "    let left: Payload = Payload(value)",
            "    let right: Payload = Payload(value)",
            "    let worker = thread",
            "        left.value + right.value",
            "",
            "    worker.join()",
        }
    );
    auto deduped_planned_drop_report = run_planned_drops(app, deduped_planned_drop_report_path);
    assert_success_with_stdout_contains(deduped_planned_drop_report, {"planned drop __orison_drop.Payload"});
    auto deduped_planned_drop_actions = run_planned_drop_actions(app, deduped_planned_drop_report_path);
    assert_success_with_stdout_contains(
        deduped_planned_drop_actions,
        {"capture left: Payload", "capture right: Payload"}
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
