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
        ("orison_driver_concurrency_capture_diagnostics_smoke_" +
         std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto thread_body_path = std::filesystem::temp_directory_path() / "thread_body_failure.or";
    write_fixture(
        thread_body_path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        let total = sum(data)",
            "    return worker.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, thread_body_path),
        "thread expression body must end with an expression statement or value return"
    );

    auto mutable_capture_path = std::filesystem::temp_directory_path() / "mutable_capture_failure.or";
    write_fixture(
        mutable_capture_path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    var attempts = 0",
            "    let request_task = task",
            "        attempts",
            "    return await request_task",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, mutable_capture_path),
        "concurrency expression cannot capture mutable outer local 'attempts'"
    );

    auto receiver_capture_path = std::filesystem::temp_directory_path() / "receiver_capture_failure.or";
    write_fixture(
        receiver_capture_path,
        "demo.thread",
        {
            "extend Worker",
            "    function spawn(this: shared This) -> Int64",
            "        let worker = thread",
            "            this.id",
            "        return worker.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, receiver_capture_path),
        "concurrency expression cannot capture receiver 'this'"
    );

    auto typed_capture_path = std::filesystem::temp_directory_path() / "typed_capture_failure.or";
    write_fixture(
        typed_capture_path,
        "demo.thread",
        {
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, typed_capture_path),
        "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis"
    );

    auto generic_capture_path = std::filesystem::temp_directory_path() / "generic_capture_failure.or";
    write_fixture(
        generic_capture_path,
        "demo.thread",
        {
            "function launch<T>(item: T) -> Int64",
            "    let worker = thread",
            "        process(item)",
            "    return worker.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, generic_capture_path),
        "thread capture 'item' of type 'T' requires future Transferable analysis"
    );

    auto shareable_thread_path = std::filesystem::temp_directory_path() / "shareable_thread_failure.or";
    write_fixture(
        shareable_thread_path,
        "demo.thread",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, shareable_thread_path),
        "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis"
    );

    auto join_receiver_path = std::filesystem::temp_directory_path() / "join_receiver_failure.or";
    write_fixture(
        join_receiver_path,
        "demo.thread",
        {
            "async function fetch() -> Int64",
            "    let request_task = task",
            "        1",
            "    return request_task.join()",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, join_receiver_path),
        "join() cannot be used with task values; use await instead"
    );

    return 0;
}
