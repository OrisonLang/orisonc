#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <span>

int main() {
    orison::driver::CompilerApp app;

    std::array<char const*, 2> version_argv {"orisonc", "--version"};
    auto result = app.run(std::span<char const* const>(version_argv.data(), version_argv.size()));

    assert(result.exit_code == 0);
    assert(result.stdout_text == "orisonc 0.1.0-dev\n");
    assert(result.stderr_text.empty());

    auto path = std::filesystem::temp_directory_path() / "orison_compiler_app_await_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return await request(url)\n";
    }

    auto path_text = path.string();
    std::array<char const*, 3> parse_argv {"orisonc", "--parse", path_text.c_str()};
    auto parse_result = app.run(std::span<char const* const>(parse_argv.data(), parse_argv.size()));

    assert(parse_result.exit_code == 1);
    assert(parse_result.stdout_text.empty());
    assert(parse_result.stderr_text.find("await expression is only valid inside async functions") != std::string::npos);

    auto task_path = std::filesystem::temp_directory_path() / "orison_compiler_app_task_failure.or";
    {
        std::ofstream output(task_path);
        output << "package demo.task\n";
        output << "function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return request(url)\n";
    }

    auto task_path_text = task_path.string();
    std::array<char const*, 3> task_argv {"orisonc", "--parse", task_path_text.c_str()};
    auto task_result = app.run(std::span<char const* const>(task_argv.data(), task_argv.size()));

    assert(task_result.exit_code == 1);
    assert(task_result.stdout_text.empty());
    assert(task_result.stderr_text.find("task expression is only valid inside async functions") != std::string::npos);

    auto thread_path = std::filesystem::temp_directory_path() / "orison_compiler_app_thread_value_failure.or";
    {
        std::ofstream output(thread_path);
        output << "package demo.thread\n";
        output << "function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        let total = sum(data)\n";
        output << "    return worker.join()\n";
    }

    auto thread_path_text = thread_path.string();
    std::array<char const*, 3> thread_argv {"orisonc", "--parse", thread_path_text.c_str()};
    auto thread_result = app.run(std::span<char const* const>(thread_argv.data(), thread_argv.size()));

    assert(thread_result.exit_code == 1);
    assert(thread_result.stdout_text.empty());
    assert(thread_result.stderr_text.find(
               "thread expression body must end with an expression statement or value return"
           ) != std::string::npos);

    auto capture_path = std::filesystem::temp_directory_path() / "orison_compiler_app_capture_failure.or";
    {
        std::ofstream output(capture_path);
        output << "package demo.task\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    var attempts = 0\n";
        output << "    let request_task = task\n";
        output << "        attempts\n";
        output << "    return await request_task\n";
    }

    auto capture_path_text = capture_path.string();
    std::array<char const*, 3> capture_argv {"orisonc", "--parse", capture_path_text.c_str()};
    auto capture_result = app.run(std::span<char const* const>(capture_argv.data(), capture_argv.size()));

    assert(capture_result.exit_code == 1);
    assert(capture_result.stdout_text.empty());
    assert(capture_result.stderr_text.find(
               "concurrency expression cannot capture mutable outer local 'attempts'"
           ) != std::string::npos);

    auto receiver_path = std::filesystem::temp_directory_path() / "orison_compiler_app_capture_this_failure.or";
    {
        std::ofstream output(receiver_path);
        output << "package demo.thread\n";
        output << "extend Worker\n";
        output << "    function spawn(this: shared This) -> Int64\n";
        output << "        let worker = thread\n";
        output << "            this.id\n";
        output << "        return worker.join()\n";
    }

    auto receiver_path_text = receiver_path.string();
    std::array<char const*, 3> receiver_argv {"orisonc", "--parse", receiver_path_text.c_str()};
    auto receiver_result = app.run(std::span<char const* const>(receiver_argv.data(), receiver_argv.size()));

    assert(receiver_result.exit_code == 1);
    assert(receiver_result.stdout_text.empty());
    assert(receiver_result.stderr_text.find(
               "concurrency expression cannot capture receiver 'this'"
           ) != std::string::npos);
    return 0;
}
