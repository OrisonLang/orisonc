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
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return await request(url)\n";
    }

    auto path_text = path.string();
    std::array<char const*, 3> parse_argv {"orisonc", "--parse", path_text.c_str()};
    auto parse_result = app.run(std::span<char const* const>(parse_argv.data(), parse_argv.size()));

    assert(parse_result.exit_code == 1);
    assert(parse_result.stdout_text.empty());
    assert(parse_result.stderr_text.find("await expression is only valid inside async functions") != std::string::npos);

    auto await_value_path = std::filesystem::temp_directory_path() / "orison_compiler_app_await_value_failure.or";
    {
        std::ofstream output(await_value_path);
        output << "package demo.await\n";
        output << "async function fetch() -> Int64\n";
        output << "    let count = 1\n";
        output << "    return await count\n";
    }

    auto await_value_path_text = await_value_path.string();
    std::array<char const*, 3> await_value_argv {"orisonc", "--parse", await_value_path_text.c_str()};
    auto await_value_result = app.run(std::span<char const* const>(await_value_argv.data(), await_value_argv.size()));

    assert(await_value_result.exit_code == 1);
    assert(await_value_result.stdout_text.empty());
    assert(await_value_result.stderr_text.find(
               "await expression currently requires a task value or declared async call result"
           ) != std::string::npos);

    auto await_non_async_call_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_await_non_async_call_failure.or";
    {
        std::ofstream output(await_non_async_call_path);
        output << "package demo.await\n";
        output << "function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let pending = request(url)\n";
        output << "    return await pending\n";
    }

    auto await_non_async_call_path_text = await_non_async_call_path.string();
    std::array<char const*, 3> await_non_async_call_argv {
        "orisonc",
        "--parse",
        await_non_async_call_path_text.c_str()
    };
    auto await_non_async_call_result =
        app.run(std::span<char const* const>(await_non_async_call_argv.data(), await_non_async_call_argv.size()));

    assert(await_non_async_call_result.exit_code == 1);
    assert(await_non_async_call_result.stdout_text.empty());
    assert(await_non_async_call_result.stderr_text.find(
               "await expression currently requires a task value or declared async call result"
           ) != std::string::npos);

    auto await_member_name_collision_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_await_member_name_collision_failure.or";
    {
        std::ofstream output(await_member_name_collision_path);
        output << "package demo.await\n";
        output << "async function run(text: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(text)\n";
        output << "extend Printer\n";
        output << "    function run(this: shared This) -> Outcome<Text, IOError>\n";
        output << "        return render(this)\n";
        output << "async function fetch(printer: Printer) -> Outcome<Text, IOError>\n";
        output << "    let pending = printer.run()\n";
        output << "    return await pending\n";
    }

    auto await_member_name_collision_path_text = await_member_name_collision_path.string();
    std::array<char const*, 3> await_member_name_collision_argv {
        "orisonc",
        "--parse",
        await_member_name_collision_path_text.c_str()
    };
    auto await_member_name_collision_result = app.run(
        std::span<char const* const>(await_member_name_collision_argv.data(), await_member_name_collision_argv.size())
    );

    assert(await_member_name_collision_result.exit_code == 1);
    assert(await_member_name_collision_result.stdout_text.empty());
    assert(await_member_name_collision_result.stderr_text.find(
               "await expression currently requires a task value or declared async call result"
           ) != std::string::npos);

    auto await_thread_value_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_await_thread_value_failure.or";
    {
        std::ofstream output(await_thread_value_path);
        output << "package demo.await\n";
        output << "async function fetch() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        1\n";
        output << "    return await worker\n";
    }

    auto await_thread_value_path_text = await_thread_value_path.string();
    std::array<char const*, 3> await_thread_value_argv {
        "orisonc",
        "--parse",
        await_thread_value_path_text.c_str()
    };
    auto await_thread_value_result =
        app.run(std::span<char const* const>(await_thread_value_argv.data(), await_thread_value_argv.size()));

    assert(await_thread_value_result.exit_code == 1);
    assert(await_thread_value_result.stdout_text.empty());
    assert(await_thread_value_result.stderr_text.find(
               "await cannot be used with thread values; use .join() instead"
           ) != std::string::npos);

    auto return_task_value_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_return_task_value_failure.or";
    {
        std::ofstream output(return_task_value_path);
        output << "package demo.task\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return request_task\n";
    }

    auto return_task_value_path_text = return_task_value_path.string();
    std::array<char const*, 3> return_task_value_argv {
        "orisonc",
        "--parse",
        return_task_value_path_text.c_str()
    };
    auto return_task_value_result =
        app.run(std::span<char const* const>(return_task_value_argv.data(), return_task_value_argv.size()));

    assert(return_task_value_result.exit_code == 1);
    assert(return_task_value_result.stdout_text.empty());
    assert(return_task_value_result.stderr_text.find(
               "return cannot forward task or async-call values; use await instead"
           ) != std::string::npos);

    auto return_thread_value_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_return_thread_value_failure.or";
    {
        std::ofstream output(return_thread_value_path);
        output << "package demo.thread\n";
        output << "function parallel_sum() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        1\n";
        output << "    return worker\n";
    }

    auto return_thread_value_path_text = return_thread_value_path.string();
    std::array<char const*, 3> return_thread_value_argv {
        "orisonc",
        "--parse",
        return_thread_value_path_text.c_str()
    };
    auto return_thread_value_result =
        app.run(std::span<char const* const>(return_thread_value_argv.data(), return_thread_value_argv.size()));

    assert(return_thread_value_result.exit_code == 1);
    assert(return_thread_value_result.stdout_text.empty());
    assert(return_thread_value_result.stderr_text.find(
               "return cannot forward thread values; use .join() instead"
           ) != std::string::npos);

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

    auto assignment_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_assignment_async_origin_success.or";
    {
        std::ofstream output(assignment_async_origin_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    pending = request(url)\n";
        output << "    return await pending\n";
    }

    auto assignment_async_origin_path_text = assignment_async_origin_path.string();
    std::array<char const*, 3> assignment_async_origin_argv {
        "orisonc",
        "--parse",
        assignment_async_origin_path_text.c_str()
    };
    auto assignment_async_origin_result = app.run(
        std::span<char const* const>(assignment_async_origin_argv.data(), assignment_async_origin_argv.size())
    );

    assert(assignment_async_origin_result.exit_code == 0);
    assert(assignment_async_origin_result.stderr_text.empty());
    assert(assignment_async_origin_result.stdout_text.find("parsed ") != std::string::npos);

    auto ternary_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_ternary_async_origin_success.or";
    {
        std::ofstream output(ternary_async_origin_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    let left = request(url)\n";
        output << "    let right = request(url)\n";
        output << "    let pending = flag ? left : right\n";
        output << "    return await pending\n";
    }

    auto ternary_async_origin_path_text = ternary_async_origin_path.string();
    std::array<char const*, 3> ternary_async_origin_argv {
        "orisonc",
        "--parse",
        ternary_async_origin_path_text.c_str()
    };
    auto ternary_async_origin_result =
        app.run(std::span<char const* const>(ternary_async_origin_argv.data(), ternary_async_origin_argv.size()));

    assert(ternary_async_origin_result.exit_code == 0);
    assert(ternary_async_origin_result.stderr_text.empty());
    assert(ternary_async_origin_result.stdout_text.find("parsed ") != std::string::npos);

    auto ternary_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_ternary_thread_origin_failure.or";
    {
        std::ofstream output(ternary_thread_origin_path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    let left = thread\n";
        output << "        1\n";
        output << "    let right = thread\n";
        output << "        2\n";
        output << "    let worker = flag ? left : right\n";
        output << "    return await worker\n";
    }

    auto ternary_thread_origin_path_text = ternary_thread_origin_path.string();
    std::array<char const*, 3> ternary_thread_origin_argv {
        "orisonc",
        "--parse",
        ternary_thread_origin_path_text.c_str()
    };
    auto ternary_thread_origin_result =
        app.run(std::span<char const* const>(ternary_thread_origin_argv.data(), ternary_thread_origin_argv.size()));

    assert(ternary_thread_origin_result.exit_code == 1);
    assert(ternary_thread_origin_result.stdout_text.empty());
    assert(ternary_thread_origin_result.stderr_text.find(
               "await cannot be used with thread values; use .join() instead"
           ) != std::string::npos);

    auto if_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_if_async_origin_success.or";
    {
        std::ofstream output(if_async_origin_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    if flag\n";
        output << "        pending = request(url)\n";
        output << "    else\n";
        output << "        pending = request(url)\n";
        output << "    return await pending\n";
    }

    auto if_async_origin_path_text = if_async_origin_path.string();
    std::array<char const*, 3> if_async_origin_argv {
        "orisonc",
        "--parse",
        if_async_origin_path_text.c_str()
    };
    auto if_async_origin_result =
        app.run(std::span<char const* const>(if_async_origin_argv.data(), if_async_origin_argv.size()));

    assert(if_async_origin_result.exit_code == 0);
    assert(if_async_origin_result.stderr_text.empty());
    assert(if_async_origin_result.stdout_text.find("parsed ") != std::string::npos);

    auto while_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_while_async_origin_success.or";
    {
        std::ofstream output(while_async_origin_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    while flag\n";
        output << "        pending = request(url)\n";
        output << "    return await pending\n";
    }

    auto while_async_origin_path_text = while_async_origin_path.string();
    std::array<char const*, 3> while_async_origin_argv {
        "orisonc",
        "--parse",
        while_async_origin_path_text.c_str()
    };
    auto while_async_origin_result =
        app.run(std::span<char const* const>(while_async_origin_argv.data(), while_async_origin_argv.size()));

    assert(while_async_origin_result.exit_code == 0);
    assert(while_async_origin_result.stderr_text.empty());
    assert(while_async_origin_result.stdout_text.find("parsed ") != std::string::npos);

    auto for_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_for_async_origin_success.or";
    {
        std::ofstream output(for_async_origin_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(items: shared View<Int64>, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    for item in items\n";
        output << "        pending = request(url)\n";
        output << "    return await pending\n";
    }

    auto for_async_origin_path_text = for_async_origin_path.string();
    std::array<char const*, 3> for_async_origin_argv {
        "orisonc",
        "--parse",
        for_async_origin_path_text.c_str()
    };
    auto for_async_origin_result =
        app.run(std::span<char const* const>(for_async_origin_argv.data(), for_async_origin_argv.size()));

    assert(for_async_origin_result.exit_code == 0);
    assert(for_async_origin_result.stderr_text.empty());
    assert(for_async_origin_result.stdout_text.find("parsed ") != std::string::npos);

    auto guard_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_guard_async_origin_success.or";
    {
        std::ofstream output(guard_async_origin_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    guard flag else\n";
        output << "        pending = thread\n";
        output << "            2\n";
        output << "        return await request(url)\n";
        output << "    return await pending\n";
    }

    auto guard_async_origin_path_text = guard_async_origin_path.string();
    std::array<char const*, 3> guard_async_origin_argv {
        "orisonc",
        "--parse",
        guard_async_origin_path_text.c_str()
    };
    auto guard_async_origin_result =
        app.run(std::span<char const* const>(guard_async_origin_argv.data(), guard_async_origin_argv.size()));

    assert(guard_async_origin_result.exit_code == 0);
    assert(guard_async_origin_result.stderr_text.empty());
    assert(guard_async_origin_result.stdout_text.find("parsed ") != std::string::npos);

    auto switch_constructor_pattern_binding_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_constructor_pattern_binding_success.or";
    {
        std::ofstream output(switch_constructor_pattern_binding_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    var head = 0\n";
        output << "    switch xs\n";
        output << "        Empty => 0\n";
        output << "        Node(head, tail) =>\n";
        output << "            let request_task = task\n";
        output << "                head\n";
        output << "            return await request_task\n";
    }

    auto switch_constructor_pattern_binding_path_text = switch_constructor_pattern_binding_path.string();
    std::array<char const*, 3> switch_constructor_pattern_binding_argv {
        "orisonc",
        "--parse",
        switch_constructor_pattern_binding_path_text.c_str()
    };
    auto switch_constructor_pattern_binding_result = app.run(
        std::span<char const* const>(
            switch_constructor_pattern_binding_argv.data(),
            switch_constructor_pattern_binding_argv.size()
        )
    );

    assert(switch_constructor_pattern_binding_result.exit_code == 0);
    assert(switch_constructor_pattern_binding_result.stderr_text.empty());
    assert(switch_constructor_pattern_binding_result.stdout_text.find("parsed ") != std::string::npos);

    auto switch_nested_constructor_pattern_binding_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_constructor_pattern_success.or";
    {
        std::ofstream output(switch_nested_constructor_pattern_binding_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(next, tail)) =>\n";
        output << "            let request_task = task\n";
        output << "                next\n";
        output << "            return await request_task\n";
        output << "        default => 0\n";
    }

    auto switch_nested_constructor_pattern_binding_path_text =
        switch_nested_constructor_pattern_binding_path.string();
    std::array<char const*, 3> switch_nested_constructor_pattern_binding_argv {
        "orisonc",
        "--parse",
        switch_nested_constructor_pattern_binding_path_text.c_str()
    };
    auto switch_nested_constructor_pattern_binding_result = app.run(
        std::span<char const* const>(
            switch_nested_constructor_pattern_binding_argv.data(),
            switch_nested_constructor_pattern_binding_argv.size()
        )
    );

    assert(switch_nested_constructor_pattern_binding_result.exit_code == 0);
    assert(switch_nested_constructor_pattern_binding_result.stderr_text.empty());
    assert(switch_nested_constructor_pattern_binding_result.stdout_text.find("parsed ") != std::string::npos);

    auto switch_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_thread_origin_failure.or";
    {
        std::ofstream output(switch_thread_origin_path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    var worker = thread\n";
        output << "        1\n";
        output << "    switch flag\n";
        output << "        true => worker = thread\n";
        output << "            2\n";
        output << "        false => worker = thread\n";
        output << "            3\n";
        output << "    return await worker\n";
    }

    auto switch_thread_origin_path_text = switch_thread_origin_path.string();
    std::array<char const*, 3> switch_thread_origin_argv {
        "orisonc",
        "--parse",
        switch_thread_origin_path_text.c_str()
    };
    auto switch_thread_origin_result =
        app.run(std::span<char const* const>(switch_thread_origin_argv.data(), switch_thread_origin_argv.size()));

    assert(switch_thread_origin_result.exit_code == 1);
    assert(switch_thread_origin_result.stdout_text.empty());
    assert(switch_thread_origin_result.stderr_text.find(
               "await cannot be used with thread values; use .join() instead"
           ) != std::string::npos);

    auto repeat_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_repeat_thread_origin_failure.or";
    {
        std::ofstream output(repeat_thread_origin_path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    var worker = 0\n";
        output << "    repeat\n";
        output << "        worker = thread\n";
        output << "            2\n";
        output << "    while flag\n";
        output << "    return await worker\n";
    }

    auto repeat_thread_origin_path_text = repeat_thread_origin_path.string();
    std::array<char const*, 3> repeat_thread_origin_argv {
        "orisonc",
        "--parse",
        repeat_thread_origin_path_text.c_str()
    };
    auto repeat_thread_origin_result =
        app.run(std::span<char const* const>(repeat_thread_origin_argv.data(), repeat_thread_origin_argv.size()));

    assert(repeat_thread_origin_result.exit_code == 1);
    assert(repeat_thread_origin_result.stdout_text.empty());
    assert(repeat_thread_origin_result.stderr_text.find(
               "await cannot be used with thread values; use .join() instead"
           ) != std::string::npos);

    auto for_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_for_thread_origin_failure.or";
    {
        std::ofstream output(for_thread_origin_path);
        output << "package demo.await\n";
        output << "async function fetch(items: shared View<Int64>) -> Int64\n";
        output << "    var worker = thread\n";
        output << "        1\n";
        output << "    for item in items\n";
        output << "        worker = thread\n";
        output << "            2\n";
        output << "    return await worker\n";
    }

    auto for_thread_origin_path_text = for_thread_origin_path.string();
    std::array<char const*, 3> for_thread_origin_argv {
        "orisonc",
        "--parse",
        for_thread_origin_path_text.c_str()
    };
    auto for_thread_origin_result =
        app.run(std::span<char const* const>(for_thread_origin_argv.data(), for_thread_origin_argv.size()));

    assert(for_thread_origin_result.exit_code == 1);
    assert(for_thread_origin_result.stdout_text.empty());
    assert(for_thread_origin_result.stderr_text.find(
               "await cannot be used with thread values; use .join() instead"
           ) != std::string::npos);

    auto guard_async_missing_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_guard_async_origin_failure.or";
    {
        std::ofstream output(guard_async_missing_origin_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = 0\n";
        output << "    guard flag else\n";
        output << "        pending = request(url)\n";
        output << "        return await request(url)\n";
        output << "    return await pending\n";
    }

    auto guard_async_missing_origin_path_text = guard_async_missing_origin_path.string();
    std::array<char const*, 3> guard_async_missing_origin_argv {
        "orisonc",
        "--parse",
        guard_async_missing_origin_path_text.c_str()
    };
    auto guard_async_missing_origin_result = app.run(
        std::span<char const* const>(
            guard_async_missing_origin_argv.data(),
            guard_async_missing_origin_argv.size()
        )
    );

    assert(guard_async_missing_origin_result.exit_code == 1);
    assert(guard_async_missing_origin_result.stdout_text.empty());
    assert(guard_async_missing_origin_result.stderr_text.find(
               "await expression currently requires a task value or declared async call result"
           ) != std::string::npos);

    auto switch_name_pattern_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_name_pattern_binding_failure.or";
    {
        std::ofstream output(switch_name_pattern_binding_failure_path);
        output << "package demo.patterns\n";
        output << "async function read(value: Int64) -> Int64\n";
        output << "    var head = 0\n";
        output << "    switch value\n";
        output << "        head =>\n";
        output << "            let request_task = task\n";
        output << "                head\n";
        output << "            return await request_task\n";
        output << "        default => 0\n";
    }

    auto switch_name_pattern_binding_failure_path_text = switch_name_pattern_binding_failure_path.string();
    std::array<char const*, 3> switch_name_pattern_binding_failure_argv {
        "orisonc",
        "--parse",
        switch_name_pattern_binding_failure_path_text.c_str()
    };
    auto switch_name_pattern_binding_failure_result = app.run(
        std::span<char const* const>(
            switch_name_pattern_binding_failure_argv.data(),
            switch_name_pattern_binding_failure_argv.size()
        )
    );

    assert(switch_name_pattern_binding_failure_result.exit_code == 1);
    assert(switch_name_pattern_binding_failure_result.stdout_text.empty());
    assert(switch_name_pattern_binding_failure_result.stderr_text.find(
               "concurrency expression cannot capture mutable outer local 'head'"
           ) != std::string::npos);

    auto switch_nested_constructor_pattern_shape_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_constructor_pattern_shape_failure.or";
    {
        std::ofstream output(switch_nested_constructor_pattern_shape_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head + 1, tail) => 0\n";
        output << "        default => 0\n";
    }

    auto switch_nested_constructor_pattern_shape_failure_path_text =
        switch_nested_constructor_pattern_shape_failure_path.string();
    std::array<char const*, 3> switch_nested_constructor_pattern_shape_failure_argv {
        "orisonc",
        "--parse",
        switch_nested_constructor_pattern_shape_failure_path_text.c_str()
    };
    auto switch_nested_constructor_pattern_shape_failure_result = app.run(
        std::span<char const* const>(
            switch_nested_constructor_pattern_shape_failure_argv.data(),
            switch_nested_constructor_pattern_shape_failure_argv.size()
        )
    );

    assert(switch_nested_constructor_pattern_shape_failure_result.exit_code == 1);
    assert(switch_nested_constructor_pattern_shape_failure_result.stdout_text.empty());
    assert(switch_nested_constructor_pattern_shape_failure_result.stderr_text.find(
               "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
           ) != std::string::npos);

    auto switch_constructor_pattern_duplicate_binding_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_duplicate_binding_failure.or";
    {
        std::ofstream output(switch_constructor_pattern_duplicate_binding_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, head) => 0\n";
        output << "        default => 0\n";
    }

    auto switch_constructor_pattern_duplicate_binding_failure_path_text =
        switch_constructor_pattern_duplicate_binding_failure_path.string();
    std::array<char const*, 3> switch_constructor_pattern_duplicate_binding_failure_argv {
        "orisonc",
        "--parse",
        switch_constructor_pattern_duplicate_binding_failure_path_text.c_str()
    };
    auto switch_constructor_pattern_duplicate_binding_failure_result = app.run(
        std::span<char const* const>(
            switch_constructor_pattern_duplicate_binding_failure_argv.data(),
            switch_constructor_pattern_duplicate_binding_failure_argv.size()
        )
    );

    assert(switch_constructor_pattern_duplicate_binding_failure_result.exit_code == 1);
    assert(switch_constructor_pattern_duplicate_binding_failure_result.stdout_text.empty());
    assert(switch_constructor_pattern_duplicate_binding_failure_result.stderr_text.find(
               "switch constructor pattern cannot bind 'head' more than once"
           ) != std::string::npos);

    auto switch_nested_constructor_pattern_duplicate_binding_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_constructor_pattern_duplicate_binding_failure.or";
    {
        std::ofstream output(switch_nested_constructor_pattern_duplicate_binding_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(head, tail)) => 0\n";
        output << "        default => 0\n";
    }

    auto switch_nested_constructor_pattern_duplicate_binding_failure_path_text =
        switch_nested_constructor_pattern_duplicate_binding_failure_path.string();
    std::array<char const*, 3> switch_nested_constructor_pattern_duplicate_binding_failure_argv {
        "orisonc",
        "--parse",
        switch_nested_constructor_pattern_duplicate_binding_failure_path_text.c_str()
    };
    auto switch_nested_constructor_pattern_duplicate_binding_failure_result = app.run(
        std::span<char const* const>(
            switch_nested_constructor_pattern_duplicate_binding_failure_argv.data(),
            switch_nested_constructor_pattern_duplicate_binding_failure_argv.size()
        )
    );

    assert(switch_nested_constructor_pattern_duplicate_binding_failure_result.exit_code == 1);
    assert(switch_nested_constructor_pattern_duplicate_binding_failure_result.stdout_text.empty());
    assert(switch_nested_constructor_pattern_duplicate_binding_failure_result.stderr_text.find(
               "switch constructor pattern cannot bind 'head' more than once"
           ) != std::string::npos);

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

    auto typed_capture_path = std::filesystem::temp_directory_path() / "orison_compiler_app_typed_capture_failure.or";
    {
        std::ofstream output(typed_capture_path);
        output << "package demo.thread\n";
        output << "function launch_processing(buffer: Buffer) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(buffer)\n";
        output << "    return worker.join()\n";
    }

    auto typed_capture_path_text = typed_capture_path.string();
    std::array<char const*, 3> typed_capture_argv {"orisonc", "--parse", typed_capture_path_text.c_str()};
    auto typed_capture_result =
        app.run(std::span<char const* const>(typed_capture_argv.data(), typed_capture_argv.size()));

    assert(typed_capture_result.exit_code == 1);
    assert(typed_capture_result.stdout_text.empty());
    assert(typed_capture_result.stderr_text.find(
               "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis"
           ) != std::string::npos);

    auto generic_capture_path = std::filesystem::temp_directory_path() / "orison_compiler_app_generic_capture_failure.or";
    {
        std::ofstream output(generic_capture_path);
        output << "package demo.thread\n";
        output << "function launch<T>(item: T) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(item)\n";
        output << "    return worker.join()\n";
    }

    auto generic_capture_path_text = generic_capture_path.string();
    std::array<char const*, 3> generic_capture_argv {"orisonc", "--parse", generic_capture_path_text.c_str()};
    auto generic_capture_result =
        app.run(std::span<char const* const>(generic_capture_argv.data(), generic_capture_argv.size()));

    assert(generic_capture_result.exit_code == 1);
    assert(generic_capture_result.stdout_text.empty());
    assert(generic_capture_result.stderr_text.find(
               "thread capture 'item' of type 'T' requires future Transferable analysis"
           ) != std::string::npos);

    auto shareable_thread_path = std::filesystem::temp_directory_path() / "orison_compiler_app_shareable_thread_failure.or";
    {
        std::ofstream output(shareable_thread_path);
        output << "package demo.thread\n";
        output << "implements Shareable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "function launch_processing(buffer: Buffer) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(buffer)\n";
        output << "    return worker.join()\n";
    }

    auto shareable_thread_path_text = shareable_thread_path.string();
    std::array<char const*, 3> shareable_thread_argv {"orisonc", "--parse", shareable_thread_path_text.c_str()};
    auto shareable_thread_result =
        app.run(std::span<char const* const>(shareable_thread_argv.data(), shareable_thread_argv.size()));

    assert(shareable_thread_result.exit_code == 1);
    assert(shareable_thread_result.stdout_text.empty());
    assert(shareable_thread_result.stderr_text.find(
               "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis"
           ) != std::string::npos);

    auto join_receiver_path = std::filesystem::temp_directory_path() / "orison_compiler_app_join_receiver_failure.or";
    {
        std::ofstream output(join_receiver_path);
        output << "package demo.thread\n";
        output << "async function fetch() -> Int64\n";
        output << "    let request_task = task\n";
        output << "        1\n";
        output << "    return request_task.join()\n";
    }

    auto join_receiver_path_text = join_receiver_path.string();
    std::array<char const*, 3> join_receiver_argv {"orisonc", "--parse", join_receiver_path_text.c_str()};
    auto join_receiver_result =
        app.run(std::span<char const* const>(join_receiver_argv.data(), join_receiver_argv.size()));

    assert(join_receiver_result.exit_code == 1);
    assert(join_receiver_result.stdout_text.empty());
    assert(join_receiver_result.stderr_text.find(
               "join() cannot be used with task values; use await instead"
           ) != std::string::npos);

    auto join_async_call_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_join_async_call_failure.or";
    {
        std::ofstream output(join_async_call_path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let pending = request(url)\n";
        output << "    return pending.join()\n";
    }

    auto join_async_call_path_text = join_async_call_path.string();
    std::array<char const*, 3> join_async_call_argv {
        "orisonc",
        "--parse",
        join_async_call_path_text.c_str()
    };
    auto join_async_call_result =
        app.run(std::span<char const* const>(join_async_call_argv.data(), join_async_call_argv.size()));

    assert(join_async_call_result.exit_code == 1);
    assert(join_async_call_result.stdout_text.empty());
    assert(join_async_call_result.stderr_text.find(
               "join() cannot be used with declared async call results; use await instead"
           ) != std::string::npos);

    auto thread_value_path = std::filesystem::temp_directory_path() / "orison_compiler_app_thread_value_failure.or";
    {
        std::ofstream output(thread_value_path);
        output << "package demo.thread\n";
        output << "function parallel_sum() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        1\n";
        output << "    return worker\n";
    }

    auto thread_value_path_text = thread_value_path.string();
    std::array<char const*, 3> thread_value_argv {"orisonc", "--parse", thread_value_path_text.c_str()};
    auto thread_value_result =
        app.run(std::span<char const* const>(thread_value_argv.data(), thread_value_argv.size()));

    assert(thread_value_result.exit_code == 1);
    assert(thread_value_result.stdout_text.empty());
    assert(thread_value_result.stderr_text.find(
               "return cannot forward thread values; use .join() instead"
           ) != std::string::npos);

    auto concrete_marker_path = std::filesystem::temp_directory_path() / "orison_compiler_app_concrete_marker_success.or";
    {
        std::ofstream output(concrete_marker_path);
        output << "package demo.thread\n";
        output << "implements Transferable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "function launch_processing(buffer: Buffer) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(buffer)\n";
        output << "    return worker.join()\n";
    }

    auto concrete_marker_path_text = concrete_marker_path.string();
    std::array<char const*, 3> concrete_marker_argv {"orisonc", "--parse", concrete_marker_path_text.c_str()};
    auto concrete_marker_result =
        app.run(std::span<char const* const>(concrete_marker_argv.data(), concrete_marker_argv.size()));

    assert(concrete_marker_result.exit_code == 0);
    assert(concrete_marker_result.stderr_text.empty());
    assert(concrete_marker_result.stdout_text.find("parsed ") != std::string::npos);

    auto shareable_task_path = std::filesystem::temp_directory_path() / "orison_compiler_app_shareable_task_success.or";
    {
        std::ofstream output(shareable_task_path);
        output << "package demo.task\n";
        output << "implements Shareable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "async function launch_processing(buffer: Buffer) -> Buffer\n";
        output << "    let worker = task\n";
        output << "        buffer\n";
        output << "    return await worker\n";
    }

    auto shareable_task_path_text = shareable_task_path.string();
    std::array<char const*, 3> shareable_task_argv {"orisonc", "--parse", shareable_task_path_text.c_str()};
    auto shareable_task_result =
        app.run(std::span<char const* const>(shareable_task_argv.data(), shareable_task_argv.size()));

    assert(shareable_task_result.exit_code == 0);
    assert(shareable_task_result.stderr_text.empty());
    assert(shareable_task_result.stdout_text.find("parsed ") != std::string::npos);

    auto thread_result_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_thread_result_failure.or";
    {
        std::ofstream output(thread_result_failure_path);
        output << "package demo.thread\n";
        output << "function launch_processing() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        let produced: Buffer = make_buffer()\n";
        output << "        produced\n";
        output << "    return worker.join()\n";
    }

    auto thread_result_failure_path_text = thread_result_failure_path.string();
    std::array<char const*, 3> thread_result_failure_argv {
        "orisonc",
        "--parse",
        thread_result_failure_path_text.c_str()
    };
    auto thread_result_failure_result =
        app.run(std::span<char const* const>(thread_result_failure_argv.data(), thread_result_failure_argv.size()));

    assert(thread_result_failure_result.exit_code == 1);
    assert(thread_result_failure_result.stdout_text.empty());
    assert(thread_result_failure_result.stderr_text.find(
               "thread result type 'Buffer' requires future Transferable analysis"
           ) != std::string::npos);

    auto task_result_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_task_result_shareable_success.or";
    {
        std::ofstream output(task_result_success_path);
        output << "package demo.task\n";
        output << "implements Shareable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "async function launch_processing() -> Buffer\n";
        output << "    let worker = task\n";
        output << "        let produced: Buffer = make_buffer()\n";
        output << "        produced\n";
        output << "    return await worker\n";
    }

    auto task_result_success_path_text = task_result_success_path.string();
    std::array<char const*, 3> task_result_success_argv {
        "orisonc",
        "--parse",
        task_result_success_path_text.c_str()
    };
    auto task_result_success_result =
        app.run(std::span<char const* const>(task_result_success_argv.data(), task_result_success_argv.size()));

    assert(task_result_success_result.exit_code == 0);
    assert(task_result_success_result.stderr_text.empty());
    assert(task_result_success_result.stdout_text.find("parsed ") != std::string::npos);
    return 0;
}
