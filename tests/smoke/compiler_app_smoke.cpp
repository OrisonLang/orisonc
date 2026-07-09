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

void write_bool_value_pattern_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 2\n";
    }
}

void write_concurrency_fixture(
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

auto run_parse(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--parse", path);
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

void assert_parse_success(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    assert(result.stdout_text.find("parsed ") != std::string::npos);
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

void assert_success_with_empty_stdout(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

void assert_parse_failure_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert_failure_with_no_stdout_contains(result, expected_message);
}

void assert_parse_failure_contains_without(
    orison::driver::CompileResult const& result,
    std::string_view expected_message,
    std::string_view unexpected_message
) {
    assert_parse_failure_contains(result, expected_message);
    assert(result.stderr_text.find(unexpected_message) == std::string::npos);
}

}  // namespace

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_compiler_app_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::driver::CompilerApp app;

    std::array<char const*, 2> version_argv {"orisonc", "--version"};
    auto result = app.run(std::span<char const* const>(version_argv.data(), version_argv.size()));

    assert(result.exit_code == 0);
    assert(result.stdout_text == "orisonc 0.1.0-dev\n");
    assert(result.stderr_text.empty());

    auto emit_path = std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm.or";
    write_concurrency_fixture(
        emit_path,
        "demo.emit",
        {
            "function main() -> UInt32",
            "    42 as UInt32",
        }
    );
    auto emit_result = run_emit_llvm(app, emit_path);
    assert(emit_result.exit_code == 0);
    assert(emit_result.stderr_text.empty());
    assert(
        emit_result.stdout_text ==
        "; Orison LLVM IR scaffold\n"
        "; package demo.emit\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 42\n"
        "}\n"
        "\n"
    );

    auto emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm_failure.or";
    write_concurrency_fixture(
        emit_failure_path,
        "demo.emit",
        {
            "function same(left: Bool, right: Bool) -> Bool",
            "    left == right",
        }
    );
    auto emit_failure = run_emit_llvm(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        emit_failure,
        "lowering does not yet support this return expression"
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
    auto drop_readiness_relations_failure = run_drop_readiness_relations(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        drop_readiness_relations_failure,
        "lowering does not yet support this return expression"
    );

    auto emit_scalar_member_assignment_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_scalar_member_assignment_failure.or";
    write_concurrency_fixture(
        emit_scalar_member_assignment_failure_path,
        "demo.emit",
        {
            "unsafe function main() -> Unit",
            "    var total: UInt32 = 0 as UInt32",
            "    total.status = 1 as UInt32",
            "    return",
        }
    );
    auto emit_scalar_member_assignment_failure =
        run_emit_llvm(app, emit_scalar_member_assignment_failure_path);
    assert_failure_with_no_stdout_contains(
        emit_scalar_member_assignment_failure,
        "lowering aggregate assignment member target is unsupported"
    );

    auto emit_scalar_index_assignment_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_scalar_index_assignment_failure.or";
    write_concurrency_fixture(
        emit_scalar_index_assignment_failure_path,
        "demo.emit",
        {
            "unsafe function main(index: UInt64) -> Unit",
            "    var total: UInt32 = 0 as UInt32",
            "    total[index] = 1 as UInt32",
            "    return",
        }
    );
    auto emit_scalar_index_assignment_failure =
        run_emit_llvm(app, emit_scalar_index_assignment_failure_path);
    assert_failure_with_no_stdout_contains(
        emit_scalar_index_assignment_failure,
        "lowering aggregate assignment index target is unsupported"
    );

    auto emit_local_record_address_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_local_record_address.or";
    write_concurrency_fixture(
        emit_local_record_address_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "unsafe function main() -> Address",
            "    var regs = Registers(0 as UInt32, 1 as UInt32)",
            "    address_of(regs.status)",
        }
    );
    auto emit_local_record_address = run_emit_llvm(app, emit_local_record_address_path);
    assert_success_with_stdout_contains(
        emit_local_record_address,
        {
            "%tmp0 = insertvalue %record.Registers undef, i32 0, 0",
            "getelementptr %record.Registers, ptr %regs.addr, i32 0, i32 1",
        }
    );

    auto emit_local_array_address_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_local_array_address.or";
    write_concurrency_fixture(
        emit_local_array_address_path,
        "demo.emit",
        {
            "record Buffer",
            "    bytes: Array<Byte, 4>",
            "unsafe function main(index: UInt64) -> Address",
            "    var buffer = Buffer([1, 2, 3, 4])",
            "    address_of(buffer.bytes[index])",
        }
    );
    auto emit_local_array_address = run_emit_llvm(app, emit_local_array_address_path);
    assert_success_with_stdout_contains(
        emit_local_array_address,
        {
            "%tmp0 = insertvalue [4 x i8] undef, i8 1, 0",
            "getelementptr [4 x i8], ptr %tmp5, i64 0, i64 %index",
        }
    );

    auto emit_aggregate_assignment_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_aggregate_assignment.or";
    write_concurrency_fixture(
        emit_aggregate_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "unsafe function main(index: UInt64) -> Address",
            "    var regs = Registers(0 as UInt32, 1 as UInt32)",
            "    regs = Registers(2 as UInt32, 3 as UInt32)",
            "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]",
            "    bytes = [5, 6, 7, 8]",
            "    address_of(bytes[index])",
        }
    );
    auto emit_aggregate_assignment = run_emit_llvm(app, emit_aggregate_assignment_path);
    assert_success_with_stdout_contains(
        emit_aggregate_assignment,
        {
            "store %record.Registers %tmp",
            "store [4 x i8] %tmp",
        }
    );

    auto emit_aggregate_element_assignment_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_aggregate_element_assignment.or";
    write_concurrency_fixture(
        emit_aggregate_element_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "unsafe function main(index: UInt64) -> Unit",
            "    var regs = Registers(0 as UInt32, 1 as UInt32)",
            "    regs.status = 4 as UInt32",
            "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]",
            "    bytes[index] = 9",
            "    return",
        }
    );
    auto emit_aggregate_element_assignment = run_emit_llvm(app, emit_aggregate_element_assignment_path);
    assert_success_with_stdout_contains(
        emit_aggregate_element_assignment,
        {
            "store i32 4, ptr %tmp",
            "store i8 9, ptr %tmp",
        }
    );

    auto emit_pointer_aggregate_assignment_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_pointer_aggregate_assignment.or";
    write_concurrency_fixture(
        emit_pointer_aggregate_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "record Buffer",
            "    bytes: Array<Byte, 4>",
            "unsafe function main(regs: Pointer<Registers>, buffer: Pointer<Buffer>, index: UInt64) -> Unit",
            "    regs.status = 4 as UInt32",
            "    buffer.bytes[index] = 7",
            "    return",
        }
    );
    auto emit_pointer_aggregate_assignment = run_emit_llvm(app, emit_pointer_aggregate_assignment_path);
    assert_success_with_stdout_contains(
        emit_pointer_aggregate_assignment,
        {
            "getelementptr %record.Registers, ptr %regs, i32 0, i32 1",
            "getelementptr %record.Buffer, ptr %buffer, i32 0, i32 0",
            "store i32 4, ptr %tmp",
            "store i8 7, ptr %tmp",
        }
    );

    auto emit_pointer_nested_aggregate_assignment_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_pointer_nested_aggregate_assignment.or";
    write_concurrency_fixture(
        emit_pointer_nested_aggregate_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "record Buffer",
            "    bytes: Array<Byte, 4>",
            "record Log",
            "    entries: Array<Registers, 2>",
            "record Matrix",
            "    rows: Array<Array<Byte, 4>, 2>",
            "unsafe function main(log: Pointer<Log>, matrix: Pointer<Matrix>, index: UInt64, inner: UInt64) -> Unit",
            "    log.entries[index].status = 8 as UInt32",
            "    matrix.rows[index][inner] = 1 as Byte",
            "    return",
        }
    );
    auto emit_pointer_nested_aggregate_assignment =
        run_emit_llvm(app, emit_pointer_nested_aggregate_assignment_path);
    assert_success_with_stdout_contains(
        emit_pointer_nested_aggregate_assignment,
        {
            "getelementptr %record.Log, ptr %log, i32 0, i32 0",
            "getelementptr [2 x %record.Registers], ptr %tmp",
            "getelementptr %record.Matrix, ptr %matrix, i32 0, i32 0",
            "getelementptr [2 x [4 x i8]], ptr %tmp",
            "store i32 8, ptr %tmp",
            "store i8 1, ptr %tmp",
        }
    );

    auto emit_thread_spawn_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_thread_spawn.or";
    write_concurrency_fixture(
        emit_thread_spawn_path,
        "demo.emit",
        {
            "function launch(value: Int64) -> Int64",
            "    let worker = thread",
            "        value + 1",
            "",
            "    worker.join()",
        }
    );
    auto emit_thread_spawn = run_emit_llvm(app, emit_thread_spawn_path);
    assert_success_with_stdout_contains(
        emit_thread_spawn,
        {
            "declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)",
            "declare void @__orison_thread_join(ptr)",
            "declare void @__orison_concurrency_spawn_failed()",
            "declare void @__orison_concurrency_handle_destroy(ptr)",
            "%worker.thread.env = alloca { i64 }",
            "store i64 %value, ptr %tmp",
            "%worker.thread.result = alloca i64",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.launch.3.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 8, ptr @__orison_thread_cleanup.launch.3.0)",
            "icmp eq ptr %worker, null",
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "worker.thread.spawn_ok.0:",
            "define private void @__orison_thread_thunk.launch.3.0(ptr %environment, ptr %result_storage)",
            "define private void @__orison_thread_cleanup.launch.3.0(ptr %environment)",
            "load i64, ptr %tmp0",
            "store i64 %tmp2, ptr %result_storage",
            "call void @__orison_thread_join(ptr %worker)",
            "load i64, ptr %worker.thread.result",
            "call void @__orison_concurrency_handle_destroy(ptr %worker)",
        }
    );

    auto emit_thread_abandoned_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_thread_abandoned.or";
    write_concurrency_fixture(
        emit_thread_abandoned_path,
        "demo.emit",
        {
            "function launch(value: Int64) -> Int64",
            "    let worker = thread",
            "        value + 1",
            "",
            "    0",
        }
    );
    auto emit_thread_abandoned = run_emit_llvm(app, emit_thread_abandoned_path);
    assert_success_with_stdout_contains(
        emit_thread_abandoned,
        {
            "declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)",
            "declare void @__orison_concurrency_handle_destroy(ptr)",
            "declare void @__orison_concurrency_spawn_failed()",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.launch.3.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 8, ptr @__orison_thread_cleanup.launch.3.0)",
            "define private void @__orison_thread_cleanup.launch.3.0(ptr %environment)",
            "icmp eq ptr %worker, null",
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "worker.thread.spawn_ok.0:",
            "call void @__orison_concurrency_handle_destroy(ptr %worker)\n"
            "  ret i64 0",
        }
    );
    assert(emit_thread_abandoned.stdout_text.find("declare void @__orison_thread_join(ptr)") == std::string::npos);

    auto emit_task_spawn_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_task_spawn.or";
    write_concurrency_fixture(
        emit_task_spawn_path,
        "demo.emit",
        {
            "async function launch(value: Int64) -> Int64",
            "    let pending = task",
            "        value + 1",
            "",
            "    await pending",
        }
    );
    auto emit_task_spawn = run_emit_llvm(app, emit_task_spawn_path);
    assert_success_with_stdout_contains(
        emit_task_spawn,
        {
            "declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)",
            "declare void @__orison_concurrency_spawn_failed()",
            "declare void @__orison_task_await(ptr)",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.launch.3.0, ptr %pending.task.env, ptr %pending.task.result, i64 8, ptr @__orison_task_cleanup.launch.3.0)",
            "define private void @__orison_task_cleanup.launch.3.0(ptr %environment)",
            "icmp eq ptr %pending, null",
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "pending.task.spawn_ok.0:",
            "call void @__orison_task_await(ptr %pending)",
            "load i64, ptr %pending.task.result",
            "call void @__orison_concurrency_handle_destroy(ptr %pending)",
        }
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_parsed_drop_candidate.or";
    auto remove_error = std::error_code {};
    std::filesystem::remove(parsed_drop_candidate_path, remove_error);
    write_concurrency_fixture(
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_parsed_drop_readiness.or";
    std::filesystem::remove(parsed_drop_readiness_path, remove_error);
    write_concurrency_fixture(
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

    auto empty_planned_drop_report = run_planned_drops(app, emit_path);
    assert_success_with_empty_stdout(empty_planned_drop_report);
    auto empty_semantic_planned_drop_report = run_semantic_planned_drops(app, emit_path);
    assert_success_with_empty_stdout(empty_semantic_planned_drop_report);
    auto empty_semantic_drop_resolution = run_semantic_drop_resolution(app, emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_resolution);
    auto empty_semantic_drop_diagnostics = run_semantic_drop_diagnostics(app, emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_diagnostics);
    auto empty_semantic_drop_lowering_authorization =
        run_semantic_drop_lowering_authorization(app, emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_lowering_authorization);
    auto empty_semantic_drop_summary = run_semantic_drop_summary(app, emit_path);
    assert_success_with_empty_stdout(empty_semantic_drop_summary);
    auto empty_planned_drop_actions = run_planned_drop_actions(app, emit_path);
    assert_success_with_empty_stdout(empty_planned_drop_actions);
    auto empty_emitted_drops = run_emitted_drops(app, emit_path);
    assert_success_with_empty_stdout(empty_emitted_drops);
    auto empty_drop_cleanup_authorization = run_drop_cleanup_authorization(app, emit_path);
    assert_success_with_empty_stdout(empty_drop_cleanup_authorization);
    auto empty_drop_readiness = run_drop_readiness(app, emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness,
        {"drop readiness snapshot semantic authorizations 0"}
    );
    auto empty_drop_readiness_summary = run_drop_readiness_summary(app, emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness_summary,
        {"drop readiness summary semantic authorized 0 blocked 0"}
    );
    auto empty_drop_readiness_relations = run_drop_readiness_relations(app, emit_path);
    assert_success_with_empty_stdout(empty_drop_readiness_relations);
    auto empty_drop_readiness_blockers = run_drop_readiness_blockers(app, emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness_blockers,
        {"drop readiness blockers cleanups 0 semantic blockers 0 semantic unresolved 0"}
    );
    auto empty_drop_readiness_source = run_drop_readiness_source_correlations(app, emit_path);
    assert_success_with_stdout_contains(
        empty_drop_readiness_source,
        {"drop readiness source correlations actions 0 semantic sites 0"}
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_deduped_planned_drop_report.or";
    remove_error = {};
    std::filesystem::remove(deduped_planned_drop_report_path, remove_error);
    write_concurrency_fixture(
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

    auto raw_write_generic_helper_returned_pointer_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_helper_returned_pointer_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_helper_returned_pointer_same_width_success_path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Int32>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_helper_returned_pointer_same_width_success_path));

    auto raw_write_generic_helper_returned_pointer_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_helper_returned_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        raw_write_generic_helper_returned_pointer_mismatch_failure_path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_generic_helper_returned_pointer_mismatch_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_generic_receiver_method_pointer_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_receiver_method_pointer_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_receiver_method_pointer_same_width_success_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    id: Int64",
            "extend Device<T>",
            "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>",
            "        return base",
            "unsafe function write_word(device: Device<Int32>, base: Pointer<Int32>, value: UInt32) -> Unit",
            "    raw_write(device.ptr(base), value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_receiver_method_pointer_same_width_success_path));

    auto raw_write_generic_receiver_method_pointer_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_receiver_method_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        raw_write_generic_receiver_method_pointer_mismatch_failure_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    id: Int64",
            "extend Device<T>",
            "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>",
            "        return base",
            "unsafe function write_word(device: Device<Byte>, base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.ptr(base), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_generic_receiver_method_pointer_mismatch_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_generic_record_pointer_field_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_pointer_field_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_record_pointer_field_same_width_success_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Int32>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_record_pointer_field_same_width_success_path));

    auto raw_write_generic_record_pointer_field_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_pointer_field_mismatch_failure.or";
    write_concurrency_fixture(
        raw_write_generic_record_pointer_field_mismatch_failure_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_generic_record_pointer_field_mismatch_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_generic_record_scalar_field_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_scalar_field_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_record_scalar_field_same_width_success_path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "unsafe function write_word(box: Box<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, box.value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_record_scalar_field_same_width_success_path));

    auto address_return_generic_record_field_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_address_return_generic_record_field_success.or";
    write_concurrency_fixture(
        address_return_generic_record_field_success_path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "function read_base(box: Box<Address>) -> Address",
            "    return box.value",
        }
    );
    assert_parse_success(run_parse(app, address_return_generic_record_field_success_path));

    auto switch_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_thread_origin_failure.or";
    write_concurrency_fixture(
        switch_thread_origin_path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = thread",
            "        1",
            "    switch flag",
            "        true => worker = thread",
            "            2",
            "        false => worker = thread",
            "            3",
            "    return await worker",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, switch_thread_origin_path),
        "await cannot be used with thread values; use .join() instead"
    );

    auto repeat_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_repeat_thread_origin_failure.or";
    write_concurrency_fixture(
        repeat_thread_origin_path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = 0",
            "    repeat",
            "        worker = thread",
            "            2",
            "    while flag",
            "    return await worker",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, repeat_thread_origin_path),
        "await cannot be used with thread values; use .join() instead"
    );

    auto for_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_for_thread_origin_failure.or";
    write_concurrency_fixture(
        for_thread_origin_path,
        "demo.await",
        {
            "async function fetch(items: shared View<Int64>) -> Int64",
            "    var worker = thread",
            "        1",
            "    for item in items",
            "        worker = thread",
            "            2",
            "    return await worker",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, for_thread_origin_path),
        "await cannot be used with thread values; use .join() instead"
    );

    auto guard_async_missing_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_guard_async_origin_failure.or";
    write_concurrency_fixture(
        guard_async_missing_origin_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = 0",
            "    guard flag else",
            "        pending = request(url)",
            "        return await request(url)",
            "    return await pending",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, guard_async_missing_origin_path),
        "await expression currently requires a task value or declared async call result"
    );

    auto switch_nonfinal_default_branch_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nonfinal_default_branch_no_cascade_failure.or";
    write_bool_value_pattern_switch_fixture(
        switch_nonfinal_default_branch_no_cascade_failure_path,
        {"default => await flag", "true => 1"}
    );

    assert_parse_failure_contains_without(
        run_parse(app, switch_nonfinal_default_branch_no_cascade_failure_path),
        "switch default case must be the final case",
        "await expression is only valid inside async functions"
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
