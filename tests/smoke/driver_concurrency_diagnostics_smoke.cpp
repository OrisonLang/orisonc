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

auto run_parse(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "--parse",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_parse_failure_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_message) != std::string_view::npos);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_concurrency_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto await_non_async_path = std::filesystem::temp_directory_path() / "orison_driver_await_failure.or";
    write_fixture(
        await_non_async_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "function fetch(url: Text) -> Outcome<Text, IOError>",
            "    return await request(url)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, await_non_async_path),
        "await expression is only valid inside async functions"
    );

    auto await_value_path = std::filesystem::temp_directory_path() / "orison_driver_await_value_failure.or";
    write_fixture(
        await_value_path,
        "demo.await",
        {
            "async function fetch() -> Int64",
            "    let count = 1",
            "    return await count",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, await_value_path),
        "await expression currently requires a task value or declared async call result"
    );

    auto await_non_async_call_path =
        std::filesystem::temp_directory_path() / "orison_driver_await_non_async_call_failure.or";
    write_fixture(
        await_non_async_call_path,
        "demo.await",
        {
            "function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return await pending",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, await_non_async_call_path),
        "await expression currently requires a task value or declared async call result"
    );

    auto await_member_name_collision_path =
        std::filesystem::temp_directory_path() / "orison_driver_await_member_name_collision_failure.or";
    write_fixture(
        await_member_name_collision_path,
        "demo.await",
        {
            "async function run(text: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(text)",
            "extend Printer",
            "    function run(this: shared This) -> Outcome<Text, IOError>",
            "        return render(this)",
            "async function fetch(printer: Printer) -> Outcome<Text, IOError>",
            "    let pending = printer.run()",
            "    return await pending",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, await_member_name_collision_path),
        "await expression currently requires a task value or declared async call result"
    );

    auto await_thread_value_path =
        std::filesystem::temp_directory_path() / "orison_driver_await_thread_value_failure.or";
    write_fixture(
        await_thread_value_path,
        "demo.await",
        {
            "async function fetch() -> Int64",
            "    let worker = thread",
            "        1",
            "    return await worker",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, await_thread_value_path),
        "await cannot be used with thread values; use .join() instead"
    );

    auto return_task_value_path =
        std::filesystem::temp_directory_path() / "orison_driver_return_task_value_failure.or";
    write_fixture(
        return_task_value_path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return request_task",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, return_task_value_path),
        "return cannot forward task or async-call values; use await instead"
    );

    auto return_thread_value_path =
        std::filesystem::temp_directory_path() / "orison_driver_return_thread_value_failure.or";
    write_fixture(
        return_thread_value_path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, return_thread_value_path),
        "return cannot forward thread values; use .join() instead"
    );

    return 0;
}
