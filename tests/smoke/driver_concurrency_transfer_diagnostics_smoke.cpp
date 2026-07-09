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
        ("orison_driver_concurrency_transfer_diagnostics_smoke_" +
         std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto join_async_call_path = std::filesystem::temp_directory_path() / "join_async_call_failure.or";
    write_fixture(
        join_async_call_path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return pending.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, join_async_call_path),
        "join() cannot be used with declared async call results; use await instead"
    );

    auto thread_value_path = std::filesystem::temp_directory_path() / "thread_value_failure.or";
    write_fixture(
        thread_value_path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, thread_value_path),
        "return cannot forward thread values; use .join() instead"
    );

    auto concrete_marker_path = std::filesystem::temp_directory_path() / "concrete_marker_success.or";
    write_fixture(
        concrete_marker_path,
        "demo.thread",
        {
            "implements Transferable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );
    assert_parse_success(run_parse(app, concrete_marker_path));

    auto shareable_task_path = std::filesystem::temp_directory_path() / "shareable_task_success.or";
    write_fixture(
        shareable_task_path,
        "demo.task",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "async function launch_processing(buffer: Buffer) -> Buffer",
            "    let worker = task",
            "        buffer",
            "    return await worker",
        }
    );
    assert_parse_success(run_parse(app, shareable_task_path));

    auto thread_result_failure_path = std::filesystem::temp_directory_path() / "thread_result_failure.or";
    write_fixture(
        thread_result_failure_path,
        "demo.thread",
        {
            "function launch_processing() -> Int64",
            "    let worker = thread",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return worker.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, thread_result_failure_path),
        "thread result type 'Buffer' requires future Transferable analysis"
    );

    auto task_result_success_path = std::filesystem::temp_directory_path() / "task_result_shareable_success.or";
    write_fixture(
        task_result_success_path,
        "demo.task",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "async function launch_processing() -> Buffer",
            "    let worker = task",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return await worker",
        }
    );
    assert_parse_success(run_parse(app, task_result_success_path));

    return 0;
}
