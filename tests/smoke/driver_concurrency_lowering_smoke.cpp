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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
