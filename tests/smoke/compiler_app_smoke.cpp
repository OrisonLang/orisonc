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

auto run_emit_llvm(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--emit-llvm", path);
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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
