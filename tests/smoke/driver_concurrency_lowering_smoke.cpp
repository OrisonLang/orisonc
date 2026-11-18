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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
