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

void assert_parse_success(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    assert(result.stdout_text.find("parsed ") != std::string_view::npos);
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
        ("orison_driver_async_origin_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto task_path = std::filesystem::temp_directory_path() / "task_failure.or";
    write_fixture(
        task_path,
        "demo.task",
        {
            "function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return request(url)",
        }
    );
    assert_parse_failure_contains(run_parse(app, task_path), "task expression is only valid inside async functions");

    auto assignment_async_origin_path = std::filesystem::temp_directory_path() / "assignment_async_origin_success.or";
    write_fixture(
        assignment_async_origin_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    pending = request(url)",
            "    return await pending",
        }
    );
    assert_parse_success(run_parse(app, assignment_async_origin_path));

    auto ternary_async_origin_path = std::filesystem::temp_directory_path() / "ternary_async_origin_success.or";
    write_fixture(
        ternary_async_origin_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    let left = request(url)",
            "    let right = request(url)",
            "    let pending = flag ? left : right",
            "    return await pending",
        }
    );
    assert_parse_success(run_parse(app, ternary_async_origin_path));

    auto ternary_thread_origin_path = std::filesystem::temp_directory_path() / "ternary_thread_origin_failure.or";
    write_fixture(
        ternary_thread_origin_path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    let left = thread",
            "        1",
            "    let right = thread",
            "        2",
            "    let worker = flag ? left : right",
            "    return await worker",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, ternary_thread_origin_path),
        "await cannot be used with thread values; use .join() instead"
    );

    auto switch_thread_origin_path = std::filesystem::temp_directory_path() / "switch_thread_origin_failure.or";
    write_fixture(
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

    auto repeat_thread_origin_path = std::filesystem::temp_directory_path() / "repeat_thread_origin_failure.or";
    write_fixture(
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

    auto if_async_origin_path = std::filesystem::temp_directory_path() / "if_async_origin_success.or";
    write_fixture(
        if_async_origin_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    if flag",
            "        pending = request(url)",
            "    else",
            "        pending = request(url)",
            "    return await pending",
        }
    );
    assert_parse_success(run_parse(app, if_async_origin_path));

    auto while_async_origin_path = std::filesystem::temp_directory_path() / "while_async_origin_success.or";
    write_fixture(
        while_async_origin_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    while flag",
            "        pending = request(url)",
            "    return await pending",
        }
    );
    assert_parse_success(run_parse(app, while_async_origin_path));

    auto for_async_origin_path = std::filesystem::temp_directory_path() / "for_async_origin_success.or";
    write_fixture(
        for_async_origin_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(items: shared View<Int64>, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    for item in items",
            "        pending = request(url)",
            "    return await pending",
        }
    );
    assert_parse_success(run_parse(app, for_async_origin_path));

    auto for_thread_origin_path = std::filesystem::temp_directory_path() / "for_thread_origin_failure.or";
    write_fixture(
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

    auto guard_async_origin_path = std::filesystem::temp_directory_path() / "guard_async_origin_success.or";
    write_fixture(
        guard_async_origin_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    guard flag else",
            "        pending = thread",
            "            2",
            "        return await request(url)",
            "    return await pending",
        }
    );
    assert_parse_success(run_parse(app, guard_async_origin_path));

    auto guard_async_missing_origin_path = std::filesystem::temp_directory_path() / "guard_async_origin_failure.or";
    write_fixture(
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

    return 0;
}
