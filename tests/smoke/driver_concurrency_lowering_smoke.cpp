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

auto run_emit_llvm(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "--emit-llvm",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_program(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "run",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
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

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_concurrency_lowering_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto emit_thread_spawn_path = std::filesystem::temp_directory_path() / "emit_thread_spawn.or";
    write_fixture(
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

    auto emit_thread_abandoned_path = std::filesystem::temp_directory_path() / "emit_thread_abandoned.or";
    write_fixture(
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

    auto emit_task_spawn_path = std::filesystem::temp_directory_path() / "emit_task_spawn.or";
    write_fixture(
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

    auto record_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_task_main.or";
    auto record_task_ir = run_emit_llvm(app, record_task_path);
    assert_success_with_stdout_contains(
        record_task_ir,
        {
            "%pending.task.env = alloca { i32 }",
            "store i32 %first, ptr %tmp0",
            "%pending.task.result = alloca %record.Pair",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.13.0, ptr %pending.task.env, ptr %pending.task.result, i64 8, ptr @__orison_task_cleanup.main.13.0)",
            "define private void @__orison_task_thunk.main.13.0(ptr %environment, ptr %result_storage)",
            "%tmp1 = load i32, ptr %tmp0",
            "store %record.Pair %tmp3, ptr %result_storage",
            "load %record.Pair, ptr %pending.task.result",
        }
    );
    auto record_task = run_program(app, record_task_path);
    assert(record_task.exit_code == 0);
    assert(record_task.stdout_text.empty());
    assert(record_task.stderr_text.empty());

    auto record_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_thread_main.or";
    auto record_thread_ir = run_emit_llvm(app, record_thread_path);
    assert_success_with_stdout_contains(
        record_thread_ir,
        {
            "%worker.thread.env = alloca { i32 }",
            "store i32 %first, ptr %tmp0",
            "%worker.thread.result = alloca %record.Pair",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.13.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 8, ptr @__orison_thread_cleanup.main.13.0)",
            "define private void @__orison_thread_thunk.main.13.0(ptr %environment, ptr %result_storage)",
            "%tmp1 = load i32, ptr %tmp0",
            "store %record.Pair %tmp3, ptr %result_storage",
            "load %record.Pair, ptr %worker.thread.result",
        }
    );
    auto record_thread = run_program(app, record_thread_path);
    assert(record_thread.exit_code == 0);
    assert(record_thread.stdout_text.empty());
    assert(record_thread.stderr_text.empty());

    auto choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_choice_task_main.or";
    auto choice_task_ir = run_emit_llvm(app, choice_task_path);
    assert_success_with_stdout_contains(
        choice_task_ir,
        {
            "%pending.task.env = alloca { i32 }",
            "store i32 %expected, ptr %tmp0",
            "%pending.task.result = alloca { i32, i32 }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.13.0, ptr %pending.task.env, ptr %pending.task.result, i64 8, ptr @__orison_task_cleanup.main.13.0)",
            "define private void @__orison_task_thunk.main.13.0(ptr %environment, ptr %result_storage)",
            "%tmp1 = load i32, ptr %tmp0",
            "%tmp2 = insertvalue { i32, i32 } undef, i32 1, 0",
            "%tmp3 = insertvalue { i32, i32 } %tmp2, i32 %tmp1, 1",
            "store { i32, i32 } %tmp3, ptr %result_storage",
            "load { i32, i32 }, ptr %pending.task.result",
            "extractvalue { i32, i32 } %tmp2, 0",
            "extractvalue { i32, i32 } %tmp2, 1",
        }
    );
    auto choice_task = run_program(app, choice_task_path);
    assert(choice_task.exit_code == 0);
    assert(choice_task.stdout_text.empty());
    assert(choice_task.stderr_text.empty());

    auto choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_choice_thread_main.or";
    auto choice_thread_ir = run_emit_llvm(app, choice_thread_path);
    assert_success_with_stdout_contains(
        choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32 }",
            "store i32 %expected, ptr %tmp0",
            "%worker.thread.result = alloca { i32, i32 }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.13.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 8, ptr @__orison_thread_cleanup.main.13.0)",
            "define private void @__orison_thread_thunk.main.13.0(ptr %environment, ptr %result_storage)",
            "%tmp1 = load i32, ptr %tmp0",
            "%tmp2 = insertvalue { i32, i32 } undef, i32 1, 0",
            "%tmp3 = insertvalue { i32, i32 } %tmp2, i32 %tmp1, 1",
            "store { i32, i32 } %tmp3, ptr %result_storage",
            "load { i32, i32 }, ptr %worker.thread.result",
            "extractvalue { i32, i32 } %tmp2, 0",
            "extractvalue { i32, i32 } %tmp2, 1",
        }
    );
    auto choice_thread = run_program(app, choice_thread_path);
    assert(choice_thread.exit_code == 0);
    assert(choice_thread.stdout_text.empty());
    assert(choice_thread.stderr_text.empty());

    auto distinct_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_distinct_choice_task_main.or";
    auto distinct_choice_task_ir = run_emit_llvm(app, distinct_choice_task_path);
    assert_success_with_stdout_contains(
        distinct_choice_task_ir,
        {
            "%pending.task.result = alloca { i32, [4 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.13.0, ptr %pending.task.env, ptr %pending.task.result, i64 8, ptr @__orison_task_cleanup.main.13.0)",
            "%tmp2 = insertvalue { i32, [4 x i8] } undef, i32 1, 0",
            "store i32 %tmp1, ptr %tmp3, align 8",
            "%tmp4 = load [4 x i8], ptr %tmp3, align 8",
            "%tmp5 = insertvalue { i32, [4 x i8] } %tmp2, [4 x i8] %tmp4, 1",
            "store { i32, [4 x i8] } %tmp5, ptr %result_storage",
            "load { i32, [4 x i8] }, ptr %pending.task.result",
            "load i1, ptr %tmp5, align 8",
            "load i32, ptr %tmp9, align 8",
        }
    );
    auto distinct_choice_task = run_program(app, distinct_choice_task_path);
    assert(distinct_choice_task.exit_code == 0);
    assert(distinct_choice_task.stdout_text.empty());
    assert(distinct_choice_task.stderr_text.empty());

    auto distinct_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_distinct_choice_thread_main.or";
    auto distinct_choice_thread_ir = run_emit_llvm(app, distinct_choice_thread_path);
    assert_success_with_stdout_contains(
        distinct_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i1 }",
            "%worker.thread.result = alloca { i32, [4 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.13.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 8, ptr @__orison_thread_cleanup.main.13.0)",
            "%tmp2 = insertvalue { i32, [4 x i8] } undef, i32 0, 0",
            "store i1 %tmp1, ptr %tmp3, align 8",
            "%tmp4 = load [4 x i8], ptr %tmp3, align 8",
            "%tmp5 = insertvalue { i32, [4 x i8] } %tmp2, [4 x i8] %tmp4, 1",
            "store { i32, [4 x i8] } %tmp5, ptr %result_storage",
            "load { i32, [4 x i8] }, ptr %worker.thread.result",
            "load i1, ptr %tmp5, align 8",
            "load i32, ptr %tmp9, align 8",
        }
    );
    auto distinct_choice_thread = run_program(app, distinct_choice_thread_path);
    assert(distinct_choice_thread.exit_code == 0);
    assert(distinct_choice_thread.stdout_text.empty());
    assert(distinct_choice_thread.stderr_text.empty());

    auto array_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_array_choice_task_main.or";
    auto array_choice_task_ir = run_emit_llvm(app, array_choice_task_path);
    assert_success_with_stdout_contains(
        array_choice_task_ir,
        {
            "%pending.task.env = alloca { i32, i32 }",
            "%pending.task.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.14.0, ptr %pending.task.env, ptr %pending.task.result, i64 12, ptr @__orison_task_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "store [2 x i32] %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "%tmp9 = insertvalue { i32, [8 x i8] } %tmp4, [8 x i8] %tmp8, 1",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %pending.task.result",
            "load [2 x i32], ptr %tmp10, align 8",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 1",
        }
    );
    auto array_choice_task = run_program(app, array_choice_task_path);
    assert(array_choice_task.exit_code == 0);
    assert(array_choice_task.stdout_text.empty());
    assert(array_choice_task.stderr_text.empty());

    auto array_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_array_choice_thread_main.or";
    auto array_choice_thread_ir = run_emit_llvm(app, array_choice_thread_path);
    assert_success_with_stdout_contains(
        array_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32 }",
            "%worker.thread.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.14.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 12, ptr @__orison_thread_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "store [2 x i32] %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "%tmp9 = insertvalue { i32, [8 x i8] } %tmp4, [8 x i8] %tmp8, 1",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %worker.thread.result",
            "load [2 x i32], ptr %tmp10, align 8",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 1",
        }
    );
    auto array_choice_thread = run_program(app, array_choice_thread_path);
    assert(array_choice_thread.exit_code == 0);
    assert(array_choice_thread.stdout_text.empty());
    assert(array_choice_thread.stderr_text.empty());

    auto record_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_choice_task_main.or";
    auto record_choice_task_ir = run_emit_llvm(app, record_choice_task_path);
    assert_success_with_stdout_contains(
        record_choice_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%pending.task.env = alloca { i32, i32 }",
            "%pending.task.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.18.0, ptr %pending.task.env, ptr %pending.task.result, i64 12, ptr @__orison_task_cleanup.main.18.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue %record.Pair undef, i32 %tmp1, 0",
            "store %record.Pair %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "%tmp9 = insertvalue { i32, [8 x i8] } %tmp4, [8 x i8] %tmp8, 1",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %pending.task.result",
            "load %record.Pair, ptr %tmp10, align 8",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 1",
        }
    );
    auto record_choice_task = run_program(app, record_choice_task_path);
    assert(record_choice_task.exit_code == 0);
    assert(record_choice_task.stdout_text.empty());
    assert(record_choice_task.stderr_text.empty());

    auto record_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_choice_thread_main.or";
    auto record_choice_thread_ir = run_emit_llvm(app, record_choice_thread_path);
    assert_success_with_stdout_contains(
        record_choice_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%worker.thread.env = alloca { i32, i32 }",
            "%worker.thread.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.18.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 12, ptr @__orison_thread_cleanup.main.18.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue %record.Pair undef, i32 %tmp1, 0",
            "store %record.Pair %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "%tmp9 = insertvalue { i32, [8 x i8] } %tmp4, [8 x i8] %tmp8, 1",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %worker.thread.result",
            "load %record.Pair, ptr %tmp10, align 8",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 1",
        }
    );
    auto record_choice_thread = run_program(app, record_choice_thread_path);
    assert(record_choice_thread.exit_code == 0);
    assert(record_choice_thread.stdout_text.empty());
    assert(record_choice_thread.stderr_text.empty());

    auto multi_payload_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_multi_payload_choice_task_main.or";
    auto multi_payload_choice_task_ir = run_emit_llvm(app, multi_payload_choice_task_path);
    assert_success_with_stdout_contains(
        multi_payload_choice_task_ir,
        {
            "%pending.task.env = alloca { i32, i32 }",
            "%pending.task.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.14.0, ptr %pending.task.env, ptr %pending.task.result, i64 12, ptr @__orison_task_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue { i32, i32 } undef, i32 %tmp1, 0",
            "%tmp6 = insertvalue { i32, i32 } %tmp5, i32 %tmp3, 1",
            "store { i32, i32 } %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %pending.task.result",
            "load { i32, i32 }, ptr %tmp10, align 8",
            "extractvalue { i32, i32 } %tmp11, 0",
            "extractvalue { i32, i32 } %tmp11, 1",
        }
    );
    auto multi_payload_choice_task = run_program(app, multi_payload_choice_task_path);
    assert(multi_payload_choice_task.exit_code == 0);
    assert(multi_payload_choice_task.stdout_text.empty());
    assert(multi_payload_choice_task.stderr_text.empty());

    auto multi_payload_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_multi_payload_choice_thread_main.or";
    auto multi_payload_choice_thread_ir = run_emit_llvm(app, multi_payload_choice_thread_path);
    assert_success_with_stdout_contains(
        multi_payload_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32 }",
            "%worker.thread.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.14.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 12, ptr @__orison_thread_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue { i32, i32 } undef, i32 %tmp1, 0",
            "%tmp6 = insertvalue { i32, i32 } %tmp5, i32 %tmp3, 1",
            "store { i32, i32 } %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %worker.thread.result",
            "load { i32, i32 }, ptr %tmp10, align 8",
            "extractvalue { i32, i32 } %tmp11, 0",
            "extractvalue { i32, i32 } %tmp11, 1",
        }
    );
    auto multi_payload_choice_thread = run_program(app, multi_payload_choice_thread_path);
    assert(multi_payload_choice_thread.exit_code == 0);
    assert(multi_payload_choice_thread.stdout_text.empty());
    assert(multi_payload_choice_thread.stderr_text.empty());

    auto mixed_payload_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_mixed_payload_choice_task_main.or";
    auto mixed_payload_choice_task_ir = run_emit_llvm(app, mixed_payload_choice_task_path);
    assert_success_with_stdout_contains(
        mixed_payload_choice_task_ir,
        {
            "%pending.task.env = alloca { i32, i1 }",
            "%pending.task.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.14.0, ptr %pending.task.env, ptr %pending.task.result, i64 12, ptr @__orison_task_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue { i32, i1 } undef, i32 %tmp1, 0",
            "%tmp6 = insertvalue { i32, i1 } %tmp5, i1 %tmp3, 1",
            "store { i32, i1 } %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %pending.task.result",
            "load { i32, i1 }, ptr %tmp10, align 8",
            "extractvalue { i32, i1 } %tmp11, 0",
            "extractvalue { i32, i1 } %tmp11, 1",
        }
    );
    auto mixed_payload_choice_task = run_program(app, mixed_payload_choice_task_path);
    assert(mixed_payload_choice_task.exit_code == 0);
    assert(mixed_payload_choice_task.stdout_text.empty());
    assert(mixed_payload_choice_task.stderr_text.empty());

    auto mixed_payload_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_mixed_payload_choice_thread_main.or";
    auto mixed_payload_choice_thread_ir = run_emit_llvm(app, mixed_payload_choice_thread_path);
    assert_success_with_stdout_contains(
        mixed_payload_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i1 }",
            "%worker.thread.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.14.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 12, ptr @__orison_thread_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue { i32, i1 } undef, i32 %tmp1, 0",
            "%tmp6 = insertvalue { i32, i1 } %tmp5, i1 %tmp3, 1",
            "store { i32, i1 } %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %worker.thread.result",
            "load { i32, i1 }, ptr %tmp10, align 8",
            "extractvalue { i32, i1 } %tmp11, 0",
            "extractvalue { i32, i1 } %tmp11, 1",
        }
    );
    auto mixed_payload_choice_thread = run_program(app, mixed_payload_choice_thread_path);
    assert(mixed_payload_choice_thread.exit_code == 0);
    assert(mixed_payload_choice_thread.stdout_text.empty());
    assert(mixed_payload_choice_thread.stderr_text.empty());

    auto aggregate_scalar_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_aggregate_scalar_choice_task_main.or";
    auto aggregate_scalar_choice_task_ir = run_emit_llvm(app, aggregate_scalar_choice_task_path);
    assert_success_with_stdout_contains(
        aggregate_scalar_choice_task_ir,
        {
            "%pending.task.env = alloca { i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [12 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.15.0, ptr %pending.task.env, ptr %pending.task.result, i64 16, ptr @__orison_task_cleanup.main.15.0)",
            "%tmp6 = insertvalue { i32, [12 x i8] } undef, i32 1, 0",
            "%tmp7 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp9 = insertvalue { [2 x i32], i32 } undef, [2 x i32] %tmp8, 0",
            "%tmp10 = insertvalue { [2 x i32], i32 } %tmp9, i32 %tmp5, 1",
            "store { [2 x i32], i32 } %tmp10, ptr %tmp11, align 8",
            "%tmp12 = load [12 x i8], ptr %tmp11, align 8",
            "store { i32, [12 x i8] } %tmp13, ptr %result_storage",
            "load { i32, [12 x i8] }, ptr %pending.task.result",
            "load { [2 x i32], i32 }, ptr %tmp11, align 8",
            "extractvalue { [2 x i32], i32 } %tmp12, 0",
            "extractvalue { [2 x i32], i32 } %tmp12, 1",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 1",
        }
    );
    auto aggregate_scalar_choice_task = run_program(app, aggregate_scalar_choice_task_path);
    assert(aggregate_scalar_choice_task.exit_code == 0);
    assert(aggregate_scalar_choice_task.stdout_text.empty());
    assert(aggregate_scalar_choice_task.stderr_text.empty());

    auto aggregate_scalar_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_aggregate_scalar_choice_thread_main.or";
    auto aggregate_scalar_choice_thread_ir = run_emit_llvm(app, aggregate_scalar_choice_thread_path);
    assert_success_with_stdout_contains(
        aggregate_scalar_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [12 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.15.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 16, ptr @__orison_thread_cleanup.main.15.0)",
            "%tmp6 = insertvalue { i32, [12 x i8] } undef, i32 1, 0",
            "%tmp7 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp9 = insertvalue { [2 x i32], i32 } undef, [2 x i32] %tmp8, 0",
            "%tmp10 = insertvalue { [2 x i32], i32 } %tmp9, i32 %tmp5, 1",
            "store { [2 x i32], i32 } %tmp10, ptr %tmp11, align 8",
            "%tmp12 = load [12 x i8], ptr %tmp11, align 8",
            "store { i32, [12 x i8] } %tmp13, ptr %result_storage",
            "load { i32, [12 x i8] }, ptr %worker.thread.result",
            "load { [2 x i32], i32 }, ptr %tmp11, align 8",
            "extractvalue { [2 x i32], i32 } %tmp12, 0",
            "extractvalue { [2 x i32], i32 } %tmp12, 1",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %values.addr, i64 0, i64 1",
        }
    );
    auto aggregate_scalar_choice_thread = run_program(app, aggregate_scalar_choice_thread_path);
    assert(aggregate_scalar_choice_thread.exit_code == 0);
    assert(aggregate_scalar_choice_thread.stdout_text.empty());
    assert(aggregate_scalar_choice_thread.stderr_text.empty());

    auto record_scalar_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_scalar_choice_task_main.or";
    auto record_scalar_choice_task_ir = run_emit_llvm(app, record_scalar_choice_task_path);
    assert_success_with_stdout_contains(
        record_scalar_choice_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%pending.task.env = alloca { i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [12 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.19.0, ptr %pending.task.env, ptr %pending.task.result, i64 16, ptr @__orison_task_cleanup.main.19.0)",
            "%tmp6 = insertvalue { i32, [12 x i8] } undef, i32 1, 0",
            "%tmp7 = insertvalue %record.Pair undef, i32 %tmp1, 0",
            "%tmp9 = insertvalue { %record.Pair, i32 } undef, %record.Pair %tmp8, 0",
            "%tmp10 = insertvalue { %record.Pair, i32 } %tmp9, i32 %tmp5, 1",
            "store { %record.Pair, i32 } %tmp10, ptr %tmp11, align 8",
            "%tmp12 = load [12 x i8], ptr %tmp11, align 8",
            "store { i32, [12 x i8] } %tmp13, ptr %result_storage",
            "load { i32, [12 x i8] }, ptr %pending.task.result",
            "load { %record.Pair, i32 }, ptr %tmp11, align 8",
            "extractvalue { %record.Pair, i32 } %tmp12, 0",
            "extractvalue { %record.Pair, i32 } %tmp12, 1",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 1",
        }
    );
    auto record_scalar_choice_task = run_program(app, record_scalar_choice_task_path);
    assert(record_scalar_choice_task.exit_code == 0);
    assert(record_scalar_choice_task.stdout_text.empty());
    assert(record_scalar_choice_task.stderr_text.empty());

    auto record_scalar_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_scalar_choice_thread_main.or";
    auto record_scalar_choice_thread_ir = run_emit_llvm(app, record_scalar_choice_thread_path);
    assert_success_with_stdout_contains(
        record_scalar_choice_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%worker.thread.env = alloca { i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [12 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.19.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 16, ptr @__orison_thread_cleanup.main.19.0)",
            "%tmp6 = insertvalue { i32, [12 x i8] } undef, i32 1, 0",
            "%tmp7 = insertvalue %record.Pair undef, i32 %tmp1, 0",
            "%tmp9 = insertvalue { %record.Pair, i32 } undef, %record.Pair %tmp8, 0",
            "%tmp10 = insertvalue { %record.Pair, i32 } %tmp9, i32 %tmp5, 1",
            "store { %record.Pair, i32 } %tmp10, ptr %tmp11, align 8",
            "%tmp12 = load [12 x i8], ptr %tmp11, align 8",
            "store { i32, [12 x i8] } %tmp13, ptr %result_storage",
            "load { i32, [12 x i8] }, ptr %worker.thread.result",
            "load { %record.Pair, i32 }, ptr %tmp11, align 8",
            "extractvalue { %record.Pair, i32 } %tmp12, 0",
            "extractvalue { %record.Pair, i32 } %tmp12, 1",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %value.addr, i32 0, i32 1",
        }
    );
    auto record_scalar_choice_thread = run_program(app, record_scalar_choice_thread_path);
    assert(record_scalar_choice_thread.exit_code == 0);
    assert(record_scalar_choice_thread.stdout_text.empty());
    assert(record_scalar_choice_thread.stderr_text.empty());

    auto generic_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_choice_task_main.or";
    auto generic_choice_task_ir = run_emit_llvm(app, generic_choice_task_path);
    assert_success_with_stdout_contains(
        generic_choice_task_ir,
        {
            "%pending.task.env = alloca { i32, i32 }",
            "%pending.task.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.14.0, ptr %pending.task.env, ptr %pending.task.result, i64 12, ptr @__orison_task_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue { i32, i32 } undef, i32 %tmp1, 0",
            "%tmp6 = insertvalue { i32, i32 } %tmp5, i32 %tmp3, 1",
            "store { i32, i32 } %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %pending.task.result",
            "load { i32, i32 }, ptr %tmp10, align 8",
            "extractvalue { i32, i32 } %tmp11, 0",
            "extractvalue { i32, i32 } %tmp11, 1",
            "@__orison_method.examples.concurrencygenericchoicetask.Result_UInt32_.placeholder",
        }
    );
    auto generic_choice_task = run_program(app, generic_choice_task_path);
    assert(generic_choice_task.exit_code == 0);
    assert(generic_choice_task.stdout_text.empty());
    assert(generic_choice_task.stderr_text.empty());

    auto generic_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_choice_thread_main.or";
    auto generic_choice_thread_ir = run_emit_llvm(app, generic_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32 }",
            "%worker.thread.result = alloca { i32, [8 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.14.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 12, ptr @__orison_thread_cleanup.main.14.0)",
            "%tmp4 = insertvalue { i32, [8 x i8] } undef, i32 1, 0",
            "%tmp5 = insertvalue { i32, i32 } undef, i32 %tmp1, 0",
            "%tmp6 = insertvalue { i32, i32 } %tmp5, i32 %tmp3, 1",
            "store { i32, i32 } %tmp6, ptr %tmp7, align 8",
            "%tmp8 = load [8 x i8], ptr %tmp7, align 8",
            "store { i32, [8 x i8] } %tmp9, ptr %result_storage",
            "load { i32, [8 x i8] }, ptr %worker.thread.result",
            "load { i32, i32 }, ptr %tmp10, align 8",
            "extractvalue { i32, i32 } %tmp11, 0",
            "extractvalue { i32, i32 } %tmp11, 1",
            "@__orison_method.examples.concurrencygenericchoicethread.Result_UInt32_.placeholder",
        }
    );
    auto generic_choice_thread = run_program(app, generic_choice_thread_path);
    assert(generic_choice_thread.exit_code == 0);
    assert(generic_choice_thread.stdout_text.empty());
    assert(generic_choice_thread.stderr_text.empty());

    auto generic_aggregate_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_aggregate_choice_task_main.or";
    auto generic_aggregate_choice_task_ir = run_emit_llvm(app, generic_aggregate_choice_task_path);
    assert_success_with_stdout_contains(
        generic_aggregate_choice_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%pending.task.env = alloca { i32, i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [16 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.20.0, ptr %pending.task.env, ptr %pending.task.result, i64 20, ptr @__orison_task_cleanup.main.20.0)",
            "%tmp8 = insertvalue { i32, [16 x i8] } undef, i32 1, 0",
            "%tmp11 = insertvalue { %record.Pair, %record.Pair } undef, %record.Pair %tmp10, 0",
            "%tmp14 = insertvalue { %record.Pair, %record.Pair } %tmp11, %record.Pair %tmp13, 1",
            "store { %record.Pair, %record.Pair } %tmp14, ptr %tmp15, align 8",
            "%tmp16 = load [16 x i8], ptr %tmp15, align 8",
            "store { i32, [16 x i8] } %tmp17, ptr %result_storage",
            "load { i32, [16 x i8] }, ptr %pending.task.result",
            "load { %record.Pair, %record.Pair }, ptr %tmp12, align 8",
            "extractvalue { %record.Pair, %record.Pair } %tmp13, 0",
            "extractvalue { %record.Pair, %record.Pair } %tmp13, 1",
            "getelementptr %record.Pair, ptr %left.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %left.addr, i32 0, i32 1",
            "getelementptr %record.Pair, ptr %right.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %right.addr, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericaggregatechoicetask.Result_Pair_.placeholder",
        }
    );
    auto generic_aggregate_choice_task = run_program(app, generic_aggregate_choice_task_path);
    assert(generic_aggregate_choice_task.exit_code == 0);
    assert(generic_aggregate_choice_task.stdout_text.empty());
    assert(generic_aggregate_choice_task.stderr_text.empty());

    auto generic_aggregate_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_aggregate_choice_thread_main.or";
    auto generic_aggregate_choice_thread_ir = run_emit_llvm(app, generic_aggregate_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_aggregate_choice_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%worker.thread.env = alloca { i32, i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [16 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.20.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 20, ptr @__orison_thread_cleanup.main.20.0)",
            "%tmp8 = insertvalue { i32, [16 x i8] } undef, i32 1, 0",
            "%tmp11 = insertvalue { %record.Pair, %record.Pair } undef, %record.Pair %tmp10, 0",
            "%tmp14 = insertvalue { %record.Pair, %record.Pair } %tmp11, %record.Pair %tmp13, 1",
            "store { %record.Pair, %record.Pair } %tmp14, ptr %tmp15, align 8",
            "%tmp16 = load [16 x i8], ptr %tmp15, align 8",
            "store { i32, [16 x i8] } %tmp17, ptr %result_storage",
            "load { i32, [16 x i8] }, ptr %worker.thread.result",
            "load { %record.Pair, %record.Pair }, ptr %tmp12, align 8",
            "extractvalue { %record.Pair, %record.Pair } %tmp13, 0",
            "extractvalue { %record.Pair, %record.Pair } %tmp13, 1",
            "getelementptr %record.Pair, ptr %left.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %left.addr, i32 0, i32 1",
            "getelementptr %record.Pair, ptr %right.addr, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %right.addr, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericaggregatechoicethread.Result_Pair_.placeholder",
        }
    );
    auto generic_aggregate_choice_thread = run_program(app, generic_aggregate_choice_thread_path);
    assert(generic_aggregate_choice_thread.exit_code == 0);
    assert(generic_aggregate_choice_thread.stdout_text.empty());
    assert(generic_aggregate_choice_thread.stderr_text.empty());

    auto generic_array_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_array_choice_task_main.or";
    auto generic_array_choice_task_ir = run_emit_llvm(app, generic_array_choice_task_path);
    assert_success_with_stdout_contains(
        generic_array_choice_task_ir,
        {
            "%pending.task.env = alloca { i32, i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [16 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.16.0, ptr %pending.task.env, ptr %pending.task.result, i64 20, ptr @__orison_task_cleanup.main.16.0)",
            "%tmp9 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp11 = insertvalue { [2 x i32], [2 x i32] } undef, [2 x i32] %tmp10, 0",
            "%tmp14 = insertvalue { [2 x i32], [2 x i32] } %tmp11, [2 x i32] %tmp13, 1",
            "store { [2 x i32], [2 x i32] } %tmp14, ptr %tmp15, align 8",
            "%tmp16 = load [16 x i8], ptr %tmp15, align 8",
            "store { i32, [16 x i8] } %tmp17, ptr %result_storage",
            "load { i32, [16 x i8] }, ptr %pending.task.result",
            "load { [2 x i32], [2 x i32] }, ptr %tmp12, align 8",
            "extractvalue { [2 x i32], [2 x i32] } %tmp13, 0",
            "extractvalue { [2 x i32], [2 x i32] } %tmp13, 1",
            "getelementptr [2 x i32], ptr %left.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %left.addr, i64 0, i64 1",
            "getelementptr [2 x i32], ptr %right.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %right.addr, i64 0, i64 1",
            "@__orison_method.examples.concurrencygenericarraychoicetask.Result_Array_UInt32__2__.placeholder",
        }
    );
    auto generic_array_choice_task = run_program(app, generic_array_choice_task_path);
    assert(generic_array_choice_task.exit_code == 0);
    assert(generic_array_choice_task.stdout_text.empty());
    assert(generic_array_choice_task.stderr_text.empty());

    auto generic_array_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_array_choice_thread_main.or";
    auto generic_array_choice_thread_ir = run_emit_llvm(app, generic_array_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_array_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [16 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.16.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 20, ptr @__orison_thread_cleanup.main.16.0)",
            "%tmp9 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp11 = insertvalue { [2 x i32], [2 x i32] } undef, [2 x i32] %tmp10, 0",
            "%tmp14 = insertvalue { [2 x i32], [2 x i32] } %tmp11, [2 x i32] %tmp13, 1",
            "store { [2 x i32], [2 x i32] } %tmp14, ptr %tmp15, align 8",
            "%tmp16 = load [16 x i8], ptr %tmp15, align 8",
            "store { i32, [16 x i8] } %tmp17, ptr %result_storage",
            "load { i32, [16 x i8] }, ptr %worker.thread.result",
            "load { [2 x i32], [2 x i32] }, ptr %tmp12, align 8",
            "extractvalue { [2 x i32], [2 x i32] } %tmp13, 0",
            "extractvalue { [2 x i32], [2 x i32] } %tmp13, 1",
            "getelementptr [2 x i32], ptr %left.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %left.addr, i64 0, i64 1",
            "getelementptr [2 x i32], ptr %right.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %right.addr, i64 0, i64 1",
            "@__orison_method.examples.concurrencygenericarraychoicethread.Result_Array_UInt32__2__.placeholder",
        }
    );
    auto generic_array_choice_thread = run_program(app, generic_array_choice_thread_path);
    assert(generic_array_choice_thread.exit_code == 0);
    assert(generic_array_choice_thread.stdout_text.empty());
    assert(generic_array_choice_thread.stderr_text.empty());

    auto generic_nested_array_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_nested_array_choice_task_main.or";
    auto generic_nested_array_choice_task_ir = run_emit_llvm(app, generic_nested_array_choice_task_path);
    assert_success_with_stdout_contains(
        generic_nested_array_choice_task_ir,
        {
            "%pending.task.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [32 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.20.0, ptr %pending.task.env, ptr %pending.task.result, i64 36, ptr @__orison_task_cleanup.main.20.0)",
            "%tmp19 = insertvalue [2 x [2 x i32]] undef, [2 x i32] %tmp18, 0",
            "%tmp23 = insertvalue { [2 x [2 x i32]], [2 x [2 x i32]] } undef, [2 x [2 x i32]] %tmp22, 0",
            "%tmp30 = insertvalue { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp23, [2 x [2 x i32]] %tmp29, 1",
            "store { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp30, ptr %tmp31, align 8",
            "%tmp32 = load [32 x i8], ptr %tmp31, align 8",
            "store { i32, [32 x i8] } %tmp33, ptr %result_storage",
            "load { i32, [32 x i8] }, ptr %pending.task.result",
            "load { [2 x [2 x i32]], [2 x [2 x i32]] }, ptr %tmp16, align 8",
            "extractvalue { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp17, 0",
            "extractvalue { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp17, 1",
            "getelementptr [2 x [2 x i32]], ptr %left.addr, i64 0, i64 0",
            "getelementptr [2 x [2 x i32]], ptr %left.addr, i64 0, i64 1",
            "getelementptr [2 x [2 x i32]], ptr %right.addr, i64 0, i64 0",
            "getelementptr [2 x [2 x i32]], ptr %right.addr, i64 0, i64 1",
            "@__orison_method.examples.concurrencygenericnestedarraychoicetask.Result_Array_Array_UInt32__2___2__.placeholder",
        }
    );
    auto generic_nested_array_choice_task = run_program(app, generic_nested_array_choice_task_path);
    assert(generic_nested_array_choice_task.exit_code == 0);
    assert(generic_nested_array_choice_task.stdout_text.empty());
    assert(generic_nested_array_choice_task.stderr_text.empty());

    auto generic_nested_array_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_nested_array_choice_thread_main.or";
    auto generic_nested_array_choice_thread_ir = run_emit_llvm(app, generic_nested_array_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_nested_array_choice_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [32 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.20.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 36, ptr @__orison_thread_cleanup.main.20.0)",
            "%tmp19 = insertvalue [2 x [2 x i32]] undef, [2 x i32] %tmp18, 0",
            "%tmp23 = insertvalue { [2 x [2 x i32]], [2 x [2 x i32]] } undef, [2 x [2 x i32]] %tmp22, 0",
            "%tmp30 = insertvalue { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp23, [2 x [2 x i32]] %tmp29, 1",
            "store { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp30, ptr %tmp31, align 8",
            "%tmp32 = load [32 x i8], ptr %tmp31, align 8",
            "store { i32, [32 x i8] } %tmp33, ptr %result_storage",
            "load { i32, [32 x i8] }, ptr %worker.thread.result",
            "load { [2 x [2 x i32]], [2 x [2 x i32]] }, ptr %tmp16, align 8",
            "extractvalue { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp17, 0",
            "extractvalue { [2 x [2 x i32]], [2 x [2 x i32]] } %tmp17, 1",
            "getelementptr [2 x [2 x i32]], ptr %left.addr, i64 0, i64 0",
            "getelementptr [2 x [2 x i32]], ptr %left.addr, i64 0, i64 1",
            "getelementptr [2 x [2 x i32]], ptr %right.addr, i64 0, i64 0",
            "getelementptr [2 x [2 x i32]], ptr %right.addr, i64 0, i64 1",
            "@__orison_method.examples.concurrencygenericnestedarraychoicethread.Result_Array_Array_UInt32__2___2__.placeholder",
        }
    );
    auto generic_nested_array_choice_thread = run_program(app, generic_nested_array_choice_thread_path);
    assert(generic_nested_array_choice_thread.exit_code == 0);
    assert(generic_nested_array_choice_thread.stdout_text.empty());
    assert(generic_nested_array_choice_thread.stderr_text.empty());

    auto generic_record_array_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_record_array_choice_task_main.or";
    auto generic_record_array_choice_task_ir = run_emit_llvm(app, generic_record_array_choice_task_path);
    assert_success_with_stdout_contains(
        generic_record_array_choice_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%pending.task.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [32 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.24.0, ptr %pending.task.env, ptr %pending.task.result, i64 36, ptr @__orison_task_cleanup.main.24.0)",
            "%tmp19 = insertvalue [2 x %record.Pair] undef, %record.Pair %tmp18, 0",
            "%tmp23 = insertvalue { [2 x %record.Pair], [2 x %record.Pair] } undef, [2 x %record.Pair] %tmp22, 0",
            "%tmp30 = insertvalue { [2 x %record.Pair], [2 x %record.Pair] } %tmp23, [2 x %record.Pair] %tmp29, 1",
            "store { [2 x %record.Pair], [2 x %record.Pair] } %tmp30, ptr %tmp31, align 8",
            "%tmp32 = load [32 x i8], ptr %tmp31, align 8",
            "store { i32, [32 x i8] } %tmp33, ptr %result_storage",
            "load { i32, [32 x i8] }, ptr %pending.task.result",
            "load { [2 x %record.Pair], [2 x %record.Pair] }, ptr %tmp16, align 8",
            "extractvalue { [2 x %record.Pair], [2 x %record.Pair] } %tmp17, 0",
            "extractvalue { [2 x %record.Pair], [2 x %record.Pair] } %tmp17, 1",
            "getelementptr [2 x %record.Pair], ptr %left.addr, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %left.addr, i64 0, i64 1",
            "getelementptr [2 x %record.Pair], ptr %right.addr, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %right.addr, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp20, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %tmp47, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericrecordarraychoicetask.Result_Array_Pair__2__.placeholder",
        }
    );
    auto generic_record_array_choice_task = run_program(app, generic_record_array_choice_task_path);
    assert(generic_record_array_choice_task.exit_code == 0);
    assert(generic_record_array_choice_task.stdout_text.empty());
    assert(generic_record_array_choice_task.stderr_text.empty());

    auto generic_record_array_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_record_array_choice_thread_main.or";
    auto generic_record_array_choice_thread_ir = run_emit_llvm(app, generic_record_array_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_record_array_choice_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%worker.thread.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [32 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.24.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 36, ptr @__orison_thread_cleanup.main.24.0)",
            "%tmp19 = insertvalue [2 x %record.Pair] undef, %record.Pair %tmp18, 0",
            "%tmp23 = insertvalue { [2 x %record.Pair], [2 x %record.Pair] } undef, [2 x %record.Pair] %tmp22, 0",
            "%tmp30 = insertvalue { [2 x %record.Pair], [2 x %record.Pair] } %tmp23, [2 x %record.Pair] %tmp29, 1",
            "store { [2 x %record.Pair], [2 x %record.Pair] } %tmp30, ptr %tmp31, align 8",
            "%tmp32 = load [32 x i8], ptr %tmp31, align 8",
            "store { i32, [32 x i8] } %tmp33, ptr %result_storage",
            "load { i32, [32 x i8] }, ptr %worker.thread.result",
            "load { [2 x %record.Pair], [2 x %record.Pair] }, ptr %tmp16, align 8",
            "extractvalue { [2 x %record.Pair], [2 x %record.Pair] } %tmp17, 0",
            "extractvalue { [2 x %record.Pair], [2 x %record.Pair] } %tmp17, 1",
            "getelementptr [2 x %record.Pair], ptr %left.addr, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %left.addr, i64 0, i64 1",
            "getelementptr [2 x %record.Pair], ptr %right.addr, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %right.addr, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp20, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %tmp47, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericrecordarraychoicethread.Result_Array_Pair__2__.placeholder",
        }
    );
    auto generic_record_array_choice_thread = run_program(app, generic_record_array_choice_thread_path);
    assert(generic_record_array_choice_thread.exit_code == 0);
    assert(generic_record_array_choice_thread.stdout_text.empty());
    assert(generic_record_array_choice_thread.stderr_text.empty());

    auto generic_array_field_record_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_array_field_record_choice_task_main.or";
    auto generic_array_field_record_choice_task_ir = run_emit_llvm(app, generic_array_field_record_choice_task_path);
    assert_success_with_stdout_contains(
        generic_array_field_record_choice_task_ir,
        {
            "%record.Batch = type { [2 x i32], i32 }",
            "%pending.task.env = alloca { i32, i32, i32, i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [24 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.22.0, ptr %pending.task.env, ptr %pending.task.result, i64 28, ptr @__orison_task_cleanup.main.22.0)",
            "%tmp15 = insertvalue %record.Batch undef, [2 x i32] %tmp14, 0",
            "%tmp17 = insertvalue { %record.Batch, %record.Batch } undef, %record.Batch %tmp16, 0",
            "%tmp22 = insertvalue { %record.Batch, %record.Batch } %tmp17, %record.Batch %tmp21, 1",
            "store { %record.Batch, %record.Batch } %tmp22, ptr %tmp23, align 8",
            "%tmp24 = load [24 x i8], ptr %tmp23, align 8",
            "store { i32, [24 x i8] } %tmp25, ptr %result_storage",
            "load { i32, [24 x i8] }, ptr %pending.task.result",
            "load { %record.Batch, %record.Batch }, ptr %tmp14, align 8",
            "extractvalue { %record.Batch, %record.Batch } %tmp15, 0",
            "extractvalue { %record.Batch, %record.Batch } %tmp15, 1",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 0",
            "getelementptr [2 x i32], ptr %tmp18, i64 0, i64 0",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 0",
            "getelementptr [2 x i32], ptr %tmp32, i64 0, i64 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericarrayfieldrecordchoicetask.Result_Batch_.placeholder",
        }
    );
    auto generic_array_field_record_choice_task = run_program(app, generic_array_field_record_choice_task_path);
    assert(generic_array_field_record_choice_task.exit_code == 0);
    assert(generic_array_field_record_choice_task.stdout_text.empty());
    assert(generic_array_field_record_choice_task.stderr_text.empty());

    auto generic_array_field_record_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_array_field_record_choice_thread_main.or";
    auto generic_array_field_record_choice_thread_ir = run_emit_llvm(app, generic_array_field_record_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_array_field_record_choice_thread_ir,
        {
            "%record.Batch = type { [2 x i32], i32 }",
            "%worker.thread.env = alloca { i32, i32, i32, i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [24 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.22.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 28, ptr @__orison_thread_cleanup.main.22.0)",
            "%tmp15 = insertvalue %record.Batch undef, [2 x i32] %tmp14, 0",
            "%tmp17 = insertvalue { %record.Batch, %record.Batch } undef, %record.Batch %tmp16, 0",
            "%tmp22 = insertvalue { %record.Batch, %record.Batch } %tmp17, %record.Batch %tmp21, 1",
            "store { %record.Batch, %record.Batch } %tmp22, ptr %tmp23, align 8",
            "%tmp24 = load [24 x i8], ptr %tmp23, align 8",
            "store { i32, [24 x i8] } %tmp25, ptr %result_storage",
            "load { i32, [24 x i8] }, ptr %worker.thread.result",
            "load { %record.Batch, %record.Batch }, ptr %tmp14, align 8",
            "extractvalue { %record.Batch, %record.Batch } %tmp15, 0",
            "extractvalue { %record.Batch, %record.Batch } %tmp15, 1",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 0",
            "getelementptr [2 x i32], ptr %tmp18, i64 0, i64 0",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 0",
            "getelementptr [2 x i32], ptr %tmp32, i64 0, i64 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericarrayfieldrecordchoicethread.Result_Batch_.placeholder",
        }
    );
    auto generic_array_field_record_choice_thread = run_program(app, generic_array_field_record_choice_thread_path);
    assert(generic_array_field_record_choice_thread.exit_code == 0);
    assert(generic_array_field_record_choice_thread.stdout_text.empty());
    assert(generic_array_field_record_choice_thread.stderr_text.empty());

    auto generic_record_array_field_record_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_record_array_field_record_choice_task_main.or";
    auto generic_record_array_field_record_choice_task_ir =
        run_emit_llvm(app, generic_record_array_field_record_choice_task_path);
    assert_success_with_stdout_contains(
        generic_record_array_field_record_choice_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%record.Batch = type { [2 x %record.Pair], i32 }",
            "%pending.task.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [40 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.30.0, ptr %pending.task.env, ptr %pending.task.result, i64 44, ptr @__orison_task_cleanup.main.30.0)",
            "%tmp27 = insertvalue %record.Batch undef, [2 x %record.Pair] %tmp26, 0",
            "%tmp29 = insertvalue { %record.Batch, %record.Batch } undef, %record.Batch %tmp28, 0",
            "%tmp38 = insertvalue { %record.Batch, %record.Batch } %tmp29, %record.Batch %tmp37, 1",
            "store { %record.Batch, %record.Batch } %tmp38, ptr %tmp39, align 8",
            "%tmp40 = load [40 x i8], ptr %tmp39, align 8",
            "store { i32, [40 x i8] } %tmp41, ptr %result_storage",
            "load { i32, [40 x i8] }, ptr %pending.task.result",
            "load { %record.Batch, %record.Batch }, ptr %tmp18, align 8",
            "extractvalue { %record.Batch, %record.Batch } %tmp19, 0",
            "extractvalue { %record.Batch, %record.Batch } %tmp19, 1",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 0",
            "getelementptr [2 x %record.Pair], ptr %tmp22, i64 0, i64 0",
            "getelementptr %record.Pair, ptr %tmp23, i32 0, i32 0",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 0",
            "getelementptr [2 x %record.Pair], ptr %tmp59, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp60, i32 0, i32 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericrecordarrayfieldrecordchoicetask.Result_Batch_.placeholder",
        }
    );
    auto generic_record_array_field_record_choice_task =
        run_program(app, generic_record_array_field_record_choice_task_path);
    assert(generic_record_array_field_record_choice_task.exit_code == 0);
    assert(generic_record_array_field_record_choice_task.stdout_text.empty());
    assert(generic_record_array_field_record_choice_task.stderr_text.empty());

    auto generic_record_array_field_record_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_record_array_field_record_choice_thread_main.or";
    auto generic_record_array_field_record_choice_thread_ir =
        run_emit_llvm(app, generic_record_array_field_record_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_record_array_field_record_choice_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%record.Batch = type { [2 x %record.Pair], i32 }",
            "%worker.thread.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [40 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.30.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 44, ptr @__orison_thread_cleanup.main.30.0)",
            "%tmp27 = insertvalue %record.Batch undef, [2 x %record.Pair] %tmp26, 0",
            "%tmp29 = insertvalue { %record.Batch, %record.Batch } undef, %record.Batch %tmp28, 0",
            "%tmp38 = insertvalue { %record.Batch, %record.Batch } %tmp29, %record.Batch %tmp37, 1",
            "store { %record.Batch, %record.Batch } %tmp38, ptr %tmp39, align 8",
            "%tmp40 = load [40 x i8], ptr %tmp39, align 8",
            "store { i32, [40 x i8] } %tmp41, ptr %result_storage",
            "load { i32, [40 x i8] }, ptr %worker.thread.result",
            "load { %record.Batch, %record.Batch }, ptr %tmp18, align 8",
            "extractvalue { %record.Batch, %record.Batch } %tmp19, 0",
            "extractvalue { %record.Batch, %record.Batch } %tmp19, 1",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 0",
            "getelementptr [2 x %record.Pair], ptr %tmp22, i64 0, i64 0",
            "getelementptr %record.Pair, ptr %tmp23, i32 0, i32 0",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 0",
            "getelementptr [2 x %record.Pair], ptr %tmp59, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp60, i32 0, i32 1",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericrecordarrayfieldrecordchoicethread.Result_Batch_.placeholder",
        }
    );
    auto generic_record_array_field_record_choice_thread =
        run_program(app, generic_record_array_field_record_choice_thread_path);
    assert(generic_record_array_field_record_choice_thread.exit_code == 0);
    assert(generic_record_array_field_record_choice_thread.stdout_text.empty());
    assert(generic_record_array_field_record_choice_thread.stderr_text.empty());

    auto generic_nested_record_array_field_record_choice_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_nested_record_array_field_record_choice_task_main.or";
    auto generic_nested_record_array_field_record_choice_task_ir =
        run_emit_llvm(app, generic_nested_record_array_field_record_choice_task_path);
    assert_success_with_stdout_contains(
        generic_nested_record_array_field_record_choice_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%record.Batch = type { [2 x [2 x %record.Pair]] }",
            "%pending.task.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%pending.task.result = alloca { i32, [64 x i8] }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.35.0, ptr %pending.task.env, ptr %pending.task.result, i64 68, ptr @__orison_task_cleanup.main.35.0)",
            "%tmp47 = insertvalue %record.Batch undef, [2 x [2 x %record.Pair]] %tmp46, 0",
            "%tmp48 = insertvalue { %record.Batch, %record.Batch } undef, %record.Batch %tmp47, 0",
            "%tmp64 = insertvalue { %record.Batch, %record.Batch } %tmp48, %record.Batch %tmp63, 1",
            "store { %record.Batch, %record.Batch } %tmp64, ptr %tmp65, align 8",
            "%tmp66 = load [64 x i8], ptr %tmp65, align 8",
            "store { i32, [64 x i8] } %tmp67, ptr %result_storage",
            "load { i32, [64 x i8] }, ptr %pending.task.result",
            "load { %record.Batch, %record.Batch }, ptr %tmp24, align 8",
            "extractvalue { %record.Batch, %record.Batch } %tmp25, 0",
            "extractvalue { %record.Batch, %record.Batch } %tmp25, 1",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 0",
            "getelementptr [2 x [2 x %record.Pair]], ptr %tmp28, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %tmp29, i64 0, i64 0",
            "getelementptr %record.Pair, ptr %tmp30, i32 0, i32 0",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 0",
            "getelementptr [2 x [2 x %record.Pair]], ptr %tmp117, i64 0, i64 1",
            "getelementptr [2 x %record.Pair], ptr %tmp118, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp119, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericnestedrecordarrayfieldrecordchoicetask.Result_Batch_.placeholder",
        }
    );
    auto generic_nested_record_array_field_record_choice_task =
        run_program(app, generic_nested_record_array_field_record_choice_task_path);
    assert(generic_nested_record_array_field_record_choice_task.exit_code == 0);
    assert(generic_nested_record_array_field_record_choice_task.stdout_text.empty());
    assert(generic_nested_record_array_field_record_choice_task.stderr_text.empty());

    auto generic_nested_record_array_field_record_choice_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_generic_nested_record_array_field_record_choice_thread_main.or";
    auto generic_nested_record_array_field_record_choice_thread_ir =
        run_emit_llvm(app, generic_nested_record_array_field_record_choice_thread_path);
    assert_success_with_stdout_contains(
        generic_nested_record_array_field_record_choice_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%record.Batch = type { [2 x [2 x %record.Pair]] }",
            "%worker.thread.env = alloca { i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }",
            "%worker.thread.result = alloca { i32, [64 x i8] }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.35.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 68, ptr @__orison_thread_cleanup.main.35.0)",
            "%tmp47 = insertvalue %record.Batch undef, [2 x [2 x %record.Pair]] %tmp46, 0",
            "%tmp48 = insertvalue { %record.Batch, %record.Batch } undef, %record.Batch %tmp47, 0",
            "%tmp64 = insertvalue { %record.Batch, %record.Batch } %tmp48, %record.Batch %tmp63, 1",
            "store { %record.Batch, %record.Batch } %tmp64, ptr %tmp65, align 8",
            "%tmp66 = load [64 x i8], ptr %tmp65, align 8",
            "store { i32, [64 x i8] } %tmp67, ptr %result_storage",
            "load { i32, [64 x i8] }, ptr %worker.thread.result",
            "load { %record.Batch, %record.Batch }, ptr %tmp24, align 8",
            "extractvalue { %record.Batch, %record.Batch } %tmp25, 0",
            "extractvalue { %record.Batch, %record.Batch } %tmp25, 1",
            "getelementptr %record.Batch, ptr %left.addr, i32 0, i32 0",
            "getelementptr [2 x [2 x %record.Pair]], ptr %tmp28, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %tmp29, i64 0, i64 0",
            "getelementptr %record.Pair, ptr %tmp30, i32 0, i32 0",
            "getelementptr %record.Batch, ptr %right.addr, i32 0, i32 0",
            "getelementptr [2 x [2 x %record.Pair]], ptr %tmp117, i64 0, i64 1",
            "getelementptr [2 x %record.Pair], ptr %tmp118, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp119, i32 0, i32 1",
            "@__orison_method.examples.concurrencygenericnestedrecordarrayfieldrecordchoicethread.Result_Batch_.placeholder",
        }
    );
    auto generic_nested_record_array_field_record_choice_thread =
        run_program(app, generic_nested_record_array_field_record_choice_thread_path);
    assert(generic_nested_record_array_field_record_choice_thread.exit_code == 0);
    assert(generic_nested_record_array_field_record_choice_thread.stdout_text.empty());
    assert(generic_nested_record_array_field_record_choice_thread.stderr_text.empty());

    auto array_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_array_task_main.or";
    auto array_task_ir = run_emit_llvm(app, array_task_path);
    assert_success_with_stdout_contains(
        array_task_ir,
        {
            "%pending.task.env = alloca { i32, i32 }",
            "%pending.task.result = alloca [2 x i32]",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.10.0, ptr %pending.task.env, ptr %pending.task.result, i64 8, ptr @__orison_task_cleanup.main.10.0)",
            "%tmp4 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp5 = insertvalue [2 x i32] %tmp4, i32 %tmp3, 1",
            "store [2 x i32] %tmp5, ptr %result_storage",
            "load [2 x i32], ptr %pending.task.result",
            "getelementptr [2 x i32], ptr %result.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %result.addr, i64 0, i64 1",
        }
    );
    auto array_task = run_program(app, array_task_path);
    assert(array_task.exit_code == 0);
    assert(array_task.stdout_text.empty());
    assert(array_task.stderr_text.empty());

    auto array_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_array_thread_main.or";
    auto array_thread_ir = run_emit_llvm(app, array_thread_path);
    assert_success_with_stdout_contains(
        array_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32 }",
            "%worker.thread.result = alloca [2 x i32]",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.10.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 8, ptr @__orison_thread_cleanup.main.10.0)",
            "%tmp4 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp5 = insertvalue [2 x i32] %tmp4, i32 %tmp3, 1",
            "store [2 x i32] %tmp5, ptr %result_storage",
            "load [2 x i32], ptr %worker.thread.result",
            "getelementptr [2 x i32], ptr %result.addr, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %result.addr, i64 0, i64 1",
        }
    );
    auto array_thread = run_program(app, array_thread_path);
    assert(array_thread.exit_code == 0);
    assert(array_thread.stdout_text.empty());
    assert(array_thread.stderr_text.empty());

    auto nested_array_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_array_task_main.or";
    auto nested_array_task_ir = run_emit_llvm(app, nested_array_task_path);
    assert_success_with_stdout_contains(
        nested_array_task_ir,
        {
            "%pending.task.env = alloca { i32, i32, i32, i32 }",
            "%pending.task.result = alloca [2 x [2 x i32]]",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.12.0, ptr %pending.task.env, ptr %pending.task.result, i64 16, ptr @__orison_task_cleanup.main.12.0)",
            "%tmp10 = insertvalue [2 x [2 x i32]] undef, [2 x i32] %tmp9, 0",
            "%tmp13 = insertvalue [2 x [2 x i32]] %tmp10, [2 x i32] %tmp12, 1",
            "store [2 x [2 x i32]] %tmp13, ptr %result_storage",
            "load [2 x [2 x i32]], ptr %pending.task.result",
            "getelementptr [2 x [2 x i32]], ptr %result.addr, i64 0, i64 0",
            "getelementptr [2 x [2 x i32]], ptr %result.addr, i64 0, i64 1",
            "getelementptr [2 x i32], ptr %tmp6, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %tmp17, i64 0, i64 1",
        }
    );
    auto nested_array_task = run_program(app, nested_array_task_path);
    assert(nested_array_task.exit_code == 0);
    assert(nested_array_task.stdout_text.empty());
    assert(nested_array_task.stderr_text.empty());

    auto nested_array_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_array_thread_main.or";
    auto nested_array_thread_ir = run_emit_llvm(app, nested_array_thread_path);
    assert_success_with_stdout_contains(
        nested_array_thread_ir,
        {
            "%worker.thread.env = alloca { i32, i32, i32, i32 }",
            "%worker.thread.result = alloca [2 x [2 x i32]]",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.12.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 16, ptr @__orison_thread_cleanup.main.12.0)",
            "%tmp10 = insertvalue [2 x [2 x i32]] undef, [2 x i32] %tmp9, 0",
            "%tmp13 = insertvalue [2 x [2 x i32]] %tmp10, [2 x i32] %tmp12, 1",
            "store [2 x [2 x i32]] %tmp13, ptr %result_storage",
            "load [2 x [2 x i32]], ptr %worker.thread.result",
            "getelementptr [2 x [2 x i32]], ptr %result.addr, i64 0, i64 0",
            "getelementptr [2 x [2 x i32]], ptr %result.addr, i64 0, i64 1",
            "getelementptr [2 x i32], ptr %tmp6, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %tmp17, i64 0, i64 1",
        }
    );
    auto nested_array_thread = run_program(app, nested_array_thread_path);
    assert(nested_array_thread.exit_code == 0);
    assert(nested_array_thread.stdout_text.empty());
    assert(nested_array_thread.stderr_text.empty());

    auto record_array_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_array_task_main.or";
    auto record_array_task_ir = run_emit_llvm(app, record_array_task_path);
    assert_success_with_stdout_contains(
        record_array_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%pending.task.env = alloca { i32, i32, i32, i32 }",
            "%pending.task.result = alloca [2 x %record.Pair]",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.16.0, ptr %pending.task.env, ptr %pending.task.result, i64 16, ptr @__orison_task_cleanup.main.16.0)",
            "%tmp8 = insertvalue %record.Pair undef, i32 %tmp1, 0",
            "%tmp10 = insertvalue [2 x %record.Pair] undef, %record.Pair %tmp9, 0",
            "%tmp13 = insertvalue [2 x %record.Pair] %tmp10, %record.Pair %tmp12, 1",
            "store [2 x %record.Pair] %tmp13, ptr %result_storage",
            "load [2 x %record.Pair], ptr %pending.task.result",
            "getelementptr [2 x %record.Pair], ptr %result.addr, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %result.addr, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp6, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %tmp17, i32 0, i32 1",
        }
    );
    auto record_array_task = run_program(app, record_array_task_path);
    assert(record_array_task.exit_code == 0);
    assert(record_array_task.stdout_text.empty());
    assert(record_array_task.stderr_text.empty());

    auto record_array_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_record_array_thread_main.or";
    auto record_array_thread_ir = run_emit_llvm(app, record_array_thread_path);
    assert_success_with_stdout_contains(
        record_array_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%worker.thread.env = alloca { i32, i32, i32, i32 }",
            "%worker.thread.result = alloca [2 x %record.Pair]",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.16.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 16, ptr @__orison_thread_cleanup.main.16.0)",
            "%tmp8 = insertvalue %record.Pair undef, i32 %tmp1, 0",
            "%tmp10 = insertvalue [2 x %record.Pair] undef, %record.Pair %tmp9, 0",
            "%tmp13 = insertvalue [2 x %record.Pair] %tmp10, %record.Pair %tmp12, 1",
            "store [2 x %record.Pair] %tmp13, ptr %result_storage",
            "load [2 x %record.Pair], ptr %worker.thread.result",
            "getelementptr [2 x %record.Pair], ptr %result.addr, i64 0, i64 0",
            "getelementptr [2 x %record.Pair], ptr %result.addr, i64 0, i64 1",
            "getelementptr %record.Pair, ptr %tmp6, i32 0, i32 0",
            "getelementptr %record.Pair, ptr %tmp17, i32 0, i32 1",
        }
    );
    auto record_array_thread = run_program(app, record_array_thread_path);
    assert(record_array_thread.exit_code == 0);
    assert(record_array_thread.stdout_text.empty());
    assert(record_array_thread.stderr_text.empty());

    auto array_field_record_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_array_field_record_task_main.or";
    auto array_field_record_task_ir = run_emit_llvm(app, array_field_record_task_path);
    assert_success_with_stdout_contains(
        array_field_record_task_ir,
        {
            "%record.Batch = type { [2 x i32], i32 }",
            "%pending.task.env = alloca { i32, i32, i32 }",
            "%pending.task.result = alloca %record.Batch",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.15.0, ptr %pending.task.env, ptr %pending.task.result, i64 12, ptr @__orison_task_cleanup.main.15.0)",
            "%tmp6 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp8 = insertvalue %record.Batch undef, [2 x i32] %tmp7, 0",
            "%tmp9 = insertvalue %record.Batch %tmp8, i32 %tmp5, 1",
            "store %record.Batch %tmp9, ptr %result_storage",
            "load %record.Batch, ptr %pending.task.result",
            "getelementptr %record.Batch, ptr %result.addr, i32 0, i32 0",
            "getelementptr [2 x i32], ptr %tmp5, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %tmp8, i64 0, i64 1",
            "getelementptr %record.Batch, ptr %result.addr, i32 0, i32 1",
        }
    );
    auto array_field_record_task = run_program(app, array_field_record_task_path);
    assert(array_field_record_task.exit_code == 0);
    assert(array_field_record_task.stdout_text.empty());
    assert(array_field_record_task.stderr_text.empty());

    auto array_field_record_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_array_field_record_thread_main.or";
    auto array_field_record_thread_ir = run_emit_llvm(app, array_field_record_thread_path);
    assert_success_with_stdout_contains(
        array_field_record_thread_ir,
        {
            "%record.Batch = type { [2 x i32], i32 }",
            "%worker.thread.env = alloca { i32, i32, i32 }",
            "%worker.thread.result = alloca %record.Batch",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.15.0, ptr %worker.thread.env, ptr %worker.thread.result, i64 12, ptr @__orison_thread_cleanup.main.15.0)",
            "%tmp6 = insertvalue [2 x i32] undef, i32 %tmp1, 0",
            "%tmp8 = insertvalue %record.Batch undef, [2 x i32] %tmp7, 0",
            "%tmp9 = insertvalue %record.Batch %tmp8, i32 %tmp5, 1",
            "store %record.Batch %tmp9, ptr %result_storage",
            "load %record.Batch, ptr %worker.thread.result",
            "getelementptr %record.Batch, ptr %result.addr, i32 0, i32 0",
            "getelementptr [2 x i32], ptr %tmp5, i64 0, i64 0",
            "getelementptr [2 x i32], ptr %tmp8, i64 0, i64 1",
            "getelementptr %record.Batch, ptr %result.addr, i32 0, i32 1",
        }
    );
    auto array_field_record_thread = run_program(app, array_field_record_thread_path);
    assert(array_field_record_thread.exit_code == 0);
    assert(array_field_record_thread.stdout_text.empty());
    assert(array_field_record_thread.stderr_text.empty());

    auto repeated_capture_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_repeated_capture_task_main.or";
    auto repeated_capture_task_ir = run_emit_llvm(app, repeated_capture_task_path);
    assert_success_with_stdout_contains(
        repeated_capture_task_ir,
        {
            "%pending.task.env = alloca { i32 }",
            "store i32 %value, ptr %tmp",
            "%tmp1 = load i32, ptr %tmp0",
            "add i32 %tmp1, %tmp1",
        }
    );
    assert(repeated_capture_task_ir.stdout_text.find("%pending.task.env = alloca { i32, i32 }") == std::string::npos);
    auto repeated_capture_task = run_program(app, repeated_capture_task_path);
    assert(repeated_capture_task.exit_code == 0);
    assert(repeated_capture_task.stdout_text.empty());
    assert(repeated_capture_task.stderr_text.empty());

    auto repeated_capture_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_repeated_capture_thread_main.or";
    auto repeated_capture_thread_ir = run_emit_llvm(app, repeated_capture_thread_path);
    assert_success_with_stdout_contains(
        repeated_capture_thread_ir,
        {
            "%worker.thread.env = alloca { i32 }",
            "store i32 %value, ptr %tmp",
            "%tmp1 = load i32, ptr %tmp0",
            "add i32 %tmp1, %tmp1",
        }
    );
    assert(repeated_capture_thread_ir.stdout_text.find("%worker.thread.env = alloca { i32, i32 }") == std::string::npos);
    auto repeated_capture_thread = run_program(app, repeated_capture_thread_path);
    assert(repeated_capture_thread.exit_code == 0);
    assert(repeated_capture_thread.stdout_text.empty());
    assert(repeated_capture_thread.stderr_text.empty());

    auto statement_body_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_statement_body_task_main.or";
    auto statement_body_task_ir = run_emit_llvm(app, statement_body_task_path);
    assert_success_with_stdout_contains(
        statement_body_task_ir,
        {
            "define private void @__orison_task_thunk.main.8.0(ptr %environment, ptr %result_storage)",
            "%doubled = add i32 0, %tmp2",
            "%result.addr = alloca i32",
            "store i32 %doubled, ptr %result.addr",
            "call void @observe(i32 %tmp5)",
            "store i32 %tmp6, ptr %result_storage",
        }
    );
    auto statement_body_task = run_program(app, statement_body_task_path);
    assert(statement_body_task.exit_code == 0);
    assert(statement_body_task.stdout_text.empty());
    assert(statement_body_task.stderr_text.empty());

    auto statement_body_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_statement_body_thread_main.or";
    auto statement_body_thread_ir = run_emit_llvm(app, statement_body_thread_path);
    assert_success_with_stdout_contains(
        statement_body_thread_ir,
        {
            "define private void @__orison_thread_thunk.main.8.0(ptr %environment, ptr %result_storage)",
            "%doubled = add i32 0, %tmp2",
            "%result.addr = alloca i32",
            "store i32 %doubled, ptr %result.addr",
            "call void @observe(i32 %tmp5)",
            "store i32 %tmp6, ptr %result_storage",
        }
    );
    auto statement_body_thread = run_program(app, statement_body_thread_path);
    assert(statement_body_thread.exit_code == 0);
    assert(statement_body_thread.stdout_text.empty());
    assert(statement_body_thread.stderr_text.empty());

    auto nested_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_task_main.or";
    auto nested_task_ir = run_emit_llvm(app, nested_task_path);
    assert_success_with_stdout_contains(
        nested_task_ir,
        {
            "%outer.task.env = alloca { i32 }",
            "define private void @__orison_task_thunk.main.5.0(ptr %environment, ptr %result_storage)",
            "define private void @__orison_task_thunk.__orison_task_thunk_main_5_0.6.0(ptr %environment, ptr %result_storage)",
            "%inner.task.env = alloca { i32 }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.__orison_task_thunk_main_5_0.6.0",
            "call void @__orison_task_await(ptr %inner)",
            "store i32 %tmp4, ptr %result_storage",
        }
    );
    auto nested_task = run_program(app, nested_task_path);
    assert(nested_task.exit_code == 0);
    assert(nested_task.stdout_text.empty());
    assert(nested_task.stderr_text.empty());

    auto nested_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_thread_main.or";
    auto nested_thread_ir = run_emit_llvm(app, nested_thread_path);
    assert_success_with_stdout_contains(
        nested_thread_ir,
        {
            "%outer.thread.env = alloca { i32 }",
            "define private void @__orison_thread_thunk.main.5.0(ptr %environment, ptr %result_storage)",
            "define private void @__orison_thread_thunk.__orison_thread_thunk_main_5_0.6.0(ptr %environment, ptr %result_storage)",
            "%inner.thread.env = alloca { i32 }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.__orison_thread_thunk_main_5_0.6.0",
            "call void @__orison_thread_join(ptr %inner)",
            "store i32 %tmp4, ptr %result_storage",
        }
    );
    auto nested_thread = run_program(app, nested_thread_path);
    assert(nested_thread.exit_code == 0);
    assert(nested_thread.stdout_text.empty());
    assert(nested_thread.stderr_text.empty());

    auto nested_local_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_local_capture_task_main.or";
    auto nested_local_task_ir = run_emit_llvm(app, nested_local_task_path);
    assert_success_with_stdout_contains(
        nested_local_task_ir,
        {
            "%outer.task.env = alloca { i32 }",
            "%local = add i32 0, %tmp2",
            "%inner.task.env = alloca { i32 }",
            "store i32 %local, ptr %tmp3",
            "define private void @__orison_task_thunk.__orison_task_thunk_main_5_0.7.0(ptr %environment, ptr %result_storage)",
            "call void @__orison_task_await(ptr %inner)",
        }
    );
    assert(nested_local_task_ir.stdout_text.find("%outer.task.env = alloca { i32, i32 }") == std::string::npos);
    auto nested_local_task = run_program(app, nested_local_task_path);
    assert(nested_local_task.exit_code == 0);
    assert(nested_local_task.stdout_text.empty());
    assert(nested_local_task.stderr_text.empty());

    auto nested_local_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_local_capture_thread_main.or";
    auto nested_local_thread_ir = run_emit_llvm(app, nested_local_thread_path);
    assert_success_with_stdout_contains(
        nested_local_thread_ir,
        {
            "%outer.thread.env = alloca { i32 }",
            "%local = add i32 0, %tmp2",
            "%inner.thread.env = alloca { i32 }",
            "store i32 %local, ptr %tmp3",
            "define private void @__orison_thread_thunk.__orison_thread_thunk_main_5_0.7.0(ptr %environment, ptr %result_storage)",
            "call void @__orison_thread_join(ptr %inner)",
        }
    );
    assert(nested_local_thread_ir.stdout_text.find("%outer.thread.env = alloca { i32, i32 }") == std::string::npos);
    auto nested_local_thread = run_program(app, nested_local_thread_path);
    assert(nested_local_thread.exit_code == 0);
    assert(nested_local_thread.stdout_text.empty());
    assert(nested_local_thread.stderr_text.empty());

    auto mixed_task_in_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_mixed_task_in_thread_main.or";
    auto mixed_task_in_thread_ir = run_emit_llvm(app, mixed_task_in_thread_path);
    assert_success_with_stdout_contains(
        mixed_task_in_thread_ir,
        {
            "declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)",
            "declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)",
            "define private void @__orison_task_thunk.__orison_thread_thunk_main_5_0.6.0(ptr %environment, ptr %result_storage)",
            "define private void @__orison_thread_thunk.main.5.0(ptr %environment, ptr %result_storage)",
            "%inner.task.env = alloca { i32 }",
            "call void @__orison_task_await(ptr %inner)",
            "call void @__orison_thread_join(ptr %outer)",
        }
    );
    auto mixed_task_in_thread = run_program(app, mixed_task_in_thread_path);
    assert(mixed_task_in_thread.exit_code == 0);
    assert(mixed_task_in_thread.stdout_text.empty());
    assert(mixed_task_in_thread.stderr_text.empty());

    auto mixed_thread_in_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_mixed_thread_in_task_main.or";
    auto mixed_thread_in_task_ir = run_emit_llvm(app, mixed_thread_in_task_path);
    assert_success_with_stdout_contains(
        mixed_thread_in_task_ir,
        {
            "declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)",
            "declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)",
            "define private void @__orison_thread_thunk.__orison_task_thunk_main_5_0.6.0(ptr %environment, ptr %result_storage)",
            "define private void @__orison_task_thunk.main.5.0(ptr %environment, ptr %result_storage)",
            "%inner.thread.env = alloca { i32 }",
            "call void @__orison_thread_join(ptr %inner)",
            "call void @__orison_task_await(ptr %outer)",
        }
    );
    auto mixed_thread_in_task = run_program(app, mixed_thread_in_task_path);
    assert(mixed_thread_in_task.exit_code == 0);
    assert(mixed_thread_in_task.stdout_text.empty());
    assert(mixed_thread_in_task.stderr_text.empty());

    auto nested_array_result_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_array_result_task_main.or";
    auto nested_array_result_task_ir = run_emit_llvm(app, nested_array_result_task_path);
    assert_success_with_stdout_contains(
        nested_array_result_task_ir,
        {
            "%outer.task.result = alloca [2 x i32]",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.10.0, ptr %outer.task.env, ptr %outer.task.result, i64 8",
            "%inner.task.result = alloca [2 x i32]",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.__orison_task_thunk_main_10_0.11.0, ptr %inner.task.env, ptr %inner.task.result, i64 8",
            "%tmp7 = load [2 x i32], ptr %inner.task.result",
            "store [2 x i32] %tmp7, ptr %result_storage",
            "%tmp3 = load [2 x i32], ptr %outer.task.result",
        }
    );
    auto nested_array_result_task = run_program(app, nested_array_result_task_path);
    assert(nested_array_result_task.exit_code == 0);
    assert(nested_array_result_task.stdout_text.empty());
    assert(nested_array_result_task.stderr_text.empty());

    auto nested_array_result_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_array_result_thread_main.or";
    auto nested_array_result_thread_ir = run_emit_llvm(app, nested_array_result_thread_path);
    assert_success_with_stdout_contains(
        nested_array_result_thread_ir,
        {
            "%outer.thread.result = alloca [2 x i32]",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.10.0, ptr %outer.thread.env, ptr %outer.thread.result, i64 8",
            "%inner.thread.result = alloca [2 x i32]",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.__orison_thread_thunk_main_10_0.11.0, ptr %inner.thread.env, ptr %inner.thread.result, i64 8",
            "%tmp7 = load [2 x i32], ptr %inner.thread.result",
            "store [2 x i32] %tmp7, ptr %result_storage",
            "%tmp3 = load [2 x i32], ptr %outer.thread.result",
        }
    );
    auto nested_array_result_thread = run_program(app, nested_array_result_thread_path);
    assert(nested_array_result_thread.exit_code == 0);
    assert(nested_array_result_thread.stdout_text.empty());
    assert(nested_array_result_thread.stderr_text.empty());

    auto nested_record_result_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_record_result_task_main.or";
    auto nested_record_result_task_ir = run_emit_llvm(app, nested_record_result_task_path);
    assert_success_with_stdout_contains(
        nested_record_result_task_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%outer.task.result = alloca %record.Pair",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.14.0, ptr %outer.task.env, ptr %outer.task.result, i64 8",
            "%inner.task.result = alloca %record.Pair",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.__orison_task_thunk_main_14_0.15.0, ptr %inner.task.env, ptr %inner.task.result, i64 8",
            "%tmp7 = load %record.Pair, ptr %inner.task.result",
            "store %record.Pair %tmp7, ptr %result_storage",
            "%tmp3 = load %record.Pair, ptr %outer.task.result",
        }
    );
    auto nested_record_result_task = run_program(app, nested_record_result_task_path);
    assert(nested_record_result_task.exit_code == 0);
    assert(nested_record_result_task.stdout_text.empty());
    assert(nested_record_result_task.stderr_text.empty());

    auto nested_record_result_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_record_result_thread_main.or";
    auto nested_record_result_thread_ir = run_emit_llvm(app, nested_record_result_thread_path);
    assert_success_with_stdout_contains(
        nested_record_result_thread_ir,
        {
            "%record.Pair = type { i32, i32 }",
            "%outer.thread.result = alloca %record.Pair",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.14.0, ptr %outer.thread.env, ptr %outer.thread.result, i64 8",
            "%inner.thread.result = alloca %record.Pair",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.__orison_thread_thunk_main_14_0.15.0, ptr %inner.thread.env, ptr %inner.thread.result, i64 8",
            "%tmp7 = load %record.Pair, ptr %inner.thread.result",
            "store %record.Pair %tmp7, ptr %result_storage",
            "%tmp3 = load %record.Pair, ptr %outer.thread.result",
        }
    );
    auto nested_record_result_thread = run_program(app, nested_record_result_thread_path);
    assert(nested_record_result_thread.exit_code == 0);
    assert(nested_record_result_thread.stdout_text.empty());
    assert(nested_record_result_thread.stderr_text.empty());

    auto nested_choice_result_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_choice_result_task_main.or";
    auto nested_choice_result_task_ir = run_emit_llvm(app, nested_choice_result_task_path);
    assert_success_with_stdout_contains(
        nested_choice_result_task_ir,
        {
            "%outer.task.result = alloca { i32, i32 }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.main.13.0, ptr %outer.task.env, ptr %outer.task.result, i64 8",
            "%inner.task.result = alloca { i32, i32 }",
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.__orison_task_thunk_main_13_0.14.0, ptr %inner.task.env, ptr %inner.task.result, i64 8",
            "%tmp4 = load { i32, i32 }, ptr %inner.task.result",
            "store { i32, i32 } %tmp4, ptr %result_storage",
            "%tmp2 = load { i32, i32 }, ptr %outer.task.result",
            "extractvalue { i32, i32 } %tmp2, 0",
            "extractvalue { i32, i32 } %tmp2, 1",
        }
    );
    auto nested_choice_result_task = run_program(app, nested_choice_result_task_path);
    assert(nested_choice_result_task.exit_code == 0);
    assert(nested_choice_result_task.stdout_text.empty());
    assert(nested_choice_result_task.stderr_text.empty());

    auto nested_choice_result_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_choice_result_thread_main.or";
    auto nested_choice_result_thread_ir = run_emit_llvm(app, nested_choice_result_thread_path);
    assert_success_with_stdout_contains(
        nested_choice_result_thread_ir,
        {
            "%outer.thread.result = alloca { i32, i32 }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.main.13.0, ptr %outer.thread.env, ptr %outer.thread.result, i64 8",
            "%inner.thread.result = alloca { i32, i32 }",
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.__orison_thread_thunk_main_13_0.14.0, ptr %inner.thread.env, ptr %inner.thread.result, i64 8",
            "%tmp4 = load { i32, i32 }, ptr %inner.thread.result",
            "store { i32, i32 } %tmp4, ptr %result_storage",
            "%tmp2 = load { i32, i32 }, ptr %outer.thread.result",
            "extractvalue { i32, i32 } %tmp2, 0",
            "extractvalue { i32, i32 } %tmp2, 1",
        }
    );
    auto nested_choice_result_thread = run_program(app, nested_choice_result_thread_path);
    assert(nested_choice_result_thread.exit_code == 0);
    assert(nested_choice_result_thread.stdout_text.empty());
    assert(nested_choice_result_thread.stderr_text.empty());

    auto nested_distinct_choice_result_task_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_distinct_choice_result_task_main.or";
    auto nested_distinct_choice_result_task_ir = run_emit_llvm(app, nested_distinct_choice_result_task_path);
    assert_success_with_stdout_contains(
        nested_distinct_choice_result_task_ir,
        {
            "%outer.task.result = alloca { i32, [4 x i8] }",
            "ptr %outer.task.result, i64 8",
            "%inner.task.result = alloca { i32, [4 x i8] }",
            "ptr %inner.task.result, i64 8",
            "load { i32, [4 x i8] }, ptr %inner.task.result",
            "store { i32, [4 x i8] }",
            "load { i32, [4 x i8] }, ptr %outer.task.result",
            "load i1, ptr",
            "load i32, ptr",
        }
    );
    auto nested_distinct_choice_result_task = run_program(app, nested_distinct_choice_result_task_path);
    assert(nested_distinct_choice_result_task.exit_code == 0);
    assert(nested_distinct_choice_result_task.stdout_text.empty());
    assert(nested_distinct_choice_result_task.stderr_text.empty());

    auto nested_distinct_choice_result_thread_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_nested_distinct_choice_result_thread_main.or";
    auto nested_distinct_choice_result_thread_ir = run_emit_llvm(app, nested_distinct_choice_result_thread_path);
    assert_success_with_stdout_contains(
        nested_distinct_choice_result_thread_ir,
        {
            "%outer.thread.result = alloca { i32, [4 x i8] }",
            "ptr %outer.thread.result, i64 8",
            "%inner.thread.result = alloca { i32, [4 x i8] }",
            "ptr %inner.thread.result, i64 8",
            "load { i32, [4 x i8] }, ptr %inner.thread.result",
            "store { i32, [4 x i8] }",
            "load { i32, [4 x i8] }, ptr %outer.thread.result",
            "load i1, ptr",
            "load i32, ptr",
        }
    );
    auto nested_distinct_choice_result_thread = run_program(app, nested_distinct_choice_result_thread_path);
    assert(nested_distinct_choice_result_thread.exit_code == 0);
    assert(nested_distinct_choice_result_thread.stdout_text.empty());
    assert(nested_distinct_choice_result_thread.stderr_text.empty());

    auto composed_await_ternary_left_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_composed_await_ternary_left_main.or";
    auto composed_await_ternary_left_ir = run_emit_llvm(app, composed_await_ternary_left_path);
    assert_success_with_stdout_contains(
        composed_await_ternary_left_ir,
        {
            "br i1 1, label %ternary.then.",
            "call void @__orison_task_await(ptr %left_pending)",
            "call void @__orison_task_await(ptr %right_pending)",
            " = phi ptr [null, %ternary.then.",
            " = phi ptr [%right_pending, %ternary.then.",
            "call void @__orison_concurrency_handle_destroy(ptr %tmp8)\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %tmp7)",
        }
    );
    auto composed_await_ternary_left = run_program(app, composed_await_ternary_left_path);
    assert(composed_await_ternary_left.exit_code == 0);
    assert(composed_await_ternary_left.stdout_text.empty());
    assert(composed_await_ternary_left.stderr_text.empty());

    auto composed_await_ternary_right_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_composed_await_ternary_right_main.or";
    auto composed_await_ternary_right_ir = run_emit_llvm(app, composed_await_ternary_right_path);
    assert_success_with_stdout_contains(
        composed_await_ternary_right_ir,
        {
            "br i1 0, label %ternary.then.",
            "call void @__orison_task_await(ptr %left_pending)",
            "call void @__orison_task_await(ptr %right_pending)",
            " = phi ptr [null, %ternary.then.",
            " = phi ptr [%right_pending, %ternary.then.",
            "call void @__orison_concurrency_handle_destroy(ptr %tmp8)\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %tmp7)",
        }
    );
    auto composed_await_ternary_right = run_program(app, composed_await_ternary_right_path);
    assert(composed_await_ternary_right.exit_code == 0);
    assert(composed_await_ternary_right.stdout_text.empty());
    assert(composed_await_ternary_right.stderr_text.empty());

    auto await_if_true_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_if_true_main.or";
    auto await_if_true_ir = run_emit_llvm(app, await_if_true_path);
    assert_success_with_stdout_contains(
        await_if_true_ir,
        {
            "store i1 1, ptr %result_storage",
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp1 = load i1, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  br i1 %tmp1, label %if.then.1, label %if.else.1",
            "%tmp2 = phi i32 [0, %if.then.1], [1, %if.else.1]",
        }
    );
    auto await_if_true = run_program(app, await_if_true_path);
    assert(await_if_true.exit_code == 0);
    assert(await_if_true.stdout_text.empty());
    assert(await_if_true.stderr_text.empty());

    auto await_if_false_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_if_false_main.or";
    auto await_if_false_ir = run_emit_llvm(app, await_if_false_path);
    assert_success_with_stdout_contains(
        await_if_false_ir,
        {
            "store i1 0, ptr %result_storage",
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp1 = load i1, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  br i1 %tmp1, label %if.then.1, label %if.else.1",
            "%tmp2 = phi i32 [1, %if.then.1], [0, %if.else.1]",
        }
    );
    auto await_if_false = run_program(app, await_if_false_path);
    assert(await_if_false.exit_code == 0);
    assert(await_if_false.stdout_text.empty());
    assert(await_if_false.stderr_text.empty());

    auto await_guard_continue_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_guard_continue_main.or";
    auto await_guard_continue_ir = run_emit_llvm(app, await_guard_continue_path);
    assert_success_with_stdout_contains(
        await_guard_continue_ir,
        {
            "store i1 1, ptr %result_storage",
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp1 = load i1, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  br i1 %tmp1, label %guard.merge.1, label %guard.failure.1",
            "guard.failure.1:\n  ret i32 1",
            "guard.merge.1:\n  ret i32 0",
        }
    );
    auto await_guard_continue = run_program(app, await_guard_continue_path);
    assert(await_guard_continue.exit_code == 0);
    assert(await_guard_continue.stdout_text.empty());
    assert(await_guard_continue.stderr_text.empty());

    auto await_guard_return_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_guard_return_main.or";
    auto await_guard_return_ir = run_emit_llvm(app, await_guard_return_path);
    assert_success_with_stdout_contains(
        await_guard_return_ir,
        {
            "store i1 0, ptr %result_storage",
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp1 = load i1, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  br i1 %tmp1, label %guard.merge.1, label %guard.failure.1",
            "guard.failure.1:\n  ret i32 0",
            "guard.merge.1:\n  ret i32 1",
        }
    );
    auto await_guard_return = run_program(app, await_guard_return_path);
    assert(await_guard_return.exit_code == 0);
    assert(await_guard_return.stdout_text.empty());
    assert(await_guard_return.stderr_text.empty());

    auto await_switch_match_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_switch_match_main.or";
    auto await_switch_match_ir = run_emit_llvm(app, await_switch_match_path);
    assert_success_with_stdout_contains(
        await_switch_match_ir,
        {
            "store i32 7, ptr %result_storage",
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp1 = load i32, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  switch i32 %tmp1, label %switch.default.1 [",
            "i32 7, label %switch.case.1.0",
            "%tmp2 = phi i32 [0, %switch.case.1.0], [1, %switch.default.1]",
        }
    );
    auto await_switch_match = run_program(app, await_switch_match_path);
    assert(await_switch_match.exit_code == 0);
    assert(await_switch_match.stdout_text.empty());
    assert(await_switch_match.stderr_text.empty());

    auto await_switch_default_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_switch_default_main.or";
    auto await_switch_default_ir = run_emit_llvm(app, await_switch_default_path);
    assert_success_with_stdout_contains(
        await_switch_default_ir,
        {
            "store i32 8, ptr %result_storage",
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp1 = load i32, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  switch i32 %tmp1, label %switch.default.1 [",
            "i32 7, label %switch.case.1.0",
            "%tmp2 = phi i32 [1, %switch.case.1.0], [0, %switch.default.1]",
        }
    );
    auto await_switch_default = run_program(app, await_switch_default_path);
    assert(await_switch_default.exit_code == 0);
    assert(await_switch_default.stdout_text.empty());
    assert(await_switch_default.stderr_text.empty());

    auto await_fixed_array_index_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_fixed_array_index_main.or";
    auto await_fixed_array_index_ir = run_emit_llvm(app, await_fixed_array_index_path);
    assert_success_with_stdout_contains(
        await_fixed_array_index_ir,
        {
            "declare void @__orison_dynamic_array_bounds_failed()",
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %aggregate_path_index4.in_bounds = icmp ult i64 %tmp3, 2",
            "br i1 %aggregate_path_index4.in_bounds, label %fixed_array.index.in_bounds.1, "
            "label %fixed_array.index.out_of_bounds.1",
            "call void @__orison_dynamic_array_bounds_failed()\n  unreachable",
            "%tmp5 = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 %tmp3",
            "%tmp6 = load i32, ptr %tmp5",
        }
    );
    auto await_fixed_array_index = run_program(app, await_fixed_array_index_path);
    assert(await_fixed_array_index.exit_code == 0);
    assert(await_fixed_array_index.stdout_text.empty());
    assert(await_fixed_array_index.stderr_text.empty());

    auto await_fixed_array_index_out_of_bounds_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_fixed_array_index_out_of_bounds_main.or";
    auto await_fixed_array_index_out_of_bounds_ir =
        run_emit_llvm(app, await_fixed_array_index_out_of_bounds_path);
    assert_success_with_stdout_contains(
        await_fixed_array_index_out_of_bounds_ir,
        {
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %aggregate_path_index4.in_bounds = icmp ult i64 %tmp3, 2",
            "br i1 %aggregate_path_index4.in_bounds, label %fixed_array.index.in_bounds.1, "
            "label %fixed_array.index.out_of_bounds.1",
            "fixed_array.index.out_of_bounds.1:\n"
            "  call void @__orison_dynamic_array_bounds_failed()\n"
            "  unreachable",
        }
    );
    auto await_fixed_array_index_out_of_bounds =
        run_program(app, await_fixed_array_index_out_of_bounds_path);
    assert(await_fixed_array_index_out_of_bounds.exit_code == 134);
    assert(await_fixed_array_index_out_of_bounds.stdout_text.empty());
    assert(await_fixed_array_index_out_of_bounds.stderr_text.empty());

    auto await_fixed_array_assignment_index_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_fixed_array_assignment_index_main.or";
    auto await_fixed_array_assignment_index_ir =
        run_emit_llvm(app, await_fixed_array_assignment_index_path);
    assert_success_with_stdout_contains(
        await_fixed_array_assignment_index_ir,
        {
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %aggregate_path_index4.in_bounds = icmp ult i64 %tmp3, 2",
            "br i1 %aggregate_path_index4.in_bounds, label %fixed_array.index.in_bounds.1, "
            "label %fixed_array.index.out_of_bounds.1",
            "%tmp5 = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 %tmp3\n"
            "  store i32 47, ptr %tmp5",
            "%tmp6 = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 1\n"
            "  %tmp7 = load i32, ptr %tmp6",
        }
    );
    auto await_fixed_array_assignment_index =
        run_program(app, await_fixed_array_assignment_index_path);
    assert(await_fixed_array_assignment_index.exit_code == 0);
    assert(await_fixed_array_assignment_index.stdout_text.empty());
    assert(await_fixed_array_assignment_index.stderr_text.empty());

    auto await_fixed_array_assignment_index_out_of_bounds_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_fixed_array_assignment_index_out_of_bounds_main.or";
    auto await_fixed_array_assignment_index_out_of_bounds_ir =
        run_emit_llvm(app, await_fixed_array_assignment_index_out_of_bounds_path);
    assert_success_with_stdout_contains(
        await_fixed_array_assignment_index_out_of_bounds_ir,
        {
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %aggregate_path_index4.in_bounds = icmp ult i64 %tmp3, 2",
            "br i1 %aggregate_path_index4.in_bounds, label %fixed_array.index.in_bounds.1, "
            "label %fixed_array.index.out_of_bounds.1",
            "fixed_array.index.out_of_bounds.1:\n"
            "  call void @__orison_dynamic_array_bounds_failed()\n"
            "  unreachable\n"
            "fixed_array.index.in_bounds.1:\n"
            "  %tmp5 = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 %tmp3\n"
            "  store i32 47, ptr %tmp5",
        }
    );
    auto await_fixed_array_assignment_index_out_of_bounds =
        run_program(app, await_fixed_array_assignment_index_out_of_bounds_path);
    assert(await_fixed_array_assignment_index_out_of_bounds.exit_code == 134);
    assert(await_fixed_array_assignment_index_out_of_bounds.stdout_text.empty());
    assert(await_fixed_array_assignment_index_out_of_bounds.stderr_text.empty());

    auto await_dynamic_array_index_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_dynamic_array_index_main.or";
    auto await_dynamic_array_index_ir = run_emit_llvm(app, await_dynamic_array_index_path);
    assert_success_with_stdout_contains(
        await_dynamic_array_index_ir,
        {
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %values.dynamic_array_index4.descriptor = load { ptr, i64, i64 }, ptr %values.addr",
            "%values.dynamic_array_index4.length = extractvalue { ptr, i64, i64 } "
            "%values.dynamic_array_index4.descriptor, 1\n"
            "  %values.dynamic_array_index4.in_bounds = icmp ult i64 %tmp3, "
            "%values.dynamic_array_index4.length",
            "br i1 %values.dynamic_array_index4.in_bounds, label %dynamic_array.index.in_bounds.3, "
            "label %dynamic_array.index.out_of_bounds.3",
            "%values.dynamic_array_index4.data = extractvalue { ptr, i64, i64 } "
            "%values.dynamic_array_index4.descriptor, 0\n"
            "  %values.dynamic_array_index4.element.addr = getelementptr i32, "
            "ptr %values.dynamic_array_index4.data, i64 %tmp3\n"
            "  %values.dynamic_array_index4.value = load i32, "
            "ptr %values.dynamic_array_index4.element.addr",
            "call void @__orison_dynamic_array_deallocate("
            "ptr %values.dynamic_array_cleanup6.cleanup.data, i64 4, "
            "i64 %values.dynamic_array_cleanup6.cleanup.capacity)\n"
            "  store { ptr, i64, i64 } zeroinitializer, ptr %values.addr\n"
            "  ret i32 %tmp5",
        }
    );
    auto await_dynamic_array_index = run_program(app, await_dynamic_array_index_path);
    assert(await_dynamic_array_index.exit_code == 0);
    assert(await_dynamic_array_index.stdout_text.empty());
    assert(await_dynamic_array_index.stderr_text.empty());

    auto await_dynamic_array_index_out_of_bounds_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_dynamic_array_index_out_of_bounds_main.or";
    auto await_dynamic_array_index_out_of_bounds_ir =
        run_emit_llvm(app, await_dynamic_array_index_out_of_bounds_path);
    assert_success_with_stdout_contains(
        await_dynamic_array_index_out_of_bounds_ir,
        {
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %values.dynamic_array_index4.descriptor = load { ptr, i64, i64 }, ptr %values.addr",
            "%values.dynamic_array_index4.length = extractvalue { ptr, i64, i64 } "
            "%values.dynamic_array_index4.descriptor, 1\n"
            "  %values.dynamic_array_index4.in_bounds = icmp ult i64 %tmp3, "
            "%values.dynamic_array_index4.length",
            "br i1 %values.dynamic_array_index4.in_bounds, label %dynamic_array.index.in_bounds.3, "
            "label %dynamic_array.index.out_of_bounds.3",
            "dynamic_array.index.out_of_bounds.3:\n"
            "  call void @__orison_dynamic_array_bounds_failed()\n"
            "  unreachable\n"
            "dynamic_array.index.in_bounds.3:\n"
            "  %values.dynamic_array_index4.data = extractvalue { ptr, i64, i64 } "
            "%values.dynamic_array_index4.descriptor, 0\n"
            "  %values.dynamic_array_index4.element.addr = getelementptr i32, "
            "ptr %values.dynamic_array_index4.data, i64 %tmp3\n"
            "  %values.dynamic_array_index4.value = load i32, "
            "ptr %values.dynamic_array_index4.element.addr",
        }
    );
    auto await_dynamic_array_index_out_of_bounds =
        run_program(app, await_dynamic_array_index_out_of_bounds_path);
    assert(await_dynamic_array_index_out_of_bounds.exit_code == 134);
    assert(await_dynamic_array_index_out_of_bounds.stdout_text.empty());
    assert(await_dynamic_array_index_out_of_bounds.stderr_text.empty());

    auto await_dynamic_array_assignment_index_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_dynamic_array_assignment_index_main.or";
    auto await_dynamic_array_assignment_index_ir =
        run_emit_llvm(app, await_dynamic_array_assignment_index_path);
    assert_success_with_stdout_contains(
        await_dynamic_array_assignment_index_ir,
        {
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %values.dynamic_array_assign4.descriptor = load { ptr, i64, i64 }, ptr %values.addr",
            "%values.dynamic_array_assign4.length = extractvalue { ptr, i64, i64 } "
            "%values.dynamic_array_assign4.descriptor, 1\n"
            "  %values.dynamic_array_assign4.in_bounds = icmp ult i64 %tmp3, "
            "%values.dynamic_array_assign4.length",
            "br i1 %values.dynamic_array_assign4.in_bounds, label %dynamic_array.assign.in_bounds.3, "
            "label %dynamic_array.assign.out_of_bounds.3",
            "%values.dynamic_array_assign4.element.addr = getelementptr i32, "
            "ptr %values.dynamic_array_assign4.data, i64 %tmp3\n"
            "  store i32 47, ptr %values.dynamic_array_assign4.element.addr",
            "%values.dynamic_array_index5.element.addr = getelementptr i32, "
            "ptr %values.dynamic_array_index5.data, i64 1\n"
            "  %values.dynamic_array_index5.value = load i32, "
            "ptr %values.dynamic_array_index5.element.addr",
            "call void @__orison_dynamic_array_deallocate("
            "ptr %values.dynamic_array_cleanup7.cleanup.data, i64 4, "
            "i64 %values.dynamic_array_cleanup7.cleanup.capacity)\n"
            "  store { ptr, i64, i64 } zeroinitializer, ptr %values.addr\n"
            "  ret i32 %tmp6",
        }
    );
    auto await_dynamic_array_assignment_index =
        run_program(app, await_dynamic_array_assignment_index_path);
    assert(await_dynamic_array_assignment_index.exit_code == 0);
    assert(await_dynamic_array_assignment_index.stdout_text.empty());
    assert(await_dynamic_array_assignment_index.stderr_text.empty());

    auto await_dynamic_array_assignment_index_out_of_bounds_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_dynamic_array_assignment_index_out_of_bounds_main.or";
    auto await_dynamic_array_assignment_index_out_of_bounds_ir =
        run_emit_llvm(app, await_dynamic_array_assignment_index_out_of_bounds_path);
    assert_success_with_stdout_contains(
        await_dynamic_array_assignment_index_out_of_bounds_ir,
        {
            "call void @__orison_task_await(ptr %pending)\n"
            "  %tmp3 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %values.dynamic_array_assign4.descriptor = load { ptr, i64, i64 }, ptr %values.addr",
            "%values.dynamic_array_assign4.length = extractvalue { ptr, i64, i64 } "
            "%values.dynamic_array_assign4.descriptor, 1\n"
            "  %values.dynamic_array_assign4.in_bounds = icmp ult i64 %tmp3, "
            "%values.dynamic_array_assign4.length",
            "br i1 %values.dynamic_array_assign4.in_bounds, label %dynamic_array.assign.in_bounds.3, "
            "label %dynamic_array.assign.out_of_bounds.3",
            "dynamic_array.assign.out_of_bounds.3:\n"
            "  call void @__orison_dynamic_array_bounds_failed()\n"
            "  unreachable\n"
            "dynamic_array.assign.in_bounds.3:\n"
            "  %values.dynamic_array_assign4.element.addr = getelementptr i32, "
            "ptr %values.dynamic_array_assign4.data, i64 %tmp3\n"
            "  store i32 47, ptr %values.dynamic_array_assign4.element.addr",
        }
    );
    auto await_dynamic_array_assignment_index_out_of_bounds =
        run_program(app, await_dynamic_array_assignment_index_out_of_bounds_path);
    assert(await_dynamic_array_assignment_index_out_of_bounds.exit_code == 134);
    assert(await_dynamic_array_assignment_index_out_of_bounds.stdout_text.empty());
    assert(await_dynamic_array_assignment_index_out_of_bounds.stderr_text.empty());

    auto await_dynamic_array_record_field_index_path = std::filesystem::path(ORISON_SOURCE_DIR) /
        "examples" / "concurrency_await_dynamic_array_record_field_index_main.or";
    auto await_dynamic_array_record_field_index_ir =
        run_emit_llvm(app, await_dynamic_array_record_field_index_path);
    assert_success_with_stdout_contains(
        await_dynamic_array_record_field_index_ir,
        {
            "%tmp6 = getelementptr %record.Holder, ptr %holder.addr, i32 0, i32 0\n"
            "  call void @__orison_task_await(ptr %pending)\n"
            "  %tmp7 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %holder.values.dynamic_array_element_path8.descriptor = load { ptr, i64, i64 }, ptr %tmp6",
            "%holder.values.dynamic_array_element_path8.length = extractvalue { ptr, i64, i64 } "
            "%holder.values.dynamic_array_element_path8.descriptor, 1\n"
            "  %holder.values.dynamic_array_element_path8.in_bounds = icmp ult i64 %tmp7, "
            "%holder.values.dynamic_array_element_path8.length",
            "br i1 %holder.values.dynamic_array_element_path8.in_bounds, "
            "label %dynamic_array.element_path.in_bounds.3, "
            "label %dynamic_array.element_path.out_of_bounds.3",
            "%holder.values.dynamic_array_element_path8.element.addr = getelementptr i32, "
            "ptr %holder.values.dynamic_array_element_path8.data, i64 %tmp7\n"
            "  %tmp9 = load i32, ptr %holder.values.dynamic_array_element_path8.element.addr",
            "call void @__orison_owned_cleanup.Holder.examples.concurrency_await_dynamic_array_record_field_index"
            "(ptr %holder.addr)\n"
            "  store %record.Holder zeroinitializer, ptr %holder.addr\n"
            "  ret i32 %tmp10",
        }
    );
    auto await_dynamic_array_record_field_index =
        run_program(app, await_dynamic_array_record_field_index_path);
    assert(await_dynamic_array_record_field_index.exit_code == 0);
    assert(await_dynamic_array_record_field_index.stdout_text.empty());
    assert(await_dynamic_array_record_field_index.stderr_text.empty());

    auto await_dynamic_array_record_field_index_out_of_bounds_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "concurrency_await_dynamic_array_record_field_index_out_of_bounds_main.or";
    auto await_dynamic_array_record_field_index_out_of_bounds_ir =
        run_emit_llvm(app, await_dynamic_array_record_field_index_out_of_bounds_path);
    assert_success_with_stdout_contains(
        await_dynamic_array_record_field_index_out_of_bounds_ir,
        {
            "%tmp6 = getelementptr %record.Holder, ptr %holder.addr, i32 0, i32 0\n"
            "  call void @__orison_task_await(ptr %pending)\n"
            "  %tmp7 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
            "  %holder.values.dynamic_array_element_path8.descriptor = load { ptr, i64, i64 }, ptr %tmp6",
            "%holder.values.dynamic_array_element_path8.length = extractvalue { ptr, i64, i64 } "
            "%holder.values.dynamic_array_element_path8.descriptor, 1\n"
            "  %holder.values.dynamic_array_element_path8.in_bounds = icmp ult i64 %tmp7, "
            "%holder.values.dynamic_array_element_path8.length",
            "br i1 %holder.values.dynamic_array_element_path8.in_bounds, "
            "label %dynamic_array.element_path.in_bounds.3, "
            "label %dynamic_array.element_path.out_of_bounds.3",
            "dynamic_array.element_path.out_of_bounds.3:\n"
            "  call void @__orison_dynamic_array_bounds_failed()\n"
            "  unreachable\n"
            "dynamic_array.element_path.in_bounds.3:\n"
            "  %holder.values.dynamic_array_element_path8.data = extractvalue { ptr, i64, i64 } "
            "%holder.values.dynamic_array_element_path8.descriptor, 0\n"
            "  %holder.values.dynamic_array_element_path8.element.addr = getelementptr i32, "
            "ptr %holder.values.dynamic_array_element_path8.data, i64 %tmp7\n"
            "  %tmp9 = load i32, ptr %holder.values.dynamic_array_element_path8.element.addr",
        }
    );
    auto await_dynamic_array_record_field_index_out_of_bounds =
        run_program(app, await_dynamic_array_record_field_index_out_of_bounds_path);
    assert(await_dynamic_array_record_field_index_out_of_bounds.exit_code == 134);
    assert(await_dynamic_array_record_field_index_out_of_bounds.stdout_text.empty());
    assert(await_dynamic_array_record_field_index_out_of_bounds.stderr_text.empty());

    auto await_dynamic_array_nested_record_field_index_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "concurrency_await_dynamic_array_nested_record_field_index_main.or";
    auto await_dynamic_array_nested_record_field_index_ir =
        run_emit_llvm(app, await_dynamic_array_nested_record_field_index_path);
    assert_success_with_stdout_contains(
        await_dynamic_array_nested_record_field_index_ir,
        {
            "%tmp8 = getelementptr %record.Outer, ptr %outer.addr, i32 0, i32 0\n"
            "  %tmp9 = getelementptr %record.Inner, ptr %tmp8, i32 0, i32 0\n"
            "  call void @__orison_task_await(ptr %pending)\n"
            "  %tmp10 = load i64, ptr %pending.task.result\n"
            "  call void @__orison_concurrency_handle_destroy(ptr %pending)",
            "%outer.inner.values.dynamic_array_element_path11.descriptor = "
            "load { ptr, i64, i64 }, ptr %tmp9\n"
            "  %outer.inner.values.dynamic_array_element_path11.length = "
            "extractvalue { ptr, i64, i64 } "
            "%outer.inner.values.dynamic_array_element_path11.descriptor, 1",
            "%outer.inner.values.dynamic_array_element_path11.in_bounds = icmp ult i64 %tmp10, "
            "%outer.inner.values.dynamic_array_element_path11.length\n"
            "  br i1 %outer.inner.values.dynamic_array_element_path11.in_bounds, "
            "label %dynamic_array.element_path.in_bounds.3, "
            "label %dynamic_array.element_path.out_of_bounds.3",
            "%outer.inner.values.dynamic_array_element_path11.element.addr = getelementptr i32, "
            "ptr %outer.inner.values.dynamic_array_element_path11.data, i64 %tmp10\n"
            "  %tmp12 = load i32, ptr %outer.inner.values.dynamic_array_element_path11.element.addr",
            "call void @__orison_owned_cleanup.Outer."
            "examples.concurrency_await_dynamic_array_nested_record_field_index(ptr %outer.addr)\n"
            "  store %record.Outer zeroinitializer, ptr %outer.addr\n"
            "  ret i32 %tmp13",
            "call void @__orison_owned_cleanup.Inner."
            "examples.concurrency_await_dynamic_array_nested_record_field_index"
            "(ptr %Outer.drop.inner.addr)",
        }
    );
    auto await_dynamic_array_nested_record_field_index =
        run_program(app, await_dynamic_array_nested_record_field_index_path);
    assert(await_dynamic_array_nested_record_field_index.exit_code == 0);
    assert(await_dynamic_array_nested_record_field_index.stdout_text.empty());
    assert(await_dynamic_array_nested_record_field_index.stderr_text.empty());

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
