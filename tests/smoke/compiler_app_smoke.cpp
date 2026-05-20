#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <span>
#include <string_view>

namespace {

void write_boxed_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: Maybe<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_boxed_pair_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice PairMaybe<T>\n";
    output << "    PairSome(left: T, right: Int64)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: PairMaybe<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    output << "        default => 0\n";
}

void write_boxed_outer_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Outer<T>\n";
    output << "    Hold(inner: Maybe<T>)\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: Outer<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    output << "        default => 0\n";
}

void write_list_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool async_function = false,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << (async_function ? "async " : "") << "function sum(xs: List<Int64>) -> Int64\n";
    output << "    switch xs\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_result_switch_with_maybe_variant_fixture(
    std::filesystem::path const& path,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "choice Result<T>\n";
    output << "    Ok(value: T)\n";
    output << "    Error\n";
    output << "function read(result: Result<Int64>) -> Int64\n";
    output << "    switch result\n";
    output << "        Some(value) => value\n";
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_envelope_result_switch_with_maybe_variant_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "choice Result<T>\n";
    output << "    Ok(value: T)\n";
    output << "    Error\n";
    output << "choice Envelope<T>\n";
    output << "    Wrap(inner: Result<T>)\n";
    output << "function read(env: Envelope<Int64>) -> Int64\n";
    output << "    switch env\n";
    output << "        Wrap(Some(value)) => value\n";
    output << "        default => 0\n";
}

void write_flag_switch_with_maybe_same_name_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "choice Flag\n";
    output << "    Some\n";
    output << "function read(flag: Flag) -> Int64\n";
    output << "    switch flag\n";
    output << "        Some => 1\n";
}

void write_envelope_pair_flag_switch_with_maybe_same_name_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "choice PairFlag\n";
    output << "    Some(left: Int64, right: Int64)\n";
    output << "choice Envelope\n";
    output << "    Wrap(inner: PairFlag)\n";
    output << "function read(env: Envelope) -> Int64\n";
    output << "    switch env\n";
    output << "        Wrap(Some(left, right)) => left\n";
    output << "        default => 0\n";
}

void write_maybe_raw_write_fixture(std::filesystem::path const& path, std::string_view maybe_payload_type) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "unsafe function write_word(maybe: Maybe<" << maybe_payload_type;
    output << ">, out: Pointer<UInt32>) -> Unit\n";
    output << "    switch maybe\n";
    output << "        Some(value) => raw_write(out, value)\n";
    output << "        default => return\n";
}

void write_maybe_unknown_constructor_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "function read(item: Maybe<Int64>) -> Int64\n";
    output << "    switch item\n";
    output << "        Missing(value) => value\n";
}

void write_nested_list_raw_write_fixture(std::filesystem::path const& path, std::string_view list_payload_type) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << "unsafe function write_next(xs: List<" << list_payload_type;
    output << ">, out: Pointer<UInt32>) -> Unit\n";
    output << "    switch xs\n";
    output << "        Node(head, Node(next, tail)) => raw_write(out, next)\n";
    output << "        default => return\n";
}

void write_nested_list_async_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
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

void write_list_async_head_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
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

void write_maybe_choice_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "function classify(item: Maybe<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 1\n";
    }
}

void write_maybe_int_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice MaybeInt\n";
    output << "    Some(value: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: MaybeInt) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 2\n";
    }
}

void write_value_then_constructor_pattern_mix_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice MaybeInt\n";
    output << "    Empty\n";
    output << "    Some(value: Int64)\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    output << "        true => 1\n";
    output << "        Some(value) => value\n";
    output << "        default => 0\n";
}

void write_bool_switch_text_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    output << "        \"ready\" => 1\n";
    output << "        default => 0\n";
}

void write_same_width_integer_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(value: UInt32) -> Int64\n";
    output << "    switch value\n";
    output << "        1 as Int32 => 1\n";
    output << "        default => 0\n";
}

void write_duplicate_bool_value_pattern_fixture(std::filesystem::path const& path, bool include_default = true) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    output << "        true => 1\n";
    output << "        true => 2\n";
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_duplicate_text_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(state: Text) -> Int64\n";
    output << "    switch state\n";
    output << "        \"ready\" => 1\n";
    output << "        \"ready\" => 2\n";
    output << "        default => 0\n";
}

void write_duplicate_integer_cast_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(value: UInt32) -> Int64\n";
    output << "    switch value\n";
    output << "        1 => 1\n";
    output << "        1 as Int32 => 2\n";
    output << "        default => 0\n";
}

void write_bool_value_pattern_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 2\n";
    }
}

void write_zero_payload_choice_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice IOError\n";
    output << "    Closed\n";
    output << "    EndOfInput\n";
    output << "    PermissionDenied\n";
    output << "function classify(error: IOError) -> Int64\n";
    output << "    switch error\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_boxed_maybe_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(value: T)\n";
    output << "    Blank\n";
    output << "function classify(item: Boxed<Maybe<Int64>>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 1\n";
    }
}

void write_pair_choice_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice PairChoice\n";
    output << "    Both(left: Int64, right: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: PairChoice) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 2\n";
    }
}

void write_number_choice_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice Number\n";
    output << "    Int(value: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: Number) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_multi_payload_choice_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice MultiPayload\n";
    output << "    First(value: Int64)\n";
    output << "    Second(value: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: MultiPayload) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_loop_control_fixture(
    std::filesystem::path const& path,
    std::string_view function_header,
    std::initializer_list<std::string_view> body_lines
) {
    std::ofstream output(path);
    output << "package demo.loops\n";
    output << "function " << function_header << "\n";
    for (auto line : body_lines) {
        output << "    " << line << "\n";
    }
}

void write_receiver_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package demo.receiver\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

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

void assert_wrap_duplicate_parse_failure(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find("switch constructor pattern 'Wrap(...)' is duplicated") != std::string::npos);
}

void assert_parse_success(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    assert(result.stdout_text.find("parsed ") != std::string::npos);
}

void assert_parse_failure_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_message) != std::string::npos);
}

void assert_parse_failure_contains_without(
    orison::driver::CompileResult const& result,
    std::string_view expected_message,
    std::string_view unexpected_message
) {
    assert_parse_failure_contains(result, expected_message);
    assert(result.stderr_text.find(unexpected_message) == std::string::npos);
}

}  // namespace

int main() {
    orison::driver::CompilerApp app;

    std::array<char const*, 2> version_argv {"orisonc", "--version"};
    auto result = app.run(std::span<char const* const>(version_argv.data(), version_argv.size()));

    assert(result.exit_code == 0);
    assert(result.stdout_text == "orisonc 0.1.0-dev\n");
    assert(result.stderr_text.empty());

    auto path = std::filesystem::temp_directory_path() / "orison_compiler_app_await_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "function fetch(url: Text) -> Outcome<Text, IOError>",
            "    return await request(url)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, path),
        "await expression is only valid inside async functions"
    );

    auto await_value_path = std::filesystem::temp_directory_path() / "orison_compiler_app_await_value_failure.or";
    write_concurrency_fixture(
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_await_non_async_call_failure.or";
    write_concurrency_fixture(
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_await_member_name_collision_failure.or";
    write_concurrency_fixture(
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_await_thread_value_failure.or";
    write_concurrency_fixture(
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_return_task_value_failure.or";
    write_concurrency_fixture(
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
        std::filesystem::temp_directory_path() / "orison_compiler_app_return_thread_value_failure.or";
    write_concurrency_fixture(
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

    auto unsafe_intrinsic_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_intrinsic_failure.or";
    write_concurrency_fixture(
        unsafe_intrinsic_failure_path,
        "demo.unsafe",
        {
            "function read_byte(p: Address) -> Byte",
            "    return raw_read(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, unsafe_intrinsic_failure_path),
        "unsafe intrinsic 'raw_read' is only valid inside unsafe functions or unsafe blocks"
    );

    auto unsafe_intrinsic_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_intrinsic_success.or";
    write_concurrency_fixture(
        unsafe_intrinsic_success_path,
        "demo.unsafe",
        {
            "function zero_byte(p: Address) -> Unit",
            "    unsafe",
            "        raw_write(p, 0)",
        }
    );
    assert_parse_success(run_parse(app, unsafe_intrinsic_success_path));

    auto address_of_shape_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_of_shape_failure.or";
    write_concurrency_fixture(
        address_of_shape_failure_path,
        "demo.unsafe",
        {
            "unsafe function pointer() -> Address",
            "    return address_of(1)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, address_of_shape_failure_path),
        "address_of currently requires an addressable storage operand"
    );

    auto raw_offset_shape_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_offset_shape_failure.or";
    write_concurrency_fixture(
        raw_offset_shape_failure_path,
        "demo.unsafe",
        {
            "unsafe function advance() -> Address",
            "    return raw_offset(1, 2)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_offset_shape_failure_path),
        "raw_offset currently requires an address-like first argument"
    );

    auto raw_offset_noninteger_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_offset_noninteger_failure.or";
    write_concurrency_fixture(
        raw_offset_noninteger_failure_path,
        "demo.unsafe",
        {
            "unsafe function advance(base: Address) -> Address",
            "    return raw_offset(base, \"one\")",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_offset_noninteger_failure_path),
        "raw_offset currently requires an integer offset argument"
    );

    auto nested_unsafe_operand_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_nested_unsafe_operand_success.or";
    write_concurrency_fixture(
        nested_unsafe_operand_success_path,
        "demo.unsafe",
        {
            "unsafe function poke(buf: exclusive Buffer, value: Byte) -> Unit",
            "    let p = address_of(buf.data[0])",
            "    raw_write(raw_offset(p, 1), value)",
        }
    );
    assert_parse_success(run_parse(app, nested_unsafe_operand_success_path));

    auto index_access_noninteger_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_index_access_noninteger_failure.or";
    write_concurrency_fixture(
        index_access_noninteger_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_byte(device: Device, value: Byte) -> Unit",
            "    raw_write(device.ptrs[\"one\"], value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, index_access_noninteger_failure_path),
        "index access currently requires an integer index expression"
    );

    auto index_access_integer_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_index_access_integer_success.or";
    write_concurrency_fixture(
        index_access_integer_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_byte(device: Device, index: UInt32, value: Byte) -> Unit",
            "    raw_write(device.ptrs[index], value)",
        }
    );
    assert_parse_success(run_parse(app, index_access_integer_success_path));

    auto unsafe_call_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_call_failure.or";
    write_concurrency_fixture(
        unsafe_call_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Address) -> UInt32",
            "    return raw_read(p)",
            "function read_twice(p: Address) -> UInt32",
            "    return read_word(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, unsafe_call_failure_path),
        "call to unsafe function 'read_word' is only valid inside unsafe functions or unsafe blocks"
    );

    auto unsafe_call_block_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_call_block_success.or";
    write_concurrency_fixture(
        unsafe_call_block_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Address) -> UInt32",
            "    return raw_read(p)",
            "function copy_word(p: Address) -> UInt32",
            "    unsafe",
            "        return read_word(p)",
        }
    );
    assert_parse_success(run_parse(app, unsafe_call_block_success_path));

    auto pointer_construction_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_failure.or";
    write_concurrency_fixture(
        pointer_construction_failure_path,
        "demo.unsafe",
        {
            "function read_byte(addr: Address) -> Byte",
            "    let p = Pointer(addr)",
            "    return raw_read(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_construction_failure_path),
        "Pointer construction is only valid inside unsafe functions or unsafe blocks"
    );

    auto pointer_construction_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_success.or";
    write_concurrency_fixture(
        pointer_construction_success_path,
        "demo.unsafe",
        {
            "function scribble(addr: Address) -> Unit",
            "    unsafe",
            "        let p = Pointer(addr)",
            "        raw_write(p, 0)",
        }
    );
    assert_parse_success(run_parse(app, pointer_construction_success_path));

    auto pointer_construction_addressof_typed_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_addressof_typed_failure.or";
    write_concurrency_fixture(
        pointer_construction_addressof_typed_failure_path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>",
            "    return Pointer(address_of(buf.data[0]))",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_construction_addressof_typed_failure_path),
        "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'"
    );

    auto pointer_construction_addressof_same_width_success_path = std::filesystem::temp_directory_path() /
                                                                  "orison_compiler_app_pointer_construction_addressof_same_width_success.or";
    write_concurrency_fixture(
        pointer_construction_addressof_same_width_success_path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function status_ptr(device: Device) -> Pointer<UInt32>",
            "    return Pointer(address_of(device.registers.status))",
        }
    );
    assert_parse_success(run_parse(app, pointer_construction_addressof_same_width_success_path));

    auto pointer_construction_noarg_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_noarg_failure.or";
    write_concurrency_fixture(
        pointer_construction_noarg_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p = Pointer()",
            "    return raw_read(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_construction_noarg_failure_path),
        "Pointer construction currently requires exactly one source argument"
    );

    auto pointer_construction_multiarg_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_multiarg_failure.or";
    write_concurrency_fixture(
        pointer_construction_multiarg_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_byte(addr: Address) -> Byte",
            "    let p = Pointer(addr, addr)",
            "    return raw_read(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_construction_multiarg_failure_path),
        "Pointer construction currently requires exactly one source argument"
    );

    auto pointer_construction_nonaddress_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_nonaddress_failure.or";
    write_concurrency_fixture(
        pointer_construction_nonaddress_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p = Pointer(\"text\")",
            "    return raw_read(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_construction_nonaddress_failure_path),
        "Pointer construction currently requires an address-like source argument"
    );

    auto pointer_construction_addressof_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_addressof_success.or";
    write_concurrency_fixture(
        pointer_construction_addressof_success_path,
        "demo.unsafe",
        {
            "unsafe function first_ptr(buf: exclusive Buffer) -> Pointer<Byte>",
            "    let p = Pointer(address_of(buf.data[0]))",
            "    return p",
        }
    );
    assert_parse_success(run_parse(app, pointer_construction_addressof_success_path));

    auto pointer_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_failure.or";
    write_concurrency_fixture(
        pointer_typed_binding_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p: Pointer<Byte> = \"text\"",
            "    return 0",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_typed_binding_failure_path),
        "pointer-typed binding initializer currently requires a structurally pointer-like expression"
    );

    auto pointer_typed_binding_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_name_failure.or";
    write_concurrency_fixture(
        pointer_typed_binding_name_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let source = \"text\"",
            "    let p: Pointer<Byte> = source",
            "    return 0",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_typed_binding_name_failure_path),
        "pointer-typed binding initializer currently requires a structurally pointer-like expression"
    );

    auto pointer_typed_binding_field_pointer_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_field_pointer_failure.or";
    write_concurrency_fixture(
        pointer_typed_binding_field_pointer_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Byte>",
            "unsafe function next_ptr(device: Device) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = device.ptr",
            "    return p",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_typed_binding_field_pointer_failure_path),
        "pointer-typed binding initializer pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );

    auto pointer_typed_binding_same_width_field_pointer_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_pointer_typed_binding_same_width_field_pointer_success.or";
    write_concurrency_fixture(
        pointer_typed_binding_same_width_field_pointer_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Int32>",
            "unsafe function next_ptr(device: Device) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = device.ptr",
            "    return p",
        }
    );
    assert_parse_success(run_parse(app, pointer_typed_binding_same_width_field_pointer_success_path));

    auto pointer_return_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_return_failure.or";
    write_concurrency_fixture(
        pointer_return_failure_path,
        "demo.unsafe",
        {
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    return \"text\"",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_return_failure_path),
        "pointer-returning function currently requires a structurally pointer-like expression"
    );

    auto pointer_return_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_return_name_failure.or";
    write_concurrency_fixture(
        pointer_return_name_failure_path,
        "demo.unsafe",
        {
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    let source = \"text\"",
            "    return source",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_return_name_failure_path),
        "pointer-returning function currently requires a structurally pointer-like expression"
    );

    auto pointer_return_helper_pointer_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_return_helper_pointer_failure.or";
    write_concurrency_fixture(
        pointer_return_helper_pointer_failure_path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return base",
            "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    return byte_ptr(base)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_return_helper_pointer_failure_path),
        "pointer-returning function pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );

    auto pointer_return_same_width_helper_pointer_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_pointer_return_same_width_helper_pointer_success.or";
    write_concurrency_fixture(
        pointer_return_same_width_helper_pointer_success_path,
        "demo.unsafe",
        {
            "unsafe function wordish_ptr(base: Pointer<Int32>) -> Pointer<Int32>",
            "    return base",
            "unsafe function word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>",
            "    return wordish_ptr(base)",
        }
    );
    assert_parse_success(run_parse(app, pointer_return_same_width_helper_pointer_success_path));

    auto raw_write_generic_helper_returned_pointer_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_helper_returned_pointer_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_helper_returned_pointer_same_width_success_path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Int32>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_helper_returned_pointer_same_width_success_path));

    auto raw_write_generic_helper_returned_pointer_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_helper_returned_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        raw_write_generic_helper_returned_pointer_mismatch_failure_path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_generic_helper_returned_pointer_mismatch_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_generic_receiver_method_pointer_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_receiver_method_pointer_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_receiver_method_pointer_same_width_success_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    id: Int64",
            "extend Device<T>",
            "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>",
            "        return base",
            "unsafe function write_word(device: Device<Int32>, base: Pointer<Int32>, value: UInt32) -> Unit",
            "    raw_write(device.ptr(base), value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_receiver_method_pointer_same_width_success_path));

    auto raw_write_generic_receiver_method_pointer_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_receiver_method_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        raw_write_generic_receiver_method_pointer_mismatch_failure_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    id: Int64",
            "extend Device<T>",
            "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>",
            "        return base",
            "unsafe function write_word(device: Device<Byte>, base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.ptr(base), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_generic_receiver_method_pointer_mismatch_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto pointer_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_success.or";
    write_concurrency_fixture(
        pointer_typed_binding_success_path,
        "demo.unsafe",
        {
            "unsafe function next_byte(base: Pointer<Byte>) -> Byte",
            "    let p: Pointer<Byte> = raw_offset(base, 1)",
            "    return raw_read(p)",
        }
    );
    assert_parse_success(run_parse(app, pointer_typed_binding_success_path));

    auto pointer_raw_offset_typed_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_rawoffset_typed_failure.or";
    write_concurrency_fixture(
        pointer_raw_offset_typed_failure_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, pointer_raw_offset_typed_failure_path),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );

    auto pointer_raw_offset_same_width_success_path = std::filesystem::temp_directory_path() /
                                                      "orison_compiler_app_pointer_rawoffset_same_width_success.or";
    write_concurrency_fixture(
        pointer_raw_offset_same_width_success_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
        }
    );
    assert_parse_success(run_parse(app, pointer_raw_offset_same_width_success_path));

    auto address_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_failure.or";
    write_concurrency_fixture(
        address_typed_binding_failure_path,
        "demo.unsafe",
        {
            "function read_base() -> Address",
            "    let base: Address = \"text\"",
            "    return 0x4000_1000",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, address_typed_binding_failure_path),
        "address-typed binding initializer currently requires a structurally address-like expression"
    );

    auto address_typed_binding_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_name_failure.or";
    write_concurrency_fixture(
        address_typed_binding_name_failure_path,
        "demo.unsafe",
        {
            "function read_base() -> Address",
            "    let source = \"text\"",
            "    let base: Address = source",
            "    return 0x4000_1000",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, address_typed_binding_name_failure_path),
        "address-typed binding initializer currently requires a structurally address-like expression"
    );

    auto address_return_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_failure.or";
    write_concurrency_fixture(
        address_return_failure_path,
        "demo.unsafe",
        {
            "function base() -> Address",
            "    return \"text\"",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, address_return_failure_path),
        "address-returning function currently requires a structurally address-like expression"
    );

    auto address_return_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_name_failure.or";
    write_concurrency_fixture(
        address_return_name_failure_path,
        "demo.unsafe",
        {
            "function base() -> Address",
            "    let source = \"text\"",
            "    return source",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, address_return_name_failure_path),
        "address-returning function currently requires a structurally address-like expression"
    );

    auto address_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_success.or";
    write_concurrency_fixture(
        address_typed_binding_success_path,
        "demo.unsafe",
        {
            "unsafe function first_addr(buf: exclusive Buffer) -> Address",
            "    let base: Address = address_of(buf.data[0])",
            "    return base",
        }
    );
    assert_parse_success(run_parse(app, address_typed_binding_success_path));

    auto address_typed_binding_field_address_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_field_address_success.or";
    write_concurrency_fixture(
        address_typed_binding_field_address_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    base: Address",
            "function read_base(device: Device) -> Address",
            "    let base: Address = device.base",
            "    return base",
        }
    );
    assert_parse_success(run_parse(app, address_typed_binding_field_address_success_path));

    auto address_typed_binding_indexed_address_success_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_address_typed_binding_indexed_address_success.or";
    write_concurrency_fixture(
        address_typed_binding_indexed_address_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "function read_base(device: Device, index: Int64) -> Address",
            "    let base: Address = device.bases[index]",
            "    return base",
        }
    );
    assert_parse_success(run_parse(app, address_typed_binding_indexed_address_success_path));

    auto address_return_helper_returned_address_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_helper_returned_address_success.or";
    write_concurrency_fixture(
        address_return_helper_returned_address_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function base_at(this: shared This, index: Int64) -> Address",
            "        let base = this.bases[index]",
            "        return base",
            "function read_base(device: Device, index: Int64) -> Address",
            "    return device.base_at(index)",
        }
    );
    assert_parse_success(run_parse(app, address_return_helper_returned_address_success_path));

    auto address_return_generic_helper_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_generic_helper_success.or";
    write_concurrency_fixture(
        address_return_generic_helper_success_path,
        "demo.unsafe",
        {
            "function id<T>(value: T) -> T",
            "    return value",
            "record Device",
            "    base: Address",
            "function read_base(device: Device) -> Address",
            "    return id(device.base)",
        }
    );
    assert_parse_success(run_parse(app, address_return_generic_helper_success_path));

    auto raw_write_generic_record_pointer_field_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_pointer_field_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_record_pointer_field_same_width_success_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Int32>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_record_pointer_field_same_width_success_path));

    auto raw_write_generic_record_pointer_field_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_pointer_field_mismatch_failure.or";
    write_concurrency_fixture(
        raw_write_generic_record_pointer_field_mismatch_failure_path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_generic_record_pointer_field_mismatch_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_generic_record_scalar_field_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_scalar_field_same_width_success.or";
    write_concurrency_fixture(
        raw_write_generic_record_scalar_field_same_width_success_path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "unsafe function write_word(box: Box<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, box.value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_generic_record_scalar_field_same_width_success_path));

    auto address_return_generic_record_field_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_address_return_generic_record_field_success.or";
    write_concurrency_fixture(
        address_return_generic_record_field_success_path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "function read_base(box: Box<Address>) -> Address",
            "    return box.value",
        }
    );
    assert_parse_success(run_parse(app, address_return_generic_record_field_success_path));

    auto raw_read_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_typed_binding_failure.or";
    write_concurrency_fixture(
        raw_read_typed_binding_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value: UInt32 = raw_read(p)",
            "    return value",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_read_typed_binding_failure_path),
        "raw_read result type 'Byte' does not match binding type 'UInt32'"
    );

    auto raw_read_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_typed_binding_success.or";
    write_concurrency_fixture(
        raw_read_typed_binding_success_path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value: Byte = raw_read(p)",
            "    return value",
        }
    );
    assert_parse_success(run_parse(app, raw_read_typed_binding_success_path));

    auto raw_read_same_width_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_same_width_binding_success.or";
    write_concurrency_fixture(
        raw_read_same_width_binding_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> Int32",
            "    let value: UInt32 = raw_read(p)",
            "    return value as Int32",
        }
    );
    assert_parse_success(run_parse(app, raw_read_same_width_binding_success_path));

    auto raw_read_return_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_return_type_failure.or";
    write_concurrency_fixture(
        raw_read_return_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_read(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_read_return_type_failure_path),
        "raw_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );

    auto raw_read_same_width_return_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_same_width_return_success.or";
    write_concurrency_fixture(
        raw_read_same_width_return_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return raw_read(p)",
        }
    );
    assert_parse_success(run_parse(app, raw_read_same_width_return_success_path));

    auto raw_write_value_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_value_type_failure.or";
    write_concurrency_fixture(
        raw_write_value_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_value_type_failure_path),
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto raw_write_same_width_integer_value_success_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_raw_write_same_width_integer_value_success.or";
    write_concurrency_fixture(
        raw_write_same_width_integer_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(p, value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_same_width_integer_value_success_path));

    auto raw_write_pointer_sized_integer_value_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_pointer_sized_integer_value_failure.or";
    write_concurrency_fixture(
        raw_write_pointer_sized_integer_value_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit",
            "    raw_write(p, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_pointer_sized_integer_value_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_computed_integer_sum_success_path = std::filesystem::temp_directory_path() /
                                                       "orison_compiler_app_raw_write_computed_integer_sum_success.or";
    write_concurrency_fixture(
        raw_write_computed_integer_sum_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(input) + 1)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_computed_integer_sum_success_path));

    auto raw_write_computed_bitwise_value_success_path = std::filesystem::temp_directory_path() /
                                                         "orison_compiler_app_raw_write_computed_bitwise_value_success.or";
    write_concurrency_fixture(
        raw_write_computed_bitwise_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(out, value bit_or 1)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_computed_bitwise_value_success_path));

    auto raw_write_computed_ternary_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                 "orison_compiler_app_raw_write_computed_ternary_pointer_sized_failure.or";
    write_concurrency_fixture(
        raw_write_computed_ternary_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    raw_write(out, flag ? left : right)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_computed_ternary_pointer_sized_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_rebound_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                         "orison_compiler_app_raw_write_rebound_computed_value_success.or";
    write_concurrency_fixture(
        raw_write_rebound_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    raw_write(out, masked)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_rebound_computed_value_success_path));

    auto raw_write_branch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                               "orison_compiler_app_raw_write_branch_merged_computed_value_success.or";
    write_concurrency_fixture(
        raw_write_branch_merged_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left bit_or 1",
            "    else",
            "        selected = right + 1",
            "    raw_write(out, selected)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_branch_merged_computed_value_success_path));

    auto raw_write_branch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_branch_merged_pointer_sized_failure.or";
    write_concurrency_fixture(
        raw_write_branch_merged_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left + 1",
            "    else",
            "        selected = right shift_left 1",
            "    raw_write(out, selected)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_branch_merged_pointer_sized_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_switch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                               "orison_compiler_app_raw_write_switch_merged_computed_value_success.or";
    write_concurrency_fixture(
        raw_write_switch_merged_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left bit_or 1",
            "        default => selected = right + 1",
            "    raw_write(out, selected)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_switch_merged_computed_value_success_path));

    auto raw_write_switch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_switch_merged_pointer_sized_failure.or";
    write_concurrency_fixture(
        raw_write_switch_merged_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left + 1",
            "        default => selected = right shift_left 1",
            "    raw_write(out, selected)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_switch_merged_pointer_sized_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_array_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                      "orison_compiler_app_raw_write_array_indexed_value_success.or";
    write_concurrency_fixture(
        raw_write_array_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    raw_write(out, items[0])",
        }
    );
    assert_parse_success(run_parse(app, raw_write_array_indexed_value_success_path));

    auto raw_write_bound_array_literal_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_raw_write_bound_array_literal_indexed_value_success.or";
    write_concurrency_fixture(
        raw_write_bound_array_literal_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    raw_write(out, staged[0])",
        }
    );
    assert_parse_success(run_parse(app, raw_write_bound_array_literal_indexed_value_success_path));

    auto raw_write_array_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_array_indexed_pointer_sized_failure.or";
    write_concurrency_fixture(
        raw_write_array_indexed_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit",
            "    raw_write(out, items[0])",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_array_indexed_pointer_sized_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_member_container_field_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                       "orison_compiler_app_raw_write_member_container_field_indexed_value_success.or";
    write_concurrency_fixture(
        raw_write_member_container_field_indexed_value_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words[0])",
        }
    );
    assert_parse_success(run_parse(app, raw_write_member_container_field_indexed_value_success_path));

    auto raw_write_nested_scalar_field_value_success_path = std::filesystem::temp_directory_path() /
                                                            "orison_compiler_app_raw_write_nested_scalar_field_value_success.or";
    write_concurrency_fixture(
        raw_write_nested_scalar_field_value_success_path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.registers.status)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_nested_scalar_field_value_success_path));

    auto raw_write_nested_scalar_field_computed_value_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_nested_scalar_field_computed_value_success.or";
    write_concurrency_fixture(
        raw_write_nested_scalar_field_computed_value_success_path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.registers.status bit_or 1)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_nested_scalar_field_computed_value_success_path));

    auto raw_write_helper_returned_container_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                          "orison_compiler_app_raw_write_helper_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        raw_write_helper_returned_container_indexed_value_success_path,
        "demo.unsafe",
        {
            "function words() -> DynamicArray<Int32>",
            "    return []",
            "unsafe function write_word(out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, words()[0])",
        }
    );
    assert_parse_success(run_parse(app, raw_write_helper_returned_container_indexed_value_success_path));

    auto raw_write_member_container_field_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                               "orison_compiler_app_raw_write_member_container_field_indexed_pointer_sized_failure.or";
    write_concurrency_fixture(
        raw_write_member_container_field_indexed_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<IntSize>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words[0])",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_member_container_field_indexed_pointer_sized_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_nested_scalar_field_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_nested_scalar_field_pointer_sized_failure.or";
    write_concurrency_fixture(
        raw_write_nested_scalar_field_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: IntSize",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.registers.status)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_nested_scalar_field_pointer_sized_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_integer_literal_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_integer_literal_success.or";
    write_concurrency_fixture(
        raw_write_integer_literal_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    raw_write(p, 0)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_integer_literal_success_path));

    auto raw_write_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_integer_cast_success.or";
    write_concurrency_fixture(
        raw_write_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as UInt32)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_integer_cast_success_path));

    auto raw_write_same_width_integer_cast_success_path = std::filesystem::temp_directory_path() /
                                                          "orison_compiler_app_raw_write_same_width_integer_cast_success.or";
    write_concurrency_fixture(
        raw_write_same_width_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as Int32)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_same_width_integer_cast_success_path));

    auto raw_write_integer_cast_target_failure_path = std::filesystem::temp_directory_path() /
                                                      "orison_compiler_app_raw_write_integer_cast_target_failure.or";
    write_concurrency_fixture(
        raw_write_integer_cast_target_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as Int64)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_integer_cast_target_failure_path),
        "raw_write value type 'Int64' does not match pointer element type 'UInt32'"
    );

    auto raw_write_pointer_sized_integer_cast_failure_path = std::filesystem::temp_directory_path() /
                                                             "orison_compiler_app_raw_write_pointer_sized_integer_cast_failure.or";
    write_concurrency_fixture(
        raw_write_pointer_sized_integer_cast_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as IntSize)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_pointer_sized_integer_cast_failure_path),
        "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto raw_write_recovered_raw_read_failure_path = std::filesystem::temp_directory_path() /
                                                     "orison_compiler_app_raw_write_recovered_raw_read_failure.or";
    write_concurrency_fixture(
        raw_write_recovered_raw_read_failure_path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_recovered_raw_read_failure_path),
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto raw_write_member_returned_raw_read_failure_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_raw_write_member_returned_raw_read_failure.or";
    write_concurrency_fixture(
        raw_write_member_returned_raw_read_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(device.byte_ptr(base), 1)))",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_member_returned_raw_read_failure_path),
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto raw_write_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_helper_type_failure.or";
    write_concurrency_fixture(
        raw_write_helper_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>",
            "    return Pointer(addr)",
            "unsafe function write_word(addr: Address, value: UInt32) -> Unit",
            "    raw_write(byte_ptr(addr), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_helper_type_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_member_helper_type_failure_path = std::filesystem::temp_directory_path() /
                                                     "orison_compiler_app_raw_write_member_helper_type_failure.or";
    write_concurrency_fixture(
        raw_write_member_helper_type_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, addr: Address) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(addr)",
            "unsafe function write_word(device: Device, addr: Address, value: UInt32) -> Unit",
            "    raw_write(device.byte_ptr(addr), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_member_helper_type_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_raw_offset_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_raw_offset_helper_type_failure.or";
    write_concurrency_fixture(
        raw_write_raw_offset_helper_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(next_byte_ptr(base), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_raw_offset_helper_type_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_member_raw_offset_helper_type_failure_path = std::filesystem::temp_directory_path() /
                                                                "orison_compiler_app_raw_write_member_raw_offset_helper_type_failure.or";
    write_concurrency_fixture(
        raw_write_member_raw_offset_helper_type_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function next_byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.next_byte_ptr(base), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_member_raw_offset_helper_type_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto raw_write_record_pointer_field_type_failure_path = std::filesystem::temp_directory_path() /
                                                            "orison_compiler_app_raw_write_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        raw_write_record_pointer_field_type_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Byte>",
            "unsafe function write_word(device: Device, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_record_pointer_field_type_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto member_field_address_pointer_constructor_success_path = std::filesystem::temp_directory_path() /
                                                                 "orison_compiler_app_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        member_field_address_pointer_constructor_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    base: Address",
            "extend Device",
            "    function byte_ptr(this: shared This) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.base)",
            "unsafe function write_byte(device: Device, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(), value)",
        }
    );
    assert_parse_success(run_parse(app, member_field_address_pointer_constructor_success_path));

    auto raw_write_indexed_record_pointer_field_type_failure_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_raw_write_indexed_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        raw_write_indexed_record_pointer_field_type_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit",
            "    raw_write(device.ptrs[index], value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, raw_write_indexed_record_pointer_field_type_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto indexed_member_field_address_pointer_constructor_success_path = std::filesystem::temp_directory_path() /
                                                                         "orison_compiler_app_indexed_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        indexed_member_field_address_pointer_constructor_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.bases[index])",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );
    assert_parse_success(run_parse(app, indexed_member_field_address_pointer_constructor_success_path));

    auto rebound_indexed_record_pointer_field_type_failure_path = std::filesystem::temp_directory_path() /
                                                                  "orison_compiler_app_rebound_indexed_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        rebound_indexed_record_pointer_field_type_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit",
            "    let p = device.ptrs[index]",
            "    raw_write(p, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, rebound_indexed_record_pointer_field_type_failure_path),
        "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
    );

    auto rebound_indexed_member_field_address_pointer_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_rebound_indexed_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        rebound_indexed_member_field_address_pointer_constructor_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            let base = this.bases[index]",
            "            return Pointer(base)",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );
    assert_parse_success(run_parse(app, rebound_indexed_member_field_address_pointer_constructor_success_path));

    auto return_rebound_indexed_record_pointer_field_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_return_rebound_indexed_record_pointer_field_success.or";
    write_concurrency_fixture(
        return_rebound_indexed_record_pointer_field_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>",
            "    let p = device.ptrs[index]",
            "    return p",
        }
    );
    assert_parse_success(run_parse(app, return_rebound_indexed_record_pointer_field_success_path));

    auto return_rebound_indexed_member_field_address_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_return_rebound_indexed_member_field_address_success.or";
    write_concurrency_fixture(
        return_rebound_indexed_member_field_address_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function base_at(this: shared This, index: Int64) -> Address",
            "        let base = this.bases[index]",
            "        return base",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.base_at(index))",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );
    assert_parse_success(run_parse(app, return_rebound_indexed_member_field_address_success_path));

    auto return_rebound_indexed_pointer_used_by_helper_success_path = std::filesystem::temp_directory_path() /
                                                                      "orison_compiler_app_return_rebound_indexed_pointer_used_by_helper_success.or";
    write_concurrency_fixture(
        return_rebound_indexed_pointer_used_by_helper_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>",
            "    let p = device.ptrs[index]",
            "    return p",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(byte_ptr(device, index), value)",
        }
    );
    assert_parse_success(run_parse(app, return_rebound_indexed_pointer_used_by_helper_success_path));

    auto return_rebound_indexed_address_pointer_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_return_rebound_indexed_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        return_rebound_indexed_address_pointer_constructor_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function base_at(this: shared This, index: Int64) -> Address",
            "        let base = this.bases[index]",
            "        return base",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.base_at(index))",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );
    assert_parse_success(run_parse(app, return_rebound_indexed_address_pointer_constructor_success_path));

    auto volatile_read_return_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_return_type_failure.or";
    write_concurrency_fixture(
        volatile_read_return_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    return volatile_read(p)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_read_return_type_failure_path),
        "volatile_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );

    auto volatile_read_same_width_return_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_same_width_return_success.or";
    write_concurrency_fixture(
        volatile_read_same_width_return_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return volatile_read(p)",
        }
    );
    assert_parse_success(run_parse(app, volatile_read_same_width_return_success_path));

    auto volatile_read_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_typed_binding_failure.or";
    write_concurrency_fixture(
        volatile_read_typed_binding_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value: UInt32 = volatile_read(p)",
            "    return value",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_read_typed_binding_failure_path),
        "volatile_read result type 'Byte' does not match binding type 'UInt32'"
    );

    auto volatile_read_same_width_binding_success_path = std::filesystem::temp_directory_path() /
                                                         "orison_compiler_app_volatile_read_same_width_binding_success.or";
    write_concurrency_fixture(
        volatile_read_same_width_binding_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> Int32",
            "    let value: UInt32 = volatile_read(p)",
            "    return value as Int32",
        }
    );
    assert_parse_success(run_parse(app, volatile_read_same_width_binding_success_path));

    auto volatile_read_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_typed_binding_success.or";
    write_concurrency_fixture(
        volatile_read_typed_binding_success_path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value: Byte = volatile_read(p)",
            "    return value",
        }
    );
    assert_parse_success(run_parse(app, volatile_read_typed_binding_success_path));

    auto volatile_write_value_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_value_type_failure.or";
    write_concurrency_fixture(
        volatile_write_value_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_value_type_failure_path),
        "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_same_width_integer_value_success_path = std::filesystem::temp_directory_path() /
                                                                "orison_compiler_app_volatile_write_same_width_integer_value_success.or";
    write_concurrency_fixture(
        volatile_write_same_width_integer_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(p, value)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_same_width_integer_value_success_path));

    auto volatile_write_pointer_sized_integer_value_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_pointer_sized_integer_value_failure.or";
    write_concurrency_fixture(
        volatile_write_pointer_sized_integer_value_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit",
            "    volatile_write(p, value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_pointer_sized_integer_value_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_computed_integer_sum_success_path = std::filesystem::temp_directory_path() /
                                                            "orison_compiler_app_volatile_write_computed_integer_sum_success.or";
    write_concurrency_fixture(
        volatile_write_computed_integer_sum_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(input) + 1)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_computed_integer_sum_success_path));

    auto volatile_write_computed_bitwise_value_success_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_volatile_write_computed_bitwise_value_success.or";
    write_concurrency_fixture(
        volatile_write_computed_bitwise_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(out, value bit_or 1)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_computed_bitwise_value_success_path));

    auto volatile_write_computed_ternary_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                      "orison_compiler_app_volatile_write_computed_ternary_pointer_sized_failure.or";
    write_concurrency_fixture(
        volatile_write_computed_ternary_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    volatile_write(out, flag ? left : right)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_computed_ternary_pointer_sized_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_rebound_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_volatile_write_rebound_computed_value_success.or";
    write_concurrency_fixture(
        volatile_write_rebound_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    volatile_write(out, masked)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_rebound_computed_value_success_path));

    auto volatile_write_branch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_volatile_write_branch_merged_computed_value_success.or";
    write_concurrency_fixture(
        volatile_write_branch_merged_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left bit_or 1",
            "    else",
            "        selected = right + 1",
            "    volatile_write(out, selected)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_branch_merged_computed_value_success_path));

    auto volatile_write_branch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_branch_merged_pointer_sized_failure.or";
    write_concurrency_fixture(
        volatile_write_branch_merged_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left + 1",
            "    else",
            "        selected = right shift_left 1",
            "    volatile_write(out, selected)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_branch_merged_pointer_sized_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_switch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_volatile_write_switch_merged_computed_value_success.or";
    write_concurrency_fixture(
        volatile_write_switch_merged_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left bit_or 1",
            "        default => selected = right + 1",
            "    volatile_write(out, selected)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_switch_merged_computed_value_success_path));

    auto volatile_write_switch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_switch_merged_pointer_sized_failure.or";
    write_concurrency_fixture(
        volatile_write_switch_merged_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left + 1",
            "        default => selected = right shift_left 1",
            "    volatile_write(out, selected)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_switch_merged_pointer_sized_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_array_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_volatile_write_array_indexed_value_success.or";
    write_concurrency_fixture(
        volatile_write_array_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    volatile_write(out, items[0])",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_array_indexed_value_success_path));

    auto volatile_write_bound_array_literal_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                         "orison_compiler_app_volatile_write_bound_array_literal_indexed_value_success.or";
    write_concurrency_fixture(
        volatile_write_bound_array_literal_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    volatile_write(out, staged[0])",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_bound_array_literal_indexed_value_success_path));

    auto volatile_write_array_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_array_indexed_pointer_sized_failure.or";
    write_concurrency_fixture(
        volatile_write_array_indexed_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit",
            "    volatile_write(out, items[0])",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_array_indexed_pointer_sized_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_member_container_field_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                            "orison_compiler_app_volatile_write_member_container_field_indexed_value_success.or";
    write_concurrency_fixture(
        volatile_write_member_container_field_indexed_value_success_path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words[0])",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_member_container_field_indexed_value_success_path));

    auto volatile_write_nested_scalar_field_value_success_path = std::filesystem::temp_directory_path() /
                                                                 "orison_compiler_app_volatile_write_nested_scalar_field_value_success.or";
    write_concurrency_fixture(
        volatile_write_nested_scalar_field_value_success_path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_nested_scalar_field_value_success_path));

    auto volatile_write_nested_scalar_field_computed_value_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_volatile_write_nested_scalar_field_computed_value_success.or";
    write_concurrency_fixture(
        volatile_write_nested_scalar_field_computed_value_success_path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status bit_or 1)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_nested_scalar_field_computed_value_success_path));

    auto volatile_write_helper_returned_container_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                               "orison_compiler_app_volatile_write_helper_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        volatile_write_helper_returned_container_indexed_value_success_path,
        "demo.unsafe",
        {
            "function words() -> DynamicArray<Int32>",
            "    return []",
            "unsafe function write_word(out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, words()[0])",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_helper_returned_container_indexed_value_success_path));

    auto volatile_write_member_container_field_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                                    "orison_compiler_app_volatile_write_member_container_field_indexed_pointer_sized_failure.or";
    write_concurrency_fixture(
        volatile_write_member_container_field_indexed_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<IntSize>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words[0])",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_member_container_field_indexed_pointer_sized_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_nested_scalar_field_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_volatile_write_nested_scalar_field_pointer_sized_failure.or";
    write_concurrency_fixture(
        volatile_write_nested_scalar_field_pointer_sized_failure_path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: IntSize",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_nested_scalar_field_pointer_sized_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_integer_literal_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_integer_literal_success.or";
    write_concurrency_fixture(
        volatile_write_integer_literal_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    volatile_write(p, 0)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_integer_literal_success_path));

    auto volatile_write_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_integer_cast_success.or";
    write_concurrency_fixture(
        volatile_write_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as UInt32)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_integer_cast_success_path));

    auto volatile_write_same_width_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_same_width_integer_cast_success.or";
    write_concurrency_fixture(
        volatile_write_same_width_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as Int32)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_same_width_integer_cast_success_path));

    auto volatile_write_integer_cast_target_failure_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_volatile_write_integer_cast_target_failure.or";
    write_concurrency_fixture(
        volatile_write_integer_cast_target_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as Int64)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_integer_cast_target_failure_path),
        "volatile_write value type 'Int64' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_pointer_sized_integer_cast_failure_path = std::filesystem::temp_directory_path() /
                                                                  "orison_compiler_app_volatile_write_pointer_sized_integer_cast_failure.or";
    write_concurrency_fixture(
        volatile_write_pointer_sized_integer_cast_failure_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as IntSize)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_pointer_sized_integer_cast_failure_path),
        "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_recovered_volatile_read_failure_path = std::filesystem::temp_directory_path() /
                                                               "orison_compiler_app_volatile_write_recovered_volatile_read_failure.or";
    write_concurrency_fixture(
        volatile_write_recovered_volatile_read_failure_path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_recovered_volatile_read_failure_path),
        "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_member_returned_volatile_read_failure_path = std::filesystem::temp_directory_path() /
                                                                     "orison_compiler_app_volatile_write_member_returned_volatile_read_failure.or";
    write_concurrency_fixture(
        volatile_write_member_returned_volatile_read_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)))",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_member_returned_volatile_read_failure_path),
        "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_helper_type_failure.or";
    write_concurrency_fixture(
        volatile_write_helper_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function word_ptr(addr: Address) -> Pointer<UInt32>",
            "    return Pointer(addr)",
            "unsafe function write_word(addr: Address, value: Byte) -> Unit",
            "    volatile_write(word_ptr(addr), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_helper_type_failure_path),
        "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_member_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_member_helper_type_failure.or";
    write_concurrency_fixture(
        volatile_write_member_helper_type_failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, addr: Address) -> Pointer<UInt32>",
            "        unsafe",
            "            return Pointer(addr)",
            "unsafe function write_word(device: Device, addr: Address, value: Byte) -> Unit",
            "    volatile_write(device.word_ptr(addr), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_member_helper_type_failure_path),
        "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto volatile_write_raw_offset_helper_type_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_volatile_write_raw_offset_helper_type_failure.or";
    write_concurrency_fixture(
        volatile_write_raw_offset_helper_type_failure_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(next_word_ptr(base), value)",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, volatile_write_raw_offset_helper_type_failure_path),
        "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto task_path = std::filesystem::temp_directory_path() / "orison_compiler_app_task_failure.or";
    write_concurrency_fixture(
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

    auto assignment_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_assignment_async_origin_success.or";
    write_concurrency_fixture(
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

    auto ternary_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_ternary_async_origin_success.or";
    write_concurrency_fixture(
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

    auto ternary_thread_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_ternary_thread_origin_failure.or";
    write_concurrency_fixture(
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

    auto if_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_if_async_origin_success.or";
    write_concurrency_fixture(
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

    auto while_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_while_async_origin_success.or";
    write_concurrency_fixture(
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

    auto for_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_for_async_origin_success.or";
    write_concurrency_fixture(
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

    auto guard_async_origin_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_guard_async_origin_success.or";
    write_concurrency_fixture(
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

    auto switch_constructor_pattern_binding_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_constructor_pattern_binding_success.or";
    write_list_async_head_capture_fixture(switch_constructor_pattern_binding_path);

    assert_parse_success(run_parse(app, switch_constructor_pattern_binding_path));

    auto switch_nested_constructor_pattern_binding_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_constructor_pattern_success.or";
    write_nested_list_async_capture_fixture(switch_nested_constructor_pattern_binding_path);

    assert_parse_success(run_parse(app, switch_nested_constructor_pattern_binding_path));

    auto switch_nested_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_payload_overlap_failure_path,
        {"Wrap(Some(value)) => 1", "Wrap(Some(other)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_nested_payload_overlap_failure_path));

    auto switch_nested_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_literal_payload_overlap_failure_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(1)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_nested_literal_payload_overlap_failure_path));

    auto switch_disjoint_nested_literal_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_disjoint_nested_literal_payload_success.or";
    write_boxed_maybe_switch_fixture(
        switch_disjoint_nested_literal_payload_success_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(2)) => 2"}
    );
    assert_parse_success(run_parse(app, switch_disjoint_nested_literal_payload_success_path));

    auto switch_nested_wildcard_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_wildcard_literal_payload_overlap_failure_path,
        {"Wrap(Some(value)) => 1", "Wrap(Some(1)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_nested_wildcard_literal_payload_overlap_failure_path)
    );

    auto switch_nested_literal_wildcard_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_literal_wildcard_payload_overlap_failure_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(value)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_nested_literal_wildcard_payload_overlap_failure_path)
    );

    auto switch_nested_multi_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_multi_payload_overlap_failure.or";
    write_boxed_pair_maybe_switch_fixture(
        switch_nested_multi_payload_overlap_failure_path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 1)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_nested_multi_payload_overlap_failure_path));

    auto switch_disjoint_nested_multi_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_disjoint_nested_multi_payload_success.or";
    write_boxed_pair_maybe_switch_fixture(
        switch_disjoint_nested_multi_payload_success_path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 2)) => 2"}
    );
    assert_parse_success(run_parse(app, switch_disjoint_nested_multi_payload_success_path));

    auto switch_mismatched_nested_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_mismatched_nested_constructor_success.or";
    write_boxed_maybe_switch_fixture(
        switch_mismatched_nested_constructor_success_path,
        {"Wrap(Some(value)) => 1", "Wrap(Empty) => 2"}
    );
    assert_parse_success(run_parse(app, switch_mismatched_nested_constructor_success_path));

    auto switch_duplicate_nested_zero_payload_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_nested_zero_payload_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_duplicate_nested_zero_payload_failure_path,
        {"Wrap(Empty) => 1", "Wrap(Empty) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_duplicate_nested_zero_payload_failure_path));

    auto switch_duplicate_nested_zero_payload_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_nested_zero_payload_no_cascade_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_duplicate_nested_zero_payload_no_cascade_failure_path,
        {"Wrap(Empty) => 1", "Wrap(Empty) => 2"},
        false
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_duplicate_nested_zero_payload_no_cascade_failure_path)
    );

    auto switch_deep_nested_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_deep_nested_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_payload_overlap_failure_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(other))) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_deep_nested_payload_overlap_failure_path));

    auto switch_disjoint_deep_nested_literal_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_disjoint_deep_nested_literal_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_disjoint_deep_nested_literal_payload_success_path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(2))) => 2"}
    );
    assert_parse_success(run_parse(app, switch_disjoint_deep_nested_literal_payload_success_path));

    auto switch_deep_nested_wildcard_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_deep_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_wildcard_literal_payload_overlap_failure_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(1))) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_deep_nested_wildcard_literal_payload_overlap_failure_path)
    );

    auto switch_deep_nested_literal_wildcard_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_deep_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_literal_wildcard_payload_overlap_failure_path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(value))) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_deep_nested_literal_wildcard_payload_overlap_failure_path)
    );

    auto switch_mismatched_deep_nested_zero_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_mismatched_deep_nested_zero_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_mismatched_deep_nested_zero_payload_success_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Empty)) => 2"}
    );
    assert_parse_success(run_parse(app, switch_mismatched_deep_nested_zero_payload_success_path));

    auto switch_duplicate_deep_nested_zero_payload_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_deep_nested_zero_payload_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_duplicate_deep_nested_zero_payload_failure_path,
        {"Wrap(Hold(Empty)) => 1", "Wrap(Hold(Empty)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_duplicate_deep_nested_zero_payload_failure_path));

    auto switch_nested_wrapped_payload_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_wrapped_payload_success.or";
    write_nested_list_raw_write_fixture(switch_nested_wrapped_payload_success_path, "Int32");

    assert_parse_success(run_parse(app, switch_nested_wrapped_payload_success_path));

    auto switch_nested_wrapped_payload_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_wrapped_payload_failure.or";
    write_nested_list_raw_write_fixture(switch_nested_wrapped_payload_failure_path, "Byte");

    assert_parse_failure_contains(
        run_parse(app, switch_nested_wrapped_payload_failure_path),
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto switch_generic_constructor_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_generic_constructor_payload_success.or";
    write_maybe_raw_write_fixture(switch_generic_constructor_payload_success_path, "Int32");

    assert_parse_success(run_parse(app, switch_generic_constructor_payload_success_path));

    auto switch_generic_constructor_payload_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_generic_constructor_payload_failure.or";
    write_maybe_raw_write_fixture(switch_generic_constructor_payload_failure_path, "Byte");

    assert_parse_failure_contains(
        run_parse(app, switch_generic_constructor_payload_failure_path),
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto switch_wrong_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_wrong_choice_variant_failure.or";
    write_result_switch_with_maybe_variant_fixture(switch_wrong_choice_variant_failure_path);

    assert_parse_failure_contains(
        run_parse(app, switch_wrong_choice_variant_failure_path),
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );

    auto switch_wrong_choice_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_wrong_choice_without_default_no_cascade_failure.or";
    write_result_switch_with_maybe_variant_fixture(
        switch_wrong_choice_without_default_no_cascade_failure_path,
        false
    );

    auto switch_wrong_choice_without_default_no_cascade_failure_result =
        run_parse(app, switch_wrong_choice_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_wrong_choice_without_default_no_cascade_failure_result,
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'",
        "switch is missing"
    );

    auto switch_subject_specific_arity_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_subject_specific_arity_success.or";
    write_flag_switch_with_maybe_same_name_fixture(switch_subject_specific_arity_success_path);

    assert_parse_success(run_parse(app, switch_subject_specific_arity_success_path));

    auto switch_nested_wrong_payload_choice_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_wrong_payload_choice_failure.or";
    write_envelope_result_switch_with_maybe_variant_fixture(switch_nested_wrong_payload_choice_failure_path);

    assert_parse_failure_contains(
        run_parse(app, switch_nested_wrong_payload_choice_failure_path),
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );

    auto switch_nested_payload_specific_arity_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_payload_specific_arity_success.or";
    write_envelope_pair_flag_switch_with_maybe_same_name_fixture(
        switch_nested_payload_specific_arity_success_path
    );

    assert_parse_success(run_parse(app, switch_nested_payload_specific_arity_success_path));

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
               "switch constructor pattern 'head' does not match any declared choice variant"
           ) != std::string::npos);

    auto switch_call_pattern_unknown_variant_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_call_pattern_unknown_variant_failure.or";
    {
        std::ofstream output(switch_call_pattern_unknown_variant_failure_path);
        output << "package demo.patterns\n";
        output << "async function read(value: Int64) -> Int64\n";
        output << "    switch value\n";
        output << "        Missing(head) => 0\n";
        output << "        default => 0\n";
    }

    auto switch_call_pattern_unknown_variant_failure_path_text =
        switch_call_pattern_unknown_variant_failure_path.string();
    std::array<char const*, 3> switch_call_pattern_unknown_variant_failure_argv {
        "orisonc",
        "--parse",
        switch_call_pattern_unknown_variant_failure_path_text.c_str()
    };
    auto switch_call_pattern_unknown_variant_failure_result = app.run(
        std::span<char const* const>(
            switch_call_pattern_unknown_variant_failure_argv.data(),
            switch_call_pattern_unknown_variant_failure_argv.size()
        )
    );

    assert(switch_call_pattern_unknown_variant_failure_result.exit_code == 1);
    assert(switch_call_pattern_unknown_variant_failure_result.stdout_text.empty());
    assert(switch_call_pattern_unknown_variant_failure_result.stderr_text.find(
               "switch constructor pattern 'Missing' does not match any declared choice variant"
           ) != std::string::npos);

    auto switch_unknown_constructor_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_unknown_constructor_without_default_no_cascade_failure.or";
    write_maybe_unknown_constructor_fixture(switch_unknown_constructor_without_default_no_cascade_failure_path);

    auto switch_unknown_constructor_without_default_no_cascade_failure_result =
        run_parse(app, switch_unknown_constructor_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_unknown_constructor_without_default_no_cascade_failure_result,
        "switch constructor pattern 'Missing' does not match any declared choice variant",
        "switch is missing"
    );

    auto switch_nested_constructor_pattern_shape_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_constructor_pattern_shape_failure.or";
    write_list_switch_fixture(switch_nested_constructor_pattern_shape_failure_path, {"Node(head + 1, tail) => 0"}, true);

    assert_parse_failure_contains(
        run_parse(app, switch_nested_constructor_pattern_shape_failure_path),
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
    );

    auto switch_constructor_payload_shape_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_payload_shape_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_constructor_payload_shape_without_default_no_cascade_failure_path,
        {"Node(head + 1, tail) => 0"},
        false,
        false
    );

    auto switch_constructor_payload_shape_without_default_no_cascade_failure_result =
        run_parse(app, switch_constructor_payload_shape_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_constructor_payload_shape_without_default_no_cascade_failure_result,
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern",
        "switch is missing"
    );

    auto switch_constructor_pattern_duplicate_binding_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_duplicate_binding_failure.or";
    write_list_switch_fixture(switch_constructor_pattern_duplicate_binding_failure_path, {"Node(head, head) => 0"}, true);

    assert_parse_failure_contains(
        run_parse(app, switch_constructor_pattern_duplicate_binding_failure_path),
        "switch constructor pattern cannot bind 'head' more than once"
    );

    auto switch_constructor_duplicate_binding_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_constructor_duplicate_binding_without_default_no_cascade_failure_path,
        {"Node(head, head) => 0"},
        false,
        false
    );

    auto switch_constructor_duplicate_binding_without_default_no_cascade_failure_result =
        run_parse(app, switch_constructor_duplicate_binding_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_constructor_duplicate_binding_without_default_no_cascade_failure_result,
        "switch constructor pattern cannot bind 'head' more than once",
        "switch is missing"
    );

    auto switch_nested_constructor_pattern_duplicate_binding_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_constructor_pattern_duplicate_binding_failure.or";
    write_list_switch_fixture(
        switch_nested_constructor_pattern_duplicate_binding_failure_path,
        {"Node(head, Node(head, tail)) => 0"},
        true
    );

    assert_parse_failure_contains(
        run_parse(app, switch_nested_constructor_pattern_duplicate_binding_failure_path),
        "switch constructor pattern cannot bind 'head' more than once"
    );

    auto switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path,
        {"Node(head, Node(head, tail)) => 0"},
        false,
        false
    );

    auto switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_result =
        run_parse(app, switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_result,
        "switch constructor pattern cannot bind 'head' more than once",
        "switch is missing"
    );

    auto switch_constructor_pattern_arity_missing_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_arity_missing_failure.or";
    write_list_switch_fixture(switch_constructor_pattern_arity_missing_failure_path, {"Node(head) => 0"}, true);

    assert_parse_failure_contains(
        run_parse(app, switch_constructor_pattern_arity_missing_failure_path),
        "switch constructor pattern 'Node' expects 2 payload values but received 1"
    );

    auto switch_constructor_pattern_arity_extra_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_arity_extra_failure.or";
    write_list_switch_fixture(switch_constructor_pattern_arity_extra_failure_path, {"Empty(value) => 0"}, true);

    assert_parse_failure_contains(
        run_parse(app, switch_constructor_pattern_arity_extra_failure_path),
        "switch constructor pattern 'Empty' expects 0 payload values but received 1"
    );

    auto switch_constructor_pattern_arity_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_constructor_pattern_arity_without_default_no_cascade_failure_path,
        {"Node(head) => 0"},
        false,
        false
    );

    auto switch_constructor_pattern_arity_without_default_no_cascade_failure_result =
        run_parse(app, switch_constructor_pattern_arity_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_constructor_pattern_arity_without_default_no_cascade_failure_result,
        "switch constructor pattern 'Node' expects 2 payload values but received 1",
        "switch is missing"
    );

    auto switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_zero_payload_constructor_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path,
        {"Empty(value) => 0"},
        false,
        false
    );

    auto switch_zero_payload_constructor_arity_without_default_no_cascade_failure_result =
        run_parse(app, switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_zero_payload_constructor_arity_without_default_no_cascade_failure_result,
        "switch constructor pattern 'Empty' expects 0 payload values but received 1",
        "switch is missing"
    );

    auto switch_pattern_mix_constructor_value_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_pattern_mix_constructor_value_failure.or";
    write_list_switch_fixture(switch_pattern_mix_constructor_value_failure_path, {"Empty => 0", "1 => 1"});

    assert_parse_failure_contains(
        run_parse(app, switch_pattern_mix_constructor_value_failure_path),
        "switch cannot mix value patterns with constructor patterns"
    );

    auto switch_pattern_mix_value_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_pattern_mix_value_constructor_failure.or";
    write_value_then_constructor_pattern_mix_fixture(switch_pattern_mix_value_constructor_failure_path);

    assert_parse_failure_contains(
        run_parse(app, switch_pattern_mix_value_constructor_failure_path),
        "switch cannot mix value patterns with constructor patterns"
    );

    auto switch_pattern_mix_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_pattern_mix_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_pattern_mix_without_default_no_cascade_failure_path,
        {"Some(value) => value", "1 => 1"}
    );

    auto switch_pattern_mix_without_default_no_cascade_failure_result =
        run_parse(app, switch_pattern_mix_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_pattern_mix_without_default_no_cascade_failure_result,
        "switch cannot mix value patterns with constructor patterns",
        "switch is missing choice variant"
    );

    auto switch_value_pattern_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_value_pattern_type_failure.or";
    write_bool_switch_text_value_pattern_fixture(switch_value_pattern_type_failure_path);

    assert_parse_failure_contains(
        run_parse(app, switch_value_pattern_type_failure_path),
        "switch value pattern type 'Text' does not match switched expression type 'Bool'"
    );

    auto switch_value_pattern_same_width_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_value_pattern_same_width_success.or";
    write_same_width_integer_value_pattern_fixture(switch_value_pattern_same_width_success_path);

    assert_parse_success(run_parse(app, switch_value_pattern_same_width_success_path));

    auto switch_duplicate_boolean_value_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_boolean_value_failure.or";
    write_duplicate_bool_value_pattern_fixture(switch_duplicate_boolean_value_failure_path);

    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_boolean_value_failure_path),
        "switch value pattern 'true' is duplicated"
    );

    auto switch_duplicate_bool_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_bool_without_default_no_cascade_failure.or";
    write_duplicate_bool_value_pattern_fixture(switch_duplicate_bool_without_default_no_cascade_failure_path, false);

    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_bool_without_default_no_cascade_failure_path),
        "switch value pattern 'true' is duplicated",
        "switch is missing boolean value pattern"
    );

    auto switch_duplicate_string_value_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_string_value_failure.or";
    write_duplicate_text_value_pattern_fixture(switch_duplicate_string_value_failure_path);

    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_string_value_failure_path),
        "switch value pattern '\"ready\"' is duplicated"
    );

    auto switch_duplicate_integer_cast_value_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_integer_cast_value_failure.or";
    write_duplicate_integer_cast_value_pattern_fixture(switch_duplicate_integer_cast_value_failure_path);

    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_integer_cast_value_failure_path),
        "switch value pattern '1 as Int32' is duplicated"
    );

    auto switch_redundant_bool_default_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_redundant_bool_default_failure.or";
    write_bool_value_pattern_switch_fixture(
        switch_redundant_bool_default_failure_path,
        {"true => 1", "false => 0"},
        true
    );

    assert_parse_failure_contains(
        run_parse(app, switch_redundant_bool_default_failure_path),
        "switch default case is redundant after true and false value patterns"
    );

    auto switch_duplicate_bool_redundant_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_bool_redundant_default_no_cascade_failure.or";
    write_bool_value_pattern_switch_fixture(
        switch_duplicate_bool_redundant_default_no_cascade_failure_path,
        {"true => 1", "false => 0", "false => 2"},
        true
    );

    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_bool_redundant_default_no_cascade_failure_path),
        "switch value pattern 'false' is duplicated",
        "switch default case is redundant"
    );

    auto switch_exhaustive_bool_without_default_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_exhaustive_bool_success.or";
    write_bool_value_pattern_switch_fixture(
        switch_exhaustive_bool_without_default_success_path,
        {"true => 1", "false => 0"}
    );

    assert_parse_success(run_parse(app, switch_exhaustive_bool_without_default_success_path));

    auto switch_missing_bool_pattern_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_missing_bool_pattern_failure.or";
    write_bool_value_pattern_switch_fixture(switch_missing_bool_pattern_failure_path, {"true => 1"});

    assert_parse_failure_contains(
        run_parse(app, switch_missing_bool_pattern_failure_path),
        "switch is missing boolean value pattern 'false'"
    );

    auto switch_redundant_choice_default_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_redundant_choice_default_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_redundant_choice_default_failure_path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"},
        true
    );

    assert_parse_failure_contains(
        run_parse(app, switch_redundant_choice_default_failure_path),
        "switch default case is redundant after all zero-payload choice variants are covered"
    );

    auto switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure_path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3", "Closed => 4"},
        true
    );

    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure_path),
        "switch constructor pattern 'Closed' is duplicated",
        "switch default case is redundant"
    );

    auto switch_exhaustive_choice_without_default_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_exhaustive_choice_success.or";
    write_zero_payload_choice_switch_fixture(
        switch_exhaustive_choice_without_default_success_path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"}
    );

    assert_parse_success(run_parse(app, switch_exhaustive_choice_without_default_success_path));

    auto switch_redundant_payload_choice_default_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_redundant_payload_choice_default_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_redundant_payload_choice_default_failure_path,
        {"Some(value) => value", "Empty => 0"},
        true
    );

    assert_parse_failure_contains(
        run_parse(app, switch_redundant_payload_choice_default_failure_path),
        "switch default case is redundant after all choice variants are covered"
    );

    auto switch_exhaustive_payload_choice_without_default_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_exhaustive_payload_choice_without_default_success_path,
        {"Some(value) => value", "Empty => 0"}
    );

    assert_parse_success(run_parse(app, switch_exhaustive_payload_choice_without_default_success_path));

    auto switch_reversed_exhaustive_payload_choice_without_default_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_reversed_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_reversed_exhaustive_payload_choice_without_default_success_path,
        {"Empty => 0", "Some(value) => value"}
    );

    assert_parse_success(run_parse(app, switch_reversed_exhaustive_payload_choice_without_default_success_path));

    auto switch_literal_payload_choice_default_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_literal_payload_choice_default_success.or";
    write_maybe_int_exhaustiveness_fixture(
        switch_literal_payload_choice_default_success_path,
        {"Some(1) => 1", "Empty => 0"},
        true
    );

    assert_parse_success(run_parse(app, switch_literal_payload_choice_default_success_path));

    auto switch_literal_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(
        switch_literal_payload_choice_missing_failure_path,
        {"Some(1) => 1", "Empty => 0"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_literal_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Some'"
    );

    auto switch_reversed_literal_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_reversed_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(
        switch_reversed_literal_payload_choice_missing_failure_path,
        {"Empty => 0", "Some(1) => 1"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_reversed_literal_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Some'"
    );

    auto switch_nested_payload_choice_default_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_payload_choice_default_success.or";
    write_boxed_maybe_exhaustiveness_fixture(
        switch_nested_payload_choice_default_success_path,
        {"Wrap(Some(value)) => value", "Blank => 0"},
        true
    );

    assert_parse_success(run_parse(app, switch_nested_payload_choice_default_success_path));

    auto switch_nested_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_payload_choice_missing_failure.or";
    write_boxed_maybe_exhaustiveness_fixture(
        switch_nested_payload_choice_missing_failure_path,
        {"Wrap(Some(value)) => value", "Blank => 0"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_nested_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Wrap'"
    );

    auto switch_partial_multi_payload_choice_default_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_partial_multi_payload_choice_default_success.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_partial_multi_payload_choice_default_success_path,
        {"Both(left, 1) => left", "Empty => 0"},
        true
    );

    assert_parse_success(run_parse(app, switch_partial_multi_payload_choice_default_success_path));

    auto switch_partial_multi_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_partial_multi_payload_choice_missing_failure.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_partial_multi_payload_choice_missing_failure_path,
        {"Both(left, 1) => left", "Empty => 0"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_partial_multi_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Both'"
    );

    auto switch_missing_payload_choice_variant_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_missing_payload_choice_variant_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_missing_payload_choice_variant_failure_path,
        {"Some(value) => value"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_missing_payload_choice_variant_failure_path),
        "switch is missing choice variant 'Empty'"
    );

    auto switch_exhaustive_multi_payload_choice_without_default_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_exhaustive_multi_payload_choice_success.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_exhaustive_multi_payload_choice_without_default_success_path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"}
    );

    assert_parse_success(run_parse(app, switch_exhaustive_multi_payload_choice_without_default_success_path));

    auto switch_redundant_multi_payload_choice_default_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_redundant_multi_payload_choice_default_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_redundant_multi_payload_choice_default_failure_path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"},
        true
    );

    assert_parse_failure_contains(
        run_parse(app, switch_redundant_multi_payload_choice_default_failure_path),
        "switch default case is redundant after all choice variants are covered"
    );

    auto switch_duplicate_payload_choice_redundant_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_payload_choice_redundant_default_no_cascade_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_redundant_default_no_cascade_failure_path,
        {"First(value) => value", "Second(value) => value", "Empty => 0", "First(other) => other"},
        true
    );

    auto switch_duplicate_payload_choice_redundant_default_no_cascade_failure_result =
        run_parse(app, switch_duplicate_payload_choice_redundant_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_duplicate_payload_choice_redundant_default_no_cascade_failure_result,
        "switch constructor pattern 'First(...)' is duplicated",
        "switch default case is redundant"
    );

    auto switch_first_missing_multi_payload_choice_variant_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_first_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_first_missing_multi_payload_choice_variant_failure_path,
        {"Second(value) => value", "Empty => 0"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_first_missing_multi_payload_choice_variant_failure_path),
        "switch is missing choice variant 'First'"
    );

    auto switch_second_missing_multi_payload_choice_variant_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_second_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_second_missing_multi_payload_choice_variant_failure_path,
        {"First(value) => value", "Empty => 0"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_second_missing_multi_payload_choice_variant_failure_path),
        "switch is missing choice variant 'Second'"
    );

    auto switch_duplicate_multi_payload_choice_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_multi_payload_choice_no_cascade_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_duplicate_multi_payload_choice_no_cascade_failure_path,
        {"First(value) => value", "First(other) => other"}
    );

    auto switch_duplicate_multi_payload_choice_no_cascade_failure_result =
        run_parse(app, switch_duplicate_multi_payload_choice_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_duplicate_multi_payload_choice_no_cascade_failure_result,
        "switch constructor pattern 'First(...)' is duplicated",
        "switch is missing choice variant"
    );

    auto switch_duplicate_payload_choice_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_payload_choice_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_without_default_no_cascade_failure_path,
        {"Some(value) => value", "Some(other) => other"}
    );

    auto switch_duplicate_payload_choice_without_default_no_cascade_failure_result =
        run_parse(app, switch_duplicate_payload_choice_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_duplicate_payload_choice_without_default_no_cascade_failure_result,
        "switch constructor pattern 'Some(...)' is duplicated",
        "switch is missing choice variant"
    );

    auto switch_duplicate_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_choice_constructor_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_duplicate_choice_constructor_failure_path,
        {"Closed => 1", "EndOfInput => 2", "Closed => 3"}
    );

    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_choice_constructor_failure_path),
        "switch constructor pattern 'Closed' is duplicated",
        "switch is missing zero-payload choice variant"
    );

    auto switch_duplicate_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_payload_choice_constructor_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_constructor_failure_path,
        {"Some(value) => 1", "Some(other) => 2"},
        true
    );

    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Some(...)' is duplicated"
    );

    auto switch_duplicate_payload_choice_constructor_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_payload_choice_constructor_no_cascade_failure.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_constructor_no_cascade_failure_path,
        {"Both(left, right) => 1", "Both(value, value) => 2"},
        true
    );

    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_payload_choice_constructor_no_cascade_failure_path),
        "switch constructor pattern 'Both(...)' is duplicated",
        "cannot bind 'value' more than once"
    );

    auto switch_duplicate_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_duplicate_literal_payload_choice_constructor_failure_path,
        {"Int(1) => 1", "Int(1) => 2"}
    );

    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_literal_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated",
        "switch value pattern"
    );

    auto switch_equivalent_integer_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_equivalent_integer_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_equivalent_integer_literal_payload_choice_constructor_failure_path,
        {"Int(1) => 1", "Int(1 as Int64) => 2"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_equivalent_integer_literal_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated"
    );

    auto switch_wildcard_then_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_wildcard_then_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_wildcard_then_literal_payload_choice_constructor_failure_path,
        {"Int(value) => 1", "Int(1) => 2"}
    );

    assert_parse_failure_contains_without(
        run_parse(app, switch_wildcard_then_literal_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated",
        "switch value pattern"
    );

    auto switch_literal_then_wildcard_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_literal_then_wildcard_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_literal_then_wildcard_payload_choice_constructor_failure_path,
        {"Int(1) => 1", "Int(value) => 2"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_literal_then_wildcard_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated"
    );

    auto switch_multi_payload_partial_overlap_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_multi_payload_partial_overlap_choice_constructor_failure.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_multi_payload_partial_overlap_choice_constructor_failure_path,
        {"Both(left, 1) => 1", "Both(other, 1) => 2"},
        true
    );

    assert_parse_failure_contains(
        run_parse(app, switch_multi_payload_partial_overlap_choice_constructor_failure_path),
        "switch constructor pattern 'Both(...)' is duplicated"
    );

    auto switch_multi_payload_disjoint_literal_choice_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_multi_payload_disjoint_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_multi_payload_disjoint_literal_choice_constructor_success_path,
        {"Both(left, 1) => 1", "Both(other, 2) => 2"},
        true
    );

    assert_parse_success(run_parse(app, switch_multi_payload_disjoint_literal_choice_constructor_success_path));

    auto switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_multi_payload_disjoint_leading_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path,
        {"Both(1, left) => 1", "Both(2, right) => 2"},
        true
    );

    assert_parse_success(run_parse(app, switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path));

    auto switch_missing_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_missing_choice_variant_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_missing_choice_variant_failure_path,
        {"Closed => 1", "EndOfInput => 2"}
    );

    assert_parse_failure_contains(
        run_parse(app, switch_missing_choice_variant_failure_path),
        "switch is missing zero-payload choice variant 'PermissionDenied'"
    );

    auto switch_nonfinal_default_branch_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nonfinal_default_branch_no_cascade_failure.or";
    write_bool_value_pattern_switch_fixture(
        switch_nonfinal_default_branch_no_cascade_failure_path,
        {"default => await flag", "true => 1"}
    );

    auto switch_nonfinal_default_branch_no_cascade_failure_result =
        run_parse(app, switch_nonfinal_default_branch_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        switch_nonfinal_default_branch_no_cascade_failure_result,
        "switch default case must be the final case",
        "await expression is only valid inside async functions"
    );

    auto break_outside_loop_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_break_outside_loop_failure.or";
    write_loop_control_fixture(break_outside_loop_failure_path, "stop() -> Unit", {"break"});

    assert_parse_failure_contains(
        run_parse(app, break_outside_loop_failure_path),
        "break statement is only valid inside loops"
    );

    auto continue_inside_loop_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_continue_inside_loop_success.or";
    write_loop_control_fixture(
        continue_inside_loop_success_path,
        "scan(items: shared View<Int64>) -> Unit",
        {"for item in items", "    continue"}
    );

    assert_parse_success(run_parse(app, continue_inside_loop_success_path));

    auto this_outside_method_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_this_outside_method_failure.or";
    write_receiver_fixture(this_outside_method_failure_path, {"function current() -> Int64", "    return this"});

    assert_parse_failure_contains(
        run_parse(app, this_outside_method_failure_path),
        "receiver 'this' is only valid inside implements or extend methods"
    );

    auto this_type_signature_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_this_type_signature_failure.or";
    write_receiver_fixture(
        this_type_signature_failure_path,
        {"function project(value: shared This) -> shared This", "    return value"}
    );

    assert_parse_failure_contains(
        run_parse(app, this_type_signature_failure_path),
        "This type is only valid inside interface, implements, or extend methods"
    );

    auto receiver_parameter_nonself_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_receiver_parameter_nonself_type_failure.or";
    write_receiver_fixture(
        receiver_parameter_nonself_type_failure_path,
        {"extend Buffer", "    function reset(this: Int64) -> Unit", "        return"}
    );

    assert_parse_failure_contains(
        run_parse(app, receiver_parameter_nonself_type_failure_path),
        "receiver parameter 'this' must use This, shared This, or exclusive This"
    );

    auto thread_path = std::filesystem::temp_directory_path() / "orison_compiler_app_thread_value_failure.or";
    write_concurrency_fixture(
        thread_path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        let total = sum(data)",
            "    return worker.join()",
        }
    );

    assert_parse_failure_contains(
        run_parse(app, thread_path),
        "thread expression body must end with an expression statement or value return"
    );

    auto capture_path = std::filesystem::temp_directory_path() / "orison_compiler_app_capture_failure.or";
    write_concurrency_fixture(
        capture_path,
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
        run_parse(app, capture_path),
        "concurrency expression cannot capture mutable outer local 'attempts'"
    );

    auto receiver_path = std::filesystem::temp_directory_path() / "orison_compiler_app_capture_this_failure.or";
    write_concurrency_fixture(
        receiver_path,
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
        run_parse(app, receiver_path),
        "concurrency expression cannot capture receiver 'this'"
    );

    auto typed_capture_path = std::filesystem::temp_directory_path() / "orison_compiler_app_typed_capture_failure.or";
    write_concurrency_fixture(
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

    auto generic_capture_path = std::filesystem::temp_directory_path() / "orison_compiler_app_generic_capture_failure.or";
    write_concurrency_fixture(
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

    auto shareable_thread_path = std::filesystem::temp_directory_path() / "orison_compiler_app_shareable_thread_failure.or";
    write_concurrency_fixture(
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

    auto join_receiver_path = std::filesystem::temp_directory_path() / "orison_compiler_app_join_receiver_failure.or";
    write_concurrency_fixture(
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

    auto join_async_call_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_join_async_call_failure.or";
    write_concurrency_fixture(
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

    auto thread_value_path = std::filesystem::temp_directory_path() / "orison_compiler_app_thread_value_failure.or";
    write_concurrency_fixture(
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

    auto concrete_marker_path = std::filesystem::temp_directory_path() / "orison_compiler_app_concrete_marker_success.or";
    write_concurrency_fixture(
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

    auto shareable_task_path = std::filesystem::temp_directory_path() / "orison_compiler_app_shareable_task_success.or";
    write_concurrency_fixture(
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

    auto thread_result_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_thread_result_failure.or";
    write_concurrency_fixture(
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

    auto task_result_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_task_result_shareable_success.or";
    write_concurrency_fixture(
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
