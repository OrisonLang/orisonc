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

void assert_success_with_empty_stdout(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

}  // namespace

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_drop_report_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::driver::CompilerApp app;

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
        "lowering does not yet support this return expression"
    );
    auto emitted_drops_failure = run_emitted_drops(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        emitted_drops_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_cleanup_sequence_verification_failure =
        run_dynamic_array_cleanup_sequence_verification(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_cleanup_sequence_verification_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_cleanup_emission_gate_failure =
        run_dynamic_array_cleanup_emission_gate(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_cleanup_emission_gate_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_descriptor_cleanup_plan_failure =
        run_dynamic_array_descriptor_cleanup_plan(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_descriptor_cleanup_plan_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_cleanup_obligations_failure =
        run_dynamic_array_cleanup_obligations(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_cleanup_obligations_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_cleanup_sequence_plan_failure =
        run_dynamic_array_cleanup_sequence_plan(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_cleanup_sequence_plan_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_cleanup_capability_failure =
        run_dynamic_array_cleanup_capability(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_cleanup_capability_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_cleanup_audit_failure =
        run_dynamic_array_cleanup_audit(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_cleanup_audit_failure,
        "lowering does not yet support this return expression"
    );
    auto dynamic_array_cleanup_production_readiness_failure =
        run_dynamic_array_cleanup_production_readiness(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        dynamic_array_cleanup_production_readiness_failure,
        "lowering does not yet support this return expression"
    );
    auto drop_readiness_relations_failure = run_drop_readiness_relations(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        drop_readiness_relations_failure,
        "lowering does not yet support this return expression"
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
    assert_success_with_stdout_contains(semantic_planned_drop_report, {"drop site __orison_drop.Payload"});
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
            "[production cleanup emission missing]",
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
            "dynamic array descriptor origin DynamicArray<Payload>",
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
            "descriptor %items.addr bound",
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
            "dynamic array descriptor origin DynamicArray<Payload>",
            "dynamic array descriptor cleanup DynamicArray<Payload>",
            "dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "dynamic array cleanup sequence __orison_dynamic_array_cleanup.0",
            "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed",
            "dynamic array cleanup emission gate __orison_dynamic_array_cleanup.0 allowed",
            "dynamic array cleanup emission capability blocked",
            "[element cleanup missing]",
            "dynamic array cleanup production readiness blocked",
            "[cleanup capability missing]",
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
            "[production signatures missing]",
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

    auto dynamic_array_authorized_cleanup_capability_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "dynamic_array_cleanup_audit.or";
    auto dynamic_array_authorized_descriptor_origins =
        run_semantic_dynamic_array_descriptor_origins(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_descriptor_origins,
        {
            "dynamic array descriptor origin DynamicArray<Payload>",
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
            "dynamic array descriptor origin DynamicArray<Payload>",
            "dynamic array descriptor cleanup DynamicArray<Payload>",
            "dynamic array cleanup obligation __orison_dynamic_array_cleanup.0",
            "dynamic array cleanup sequence __orison_dynamic_array_cleanup.0",
            "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed",
            "dynamic array cleanup emission gate __orison_dynamic_array_cleanup.0 allowed",
            "dynamic array cleanup emission capability proven",
            "[element cleanup ok]",
            "dynamic array cleanup production readiness blocked",
            "[production signatures missing]",
        }
    );
    auto dynamic_array_authorized_production_readiness =
        run_dynamic_array_cleanup_production_readiness(app, dynamic_array_authorized_cleanup_capability_path);
    assert_success_with_stdout_contains(
        dynamic_array_authorized_production_readiness,
        {
            "dynamic array cleanup production readiness blocked",
            "[cleanup capability ok]",
            "[production signatures missing]",
            "[production construction missing]",
            "[production cleanup emission missing]",
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
