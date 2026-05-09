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

}  // namespace

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

    auto unsafe_intrinsic_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_intrinsic_failure.or";
    {
        std::ofstream output(unsafe_intrinsic_failure_path);
        output << "package demo.unsafe\n";
        output << "function read_byte(p: Address) -> Byte\n";
        output << "    return raw_read(p)\n";
    }

    auto unsafe_intrinsic_failure_path_text = unsafe_intrinsic_failure_path.string();
    std::array<char const*, 3> unsafe_intrinsic_failure_argv {
        "orisonc",
        "--parse",
        unsafe_intrinsic_failure_path_text.c_str()
    };
    auto unsafe_intrinsic_failure_result = app.run(
        std::span<char const* const>(
            unsafe_intrinsic_failure_argv.data(),
            unsafe_intrinsic_failure_argv.size()
        )
    );

    assert(unsafe_intrinsic_failure_result.exit_code == 1);
    assert(unsafe_intrinsic_failure_result.stdout_text.empty());
    assert(unsafe_intrinsic_failure_result.stderr_text.find(
               "unsafe intrinsic 'raw_read' is only valid inside unsafe functions or unsafe blocks"
           ) != std::string::npos);

    auto unsafe_intrinsic_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_intrinsic_success.or";
    {
        std::ofstream output(unsafe_intrinsic_success_path);
        output << "package demo.unsafe\n";
        output << "function zero_byte(p: Address) -> Unit\n";
        output << "    unsafe\n";
        output << "        raw_write(p, 0)\n";
    }

    auto unsafe_intrinsic_success_path_text = unsafe_intrinsic_success_path.string();
    std::array<char const*, 3> unsafe_intrinsic_success_argv {
        "orisonc",
        "--parse",
        unsafe_intrinsic_success_path_text.c_str()
    };
    auto unsafe_intrinsic_success_result = app.run(
        std::span<char const* const>(
            unsafe_intrinsic_success_argv.data(),
            unsafe_intrinsic_success_argv.size()
        )
    );

    assert(unsafe_intrinsic_success_result.exit_code == 0);
    assert(unsafe_intrinsic_success_result.stderr_text.empty());
    assert(unsafe_intrinsic_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto address_of_shape_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_of_shape_failure.or";
    {
        std::ofstream output(address_of_shape_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function pointer() -> Address\n";
        output << "    return address_of(1)\n";
    }

    auto address_of_shape_failure_path_text = address_of_shape_failure_path.string();
    std::array<char const*, 3> address_of_shape_failure_argv {
        "orisonc",
        "--parse",
        address_of_shape_failure_path_text.c_str()
    };
    auto address_of_shape_failure_result = app.run(
        std::span<char const* const>(
            address_of_shape_failure_argv.data(),
            address_of_shape_failure_argv.size()
        )
    );

    assert(address_of_shape_failure_result.exit_code == 1);
    assert(address_of_shape_failure_result.stdout_text.empty());
    assert(address_of_shape_failure_result.stderr_text.find(
               "address_of currently requires an addressable storage operand"
           ) != std::string::npos);

    auto raw_offset_shape_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_offset_shape_failure.or";
    {
        std::ofstream output(raw_offset_shape_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function advance() -> Address\n";
        output << "    return raw_offset(1, 2)\n";
    }

    auto raw_offset_shape_failure_path_text = raw_offset_shape_failure_path.string();
    std::array<char const*, 3> raw_offset_shape_failure_argv {
        "orisonc",
        "--parse",
        raw_offset_shape_failure_path_text.c_str()
    };
    auto raw_offset_shape_failure_result = app.run(
        std::span<char const* const>(
            raw_offset_shape_failure_argv.data(),
            raw_offset_shape_failure_argv.size()
        )
    );

    assert(raw_offset_shape_failure_result.exit_code == 1);
    assert(raw_offset_shape_failure_result.stdout_text.empty());
    assert(raw_offset_shape_failure_result.stderr_text.find(
               "raw_offset currently requires an address-like first argument"
           ) != std::string::npos);

    auto raw_offset_noninteger_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_offset_noninteger_failure.or";
    {
        std::ofstream output(raw_offset_noninteger_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function advance(base: Address) -> Address\n";
        output << "    return raw_offset(base, \"one\")\n";
    }

    auto raw_offset_noninteger_failure_path_text = raw_offset_noninteger_failure_path.string();
    std::array<char const*, 3> raw_offset_noninteger_failure_argv {
        "orisonc",
        "--parse",
        raw_offset_noninteger_failure_path_text.c_str()
    };
    auto raw_offset_noninteger_failure_result = app.run(
        std::span<char const* const>(
            raw_offset_noninteger_failure_argv.data(),
            raw_offset_noninteger_failure_argv.size()
        )
    );

    assert(raw_offset_noninteger_failure_result.exit_code == 1);
    assert(raw_offset_noninteger_failure_result.stdout_text.empty());
    assert(raw_offset_noninteger_failure_result.stderr_text.find(
               "raw_offset currently requires an integer offset argument"
           ) != std::string::npos);

    auto nested_unsafe_operand_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_nested_unsafe_operand_success.or";
    {
        std::ofstream output(nested_unsafe_operand_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function poke(buf: exclusive Buffer, value: Byte) -> Unit\n";
        output << "    let p = address_of(buf.data[0])\n";
        output << "    raw_write(raw_offset(p, 1), value)\n";
    }

    auto nested_unsafe_operand_success_path_text = nested_unsafe_operand_success_path.string();
    std::array<char const*, 3> nested_unsafe_operand_success_argv {
        "orisonc",
        "--parse",
        nested_unsafe_operand_success_path_text.c_str()
    };
    auto nested_unsafe_operand_success_result = app.run(
        std::span<char const* const>(
            nested_unsafe_operand_success_argv.data(),
            nested_unsafe_operand_success_argv.size()
        )
    );

    assert(nested_unsafe_operand_success_result.exit_code == 0);
    assert(nested_unsafe_operand_success_result.stderr_text.empty());
    assert(nested_unsafe_operand_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto index_access_noninteger_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_index_access_noninteger_failure.or";
    {
        std::ofstream output(index_access_noninteger_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_byte(device: Device, value: Byte) -> Unit\n";
        output << "    raw_write(device.ptrs[\"one\"], value)\n";
    }

    auto index_access_noninteger_failure_path_text = index_access_noninteger_failure_path.string();
    std::array<char const*, 3> index_access_noninteger_failure_argv {
        "orisonc",
        "--parse",
        index_access_noninteger_failure_path_text.c_str()
    };
    auto index_access_noninteger_failure_result = app.run(
        std::span<char const* const>(
            index_access_noninteger_failure_argv.data(),
            index_access_noninteger_failure_argv.size()
        )
    );

    assert(index_access_noninteger_failure_result.exit_code == 1);
    assert(index_access_noninteger_failure_result.stdout_text.empty());
    assert(index_access_noninteger_failure_result.stderr_text.find(
               "index access currently requires an integer index expression"
           ) != std::string::npos);

    auto index_access_integer_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_index_access_integer_success.or";
    {
        std::ofstream output(index_access_integer_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_byte(device: Device, index: UInt32, value: Byte) -> Unit\n";
        output << "    raw_write(device.ptrs[index], value)\n";
    }

    auto index_access_integer_success_path_text = index_access_integer_success_path.string();
    std::array<char const*, 3> index_access_integer_success_argv {
        "orisonc",
        "--parse",
        index_access_integer_success_path_text.c_str()
    };
    auto index_access_integer_success_result = app.run(
        std::span<char const* const>(
            index_access_integer_success_argv.data(),
            index_access_integer_success_argv.size()
        )
    );

    assert(index_access_integer_success_result.exit_code == 0);
    assert(index_access_integer_success_result.stderr_text.empty());
    assert(index_access_integer_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto unsafe_call_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_call_failure.or";
    {
        std::ofstream output(unsafe_call_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Address) -> UInt32\n";
        output << "    return raw_read(p)\n";
        output << "function read_twice(p: Address) -> UInt32\n";
        output << "    return read_word(p)\n";
    }

    auto unsafe_call_failure_path_text = unsafe_call_failure_path.string();
    std::array<char const*, 3> unsafe_call_failure_argv {
        "orisonc",
        "--parse",
        unsafe_call_failure_path_text.c_str()
    };
    auto unsafe_call_failure_result = app.run(
        std::span<char const* const>(unsafe_call_failure_argv.data(), unsafe_call_failure_argv.size())
    );

    assert(unsafe_call_failure_result.exit_code == 1);
    assert(unsafe_call_failure_result.stdout_text.empty());
    assert(unsafe_call_failure_result.stderr_text.find(
               "call to unsafe function 'read_word' is only valid inside unsafe functions or unsafe blocks"
           ) != std::string::npos);

    auto unsafe_call_block_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_unsafe_call_block_success.or";
    {
        std::ofstream output(unsafe_call_block_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Address) -> UInt32\n";
        output << "    return raw_read(p)\n";
        output << "function copy_word(p: Address) -> UInt32\n";
        output << "    unsafe\n";
        output << "        return read_word(p)\n";
    }

    auto unsafe_call_block_success_path_text = unsafe_call_block_success_path.string();
    std::array<char const*, 3> unsafe_call_block_success_argv {
        "orisonc",
        "--parse",
        unsafe_call_block_success_path_text.c_str()
    };
    auto unsafe_call_block_success_result = app.run(
        std::span<char const* const>(
            unsafe_call_block_success_argv.data(),
            unsafe_call_block_success_argv.size()
        )
    );

    assert(unsafe_call_block_success_result.exit_code == 0);
    assert(unsafe_call_block_success_result.stderr_text.empty());
    assert(unsafe_call_block_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto pointer_construction_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_failure.or";
    {
        std::ofstream output(pointer_construction_failure_path);
        output << "package demo.unsafe\n";
        output << "function read_byte(addr: Address) -> Byte\n";
        output << "    let p = Pointer(addr)\n";
        output << "    return raw_read(p)\n";
    }

    auto pointer_construction_failure_path_text = pointer_construction_failure_path.string();
    std::array<char const*, 3> pointer_construction_failure_argv {
        "orisonc",
        "--parse",
        pointer_construction_failure_path_text.c_str()
    };
    auto pointer_construction_failure_result = app.run(
        std::span<char const* const>(
            pointer_construction_failure_argv.data(),
            pointer_construction_failure_argv.size()
        )
    );

    assert(pointer_construction_failure_result.exit_code == 1);
    assert(pointer_construction_failure_result.stdout_text.empty());
    assert(pointer_construction_failure_result.stderr_text.find(
               "Pointer construction is only valid inside unsafe functions or unsafe blocks"
           ) != std::string::npos);

    auto pointer_construction_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_success.or";
    {
        std::ofstream output(pointer_construction_success_path);
        output << "package demo.unsafe\n";
        output << "function scribble(addr: Address) -> Unit\n";
        output << "    unsafe\n";
        output << "        let p = Pointer(addr)\n";
        output << "        raw_write(p, 0)\n";
    }

    auto pointer_construction_success_path_text = pointer_construction_success_path.string();
    std::array<char const*, 3> pointer_construction_success_argv {
        "orisonc",
        "--parse",
        pointer_construction_success_path_text.c_str()
    };
    auto pointer_construction_success_result = app.run(
        std::span<char const* const>(
            pointer_construction_success_argv.data(),
            pointer_construction_success_argv.size()
        )
    );

    assert(pointer_construction_success_result.exit_code == 0);
    assert(pointer_construction_success_result.stderr_text.empty());
    assert(pointer_construction_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto pointer_construction_addressof_typed_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_addressof_typed_failure.or";
    {
        std::ofstream output(pointer_construction_addressof_typed_failure_path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>\n";
        output << "    return Pointer(address_of(buf.data[0]))\n";
    }

    auto pointer_construction_addressof_typed_failure_path_text =
        pointer_construction_addressof_typed_failure_path.string();
    std::array<char const*, 3> pointer_construction_addressof_typed_failure_argv {
        "orisonc",
        "--parse",
        pointer_construction_addressof_typed_failure_path_text.c_str()
    };
    auto pointer_construction_addressof_typed_failure_result = app.run(
        std::span<char const* const>(
            pointer_construction_addressof_typed_failure_argv.data(),
            pointer_construction_addressof_typed_failure_argv.size()
        )
    );

    assert(pointer_construction_addressof_typed_failure_result.exit_code == 1);
    assert(pointer_construction_addressof_typed_failure_result.stdout_text.empty());
    assert(pointer_construction_addressof_typed_failure_result.stderr_text.find(
               "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'"
           ) != std::string::npos);

    auto pointer_construction_addressof_same_width_success_path = std::filesystem::temp_directory_path() /
                                                                  "orison_compiler_app_pointer_construction_addressof_same_width_success.or";
    {
        std::ofstream output(pointer_construction_addressof_same_width_success_path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function status_ptr(device: Device) -> Pointer<UInt32>\n";
        output << "    return Pointer(address_of(device.registers.status))\n";
    }

    auto pointer_construction_addressof_same_width_success_path_text =
        pointer_construction_addressof_same_width_success_path.string();
    std::array<char const*, 3> pointer_construction_addressof_same_width_success_argv {
        "orisonc",
        "--parse",
        pointer_construction_addressof_same_width_success_path_text.c_str()
    };
    auto pointer_construction_addressof_same_width_success_result = app.run(
        std::span<char const* const>(
            pointer_construction_addressof_same_width_success_argv.data(),
            pointer_construction_addressof_same_width_success_argv.size()
        )
    );

    assert(pointer_construction_addressof_same_width_success_result.exit_code == 0);
    assert(pointer_construction_addressof_same_width_success_result.stderr_text.empty());

    auto pointer_construction_noarg_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_noarg_failure.or";
    {
        std::ofstream output(pointer_construction_noarg_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let p = Pointer()\n";
        output << "    return raw_read(p)\n";
    }

    auto pointer_construction_noarg_failure_path_text = pointer_construction_noarg_failure_path.string();
    std::array<char const*, 3> pointer_construction_noarg_failure_argv {
        "orisonc",
        "--parse",
        pointer_construction_noarg_failure_path_text.c_str()
    };
    auto pointer_construction_noarg_failure_result = app.run(
        std::span<char const* const>(
            pointer_construction_noarg_failure_argv.data(),
            pointer_construction_noarg_failure_argv.size()
        )
    );

    assert(pointer_construction_noarg_failure_result.exit_code == 1);
    assert(pointer_construction_noarg_failure_result.stdout_text.empty());
    assert(pointer_construction_noarg_failure_result.stderr_text.find(
               "Pointer construction currently requires exactly one source argument"
           ) != std::string::npos);

    auto pointer_construction_multiarg_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_multiarg_failure.or";
    {
        std::ofstream output(pointer_construction_multiarg_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(addr: Address) -> Byte\n";
        output << "    let p = Pointer(addr, addr)\n";
        output << "    return raw_read(p)\n";
    }

    auto pointer_construction_multiarg_failure_path_text = pointer_construction_multiarg_failure_path.string();
    std::array<char const*, 3> pointer_construction_multiarg_failure_argv {
        "orisonc",
        "--parse",
        pointer_construction_multiarg_failure_path_text.c_str()
    };
    auto pointer_construction_multiarg_failure_result = app.run(
        std::span<char const* const>(
            pointer_construction_multiarg_failure_argv.data(),
            pointer_construction_multiarg_failure_argv.size()
        )
    );

    assert(pointer_construction_multiarg_failure_result.exit_code == 1);
    assert(pointer_construction_multiarg_failure_result.stdout_text.empty());
    assert(pointer_construction_multiarg_failure_result.stderr_text.find(
               "Pointer construction currently requires exactly one source argument"
           ) != std::string::npos);

    auto pointer_construction_nonaddress_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_nonaddress_failure.or";
    {
        std::ofstream output(pointer_construction_nonaddress_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let p = Pointer(\"text\")\n";
        output << "    return raw_read(p)\n";
    }

    auto pointer_construction_nonaddress_failure_path_text = pointer_construction_nonaddress_failure_path.string();
    std::array<char const*, 3> pointer_construction_nonaddress_failure_argv {
        "orisonc",
        "--parse",
        pointer_construction_nonaddress_failure_path_text.c_str()
    };
    auto pointer_construction_nonaddress_failure_result = app.run(
        std::span<char const* const>(
            pointer_construction_nonaddress_failure_argv.data(),
            pointer_construction_nonaddress_failure_argv.size()
        )
    );

    assert(pointer_construction_nonaddress_failure_result.exit_code == 1);
    assert(pointer_construction_nonaddress_failure_result.stdout_text.empty());
    assert(pointer_construction_nonaddress_failure_result.stderr_text.find(
               "Pointer construction currently requires an address-like source argument"
           ) != std::string::npos);

    auto pointer_construction_addressof_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_construction_addressof_success.or";
    {
        std::ofstream output(pointer_construction_addressof_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function first_ptr(buf: exclusive Buffer) -> Pointer<Byte>\n";
        output << "    let p = Pointer(address_of(buf.data[0]))\n";
        output << "    return p\n";
    }

    auto pointer_construction_addressof_success_path_text = pointer_construction_addressof_success_path.string();
    std::array<char const*, 3> pointer_construction_addressof_success_argv {
        "orisonc",
        "--parse",
        pointer_construction_addressof_success_path_text.c_str()
    };
    auto pointer_construction_addressof_success_result = app.run(
        std::span<char const* const>(
            pointer_construction_addressof_success_argv.data(),
            pointer_construction_addressof_success_argv.size()
        )
    );

    assert(pointer_construction_addressof_success_result.exit_code == 0);
    assert(pointer_construction_addressof_success_result.stderr_text.empty());
    assert(pointer_construction_addressof_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto pointer_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_failure.or";
    {
        std::ofstream output(pointer_typed_binding_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let p: Pointer<Byte> = \"text\"\n";
        output << "    return 0\n";
    }

    auto pointer_typed_binding_failure_path_text = pointer_typed_binding_failure_path.string();
    std::array<char const*, 3> pointer_typed_binding_failure_argv {
        "orisonc",
        "--parse",
        pointer_typed_binding_failure_path_text.c_str()
    };
    auto pointer_typed_binding_failure_result = app.run(
        std::span<char const* const>(
            pointer_typed_binding_failure_argv.data(),
            pointer_typed_binding_failure_argv.size()
        )
    );

    assert(pointer_typed_binding_failure_result.exit_code == 1);
    assert(pointer_typed_binding_failure_result.stdout_text.empty());
    assert(pointer_typed_binding_failure_result.stderr_text.find(
               "pointer-typed binding initializer currently requires a structurally pointer-like expression"
           ) != std::string::npos);

    auto pointer_typed_binding_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_name_failure.or";
    {
        std::ofstream output(pointer_typed_binding_name_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let source = \"text\"\n";
        output << "    let p: Pointer<Byte> = source\n";
        output << "    return 0\n";
    }

    auto pointer_typed_binding_name_failure_path_text = pointer_typed_binding_name_failure_path.string();
    std::array<char const*, 3> pointer_typed_binding_name_failure_argv {
        "orisonc",
        "--parse",
        pointer_typed_binding_name_failure_path_text.c_str()
    };
    auto pointer_typed_binding_name_failure_result = app.run(
        std::span<char const* const>(
            pointer_typed_binding_name_failure_argv.data(),
            pointer_typed_binding_name_failure_argv.size()
        )
    );

    assert(pointer_typed_binding_name_failure_result.exit_code == 1);
    assert(pointer_typed_binding_name_failure_result.stdout_text.empty());
    assert(pointer_typed_binding_name_failure_result.stderr_text.find(
               "pointer-typed binding initializer currently requires a structurally pointer-like expression"
           ) != std::string::npos);

    auto pointer_typed_binding_field_pointer_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_field_pointer_failure.or";
    {
        std::ofstream output(pointer_typed_binding_field_pointer_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptr: Pointer<Byte>\n";
        output << "unsafe function next_ptr(device: Device) -> Pointer<UInt32>\n";
        output << "    let p: Pointer<UInt32> = device.ptr\n";
        output << "    return p\n";
    }

    auto pointer_typed_binding_field_pointer_failure_path_text =
        pointer_typed_binding_field_pointer_failure_path.string();
    std::array<char const*, 3> pointer_typed_binding_field_pointer_failure_argv {
        "orisonc",
        "--parse",
        pointer_typed_binding_field_pointer_failure_path_text.c_str()
    };
    auto pointer_typed_binding_field_pointer_failure_result = app.run(
        std::span<char const* const>(
            pointer_typed_binding_field_pointer_failure_argv.data(),
            pointer_typed_binding_field_pointer_failure_argv.size()
        )
    );

    assert(pointer_typed_binding_field_pointer_failure_result.exit_code == 1);
    assert(pointer_typed_binding_field_pointer_failure_result.stdout_text.empty());
    assert(pointer_typed_binding_field_pointer_failure_result.stderr_text.find(
               "pointer-typed binding initializer pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
           ) != std::string::npos);

    auto pointer_typed_binding_same_width_field_pointer_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_pointer_typed_binding_same_width_field_pointer_success.or";
    {
        std::ofstream output(pointer_typed_binding_same_width_field_pointer_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptr: Pointer<Int32>\n";
        output << "unsafe function next_ptr(device: Device) -> Pointer<UInt32>\n";
        output << "    let p: Pointer<UInt32> = device.ptr\n";
        output << "    return p\n";
    }

    auto pointer_typed_binding_same_width_field_pointer_success_path_text =
        pointer_typed_binding_same_width_field_pointer_success_path.string();
    std::array<char const*, 3> pointer_typed_binding_same_width_field_pointer_success_argv {
        "orisonc",
        "--parse",
        pointer_typed_binding_same_width_field_pointer_success_path_text.c_str()
    };
    auto pointer_typed_binding_same_width_field_pointer_success_result = app.run(
        std::span<char const* const>(
            pointer_typed_binding_same_width_field_pointer_success_argv.data(),
            pointer_typed_binding_same_width_field_pointer_success_argv.size()
        )
    );

    assert(pointer_typed_binding_same_width_field_pointer_success_result.exit_code == 0);
    assert(pointer_typed_binding_same_width_field_pointer_success_result.stderr_text.empty());

    auto pointer_return_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_return_failure.or";
    {
        std::ofstream output(pointer_return_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_ptr() -> Pointer<Byte>\n";
        output << "    return \"text\"\n";
    }

    auto pointer_return_failure_path_text = pointer_return_failure_path.string();
    std::array<char const*, 3> pointer_return_failure_argv {
        "orisonc",
        "--parse",
        pointer_return_failure_path_text.c_str()
    };
    auto pointer_return_failure_result = app.run(
        std::span<char const* const>(
            pointer_return_failure_argv.data(),
            pointer_return_failure_argv.size()
        )
    );

    assert(pointer_return_failure_result.exit_code == 1);
    assert(pointer_return_failure_result.stdout_text.empty());
    assert(pointer_return_failure_result.stderr_text.find(
               "pointer-returning function currently requires a structurally pointer-like expression"
           ) != std::string::npos);

    auto pointer_return_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_return_name_failure.or";
    {
        std::ofstream output(pointer_return_name_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_ptr() -> Pointer<Byte>\n";
        output << "    let source = \"text\"\n";
        output << "    return source\n";
    }

    auto pointer_return_name_failure_path_text = pointer_return_name_failure_path.string();
    std::array<char const*, 3> pointer_return_name_failure_argv {
        "orisonc",
        "--parse",
        pointer_return_name_failure_path_text.c_str()
    };
    auto pointer_return_name_failure_result = app.run(
        std::span<char const* const>(
            pointer_return_name_failure_argv.data(),
            pointer_return_name_failure_argv.size()
        )
    );

    assert(pointer_return_name_failure_result.exit_code == 1);
    assert(pointer_return_name_failure_result.stdout_text.empty());
    assert(pointer_return_name_failure_result.stderr_text.find(
               "pointer-returning function currently requires a structurally pointer-like expression"
           ) != std::string::npos);

    auto pointer_return_helper_pointer_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_return_helper_pointer_failure.or";
    {
        std::ofstream output(pointer_return_helper_pointer_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return base\n";
        output << "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>\n";
        output << "    return byte_ptr(base)\n";
    }

    auto pointer_return_helper_pointer_failure_path_text =
        pointer_return_helper_pointer_failure_path.string();
    std::array<char const*, 3> pointer_return_helper_pointer_failure_argv {
        "orisonc",
        "--parse",
        pointer_return_helper_pointer_failure_path_text.c_str()
    };
    auto pointer_return_helper_pointer_failure_result = app.run(
        std::span<char const* const>(
            pointer_return_helper_pointer_failure_argv.data(),
            pointer_return_helper_pointer_failure_argv.size()
        )
    );

    assert(pointer_return_helper_pointer_failure_result.exit_code == 1);
    assert(pointer_return_helper_pointer_failure_result.stdout_text.empty());
    assert(pointer_return_helper_pointer_failure_result.stderr_text.find(
               "pointer-returning function pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
           ) != std::string::npos);

    auto pointer_return_same_width_helper_pointer_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_pointer_return_same_width_helper_pointer_success.or";
    {
        std::ofstream output(pointer_return_same_width_helper_pointer_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function wordish_ptr(base: Pointer<Int32>) -> Pointer<Int32>\n";
        output << "    return base\n";
        output << "unsafe function word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>\n";
        output << "    return wordish_ptr(base)\n";
    }

    auto pointer_return_same_width_helper_pointer_success_path_text =
        pointer_return_same_width_helper_pointer_success_path.string();
    std::array<char const*, 3> pointer_return_same_width_helper_pointer_success_argv {
        "orisonc",
        "--parse",
        pointer_return_same_width_helper_pointer_success_path_text.c_str()
    };
    auto pointer_return_same_width_helper_pointer_success_result = app.run(
        std::span<char const* const>(
            pointer_return_same_width_helper_pointer_success_argv.data(),
            pointer_return_same_width_helper_pointer_success_argv.size()
        )
    );

    assert(pointer_return_same_width_helper_pointer_success_result.exit_code == 0);
    assert(pointer_return_same_width_helper_pointer_success_result.stderr_text.empty());

    auto raw_write_generic_helper_returned_pointer_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_helper_returned_pointer_same_width_success.or";
    {
        std::ofstream output(raw_write_generic_helper_returned_pointer_same_width_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>\n";
        output << "    return base\n";
        output << "unsafe function write_word(base: Pointer<Int32>, value: UInt32) -> Unit\n";
        output << "    raw_write(id_ptr(base), value)\n";
    }

    auto raw_write_generic_helper_returned_pointer_same_width_success_path_text =
        raw_write_generic_helper_returned_pointer_same_width_success_path.string();
    std::array<char const*, 3> raw_write_generic_helper_returned_pointer_same_width_success_argv {
        "orisonc",
        "--parse",
        raw_write_generic_helper_returned_pointer_same_width_success_path_text.c_str()
    };
    auto raw_write_generic_helper_returned_pointer_same_width_success_result = app.run(
        std::span<char const* const>(
            raw_write_generic_helper_returned_pointer_same_width_success_argv.data(),
            raw_write_generic_helper_returned_pointer_same_width_success_argv.size()
        )
    );

    assert(raw_write_generic_helper_returned_pointer_same_width_success_result.exit_code == 0);
    assert(raw_write_generic_helper_returned_pointer_same_width_success_result.stderr_text.empty());

    auto raw_write_generic_helper_returned_pointer_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_helper_returned_pointer_mismatch_failure.or";
    {
        std::ofstream output(raw_write_generic_helper_returned_pointer_mismatch_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>\n";
        output << "    return base\n";
        output << "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(id_ptr(base), value)\n";
    }

    auto raw_write_generic_helper_returned_pointer_mismatch_failure_path_text =
        raw_write_generic_helper_returned_pointer_mismatch_failure_path.string();
    std::array<char const*, 3> raw_write_generic_helper_returned_pointer_mismatch_failure_argv {
        "orisonc",
        "--parse",
        raw_write_generic_helper_returned_pointer_mismatch_failure_path_text.c_str()
    };
    auto raw_write_generic_helper_returned_pointer_mismatch_failure_result = app.run(
        std::span<char const* const>(
            raw_write_generic_helper_returned_pointer_mismatch_failure_argv.data(),
            raw_write_generic_helper_returned_pointer_mismatch_failure_argv.size()
        )
    );

    assert(raw_write_generic_helper_returned_pointer_mismatch_failure_result.exit_code == 1);
    assert(raw_write_generic_helper_returned_pointer_mismatch_failure_result.stdout_text.empty());
    assert(raw_write_generic_helper_returned_pointer_mismatch_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto raw_write_generic_receiver_method_pointer_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_receiver_method_pointer_same_width_success.or";
    {
        std::ofstream output(raw_write_generic_receiver_method_pointer_same_width_success_path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    id: Int64\n";
        output << "extend Device<T>\n";
        output << "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>\n";
        output << "        return base\n";
        output << "unsafe function write_word(device: Device<Int32>, base: Pointer<Int32>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr(base), value)\n";
    }

    auto raw_write_generic_receiver_method_pointer_same_width_success_path_text =
        raw_write_generic_receiver_method_pointer_same_width_success_path.string();
    std::array<char const*, 3> raw_write_generic_receiver_method_pointer_same_width_success_argv {
        "orisonc",
        "--parse",
        raw_write_generic_receiver_method_pointer_same_width_success_path_text.c_str()
    };
    auto raw_write_generic_receiver_method_pointer_same_width_success_result = app.run(
        std::span<char const* const>(
            raw_write_generic_receiver_method_pointer_same_width_success_argv.data(),
            raw_write_generic_receiver_method_pointer_same_width_success_argv.size()
        )
    );

    assert(raw_write_generic_receiver_method_pointer_same_width_success_result.exit_code == 0);
    assert(raw_write_generic_receiver_method_pointer_same_width_success_result.stderr_text.empty());

    auto raw_write_generic_receiver_method_pointer_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_receiver_method_pointer_mismatch_failure.or";
    {
        std::ofstream output(raw_write_generic_receiver_method_pointer_mismatch_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    id: Int64\n";
        output << "extend Device<T>\n";
        output << "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>\n";
        output << "        return base\n";
        output << "unsafe function write_word(device: Device<Byte>, base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr(base), value)\n";
    }

    auto raw_write_generic_receiver_method_pointer_mismatch_failure_path_text =
        raw_write_generic_receiver_method_pointer_mismatch_failure_path.string();
    std::array<char const*, 3> raw_write_generic_receiver_method_pointer_mismatch_failure_argv {
        "orisonc",
        "--parse",
        raw_write_generic_receiver_method_pointer_mismatch_failure_path_text.c_str()
    };
    auto raw_write_generic_receiver_method_pointer_mismatch_failure_result = app.run(
        std::span<char const* const>(
            raw_write_generic_receiver_method_pointer_mismatch_failure_argv.data(),
            raw_write_generic_receiver_method_pointer_mismatch_failure_argv.size()
        )
    );

    assert(raw_write_generic_receiver_method_pointer_mismatch_failure_result.exit_code == 1);
    assert(raw_write_generic_receiver_method_pointer_mismatch_failure_result.stdout_text.empty());
    assert(raw_write_generic_receiver_method_pointer_mismatch_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto pointer_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_typed_binding_success.or";
    {
        std::ofstream output(pointer_typed_binding_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_byte(base: Pointer<Byte>) -> Byte\n";
        output << "    let p: Pointer<Byte> = raw_offset(base, 1)\n";
        output << "    return raw_read(p)\n";
    }

    auto pointer_typed_binding_success_path_text = pointer_typed_binding_success_path.string();
    std::array<char const*, 3> pointer_typed_binding_success_argv {
        "orisonc",
        "--parse",
        pointer_typed_binding_success_path_text.c_str()
    };
    auto pointer_typed_binding_success_result = app.run(
        std::span<char const* const>(
            pointer_typed_binding_success_argv.data(),
            pointer_typed_binding_success_argv.size()
        )
    );

    assert(pointer_typed_binding_success_result.exit_code == 0);
    assert(pointer_typed_binding_success_result.stderr_text.empty());
    assert(pointer_typed_binding_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto pointer_raw_offset_typed_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_pointer_rawoffset_typed_failure.or";
    {
        std::ofstream output(pointer_raw_offset_typed_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>\n";
        output << "    return raw_offset(base, 1)\n";
    }

    auto pointer_raw_offset_typed_failure_path_text = pointer_raw_offset_typed_failure_path.string();
    std::array<char const*, 3> pointer_raw_offset_typed_failure_argv {
        "orisonc",
        "--parse",
        pointer_raw_offset_typed_failure_path_text.c_str()
    };
    auto pointer_raw_offset_typed_failure_result = app.run(
        std::span<char const* const>(
            pointer_raw_offset_typed_failure_argv.data(),
            pointer_raw_offset_typed_failure_argv.size()
        )
    );

    assert(pointer_raw_offset_typed_failure_result.exit_code == 1);
    assert(pointer_raw_offset_typed_failure_result.stdout_text.empty());
    assert(pointer_raw_offset_typed_failure_result.stderr_text.find(
               "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
           ) != std::string::npos);

    auto pointer_raw_offset_same_width_success_path = std::filesystem::temp_directory_path() /
                                                      "orison_compiler_app_pointer_rawoffset_same_width_success.or";
    {
        std::ofstream output(pointer_raw_offset_same_width_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>\n";
        output << "    return raw_offset(base, 1)\n";
    }

    auto pointer_raw_offset_same_width_success_path_text =
        pointer_raw_offset_same_width_success_path.string();
    std::array<char const*, 3> pointer_raw_offset_same_width_success_argv {
        "orisonc",
        "--parse",
        pointer_raw_offset_same_width_success_path_text.c_str()
    };
    auto pointer_raw_offset_same_width_success_result = app.run(
        std::span<char const* const>(
            pointer_raw_offset_same_width_success_argv.data(),
            pointer_raw_offset_same_width_success_argv.size()
        )
    );

    assert(pointer_raw_offset_same_width_success_result.exit_code == 0);
    assert(pointer_raw_offset_same_width_success_result.stderr_text.empty());

    auto address_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_failure.or";
    {
        std::ofstream output(address_typed_binding_failure_path);
        output << "package demo.unsafe\n";
        output << "function read_base() -> Address\n";
        output << "    let base: Address = \"text\"\n";
        output << "    return 0x4000_1000\n";
    }

    auto address_typed_binding_failure_path_text = address_typed_binding_failure_path.string();
    std::array<char const*, 3> address_typed_binding_failure_argv {
        "orisonc",
        "--parse",
        address_typed_binding_failure_path_text.c_str()
    };
    auto address_typed_binding_failure_result = app.run(
        std::span<char const* const>(
            address_typed_binding_failure_argv.data(),
            address_typed_binding_failure_argv.size()
        )
    );

    assert(address_typed_binding_failure_result.exit_code == 1);
    assert(address_typed_binding_failure_result.stdout_text.empty());
    assert(address_typed_binding_failure_result.stderr_text.find(
               "address-typed binding initializer currently requires a structurally address-like expression"
           ) != std::string::npos);

    auto address_typed_binding_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_name_failure.or";
    {
        std::ofstream output(address_typed_binding_name_failure_path);
        output << "package demo.unsafe\n";
        output << "function read_base() -> Address\n";
        output << "    let source = \"text\"\n";
        output << "    let base: Address = source\n";
        output << "    return 0x4000_1000\n";
    }

    auto address_typed_binding_name_failure_path_text = address_typed_binding_name_failure_path.string();
    std::array<char const*, 3> address_typed_binding_name_failure_argv {
        "orisonc",
        "--parse",
        address_typed_binding_name_failure_path_text.c_str()
    };
    auto address_typed_binding_name_failure_result = app.run(
        std::span<char const* const>(
            address_typed_binding_name_failure_argv.data(),
            address_typed_binding_name_failure_argv.size()
        )
    );

    assert(address_typed_binding_name_failure_result.exit_code == 1);
    assert(address_typed_binding_name_failure_result.stdout_text.empty());
    assert(address_typed_binding_name_failure_result.stderr_text.find(
               "address-typed binding initializer currently requires a structurally address-like expression"
           ) != std::string::npos);

    auto address_return_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_failure.or";
    {
        std::ofstream output(address_return_failure_path);
        output << "package demo.unsafe\n";
        output << "function base() -> Address\n";
        output << "    return \"text\"\n";
    }

    auto address_return_failure_path_text = address_return_failure_path.string();
    std::array<char const*, 3> address_return_failure_argv {
        "orisonc",
        "--parse",
        address_return_failure_path_text.c_str()
    };
    auto address_return_failure_result = app.run(
        std::span<char const* const>(
            address_return_failure_argv.data(),
            address_return_failure_argv.size()
        )
    );

    assert(address_return_failure_result.exit_code == 1);
    assert(address_return_failure_result.stdout_text.empty());
    assert(address_return_failure_result.stderr_text.find(
               "address-returning function currently requires a structurally address-like expression"
           ) != std::string::npos);

    auto address_return_name_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_name_failure.or";
    {
        std::ofstream output(address_return_name_failure_path);
        output << "package demo.unsafe\n";
        output << "function base() -> Address\n";
        output << "    let source = \"text\"\n";
        output << "    return source\n";
    }

    auto address_return_name_failure_path_text = address_return_name_failure_path.string();
    std::array<char const*, 3> address_return_name_failure_argv {
        "orisonc",
        "--parse",
        address_return_name_failure_path_text.c_str()
    };
    auto address_return_name_failure_result = app.run(
        std::span<char const* const>(
            address_return_name_failure_argv.data(),
            address_return_name_failure_argv.size()
        )
    );

    assert(address_return_name_failure_result.exit_code == 1);
    assert(address_return_name_failure_result.stdout_text.empty());
    assert(address_return_name_failure_result.stderr_text.find(
               "address-returning function currently requires a structurally address-like expression"
           ) != std::string::npos);

    auto address_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_success.or";
    {
        std::ofstream output(address_typed_binding_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function first_addr(buf: exclusive Buffer) -> Address\n";
        output << "    let base: Address = address_of(buf.data[0])\n";
        output << "    return base\n";
    }

    auto address_typed_binding_success_path_text = address_typed_binding_success_path.string();
    std::array<char const*, 3> address_typed_binding_success_argv {
        "orisonc",
        "--parse",
        address_typed_binding_success_path_text.c_str()
    };
    auto address_typed_binding_success_result = app.run(
        std::span<char const* const>(
            address_typed_binding_success_argv.data(),
            address_typed_binding_success_argv.size()
        )
    );

    assert(address_typed_binding_success_result.exit_code == 0);
    assert(address_typed_binding_success_result.stderr_text.empty());
    assert(address_typed_binding_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto address_typed_binding_field_address_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_typed_binding_field_address_success.or";
    {
        std::ofstream output(address_typed_binding_field_address_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    base: Address\n";
        output << "function read_base(device: Device) -> Address\n";
        output << "    let base: Address = device.base\n";
        output << "    return base\n";
    }

    auto address_typed_binding_field_address_success_path_text =
        address_typed_binding_field_address_success_path.string();
    std::array<char const*, 3> address_typed_binding_field_address_success_argv {
        "orisonc",
        "--parse",
        address_typed_binding_field_address_success_path_text.c_str()
    };
    auto address_typed_binding_field_address_success_result = app.run(
        std::span<char const* const>(
            address_typed_binding_field_address_success_argv.data(),
            address_typed_binding_field_address_success_argv.size()
        )
    );

    assert(address_typed_binding_field_address_success_result.exit_code == 0);
    assert(address_typed_binding_field_address_success_result.stderr_text.empty());

    auto address_typed_binding_indexed_address_success_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_address_typed_binding_indexed_address_success.or";
    {
        std::ofstream output(address_typed_binding_indexed_address_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "function read_base(device: Device, index: Int64) -> Address\n";
        output << "    let base: Address = device.bases[index]\n";
        output << "    return base\n";
    }

    auto address_typed_binding_indexed_address_success_path_text =
        address_typed_binding_indexed_address_success_path.string();
    std::array<char const*, 3> address_typed_binding_indexed_address_success_argv {
        "orisonc",
        "--parse",
        address_typed_binding_indexed_address_success_path_text.c_str()
    };
    auto address_typed_binding_indexed_address_success_result = app.run(
        std::span<char const* const>(
            address_typed_binding_indexed_address_success_argv.data(),
            address_typed_binding_indexed_address_success_argv.size()
        )
    );

    assert(address_typed_binding_indexed_address_success_result.exit_code == 0);
    assert(address_typed_binding_indexed_address_success_result.stderr_text.empty());

    auto address_return_helper_returned_address_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_helper_returned_address_success.or";
    {
        std::ofstream output(address_return_helper_returned_address_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function base_at(this: shared This, index: Int64) -> Address\n";
        output << "        let base = this.bases[index]\n";
        output << "        return base\n";
        output << "function read_base(device: Device, index: Int64) -> Address\n";
        output << "    return device.base_at(index)\n";
    }

    auto address_return_helper_returned_address_success_path_text =
        address_return_helper_returned_address_success_path.string();
    std::array<char const*, 3> address_return_helper_returned_address_success_argv {
        "orisonc",
        "--parse",
        address_return_helper_returned_address_success_path_text.c_str()
    };
    auto address_return_helper_returned_address_success_result = app.run(
        std::span<char const* const>(
            address_return_helper_returned_address_success_argv.data(),
            address_return_helper_returned_address_success_argv.size()
        )
    );

    assert(address_return_helper_returned_address_success_result.exit_code == 0);
    assert(address_return_helper_returned_address_success_result.stderr_text.empty());

    auto address_return_generic_helper_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_address_return_generic_helper_success.or";
    {
        std::ofstream output(address_return_generic_helper_success_path);
        output << "package demo.unsafe\n";
        output << "function id<T>(value: T) -> T\n";
        output << "    return value\n";
        output << "record Device\n";
        output << "    base: Address\n";
        output << "function read_base(device: Device) -> Address\n";
        output << "    return id(device.base)\n";
    }

    auto address_return_generic_helper_success_path_text =
        address_return_generic_helper_success_path.string();
    std::array<char const*, 3> address_return_generic_helper_success_argv {
        "orisonc",
        "--parse",
        address_return_generic_helper_success_path_text.c_str()
    };
    auto address_return_generic_helper_success_result = app.run(
        std::span<char const* const>(
            address_return_generic_helper_success_argv.data(),
            address_return_generic_helper_success_argv.size()
        )
    );

    assert(address_return_generic_helper_success_result.exit_code == 0);
    assert(address_return_generic_helper_success_result.stderr_text.empty());

    auto raw_write_generic_record_pointer_field_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_pointer_field_same_width_success.or";
    {
        std::ofstream output(raw_write_generic_record_pointer_field_same_width_success_path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    ptr: Pointer<T>\n";
        output << "unsafe function write_word(device: Device<Int32>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr, value)\n";
    }

    auto raw_write_generic_record_pointer_field_same_width_success_path_text =
        raw_write_generic_record_pointer_field_same_width_success_path.string();
    std::array<char const*, 3> raw_write_generic_record_pointer_field_same_width_success_argv {
        "orisonc",
        "--parse",
        raw_write_generic_record_pointer_field_same_width_success_path_text.c_str()
    };
    auto raw_write_generic_record_pointer_field_same_width_success_result = app.run(
        std::span<char const* const>(
            raw_write_generic_record_pointer_field_same_width_success_argv.data(),
            raw_write_generic_record_pointer_field_same_width_success_argv.size()
        )
    );

    assert(raw_write_generic_record_pointer_field_same_width_success_result.exit_code == 0);
    assert(raw_write_generic_record_pointer_field_same_width_success_result.stderr_text.empty());

    auto raw_write_generic_record_pointer_field_mismatch_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_pointer_field_mismatch_failure.or";
    {
        std::ofstream output(raw_write_generic_record_pointer_field_mismatch_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    ptr: Pointer<T>\n";
        output << "unsafe function write_word(device: Device<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr, value)\n";
    }

    auto raw_write_generic_record_pointer_field_mismatch_failure_path_text =
        raw_write_generic_record_pointer_field_mismatch_failure_path.string();
    std::array<char const*, 3> raw_write_generic_record_pointer_field_mismatch_failure_argv {
        "orisonc",
        "--parse",
        raw_write_generic_record_pointer_field_mismatch_failure_path_text.c_str()
    };
    auto raw_write_generic_record_pointer_field_mismatch_failure_result = app.run(
        std::span<char const* const>(
            raw_write_generic_record_pointer_field_mismatch_failure_argv.data(),
            raw_write_generic_record_pointer_field_mismatch_failure_argv.size()
        )
    );

    assert(raw_write_generic_record_pointer_field_mismatch_failure_result.exit_code == 1);
    assert(raw_write_generic_record_pointer_field_mismatch_failure_result.stdout_text.empty());
    assert(raw_write_generic_record_pointer_field_mismatch_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto raw_write_generic_record_scalar_field_same_width_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_generic_record_scalar_field_same_width_success.or";
    {
        std::ofstream output(raw_write_generic_record_scalar_field_same_width_success_path);
        output << "package demo.unsafe\n";
        output << "record Box<T>\n";
        output << "    value: T\n";
        output << "unsafe function write_word(box: Box<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, box.value)\n";
    }

    auto raw_write_generic_record_scalar_field_same_width_success_path_text =
        raw_write_generic_record_scalar_field_same_width_success_path.string();
    std::array<char const*, 3> raw_write_generic_record_scalar_field_same_width_success_argv {
        "orisonc",
        "--parse",
        raw_write_generic_record_scalar_field_same_width_success_path_text.c_str()
    };
    auto raw_write_generic_record_scalar_field_same_width_success_result = app.run(
        std::span<char const* const>(
            raw_write_generic_record_scalar_field_same_width_success_argv.data(),
            raw_write_generic_record_scalar_field_same_width_success_argv.size()
        )
    );

    assert(raw_write_generic_record_scalar_field_same_width_success_result.exit_code == 0);
    assert(raw_write_generic_record_scalar_field_same_width_success_result.stderr_text.empty());

    auto address_return_generic_record_field_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_address_return_generic_record_field_success.or";
    {
        std::ofstream output(address_return_generic_record_field_success_path);
        output << "package demo.unsafe\n";
        output << "record Box<T>\n";
        output << "    value: T\n";
        output << "function read_base(box: Box<Address>) -> Address\n";
        output << "    return box.value\n";
    }

    auto address_return_generic_record_field_success_path_text =
        address_return_generic_record_field_success_path.string();
    std::array<char const*, 3> address_return_generic_record_field_success_argv {
        "orisonc",
        "--parse",
        address_return_generic_record_field_success_path_text.c_str()
    };
    auto address_return_generic_record_field_success_result = app.run(
        std::span<char const* const>(
            address_return_generic_record_field_success_argv.data(),
            address_return_generic_record_field_success_argv.size()
        )
    );

    assert(address_return_generic_record_field_success_result.exit_code == 0);
    assert(address_return_generic_record_field_success_result.stderr_text.empty());

    auto raw_read_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_typed_binding_failure.or";
    {
        std::ofstream output(raw_read_typed_binding_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> UInt32\n";
        output << "    let value: UInt32 = raw_read(p)\n";
        output << "    return value\n";
    }

    auto raw_read_typed_binding_failure_path_text = raw_read_typed_binding_failure_path.string();
    std::array<char const*, 3> raw_read_typed_binding_failure_argv {
        "orisonc",
        "--parse",
        raw_read_typed_binding_failure_path_text.c_str()
    };
    auto raw_read_typed_binding_failure_result = app.run(
        std::span<char const* const>(
            raw_read_typed_binding_failure_argv.data(),
            raw_read_typed_binding_failure_argv.size()
        )
    );

    assert(raw_read_typed_binding_failure_result.exit_code == 1);
    assert(raw_read_typed_binding_failure_result.stdout_text.empty());
    assert(raw_read_typed_binding_failure_result.stderr_text.find(
               "raw_read result type 'Byte' does not match binding type 'UInt32'"
           ) != std::string::npos);

    auto raw_read_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_typed_binding_success.or";
    {
        std::ofstream output(raw_read_typed_binding_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(p: Pointer<Byte>) -> Byte\n";
        output << "    let value: Byte = raw_read(p)\n";
        output << "    return value\n";
    }

    auto raw_read_typed_binding_success_path_text = raw_read_typed_binding_success_path.string();
    std::array<char const*, 3> raw_read_typed_binding_success_argv {
        "orisonc",
        "--parse",
        raw_read_typed_binding_success_path_text.c_str()
    };
    auto raw_read_typed_binding_success_result = app.run(
        std::span<char const* const>(
            raw_read_typed_binding_success_argv.data(),
            raw_read_typed_binding_success_argv.size()
        )
    );

    assert(raw_read_typed_binding_success_result.exit_code == 0);
    assert(raw_read_typed_binding_success_result.stderr_text.empty());

    auto raw_read_same_width_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_same_width_binding_success.or";
    {
        std::ofstream output(raw_read_same_width_binding_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> Int32\n";
        output << "    let value: UInt32 = raw_read(p)\n";
        output << "    return value as Int32\n";
    }

    auto raw_read_same_width_binding_success_path_text = raw_read_same_width_binding_success_path.string();
    std::array<char const*, 3> raw_read_same_width_binding_success_argv {
        "orisonc",
        "--parse",
        raw_read_same_width_binding_success_path_text.c_str()
    };
    auto raw_read_same_width_binding_success_result = app.run(
        std::span<char const* const>(
            raw_read_same_width_binding_success_argv.data(),
            raw_read_same_width_binding_success_argv.size()
        )
    );

    assert(raw_read_same_width_binding_success_result.exit_code == 0);
    assert(raw_read_same_width_binding_success_result.stderr_text.empty());

    auto raw_read_return_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_return_type_failure.or";
    {
        std::ofstream output(raw_read_return_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_read(p)\n";
    }

    auto raw_read_return_type_failure_path_text = raw_read_return_type_failure_path.string();
    std::array<char const*, 3> raw_read_return_type_failure_argv {
        "orisonc",
        "--parse",
        raw_read_return_type_failure_path_text.c_str()
    };
    auto raw_read_return_type_failure_result = app.run(
        std::span<char const* const>(
            raw_read_return_type_failure_argv.data(),
            raw_read_return_type_failure_argv.size()
        )
    );

    assert(raw_read_return_type_failure_result.exit_code == 1);
    assert(raw_read_return_type_failure_result.stdout_text.empty());
    assert(raw_read_return_type_failure_result.stderr_text.find(
               "raw_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
           ) != std::string::npos);

    auto raw_read_same_width_return_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_read_same_width_return_success.or";
    {
        std::ofstream output(raw_read_same_width_return_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> UInt32\n";
        output << "    return raw_read(p)\n";
    }

    auto raw_read_same_width_return_success_path_text = raw_read_same_width_return_success_path.string();
    std::array<char const*, 3> raw_read_same_width_return_success_argv {
        "orisonc",
        "--parse",
        raw_read_same_width_return_success_path_text.c_str()
    };
    auto raw_read_same_width_return_success_result = app.run(
        std::span<char const* const>(
            raw_read_same_width_return_success_argv.data(),
            raw_read_same_width_return_success_argv.size()
        )
    );

    assert(raw_read_same_width_return_success_result.exit_code == 0);
    assert(raw_read_same_width_return_success_result.stderr_text.empty());

    auto raw_write_value_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_value_type_failure.or";
    {
        std::ofstream output(raw_write_value_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value)\n";
    }

    auto raw_write_value_type_failure_path_text = raw_write_value_type_failure_path.string();
    std::array<char const*, 3> raw_write_value_type_failure_argv {
        "orisonc",
        "--parse",
        raw_write_value_type_failure_path_text.c_str()
    };
    auto raw_write_value_type_failure_result = app.run(
        std::span<char const* const>(
            raw_write_value_type_failure_argv.data(),
            raw_write_value_type_failure_argv.size()
        )
    );

    assert(raw_write_value_type_failure_result.exit_code == 1);
    assert(raw_write_value_type_failure_result.stdout_text.empty());
    assert(raw_write_value_type_failure_result.stderr_text.find(
               "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_same_width_integer_value_success_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_raw_write_same_width_integer_value_success.or";
    {
        std::ofstream output(raw_write_same_width_integer_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    raw_write(p, value)\n";
    }

    auto raw_write_same_width_integer_value_success_path_text =
        raw_write_same_width_integer_value_success_path.string();
    std::array<char const*, 3> raw_write_same_width_integer_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_same_width_integer_value_success_path_text.c_str()
    };
    auto raw_write_same_width_integer_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_same_width_integer_value_success_argv.data(),
            raw_write_same_width_integer_value_success_argv.size()
        )
    );

    assert(raw_write_same_width_integer_value_success_result.exit_code == 0);
    assert(raw_write_same_width_integer_value_success_result.stderr_text.empty());

    auto raw_write_pointer_sized_integer_value_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_pointer_sized_integer_value_failure.or";
    {
        std::ofstream output(raw_write_pointer_sized_integer_value_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit\n";
        output << "    raw_write(p, value)\n";
    }

    auto raw_write_pointer_sized_integer_value_failure_path_text =
        raw_write_pointer_sized_integer_value_failure_path.string();
    std::array<char const*, 3> raw_write_pointer_sized_integer_value_failure_argv {
        "orisonc",
        "--parse",
        raw_write_pointer_sized_integer_value_failure_path_text.c_str()
    };
    auto raw_write_pointer_sized_integer_value_failure_result = app.run(
        std::span<char const* const>(
            raw_write_pointer_sized_integer_value_failure_argv.data(),
            raw_write_pointer_sized_integer_value_failure_argv.size()
        )
    );

    assert(raw_write_pointer_sized_integer_value_failure_result.exit_code == 1);
    assert(raw_write_pointer_sized_integer_value_failure_result.stdout_text.empty());
    assert(raw_write_pointer_sized_integer_value_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_computed_integer_sum_success_path = std::filesystem::temp_directory_path() /
                                                       "orison_compiler_app_raw_write_computed_integer_sum_success.or";
    {
        std::ofstream output(raw_write_computed_integer_sum_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(input) + 1)\n";
    }

    auto raw_write_computed_integer_sum_success_path_text =
        raw_write_computed_integer_sum_success_path.string();
    std::array<char const*, 3> raw_write_computed_integer_sum_success_argv {
        "orisonc",
        "--parse",
        raw_write_computed_integer_sum_success_path_text.c_str()
    };
    auto raw_write_computed_integer_sum_success_result = app.run(
        std::span<char const* const>(
            raw_write_computed_integer_sum_success_argv.data(),
            raw_write_computed_integer_sum_success_argv.size()
        )
    );

    assert(raw_write_computed_integer_sum_success_result.exit_code == 0);
    assert(raw_write_computed_integer_sum_success_result.stderr_text.empty());

    auto raw_write_computed_bitwise_value_success_path = std::filesystem::temp_directory_path() /
                                                         "orison_compiler_app_raw_write_computed_bitwise_value_success.or";
    {
        std::ofstream output(raw_write_computed_bitwise_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    raw_write(out, value bit_or 1)\n";
    }

    auto raw_write_computed_bitwise_value_success_path_text =
        raw_write_computed_bitwise_value_success_path.string();
    std::array<char const*, 3> raw_write_computed_bitwise_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_computed_bitwise_value_success_path_text.c_str()
    };
    auto raw_write_computed_bitwise_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_computed_bitwise_value_success_argv.data(),
            raw_write_computed_bitwise_value_success_argv.size()
        )
    );

    assert(raw_write_computed_bitwise_value_success_result.exit_code == 0);
    assert(raw_write_computed_bitwise_value_success_result.stderr_text.empty());

    auto raw_write_computed_ternary_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                 "orison_compiler_app_raw_write_computed_ternary_pointer_sized_failure.or";
    {
        std::ofstream output(raw_write_computed_ternary_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    raw_write(out, flag ? left : right)\n";
    }

    auto raw_write_computed_ternary_pointer_sized_failure_path_text =
        raw_write_computed_ternary_pointer_sized_failure_path.string();
    std::array<char const*, 3> raw_write_computed_ternary_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        raw_write_computed_ternary_pointer_sized_failure_path_text.c_str()
    };
    auto raw_write_computed_ternary_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            raw_write_computed_ternary_pointer_sized_failure_argv.data(),
            raw_write_computed_ternary_pointer_sized_failure_argv.size()
        )
    );

    assert(raw_write_computed_ternary_pointer_sized_failure_result.exit_code == 1);
    assert(raw_write_computed_ternary_pointer_sized_failure_result.stdout_text.empty());
    assert(raw_write_computed_ternary_pointer_sized_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_rebound_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                         "orison_compiler_app_raw_write_rebound_computed_value_success.or";
    {
        std::ofstream output(raw_write_rebound_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    let masked = value bit_or 1\n";
        output << "    raw_write(out, masked)\n";
    }

    auto raw_write_rebound_computed_value_success_path_text =
        raw_write_rebound_computed_value_success_path.string();
    std::array<char const*, 3> raw_write_rebound_computed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_rebound_computed_value_success_path_text.c_str()
    };
    auto raw_write_rebound_computed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_rebound_computed_value_success_argv.data(),
            raw_write_rebound_computed_value_success_argv.size()
        )
    );

    assert(raw_write_rebound_computed_value_success_result.exit_code == 0);
    assert(raw_write_rebound_computed_value_success_result.stderr_text.empty());

    auto raw_write_branch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                               "orison_compiler_app_raw_write_branch_merged_computed_value_success.or";
    {
        std::ofstream output(raw_write_branch_merged_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left bit_or 1\n";
        output << "    else\n";
        output << "        selected = right + 1\n";
        output << "    raw_write(out, selected)\n";
    }

    auto raw_write_branch_merged_computed_value_success_path_text =
        raw_write_branch_merged_computed_value_success_path.string();
    std::array<char const*, 3> raw_write_branch_merged_computed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_branch_merged_computed_value_success_path_text.c_str()
    };
    auto raw_write_branch_merged_computed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_branch_merged_computed_value_success_argv.data(),
            raw_write_branch_merged_computed_value_success_argv.size()
        )
    );

    assert(raw_write_branch_merged_computed_value_success_result.exit_code == 0);
    assert(raw_write_branch_merged_computed_value_success_result.stderr_text.empty());

    auto raw_write_branch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_branch_merged_pointer_sized_failure.or";
    {
        std::ofstream output(raw_write_branch_merged_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left + 1\n";
        output << "    else\n";
        output << "        selected = right shift_left 1\n";
        output << "    raw_write(out, selected)\n";
    }

    auto raw_write_branch_merged_pointer_sized_failure_path_text =
        raw_write_branch_merged_pointer_sized_failure_path.string();
    std::array<char const*, 3> raw_write_branch_merged_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        raw_write_branch_merged_pointer_sized_failure_path_text.c_str()
    };
    auto raw_write_branch_merged_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            raw_write_branch_merged_pointer_sized_failure_argv.data(),
            raw_write_branch_merged_pointer_sized_failure_argv.size()
        )
    );

    assert(raw_write_branch_merged_pointer_sized_failure_result.exit_code == 1);
    assert(raw_write_branch_merged_pointer_sized_failure_result.stdout_text.empty());
    assert(raw_write_branch_merged_pointer_sized_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_switch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                               "orison_compiler_app_raw_write_switch_merged_computed_value_success.or";
    {
        std::ofstream output(raw_write_switch_merged_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left bit_or 1\n";
        output << "        default => selected = right + 1\n";
        output << "    raw_write(out, selected)\n";
    }

    auto raw_write_switch_merged_computed_value_success_path_text =
        raw_write_switch_merged_computed_value_success_path.string();
    std::array<char const*, 3> raw_write_switch_merged_computed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_switch_merged_computed_value_success_path_text.c_str()
    };
    auto raw_write_switch_merged_computed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_switch_merged_computed_value_success_argv.data(),
            raw_write_switch_merged_computed_value_success_argv.size()
        )
    );

    assert(raw_write_switch_merged_computed_value_success_result.exit_code == 0);
    assert(raw_write_switch_merged_computed_value_success_result.stderr_text.empty());

    auto raw_write_switch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_switch_merged_pointer_sized_failure.or";
    {
        std::ofstream output(raw_write_switch_merged_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left + 1\n";
        output << "        default => selected = right shift_left 1\n";
        output << "    raw_write(out, selected)\n";
    }

    auto raw_write_switch_merged_pointer_sized_failure_path_text =
        raw_write_switch_merged_pointer_sized_failure_path.string();
    std::array<char const*, 3> raw_write_switch_merged_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        raw_write_switch_merged_pointer_sized_failure_path_text.c_str()
    };
    auto raw_write_switch_merged_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            raw_write_switch_merged_pointer_sized_failure_argv.data(),
            raw_write_switch_merged_pointer_sized_failure_argv.size()
        )
    );

    assert(raw_write_switch_merged_pointer_sized_failure_result.exit_code == 1);
    assert(raw_write_switch_merged_pointer_sized_failure_result.stdout_text.empty());
    assert(raw_write_switch_merged_pointer_sized_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_array_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                      "orison_compiler_app_raw_write_array_indexed_value_success.or";
    {
        std::ofstream output(raw_write_array_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit\n";
        output << "    raw_write(out, items[0])\n";
    }

    auto raw_write_array_indexed_value_success_path_text =
        raw_write_array_indexed_value_success_path.string();
    std::array<char const*, 3> raw_write_array_indexed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_array_indexed_value_success_path_text.c_str()
    };
    auto raw_write_array_indexed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_array_indexed_value_success_argv.data(),
            raw_write_array_indexed_value_success_argv.size()
        )
    );

    assert(raw_write_array_indexed_value_success_result.exit_code == 0);
    assert(raw_write_array_indexed_value_success_result.stderr_text.empty());

    auto raw_write_bound_array_literal_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_raw_write_bound_array_literal_indexed_value_success.or";
    {
        std::ofstream output(raw_write_bound_array_literal_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit\n";
        output << "    let staged = [left, right]\n";
        output << "    raw_write(out, staged[0])\n";
    }

    auto raw_write_bound_array_literal_indexed_value_success_path_text =
        raw_write_bound_array_literal_indexed_value_success_path.string();
    std::array<char const*, 3> raw_write_bound_array_literal_indexed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_bound_array_literal_indexed_value_success_path_text.c_str()
    };
    auto raw_write_bound_array_literal_indexed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_bound_array_literal_indexed_value_success_argv.data(),
            raw_write_bound_array_literal_indexed_value_success_argv.size()
        )
    );

    assert(raw_write_bound_array_literal_indexed_value_success_result.exit_code == 0);
    assert(raw_write_bound_array_literal_indexed_value_success_result.stderr_text.empty());

    auto raw_write_array_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_raw_write_array_indexed_pointer_sized_failure.or";
    {
        std::ofstream output(raw_write_array_indexed_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit\n";
        output << "    raw_write(out, items[0])\n";
    }

    auto raw_write_array_indexed_pointer_sized_failure_path_text =
        raw_write_array_indexed_pointer_sized_failure_path.string();
    std::array<char const*, 3> raw_write_array_indexed_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        raw_write_array_indexed_pointer_sized_failure_path_text.c_str()
    };
    auto raw_write_array_indexed_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            raw_write_array_indexed_pointer_sized_failure_argv.data(),
            raw_write_array_indexed_pointer_sized_failure_argv.size()
        )
    );

    assert(raw_write_array_indexed_pointer_sized_failure_result.exit_code == 1);
    assert(raw_write_array_indexed_pointer_sized_failure_result.stdout_text.empty());
    assert(raw_write_array_indexed_pointer_sized_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_member_container_field_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                       "orison_compiler_app_raw_write_member_container_field_indexed_value_success.or";
    {
        std::ofstream output(raw_write_member_container_field_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<Int32>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.words[0])\n";
    }

    auto raw_write_member_container_field_indexed_value_success_path_text =
        raw_write_member_container_field_indexed_value_success_path.string();
    std::array<char const*, 3> raw_write_member_container_field_indexed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_member_container_field_indexed_value_success_path_text.c_str()
    };
    auto raw_write_member_container_field_indexed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_member_container_field_indexed_value_success_argv.data(),
            raw_write_member_container_field_indexed_value_success_argv.size()
        )
    );

    assert(raw_write_member_container_field_indexed_value_success_result.exit_code == 0);
    assert(raw_write_member_container_field_indexed_value_success_result.stderr_text.empty());

    auto raw_write_nested_scalar_field_value_success_path = std::filesystem::temp_directory_path() /
                                                            "orison_compiler_app_raw_write_nested_scalar_field_value_success.or";
    {
        std::ofstream output(raw_write_nested_scalar_field_value_success_path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.registers.status)\n";
    }

    auto raw_write_nested_scalar_field_value_success_path_text =
        raw_write_nested_scalar_field_value_success_path.string();
    std::array<char const*, 3> raw_write_nested_scalar_field_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_nested_scalar_field_value_success_path_text.c_str()
    };
    auto raw_write_nested_scalar_field_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_nested_scalar_field_value_success_argv.data(),
            raw_write_nested_scalar_field_value_success_argv.size()
        )
    );

    assert(raw_write_nested_scalar_field_value_success_result.exit_code == 0);
    assert(raw_write_nested_scalar_field_value_success_result.stderr_text.empty());

    auto raw_write_nested_scalar_field_computed_value_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_nested_scalar_field_computed_value_success.or";
    {
        std::ofstream output(raw_write_nested_scalar_field_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.registers.status bit_or 1)\n";
    }

    auto raw_write_nested_scalar_field_computed_value_success_path_text =
        raw_write_nested_scalar_field_computed_value_success_path.string();
    std::array<char const*, 3> raw_write_nested_scalar_field_computed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_nested_scalar_field_computed_value_success_path_text.c_str()
    };
    auto raw_write_nested_scalar_field_computed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_nested_scalar_field_computed_value_success_argv.data(),
            raw_write_nested_scalar_field_computed_value_success_argv.size()
        )
    );

    assert(raw_write_nested_scalar_field_computed_value_success_result.exit_code == 0);
    assert(raw_write_nested_scalar_field_computed_value_success_result.stderr_text.empty());

    auto raw_write_helper_returned_container_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                          "orison_compiler_app_raw_write_helper_returned_container_indexed_value_success.or";
    {
        std::ofstream output(raw_write_helper_returned_container_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "function words() -> DynamicArray<Int32>\n";
        output << "    return []\n";
        output << "unsafe function write_word(out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, words()[0])\n";
    }

    auto raw_write_helper_returned_container_indexed_value_success_path_text =
        raw_write_helper_returned_container_indexed_value_success_path.string();
    std::array<char const*, 3> raw_write_helper_returned_container_indexed_value_success_argv {
        "orisonc",
        "--parse",
        raw_write_helper_returned_container_indexed_value_success_path_text.c_str()
    };
    auto raw_write_helper_returned_container_indexed_value_success_result = app.run(
        std::span<char const* const>(
            raw_write_helper_returned_container_indexed_value_success_argv.data(),
            raw_write_helper_returned_container_indexed_value_success_argv.size()
        )
    );

    assert(raw_write_helper_returned_container_indexed_value_success_result.exit_code == 0);
    assert(raw_write_helper_returned_container_indexed_value_success_result.stderr_text.empty());

    auto raw_write_member_container_field_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                               "orison_compiler_app_raw_write_member_container_field_indexed_pointer_sized_failure.or";
    {
        std::ofstream output(raw_write_member_container_field_indexed_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<IntSize>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.words[0])\n";
    }

    auto raw_write_member_container_field_indexed_pointer_sized_failure_path_text =
        raw_write_member_container_field_indexed_pointer_sized_failure_path.string();
    std::array<char const*, 3> raw_write_member_container_field_indexed_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        raw_write_member_container_field_indexed_pointer_sized_failure_path_text.c_str()
    };
    auto raw_write_member_container_field_indexed_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            raw_write_member_container_field_indexed_pointer_sized_failure_argv.data(),
            raw_write_member_container_field_indexed_pointer_sized_failure_argv.size()
        )
    );

    assert(raw_write_member_container_field_indexed_pointer_sized_failure_result.exit_code == 1);
    assert(raw_write_member_container_field_indexed_pointer_sized_failure_result.stdout_text.empty());
    assert(raw_write_member_container_field_indexed_pointer_sized_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_nested_scalar_field_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_raw_write_nested_scalar_field_pointer_sized_failure.or";
    {
        std::ofstream output(raw_write_nested_scalar_field_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: IntSize\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.registers.status)\n";
    }

    auto raw_write_nested_scalar_field_pointer_sized_failure_path_text =
        raw_write_nested_scalar_field_pointer_sized_failure_path.string();
    std::array<char const*, 3> raw_write_nested_scalar_field_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        raw_write_nested_scalar_field_pointer_sized_failure_path_text.c_str()
    };
    auto raw_write_nested_scalar_field_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            raw_write_nested_scalar_field_pointer_sized_failure_argv.data(),
            raw_write_nested_scalar_field_pointer_sized_failure_argv.size()
        )
    );

    assert(raw_write_nested_scalar_field_pointer_sized_failure_result.exit_code == 1);
    assert(raw_write_nested_scalar_field_pointer_sized_failure_result.stdout_text.empty());
    assert(raw_write_nested_scalar_field_pointer_sized_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_integer_literal_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_integer_literal_success.or";
    {
        std::ofstream output(raw_write_integer_literal_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(p, 0)\n";
    }

    auto raw_write_integer_literal_success_path_text = raw_write_integer_literal_success_path.string();
    std::array<char const*, 3> raw_write_integer_literal_success_argv {
        "orisonc",
        "--parse",
        raw_write_integer_literal_success_path_text.c_str()
    };
    auto raw_write_integer_literal_success_result = app.run(
        std::span<char const* const>(
            raw_write_integer_literal_success_argv.data(),
            raw_write_integer_literal_success_argv.size()
        )
    );

    assert(raw_write_integer_literal_success_result.exit_code == 0);
    assert(raw_write_integer_literal_success_result.stderr_text.empty());

    auto raw_write_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_integer_cast_success.or";
    {
        std::ofstream output(raw_write_integer_cast_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as UInt32)\n";
    }

    auto raw_write_integer_cast_success_path_text = raw_write_integer_cast_success_path.string();
    std::array<char const*, 3> raw_write_integer_cast_success_argv {
        "orisonc",
        "--parse",
        raw_write_integer_cast_success_path_text.c_str()
    };
    auto raw_write_integer_cast_success_result = app.run(
        std::span<char const* const>(
            raw_write_integer_cast_success_argv.data(),
            raw_write_integer_cast_success_argv.size()
        )
    );

    assert(raw_write_integer_cast_success_result.exit_code == 0);
    assert(raw_write_integer_cast_success_result.stderr_text.empty());

    auto raw_write_same_width_integer_cast_success_path = std::filesystem::temp_directory_path() /
                                                          "orison_compiler_app_raw_write_same_width_integer_cast_success.or";
    {
        std::ofstream output(raw_write_same_width_integer_cast_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as Int32)\n";
    }

    auto raw_write_same_width_integer_cast_success_path_text =
        raw_write_same_width_integer_cast_success_path.string();
    std::array<char const*, 3> raw_write_same_width_integer_cast_success_argv {
        "orisonc",
        "--parse",
        raw_write_same_width_integer_cast_success_path_text.c_str()
    };
    auto raw_write_same_width_integer_cast_success_result = app.run(
        std::span<char const* const>(
            raw_write_same_width_integer_cast_success_argv.data(),
            raw_write_same_width_integer_cast_success_argv.size()
        )
    );

    assert(raw_write_same_width_integer_cast_success_result.exit_code == 0);
    assert(raw_write_same_width_integer_cast_success_result.stderr_text.empty());

    auto raw_write_integer_cast_target_failure_path = std::filesystem::temp_directory_path() /
                                                      "orison_compiler_app_raw_write_integer_cast_target_failure.or";
    {
        std::ofstream output(raw_write_integer_cast_target_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as Int64)\n";
    }

    auto raw_write_integer_cast_target_failure_path_text = raw_write_integer_cast_target_failure_path.string();
    std::array<char const*, 3> raw_write_integer_cast_target_failure_argv {
        "orisonc",
        "--parse",
        raw_write_integer_cast_target_failure_path_text.c_str()
    };
    auto raw_write_integer_cast_target_failure_result = app.run(
        std::span<char const* const>(
            raw_write_integer_cast_target_failure_argv.data(),
            raw_write_integer_cast_target_failure_argv.size()
        )
    );

    assert(raw_write_integer_cast_target_failure_result.exit_code == 1);
    assert(raw_write_integer_cast_target_failure_result.stdout_text.empty());
    assert(raw_write_integer_cast_target_failure_result.stderr_text.find(
               "raw_write value type 'Int64' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_pointer_sized_integer_cast_failure_path = std::filesystem::temp_directory_path() /
                                                             "orison_compiler_app_raw_write_pointer_sized_integer_cast_failure.or";
    {
        std::ofstream output(raw_write_pointer_sized_integer_cast_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as IntSize)\n";
    }

    auto raw_write_pointer_sized_integer_cast_failure_path_text =
        raw_write_pointer_sized_integer_cast_failure_path.string();
    std::array<char const*, 3> raw_write_pointer_sized_integer_cast_failure_argv {
        "orisonc",
        "--parse",
        raw_write_pointer_sized_integer_cast_failure_path_text.c_str()
    };
    auto raw_write_pointer_sized_integer_cast_failure_result = app.run(
        std::span<char const* const>(
            raw_write_pointer_sized_integer_cast_failure_argv.data(),
            raw_write_pointer_sized_integer_cast_failure_argv.size()
        )
    );

    assert(raw_write_pointer_sized_integer_cast_failure_result.exit_code == 1);
    assert(raw_write_pointer_sized_integer_cast_failure_result.stdout_text.empty());
    assert(raw_write_pointer_sized_integer_cast_failure_result.stderr_text.find(
               "raw_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_recovered_raw_read_failure_path = std::filesystem::temp_directory_path() /
                                                     "orison_compiler_app_raw_write_recovered_raw_read_failure.or";
    {
        std::ofstream output(raw_write_recovered_raw_read_failure_path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))\n";
    }

    auto raw_write_recovered_raw_read_failure_path_text = raw_write_recovered_raw_read_failure_path.string();
    std::array<char const*, 3> raw_write_recovered_raw_read_failure_argv {
        "orisonc",
        "--parse",
        raw_write_recovered_raw_read_failure_path_text.c_str()
    };
    auto raw_write_recovered_raw_read_failure_result = app.run(
        std::span<char const* const>(
            raw_write_recovered_raw_read_failure_argv.data(),
            raw_write_recovered_raw_read_failure_argv.size()
        )
    );

    assert(raw_write_recovered_raw_read_failure_result.exit_code == 1);
    assert(raw_write_recovered_raw_read_failure_result.stdout_text.empty());
    assert(raw_write_recovered_raw_read_failure_result.stderr_text.find(
               "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_member_returned_raw_read_failure_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_raw_write_member_returned_raw_read_failure.or";
    {
        std::ofstream output(raw_write_member_returned_raw_read_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(raw_offset(device.byte_ptr(base), 1)))\n";
    }

    auto raw_write_member_returned_raw_read_failure_path_text =
        raw_write_member_returned_raw_read_failure_path.string();
    std::array<char const*, 3> raw_write_member_returned_raw_read_failure_argv {
        "orisonc",
        "--parse",
        raw_write_member_returned_raw_read_failure_path_text.c_str()
    };
    auto raw_write_member_returned_raw_read_failure_result = app.run(
        std::span<char const* const>(
            raw_write_member_returned_raw_read_failure_argv.data(),
            raw_write_member_returned_raw_read_failure_argv.size()
        )
    );

    assert(raw_write_member_returned_raw_read_failure_result.exit_code == 1);
    assert(raw_write_member_returned_raw_read_failure_result.stdout_text.empty());
    assert(raw_write_member_returned_raw_read_failure_result.stderr_text.find(
               "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto raw_write_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_helper_type_failure.or";
    {
        std::ofstream output(raw_write_helper_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>\n";
        output << "    return Pointer(addr)\n";
        output << "unsafe function write_word(addr: Address, value: UInt32) -> Unit\n";
        output << "    raw_write(byte_ptr(addr), value)\n";
    }

    auto raw_write_helper_type_failure_path_text = raw_write_helper_type_failure_path.string();
    std::array<char const*, 3> raw_write_helper_type_failure_argv {
        "orisonc",
        "--parse",
        raw_write_helper_type_failure_path_text.c_str()
    };
    auto raw_write_helper_type_failure_result = app.run(
        std::span<char const* const>(
            raw_write_helper_type_failure_argv.data(),
            raw_write_helper_type_failure_argv.size()
        )
    );

    assert(raw_write_helper_type_failure_result.exit_code == 1);
    assert(raw_write_helper_type_failure_result.stdout_text.empty());
    assert(raw_write_helper_type_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto raw_write_member_helper_type_failure_path = std::filesystem::temp_directory_path() /
                                                     "orison_compiler_app_raw_write_member_helper_type_failure.or";
    {
        std::ofstream output(raw_write_member_helper_type_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, addr: Address) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(addr)\n";
        output << "unsafe function write_word(device: Device, addr: Address, value: UInt32) -> Unit\n";
        output << "    raw_write(device.byte_ptr(addr), value)\n";
    }

    auto raw_write_member_helper_type_failure_path_text = raw_write_member_helper_type_failure_path.string();
    std::array<char const*, 3> raw_write_member_helper_type_failure_argv {
        "orisonc",
        "--parse",
        raw_write_member_helper_type_failure_path_text.c_str()
    };
    auto raw_write_member_helper_type_failure_result = app.run(
        std::span<char const* const>(
            raw_write_member_helper_type_failure_argv.data(),
            raw_write_member_helper_type_failure_argv.size()
        )
    );

    assert(raw_write_member_helper_type_failure_result.exit_code == 1);
    assert(raw_write_member_helper_type_failure_result.stdout_text.empty());
    assert(raw_write_member_helper_type_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto raw_write_raw_offset_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_raw_write_raw_offset_helper_type_failure.or";
    {
        std::ofstream output(raw_write_raw_offset_helper_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_offset(base, 1)\n";
        output << "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(next_byte_ptr(base), value)\n";
    }

    auto raw_write_raw_offset_helper_type_failure_path_text = raw_write_raw_offset_helper_type_failure_path.string();
    std::array<char const*, 3> raw_write_raw_offset_helper_type_failure_argv {
        "orisonc",
        "--parse",
        raw_write_raw_offset_helper_type_failure_path_text.c_str()
    };
    auto raw_write_raw_offset_helper_type_failure_result = app.run(
        std::span<char const* const>(
            raw_write_raw_offset_helper_type_failure_argv.data(),
            raw_write_raw_offset_helper_type_failure_argv.size()
        )
    );

    assert(raw_write_raw_offset_helper_type_failure_result.exit_code == 1);
    assert(raw_write_raw_offset_helper_type_failure_result.stdout_text.empty());
    assert(raw_write_raw_offset_helper_type_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto raw_write_member_raw_offset_helper_type_failure_path = std::filesystem::temp_directory_path() /
                                                                "orison_compiler_app_raw_write_member_raw_offset_helper_type_failure.or";
    {
        std::ofstream output(raw_write_member_raw_offset_helper_type_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function next_byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.next_byte_ptr(base), value)\n";
    }

    auto raw_write_member_raw_offset_helper_type_failure_path_text =
        raw_write_member_raw_offset_helper_type_failure_path.string();
    std::array<char const*, 3> raw_write_member_raw_offset_helper_type_failure_argv {
        "orisonc",
        "--parse",
        raw_write_member_raw_offset_helper_type_failure_path_text.c_str()
    };
    auto raw_write_member_raw_offset_helper_type_failure_result = app.run(
        std::span<char const* const>(
            raw_write_member_raw_offset_helper_type_failure_argv.data(),
            raw_write_member_raw_offset_helper_type_failure_argv.size()
        )
    );

    assert(raw_write_member_raw_offset_helper_type_failure_result.exit_code == 1);
    assert(raw_write_member_raw_offset_helper_type_failure_result.stdout_text.empty());
    assert(raw_write_member_raw_offset_helper_type_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto raw_write_record_pointer_field_type_failure_path = std::filesystem::temp_directory_path() /
                                                            "orison_compiler_app_raw_write_record_pointer_field_type_failure.or";
    {
        std::ofstream output(raw_write_record_pointer_field_type_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptr: Pointer<Byte>\n";
        output << "unsafe function write_word(device: Device, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr, value)\n";
    }

    auto raw_write_record_pointer_field_type_failure_path_text =
        raw_write_record_pointer_field_type_failure_path.string();
    std::array<char const*, 3> raw_write_record_pointer_field_type_failure_argv {
        "orisonc",
        "--parse",
        raw_write_record_pointer_field_type_failure_path_text.c_str()
    };
    auto raw_write_record_pointer_field_type_failure_result = app.run(
        std::span<char const* const>(
            raw_write_record_pointer_field_type_failure_argv.data(),
            raw_write_record_pointer_field_type_failure_argv.size()
        )
    );

    assert(raw_write_record_pointer_field_type_failure_result.exit_code == 1);
    assert(raw_write_record_pointer_field_type_failure_result.stdout_text.empty());
    assert(raw_write_record_pointer_field_type_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto member_field_address_pointer_constructor_success_path = std::filesystem::temp_directory_path() /
                                                                 "orison_compiler_app_member_field_address_pointer_constructor_success.or";
    {
        std::ofstream output(member_field_address_pointer_constructor_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    base: Address\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.base)\n";
        output << "unsafe function write_byte(device: Device, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(), value)\n";
    }

    auto member_field_address_pointer_constructor_success_path_text =
        member_field_address_pointer_constructor_success_path.string();
    std::array<char const*, 3> member_field_address_pointer_constructor_success_argv {
        "orisonc",
        "--parse",
        member_field_address_pointer_constructor_success_path_text.c_str()
    };
    auto member_field_address_pointer_constructor_success_result = app.run(
        std::span<char const* const>(
            member_field_address_pointer_constructor_success_argv.data(),
            member_field_address_pointer_constructor_success_argv.size()
        )
    );

    assert(member_field_address_pointer_constructor_success_result.exit_code == 0);
    assert(member_field_address_pointer_constructor_success_result.stderr_text.empty());

    auto raw_write_indexed_record_pointer_field_type_failure_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_raw_write_indexed_record_pointer_field_type_failure.or";
    {
        std::ofstream output(raw_write_indexed_record_pointer_field_type_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptrs[index], value)\n";
    }

    auto raw_write_indexed_record_pointer_field_type_failure_path_text =
        raw_write_indexed_record_pointer_field_type_failure_path.string();
    std::array<char const*, 3> raw_write_indexed_record_pointer_field_type_failure_argv {
        "orisonc",
        "--parse",
        raw_write_indexed_record_pointer_field_type_failure_path_text.c_str()
    };
    auto raw_write_indexed_record_pointer_field_type_failure_result = app.run(
        std::span<char const* const>(
            raw_write_indexed_record_pointer_field_type_failure_argv.data(),
            raw_write_indexed_record_pointer_field_type_failure_argv.size()
        )
    );

    assert(raw_write_indexed_record_pointer_field_type_failure_result.exit_code == 1);
    assert(raw_write_indexed_record_pointer_field_type_failure_result.stdout_text.empty());
    assert(raw_write_indexed_record_pointer_field_type_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto indexed_member_field_address_pointer_constructor_success_path = std::filesystem::temp_directory_path() /
                                                                         "orison_compiler_app_indexed_member_field_address_pointer_constructor_success.or";
    {
        std::ofstream output(indexed_member_field_address_pointer_constructor_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.bases[index])\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
    }

    auto indexed_member_field_address_pointer_constructor_success_path_text =
        indexed_member_field_address_pointer_constructor_success_path.string();
    std::array<char const*, 3> indexed_member_field_address_pointer_constructor_success_argv {
        "orisonc",
        "--parse",
        indexed_member_field_address_pointer_constructor_success_path_text.c_str()
    };
    auto indexed_member_field_address_pointer_constructor_success_result = app.run(
        std::span<char const* const>(
            indexed_member_field_address_pointer_constructor_success_argv.data(),
            indexed_member_field_address_pointer_constructor_success_argv.size()
        )
    );

    assert(indexed_member_field_address_pointer_constructor_success_result.exit_code == 0);
    assert(indexed_member_field_address_pointer_constructor_success_result.stderr_text.empty());

    auto rebound_indexed_record_pointer_field_type_failure_path = std::filesystem::temp_directory_path() /
                                                                  "orison_compiler_app_rebound_indexed_record_pointer_field_type_failure.or";
    {
        std::ofstream output(rebound_indexed_record_pointer_field_type_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit\n";
        output << "    let p = device.ptrs[index]\n";
        output << "    raw_write(p, value)\n";
    }

    auto rebound_indexed_record_pointer_field_type_failure_path_text =
        rebound_indexed_record_pointer_field_type_failure_path.string();
    std::array<char const*, 3> rebound_indexed_record_pointer_field_type_failure_argv {
        "orisonc",
        "--parse",
        rebound_indexed_record_pointer_field_type_failure_path_text.c_str()
    };
    auto rebound_indexed_record_pointer_field_type_failure_result = app.run(
        std::span<char const* const>(
            rebound_indexed_record_pointer_field_type_failure_argv.data(),
            rebound_indexed_record_pointer_field_type_failure_argv.size()
        )
    );

    assert(rebound_indexed_record_pointer_field_type_failure_result.exit_code == 1);
    assert(rebound_indexed_record_pointer_field_type_failure_result.stdout_text.empty());
    assert(rebound_indexed_record_pointer_field_type_failure_result.stderr_text.find(
               "raw_write value type 'UInt32' does not match pointer element type 'Byte'"
           ) != std::string::npos);

    auto rebound_indexed_member_field_address_pointer_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_rebound_indexed_member_field_address_pointer_constructor_success.or";
    {
        std::ofstream output(rebound_indexed_member_field_address_pointer_constructor_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            let base = this.bases[index]\n";
        output << "            return Pointer(base)\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
    }

    auto rebound_indexed_member_field_address_pointer_constructor_success_path_text =
        rebound_indexed_member_field_address_pointer_constructor_success_path.string();
    std::array<char const*, 3> rebound_indexed_member_field_address_pointer_constructor_success_argv {
        "orisonc",
        "--parse",
        rebound_indexed_member_field_address_pointer_constructor_success_path_text.c_str()
    };
    auto rebound_indexed_member_field_address_pointer_constructor_success_result = app.run(
        std::span<char const* const>(
            rebound_indexed_member_field_address_pointer_constructor_success_argv.data(),
            rebound_indexed_member_field_address_pointer_constructor_success_argv.size()
        )
    );

    assert(rebound_indexed_member_field_address_pointer_constructor_success_result.exit_code == 0);
    assert(rebound_indexed_member_field_address_pointer_constructor_success_result.stderr_text.empty());

    auto return_rebound_indexed_record_pointer_field_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_return_rebound_indexed_record_pointer_field_success.or";
    {
        std::ofstream output(return_rebound_indexed_record_pointer_field_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>\n";
        output << "    let p = device.ptrs[index]\n";
        output << "    return p\n";
    }

    auto return_rebound_indexed_record_pointer_field_success_path_text =
        return_rebound_indexed_record_pointer_field_success_path.string();
    std::array<char const*, 3> return_rebound_indexed_record_pointer_field_success_argv {
        "orisonc",
        "--parse",
        return_rebound_indexed_record_pointer_field_success_path_text.c_str()
    };
    auto return_rebound_indexed_record_pointer_field_success_result = app.run(
        std::span<char const* const>(
            return_rebound_indexed_record_pointer_field_success_argv.data(),
            return_rebound_indexed_record_pointer_field_success_argv.size()
        )
    );

    assert(return_rebound_indexed_record_pointer_field_success_result.exit_code == 0);
    assert(return_rebound_indexed_record_pointer_field_success_result.stderr_text.empty());

    auto return_rebound_indexed_member_field_address_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_return_rebound_indexed_member_field_address_success.or";
    {
        std::ofstream output(return_rebound_indexed_member_field_address_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function base_at(this: shared This, index: Int64) -> Address\n";
        output << "        let base = this.bases[index]\n";
        output << "        return base\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.base_at(index))\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
    }

    auto return_rebound_indexed_member_field_address_success_path_text =
        return_rebound_indexed_member_field_address_success_path.string();
    std::array<char const*, 3> return_rebound_indexed_member_field_address_success_argv {
        "orisonc",
        "--parse",
        return_rebound_indexed_member_field_address_success_path_text.c_str()
    };
    auto return_rebound_indexed_member_field_address_success_result = app.run(
        std::span<char const* const>(
            return_rebound_indexed_member_field_address_success_argv.data(),
            return_rebound_indexed_member_field_address_success_argv.size()
        )
    );

    assert(return_rebound_indexed_member_field_address_success_result.exit_code == 0);
    assert(return_rebound_indexed_member_field_address_success_result.stderr_text.empty());

    auto return_rebound_indexed_pointer_used_by_helper_success_path = std::filesystem::temp_directory_path() /
                                                                      "orison_compiler_app_return_rebound_indexed_pointer_used_by_helper_success.or";
    {
        std::ofstream output(return_rebound_indexed_pointer_used_by_helper_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>\n";
        output << "    let p = device.ptrs[index]\n";
        output << "    return p\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(byte_ptr(device, index), value)\n";
    }

    auto return_rebound_indexed_pointer_used_by_helper_success_path_text =
        return_rebound_indexed_pointer_used_by_helper_success_path.string();
    std::array<char const*, 3> return_rebound_indexed_pointer_used_by_helper_success_argv {
        "orisonc",
        "--parse",
        return_rebound_indexed_pointer_used_by_helper_success_path_text.c_str()
    };
    auto return_rebound_indexed_pointer_used_by_helper_success_result = app.run(
        std::span<char const* const>(
            return_rebound_indexed_pointer_used_by_helper_success_argv.data(),
            return_rebound_indexed_pointer_used_by_helper_success_argv.size()
        )
    );

    assert(return_rebound_indexed_pointer_used_by_helper_success_result.exit_code == 0);
    assert(return_rebound_indexed_pointer_used_by_helper_success_result.stderr_text.empty());

    auto return_rebound_indexed_address_pointer_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_return_rebound_indexed_address_pointer_constructor_success.or";
    {
        std::ofstream output(return_rebound_indexed_address_pointer_constructor_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function base_at(this: shared This, index: Int64) -> Address\n";
        output << "        let base = this.bases[index]\n";
        output << "        return base\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.base_at(index))\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
    }

    auto return_rebound_indexed_address_pointer_constructor_success_path_text =
        return_rebound_indexed_address_pointer_constructor_success_path.string();
    std::array<char const*, 3> return_rebound_indexed_address_pointer_constructor_success_argv {
        "orisonc",
        "--parse",
        return_rebound_indexed_address_pointer_constructor_success_path_text.c_str()
    };
    auto return_rebound_indexed_address_pointer_constructor_success_result = app.run(
        std::span<char const* const>(
            return_rebound_indexed_address_pointer_constructor_success_argv.data(),
            return_rebound_indexed_address_pointer_constructor_success_argv.size()
        )
    );

    assert(return_rebound_indexed_address_pointer_constructor_success_result.exit_code == 0);
    assert(return_rebound_indexed_address_pointer_constructor_success_result.stderr_text.empty());

    auto volatile_read_return_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_return_type_failure.or";
    {
        std::ofstream output(volatile_read_return_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return volatile_read(p)\n";
    }

    auto volatile_read_return_type_failure_path_text = volatile_read_return_type_failure_path.string();
    std::array<char const*, 3> volatile_read_return_type_failure_argv {
        "orisonc",
        "--parse",
        volatile_read_return_type_failure_path_text.c_str()
    };
    auto volatile_read_return_type_failure_result = app.run(
        std::span<char const* const>(
            volatile_read_return_type_failure_argv.data(),
            volatile_read_return_type_failure_argv.size()
        )
    );

    assert(volatile_read_return_type_failure_result.exit_code == 1);
    assert(volatile_read_return_type_failure_result.stdout_text.empty());
    assert(volatile_read_return_type_failure_result.stderr_text.find(
               "volatile_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
           ) != std::string::npos);

    auto volatile_read_same_width_return_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_same_width_return_success.or";
    {
        std::ofstream output(volatile_read_same_width_return_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> UInt32\n";
        output << "    return volatile_read(p)\n";
    }

    auto volatile_read_same_width_return_success_path_text =
        volatile_read_same_width_return_success_path.string();
    std::array<char const*, 3> volatile_read_same_width_return_success_argv {
        "orisonc",
        "--parse",
        volatile_read_same_width_return_success_path_text.c_str()
    };
    auto volatile_read_same_width_return_success_result = app.run(
        std::span<char const* const>(
            volatile_read_same_width_return_success_argv.data(),
            volatile_read_same_width_return_success_argv.size()
        )
    );

    assert(volatile_read_same_width_return_success_result.exit_code == 0);
    assert(volatile_read_same_width_return_success_result.stderr_text.empty());

    auto volatile_read_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_typed_binding_failure.or";
    {
        std::ofstream output(volatile_read_typed_binding_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> UInt32\n";
        output << "    let value: UInt32 = volatile_read(p)\n";
        output << "    return value\n";
    }

    auto volatile_read_typed_binding_failure_path_text = volatile_read_typed_binding_failure_path.string();
    std::array<char const*, 3> volatile_read_typed_binding_failure_argv {
        "orisonc",
        "--parse",
        volatile_read_typed_binding_failure_path_text.c_str()
    };
    auto volatile_read_typed_binding_failure_result = app.run(
        std::span<char const* const>(
            volatile_read_typed_binding_failure_argv.data(),
            volatile_read_typed_binding_failure_argv.size()
        )
    );

    assert(volatile_read_typed_binding_failure_result.exit_code == 1);
    assert(volatile_read_typed_binding_failure_result.stdout_text.empty());
    assert(volatile_read_typed_binding_failure_result.stderr_text.find(
               "volatile_read result type 'Byte' does not match binding type 'UInt32'"
           ) != std::string::npos);

    auto volatile_read_same_width_binding_success_path = std::filesystem::temp_directory_path() /
                                                         "orison_compiler_app_volatile_read_same_width_binding_success.or";
    {
        std::ofstream output(volatile_read_same_width_binding_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> Int32\n";
        output << "    let value: UInt32 = volatile_read(p)\n";
        output << "    return value as Int32\n";
    }

    auto volatile_read_same_width_binding_success_path_text =
        volatile_read_same_width_binding_success_path.string();
    std::array<char const*, 3> volatile_read_same_width_binding_success_argv {
        "orisonc",
        "--parse",
        volatile_read_same_width_binding_success_path_text.c_str()
    };
    auto volatile_read_same_width_binding_success_result = app.run(
        std::span<char const* const>(
            volatile_read_same_width_binding_success_argv.data(),
            volatile_read_same_width_binding_success_argv.size()
        )
    );

    assert(volatile_read_same_width_binding_success_result.exit_code == 0);
    assert(volatile_read_same_width_binding_success_result.stderr_text.empty());

    auto volatile_read_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_read_typed_binding_success.or";
    {
        std::ofstream output(volatile_read_typed_binding_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(p: Pointer<Byte>) -> Byte\n";
        output << "    let value: Byte = volatile_read(p)\n";
        output << "    return value\n";
    }

    auto volatile_read_typed_binding_success_path_text = volatile_read_typed_binding_success_path.string();
    std::array<char const*, 3> volatile_read_typed_binding_success_argv {
        "orisonc",
        "--parse",
        volatile_read_typed_binding_success_path_text.c_str()
    };
    auto volatile_read_typed_binding_success_result = app.run(
        std::span<char const* const>(
            volatile_read_typed_binding_success_argv.data(),
            volatile_read_typed_binding_success_argv.size()
        )
    );

    assert(volatile_read_typed_binding_success_result.exit_code == 0);
    assert(volatile_read_typed_binding_success_result.stderr_text.empty());

    auto volatile_write_value_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_value_type_failure.or";
    {
        std::ofstream output(volatile_write_value_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value)\n";
    }

    auto volatile_write_value_type_failure_path_text = volatile_write_value_type_failure_path.string();
    std::array<char const*, 3> volatile_write_value_type_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_value_type_failure_path_text.c_str()
    };
    auto volatile_write_value_type_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_value_type_failure_argv.data(),
            volatile_write_value_type_failure_argv.size()
        )
    );

    assert(volatile_write_value_type_failure_result.exit_code == 1);
    assert(volatile_write_value_type_failure_result.stdout_text.empty());
    assert(volatile_write_value_type_failure_result.stderr_text.find(
               "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_same_width_integer_value_success_path = std::filesystem::temp_directory_path() /
                                                                "orison_compiler_app_volatile_write_same_width_integer_value_success.or";
    {
        std::ofstream output(volatile_write_same_width_integer_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    volatile_write(p, value)\n";
    }

    auto volatile_write_same_width_integer_value_success_path_text =
        volatile_write_same_width_integer_value_success_path.string();
    std::array<char const*, 3> volatile_write_same_width_integer_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_same_width_integer_value_success_path_text.c_str()
    };
    auto volatile_write_same_width_integer_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_same_width_integer_value_success_argv.data(),
            volatile_write_same_width_integer_value_success_argv.size()
        )
    );

    assert(volatile_write_same_width_integer_value_success_result.exit_code == 0);
    assert(volatile_write_same_width_integer_value_success_result.stderr_text.empty());

    auto volatile_write_pointer_sized_integer_value_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_pointer_sized_integer_value_failure.or";
    {
        std::ofstream output(volatile_write_pointer_sized_integer_value_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit\n";
        output << "    volatile_write(p, value)\n";
    }

    auto volatile_write_pointer_sized_integer_value_failure_path_text =
        volatile_write_pointer_sized_integer_value_failure_path.string();
    std::array<char const*, 3> volatile_write_pointer_sized_integer_value_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_pointer_sized_integer_value_failure_path_text.c_str()
    };
    auto volatile_write_pointer_sized_integer_value_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_pointer_sized_integer_value_failure_argv.data(),
            volatile_write_pointer_sized_integer_value_failure_argv.size()
        )
    );

    assert(volatile_write_pointer_sized_integer_value_failure_result.exit_code == 1);
    assert(volatile_write_pointer_sized_integer_value_failure_result.stdout_text.empty());
    assert(volatile_write_pointer_sized_integer_value_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_computed_integer_sum_success_path = std::filesystem::temp_directory_path() /
                                                            "orison_compiler_app_volatile_write_computed_integer_sum_success.or";
    {
        std::ofstream output(volatile_write_computed_integer_sum_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(input) + 1)\n";
    }

    auto volatile_write_computed_integer_sum_success_path_text =
        volatile_write_computed_integer_sum_success_path.string();
    std::array<char const*, 3> volatile_write_computed_integer_sum_success_argv {
        "orisonc",
        "--parse",
        volatile_write_computed_integer_sum_success_path_text.c_str()
    };
    auto volatile_write_computed_integer_sum_success_result = app.run(
        std::span<char const* const>(
            volatile_write_computed_integer_sum_success_argv.data(),
            volatile_write_computed_integer_sum_success_argv.size()
        )
    );

    assert(volatile_write_computed_integer_sum_success_result.exit_code == 0);
    assert(volatile_write_computed_integer_sum_success_result.stderr_text.empty());

    auto volatile_write_computed_bitwise_value_success_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_volatile_write_computed_bitwise_value_success.or";
    {
        std::ofstream output(volatile_write_computed_bitwise_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    volatile_write(out, value bit_or 1)\n";
    }

    auto volatile_write_computed_bitwise_value_success_path_text =
        volatile_write_computed_bitwise_value_success_path.string();
    std::array<char const*, 3> volatile_write_computed_bitwise_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_computed_bitwise_value_success_path_text.c_str()
    };
    auto volatile_write_computed_bitwise_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_computed_bitwise_value_success_argv.data(),
            volatile_write_computed_bitwise_value_success_argv.size()
        )
    );

    assert(volatile_write_computed_bitwise_value_success_result.exit_code == 0);
    assert(volatile_write_computed_bitwise_value_success_result.stderr_text.empty());

    auto volatile_write_computed_ternary_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                      "orison_compiler_app_volatile_write_computed_ternary_pointer_sized_failure.or";
    {
        std::ofstream output(volatile_write_computed_ternary_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    volatile_write(out, flag ? left : right)\n";
    }

    auto volatile_write_computed_ternary_pointer_sized_failure_path_text =
        volatile_write_computed_ternary_pointer_sized_failure_path.string();
    std::array<char const*, 3> volatile_write_computed_ternary_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_computed_ternary_pointer_sized_failure_path_text.c_str()
    };
    auto volatile_write_computed_ternary_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_computed_ternary_pointer_sized_failure_argv.data(),
            volatile_write_computed_ternary_pointer_sized_failure_argv.size()
        )
    );

    assert(volatile_write_computed_ternary_pointer_sized_failure_result.exit_code == 1);
    assert(volatile_write_computed_ternary_pointer_sized_failure_result.stdout_text.empty());
    assert(volatile_write_computed_ternary_pointer_sized_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_rebound_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_volatile_write_rebound_computed_value_success.or";
    {
        std::ofstream output(volatile_write_rebound_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    let masked = value bit_or 1\n";
        output << "    volatile_write(out, masked)\n";
    }

    auto volatile_write_rebound_computed_value_success_path_text =
        volatile_write_rebound_computed_value_success_path.string();
    std::array<char const*, 3> volatile_write_rebound_computed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_rebound_computed_value_success_path_text.c_str()
    };
    auto volatile_write_rebound_computed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_rebound_computed_value_success_argv.data(),
            volatile_write_rebound_computed_value_success_argv.size()
        )
    );

    assert(volatile_write_rebound_computed_value_success_result.exit_code == 0);
    assert(volatile_write_rebound_computed_value_success_result.stderr_text.empty());

    auto volatile_write_branch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_volatile_write_branch_merged_computed_value_success.or";
    {
        std::ofstream output(volatile_write_branch_merged_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left bit_or 1\n";
        output << "    else\n";
        output << "        selected = right + 1\n";
        output << "    volatile_write(out, selected)\n";
    }

    auto volatile_write_branch_merged_computed_value_success_path_text =
        volatile_write_branch_merged_computed_value_success_path.string();
    std::array<char const*, 3> volatile_write_branch_merged_computed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_branch_merged_computed_value_success_path_text.c_str()
    };
    auto volatile_write_branch_merged_computed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_branch_merged_computed_value_success_argv.data(),
            volatile_write_branch_merged_computed_value_success_argv.size()
        )
    );

    assert(volatile_write_branch_merged_computed_value_success_result.exit_code == 0);
    assert(volatile_write_branch_merged_computed_value_success_result.stderr_text.empty());

    auto volatile_write_branch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_branch_merged_pointer_sized_failure.or";
    {
        std::ofstream output(volatile_write_branch_merged_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left + 1\n";
        output << "    else\n";
        output << "        selected = right shift_left 1\n";
        output << "    volatile_write(out, selected)\n";
    }

    auto volatile_write_branch_merged_pointer_sized_failure_path_text =
        volatile_write_branch_merged_pointer_sized_failure_path.string();
    std::array<char const*, 3> volatile_write_branch_merged_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_branch_merged_pointer_sized_failure_path_text.c_str()
    };
    auto volatile_write_branch_merged_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_branch_merged_pointer_sized_failure_argv.data(),
            volatile_write_branch_merged_pointer_sized_failure_argv.size()
        )
    );

    assert(volatile_write_branch_merged_pointer_sized_failure_result.exit_code == 1);
    assert(volatile_write_branch_merged_pointer_sized_failure_result.stdout_text.empty());
    assert(volatile_write_branch_merged_pointer_sized_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_switch_merged_computed_value_success_path = std::filesystem::temp_directory_path() /
                                                                    "orison_compiler_app_volatile_write_switch_merged_computed_value_success.or";
    {
        std::ofstream output(volatile_write_switch_merged_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left bit_or 1\n";
        output << "        default => selected = right + 1\n";
        output << "    volatile_write(out, selected)\n";
    }

    auto volatile_write_switch_merged_computed_value_success_path_text =
        volatile_write_switch_merged_computed_value_success_path.string();
    std::array<char const*, 3> volatile_write_switch_merged_computed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_switch_merged_computed_value_success_path_text.c_str()
    };
    auto volatile_write_switch_merged_computed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_switch_merged_computed_value_success_argv.data(),
            volatile_write_switch_merged_computed_value_success_argv.size()
        )
    );

    assert(volatile_write_switch_merged_computed_value_success_result.exit_code == 0);
    assert(volatile_write_switch_merged_computed_value_success_result.stderr_text.empty());

    auto volatile_write_switch_merged_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_switch_merged_pointer_sized_failure.or";
    {
        std::ofstream output(volatile_write_switch_merged_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left + 1\n";
        output << "        default => selected = right shift_left 1\n";
        output << "    volatile_write(out, selected)\n";
    }

    auto volatile_write_switch_merged_pointer_sized_failure_path_text =
        volatile_write_switch_merged_pointer_sized_failure_path.string();
    std::array<char const*, 3> volatile_write_switch_merged_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_switch_merged_pointer_sized_failure_path_text.c_str()
    };
    auto volatile_write_switch_merged_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_switch_merged_pointer_sized_failure_argv.data(),
            volatile_write_switch_merged_pointer_sized_failure_argv.size()
        )
    );

    assert(volatile_write_switch_merged_pointer_sized_failure_result.exit_code == 1);
    assert(volatile_write_switch_merged_pointer_sized_failure_result.stdout_text.empty());
    assert(volatile_write_switch_merged_pointer_sized_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_array_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_volatile_write_array_indexed_value_success.or";
    {
        std::ofstream output(volatile_write_array_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit\n";
        output << "    volatile_write(out, items[0])\n";
    }

    auto volatile_write_array_indexed_value_success_path_text =
        volatile_write_array_indexed_value_success_path.string();
    std::array<char const*, 3> volatile_write_array_indexed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_array_indexed_value_success_path_text.c_str()
    };
    auto volatile_write_array_indexed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_array_indexed_value_success_argv.data(),
            volatile_write_array_indexed_value_success_argv.size()
        )
    );

    assert(volatile_write_array_indexed_value_success_result.exit_code == 0);
    assert(volatile_write_array_indexed_value_success_result.stderr_text.empty());

    auto volatile_write_bound_array_literal_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                         "orison_compiler_app_volatile_write_bound_array_literal_indexed_value_success.or";
    {
        std::ofstream output(volatile_write_bound_array_literal_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit\n";
        output << "    let staged = [left, right]\n";
        output << "    volatile_write(out, staged[0])\n";
    }

    auto volatile_write_bound_array_literal_indexed_value_success_path_text =
        volatile_write_bound_array_literal_indexed_value_success_path.string();
    std::array<char const*, 3> volatile_write_bound_array_literal_indexed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_bound_array_literal_indexed_value_success_path_text.c_str()
    };
    auto volatile_write_bound_array_literal_indexed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_bound_array_literal_indexed_value_success_argv.data(),
            volatile_write_bound_array_literal_indexed_value_success_argv.size()
        )
    );

    assert(volatile_write_bound_array_literal_indexed_value_success_result.exit_code == 0);
    assert(volatile_write_bound_array_literal_indexed_value_success_result.stderr_text.empty());

    auto volatile_write_array_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                   "orison_compiler_app_volatile_write_array_indexed_pointer_sized_failure.or";
    {
        std::ofstream output(volatile_write_array_indexed_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit\n";
        output << "    volatile_write(out, items[0])\n";
    }

    auto volatile_write_array_indexed_pointer_sized_failure_path_text =
        volatile_write_array_indexed_pointer_sized_failure_path.string();
    std::array<char const*, 3> volatile_write_array_indexed_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_array_indexed_pointer_sized_failure_path_text.c_str()
    };
    auto volatile_write_array_indexed_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_array_indexed_pointer_sized_failure_argv.data(),
            volatile_write_array_indexed_pointer_sized_failure_argv.size()
        )
    );

    assert(volatile_write_array_indexed_pointer_sized_failure_result.exit_code == 1);
    assert(volatile_write_array_indexed_pointer_sized_failure_result.stdout_text.empty());
    assert(volatile_write_array_indexed_pointer_sized_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_member_container_field_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                            "orison_compiler_app_volatile_write_member_container_field_indexed_value_success.or";
    {
        std::ofstream output(volatile_write_member_container_field_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<Int32>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.words[0])\n";
    }

    auto volatile_write_member_container_field_indexed_value_success_path_text =
        volatile_write_member_container_field_indexed_value_success_path.string();
    std::array<char const*, 3> volatile_write_member_container_field_indexed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_member_container_field_indexed_value_success_path_text.c_str()
    };
    auto volatile_write_member_container_field_indexed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_member_container_field_indexed_value_success_argv.data(),
            volatile_write_member_container_field_indexed_value_success_argv.size()
        )
    );

    assert(volatile_write_member_container_field_indexed_value_success_result.exit_code == 0);
    assert(volatile_write_member_container_field_indexed_value_success_result.stderr_text.empty());

    auto volatile_write_nested_scalar_field_value_success_path = std::filesystem::temp_directory_path() /
                                                                 "orison_compiler_app_volatile_write_nested_scalar_field_value_success.or";
    {
        std::ofstream output(volatile_write_nested_scalar_field_value_success_path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.registers.status)\n";
    }

    auto volatile_write_nested_scalar_field_value_success_path_text =
        volatile_write_nested_scalar_field_value_success_path.string();
    std::array<char const*, 3> volatile_write_nested_scalar_field_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_nested_scalar_field_value_success_path_text.c_str()
    };
    auto volatile_write_nested_scalar_field_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_nested_scalar_field_value_success_argv.data(),
            volatile_write_nested_scalar_field_value_success_argv.size()
        )
    );

    assert(volatile_write_nested_scalar_field_value_success_result.exit_code == 0);
    assert(volatile_write_nested_scalar_field_value_success_result.stderr_text.empty());

    auto volatile_write_nested_scalar_field_computed_value_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_volatile_write_nested_scalar_field_computed_value_success.or";
    {
        std::ofstream output(volatile_write_nested_scalar_field_computed_value_success_path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.registers.status bit_or 1)\n";
    }

    auto volatile_write_nested_scalar_field_computed_value_success_path_text =
        volatile_write_nested_scalar_field_computed_value_success_path.string();
    std::array<char const*, 3> volatile_write_nested_scalar_field_computed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_nested_scalar_field_computed_value_success_path_text.c_str()
    };
    auto volatile_write_nested_scalar_field_computed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_nested_scalar_field_computed_value_success_argv.data(),
            volatile_write_nested_scalar_field_computed_value_success_argv.size()
        )
    );

    assert(volatile_write_nested_scalar_field_computed_value_success_result.exit_code == 0);
    assert(volatile_write_nested_scalar_field_computed_value_success_result.stderr_text.empty());

    auto volatile_write_helper_returned_container_indexed_value_success_path = std::filesystem::temp_directory_path() /
                                                                               "orison_compiler_app_volatile_write_helper_returned_container_indexed_value_success.or";
    {
        std::ofstream output(volatile_write_helper_returned_container_indexed_value_success_path);
        output << "package demo.unsafe\n";
        output << "function words() -> DynamicArray<Int32>\n";
        output << "    return []\n";
        output << "unsafe function write_word(out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, words()[0])\n";
    }

    auto volatile_write_helper_returned_container_indexed_value_success_path_text =
        volatile_write_helper_returned_container_indexed_value_success_path.string();
    std::array<char const*, 3> volatile_write_helper_returned_container_indexed_value_success_argv {
        "orisonc",
        "--parse",
        volatile_write_helper_returned_container_indexed_value_success_path_text.c_str()
    };
    auto volatile_write_helper_returned_container_indexed_value_success_result = app.run(
        std::span<char const* const>(
            volatile_write_helper_returned_container_indexed_value_success_argv.data(),
            volatile_write_helper_returned_container_indexed_value_success_argv.size()
        )
    );

    assert(volatile_write_helper_returned_container_indexed_value_success_result.exit_code == 0);
    assert(volatile_write_helper_returned_container_indexed_value_success_result.stderr_text.empty());

    auto volatile_write_member_container_field_indexed_pointer_sized_failure_path = std::filesystem::temp_directory_path() /
                                                                                    "orison_compiler_app_volatile_write_member_container_field_indexed_pointer_sized_failure.or";
    {
        std::ofstream output(volatile_write_member_container_field_indexed_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<IntSize>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.words[0])\n";
    }

    auto volatile_write_member_container_field_indexed_pointer_sized_failure_path_text =
        volatile_write_member_container_field_indexed_pointer_sized_failure_path.string();
    std::array<char const*, 3> volatile_write_member_container_field_indexed_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_member_container_field_indexed_pointer_sized_failure_path_text.c_str()
    };
    auto volatile_write_member_container_field_indexed_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_member_container_field_indexed_pointer_sized_failure_argv.data(),
            volatile_write_member_container_field_indexed_pointer_sized_failure_argv.size()
        )
    );

    assert(volatile_write_member_container_field_indexed_pointer_sized_failure_result.exit_code == 1);
    assert(volatile_write_member_container_field_indexed_pointer_sized_failure_result.stdout_text.empty());
    assert(volatile_write_member_container_field_indexed_pointer_sized_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_nested_scalar_field_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_volatile_write_nested_scalar_field_pointer_sized_failure.or";
    {
        std::ofstream output(volatile_write_nested_scalar_field_pointer_sized_failure_path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: IntSize\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.registers.status)\n";
    }

    auto volatile_write_nested_scalar_field_pointer_sized_failure_path_text =
        volatile_write_nested_scalar_field_pointer_sized_failure_path.string();
    std::array<char const*, 3> volatile_write_nested_scalar_field_pointer_sized_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_nested_scalar_field_pointer_sized_failure_path_text.c_str()
    };
    auto volatile_write_nested_scalar_field_pointer_sized_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_nested_scalar_field_pointer_sized_failure_argv.data(),
            volatile_write_nested_scalar_field_pointer_sized_failure_argv.size()
        )
    );

    assert(volatile_write_nested_scalar_field_pointer_sized_failure_result.exit_code == 1);
    assert(volatile_write_nested_scalar_field_pointer_sized_failure_result.stdout_text.empty());
    assert(volatile_write_nested_scalar_field_pointer_sized_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_integer_literal_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_integer_literal_success.or";
    {
        std::ofstream output(volatile_write_integer_literal_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(p, 0)\n";
    }

    auto volatile_write_integer_literal_success_path_text = volatile_write_integer_literal_success_path.string();
    std::array<char const*, 3> volatile_write_integer_literal_success_argv {
        "orisonc",
        "--parse",
        volatile_write_integer_literal_success_path_text.c_str()
    };
    auto volatile_write_integer_literal_success_result = app.run(
        std::span<char const* const>(
            volatile_write_integer_literal_success_argv.data(),
            volatile_write_integer_literal_success_argv.size()
        )
    );

    assert(volatile_write_integer_literal_success_result.exit_code == 0);
    assert(volatile_write_integer_literal_success_result.stderr_text.empty());

    auto volatile_write_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_integer_cast_success.or";
    {
        std::ofstream output(volatile_write_integer_cast_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as UInt32)\n";
    }

    auto volatile_write_integer_cast_success_path_text = volatile_write_integer_cast_success_path.string();
    std::array<char const*, 3> volatile_write_integer_cast_success_argv {
        "orisonc",
        "--parse",
        volatile_write_integer_cast_success_path_text.c_str()
    };
    auto volatile_write_integer_cast_success_result = app.run(
        std::span<char const* const>(
            volatile_write_integer_cast_success_argv.data(),
            volatile_write_integer_cast_success_argv.size()
        )
    );

    assert(volatile_write_integer_cast_success_result.exit_code == 0);
    assert(volatile_write_integer_cast_success_result.stderr_text.empty());

    auto volatile_write_same_width_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_same_width_integer_cast_success.or";
    {
        std::ofstream output(volatile_write_same_width_integer_cast_success_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as Int32)\n";
    }

    auto volatile_write_same_width_integer_cast_success_path_text =
        volatile_write_same_width_integer_cast_success_path.string();
    std::array<char const*, 3> volatile_write_same_width_integer_cast_success_argv {
        "orisonc",
        "--parse",
        volatile_write_same_width_integer_cast_success_path_text.c_str()
    };
    auto volatile_write_same_width_integer_cast_success_result = app.run(
        std::span<char const* const>(
            volatile_write_same_width_integer_cast_success_argv.data(),
            volatile_write_same_width_integer_cast_success_argv.size()
        )
    );

    assert(volatile_write_same_width_integer_cast_success_result.exit_code == 0);
    assert(volatile_write_same_width_integer_cast_success_result.stderr_text.empty());

    auto volatile_write_integer_cast_target_failure_path = std::filesystem::temp_directory_path() /
                                                           "orison_compiler_app_volatile_write_integer_cast_target_failure.or";
    {
        std::ofstream output(volatile_write_integer_cast_target_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as Int64)\n";
    }

    auto volatile_write_integer_cast_target_failure_path_text =
        volatile_write_integer_cast_target_failure_path.string();
    std::array<char const*, 3> volatile_write_integer_cast_target_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_integer_cast_target_failure_path_text.c_str()
    };
    auto volatile_write_integer_cast_target_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_integer_cast_target_failure_argv.data(),
            volatile_write_integer_cast_target_failure_argv.size()
        )
    );

    assert(volatile_write_integer_cast_target_failure_result.exit_code == 1);
    assert(volatile_write_integer_cast_target_failure_result.stdout_text.empty());
    assert(volatile_write_integer_cast_target_failure_result.stderr_text.find(
               "volatile_write value type 'Int64' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_pointer_sized_integer_cast_failure_path = std::filesystem::temp_directory_path() /
                                                                  "orison_compiler_app_volatile_write_pointer_sized_integer_cast_failure.or";
    {
        std::ofstream output(volatile_write_pointer_sized_integer_cast_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as IntSize)\n";
    }

    auto volatile_write_pointer_sized_integer_cast_failure_path_text =
        volatile_write_pointer_sized_integer_cast_failure_path.string();
    std::array<char const*, 3> volatile_write_pointer_sized_integer_cast_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_pointer_sized_integer_cast_failure_path_text.c_str()
    };
    auto volatile_write_pointer_sized_integer_cast_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_pointer_sized_integer_cast_failure_argv.data(),
            volatile_write_pointer_sized_integer_cast_failure_argv.size()
        )
    );

    assert(volatile_write_pointer_sized_integer_cast_failure_result.exit_code == 1);
    assert(volatile_write_pointer_sized_integer_cast_failure_result.stdout_text.empty());
    assert(volatile_write_pointer_sized_integer_cast_failure_result.stderr_text.find(
               "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_recovered_volatile_read_failure_path = std::filesystem::temp_directory_path() /
                                                               "orison_compiler_app_volatile_write_recovered_volatile_read_failure.or";
    {
        std::ofstream output(volatile_write_recovered_volatile_read_failure_path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))\n";
    }

    auto volatile_write_recovered_volatile_read_failure_path_text =
        volatile_write_recovered_volatile_read_failure_path.string();
    std::array<char const*, 3> volatile_write_recovered_volatile_read_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_recovered_volatile_read_failure_path_text.c_str()
    };
    auto volatile_write_recovered_volatile_read_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_recovered_volatile_read_failure_argv.data(),
            volatile_write_recovered_volatile_read_failure_argv.size()
        )
    );

    assert(volatile_write_recovered_volatile_read_failure_result.exit_code == 1);
    assert(volatile_write_recovered_volatile_read_failure_result.stdout_text.empty());
    assert(volatile_write_recovered_volatile_read_failure_result.stderr_text.find(
               "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_member_returned_volatile_read_failure_path = std::filesystem::temp_directory_path() /
                                                                     "orison_compiler_app_volatile_write_member_returned_volatile_read_failure.or";
    {
        std::ofstream output(volatile_write_member_returned_volatile_read_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)))\n";
    }

    auto volatile_write_member_returned_volatile_read_failure_path_text =
        volatile_write_member_returned_volatile_read_failure_path.string();
    std::array<char const*, 3> volatile_write_member_returned_volatile_read_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_member_returned_volatile_read_failure_path_text.c_str()
    };
    auto volatile_write_member_returned_volatile_read_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_member_returned_volatile_read_failure_argv.data(),
            volatile_write_member_returned_volatile_read_failure_argv.size()
        )
    );

    assert(volatile_write_member_returned_volatile_read_failure_result.exit_code == 1);
    assert(volatile_write_member_returned_volatile_read_failure_result.stdout_text.empty());
    assert(volatile_write_member_returned_volatile_read_failure_result.stderr_text.find(
               "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_helper_type_failure.or";
    {
        std::ofstream output(volatile_write_helper_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function word_ptr(addr: Address) -> Pointer<UInt32>\n";
        output << "    return Pointer(addr)\n";
        output << "unsafe function write_word(addr: Address, value: Byte) -> Unit\n";
        output << "    volatile_write(word_ptr(addr), value)\n";
    }

    auto volatile_write_helper_type_failure_path_text = volatile_write_helper_type_failure_path.string();
    std::array<char const*, 3> volatile_write_helper_type_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_helper_type_failure_path_text.c_str()
    };
    auto volatile_write_helper_type_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_helper_type_failure_argv.data(),
            volatile_write_helper_type_failure_argv.size()
        )
    );

    assert(volatile_write_helper_type_failure_result.exit_code == 1);
    assert(volatile_write_helper_type_failure_result.stdout_text.empty());
    assert(volatile_write_helper_type_failure_result.stderr_text.find(
               "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_member_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_volatile_write_member_helper_type_failure.or";
    {
        std::ofstream output(volatile_write_member_helper_type_failure_path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function word_ptr(this: shared This, addr: Address) -> Pointer<UInt32>\n";
        output << "        unsafe\n";
        output << "            return Pointer(addr)\n";
        output << "unsafe function write_word(device: Device, addr: Address, value: Byte) -> Unit\n";
        output << "    volatile_write(device.word_ptr(addr), value)\n";
    }

    auto volatile_write_member_helper_type_failure_path_text = volatile_write_member_helper_type_failure_path.string();
    std::array<char const*, 3> volatile_write_member_helper_type_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_member_helper_type_failure_path_text.c_str()
    };
    auto volatile_write_member_helper_type_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_member_helper_type_failure_argv.data(),
            volatile_write_member_helper_type_failure_argv.size()
        )
    );

    assert(volatile_write_member_helper_type_failure_result.exit_code == 1);
    assert(volatile_write_member_helper_type_failure_result.stdout_text.empty());
    assert(volatile_write_member_helper_type_failure_result.stderr_text.find(
               "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto volatile_write_raw_offset_helper_type_failure_path = std::filesystem::temp_directory_path() /
                                                              "orison_compiler_app_volatile_write_raw_offset_helper_type_failure.or";
    {
        std::ofstream output(volatile_write_raw_offset_helper_type_failure_path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_word_ptr(base: Pointer<UInt32>) -> Pointer<UInt32>\n";
        output << "    return raw_offset(base, 1)\n";
        output << "unsafe function write_word(base: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(next_word_ptr(base), value)\n";
    }

    auto volatile_write_raw_offset_helper_type_failure_path_text =
        volatile_write_raw_offset_helper_type_failure_path.string();
    std::array<char const*, 3> volatile_write_raw_offset_helper_type_failure_argv {
        "orisonc",
        "--parse",
        volatile_write_raw_offset_helper_type_failure_path_text.c_str()
    };
    auto volatile_write_raw_offset_helper_type_failure_result = app.run(
        std::span<char const* const>(
            volatile_write_raw_offset_helper_type_failure_argv.data(),
            volatile_write_raw_offset_helper_type_failure_argv.size()
        )
    );

    assert(volatile_write_raw_offset_helper_type_failure_result.exit_code == 1);
    assert(volatile_write_raw_offset_helper_type_failure_result.stdout_text.empty());
    assert(volatile_write_raw_offset_helper_type_failure_result.stderr_text.find(
               "volatile_write value type 'Byte' does not match pointer element type 'UInt32'"
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

    assert_parse_success(switch_nested_constructor_pattern_binding_result);

    auto switch_nested_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_payload_overlap_failure_path,
        {"Wrap(Some(value)) => 1", "Wrap(Some(other)) => 2"}
    );
    auto switch_nested_payload_overlap_failure_result =
        run_parse(app, switch_nested_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_nested_payload_overlap_failure_result);

    auto switch_nested_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_literal_payload_overlap_failure_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(1)) => 2"}
    );
    auto switch_nested_literal_payload_overlap_failure_result =
        run_parse(app, switch_nested_literal_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_nested_literal_payload_overlap_failure_result);

    auto switch_disjoint_nested_literal_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_disjoint_nested_literal_payload_success.or";
    write_boxed_maybe_switch_fixture(
        switch_disjoint_nested_literal_payload_success_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(2)) => 2"}
    );
    auto switch_disjoint_nested_literal_payload_success_result =
        run_parse(app, switch_disjoint_nested_literal_payload_success_path);

    assert_parse_success(switch_disjoint_nested_literal_payload_success_result);

    auto switch_nested_wildcard_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_wildcard_literal_payload_overlap_failure_path,
        {"Wrap(Some(value)) => 1", "Wrap(Some(1)) => 2"}
    );
    auto switch_nested_wildcard_literal_payload_overlap_failure_result =
        run_parse(app, switch_nested_wildcard_literal_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_nested_wildcard_literal_payload_overlap_failure_result);

    auto switch_nested_literal_wildcard_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_literal_wildcard_payload_overlap_failure_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(value)) => 2"}
    );
    auto switch_nested_literal_wildcard_payload_overlap_failure_result =
        run_parse(app, switch_nested_literal_wildcard_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_nested_literal_wildcard_payload_overlap_failure_result);

    auto switch_nested_multi_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_multi_payload_overlap_failure.or";
    write_boxed_pair_maybe_switch_fixture(
        switch_nested_multi_payload_overlap_failure_path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 1)) => 2"}
    );
    auto switch_nested_multi_payload_overlap_failure_result =
        run_parse(app, switch_nested_multi_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_nested_multi_payload_overlap_failure_result);

    auto switch_disjoint_nested_multi_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_disjoint_nested_multi_payload_success.or";
    write_boxed_pair_maybe_switch_fixture(
        switch_disjoint_nested_multi_payload_success_path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 2)) => 2"}
    );
    auto switch_disjoint_nested_multi_payload_success_result =
        run_parse(app, switch_disjoint_nested_multi_payload_success_path);

    assert_parse_success(switch_disjoint_nested_multi_payload_success_result);

    auto switch_mismatched_nested_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_mismatched_nested_constructor_success.or";
    write_boxed_maybe_switch_fixture(
        switch_mismatched_nested_constructor_success_path,
        {"Wrap(Some(value)) => 1", "Wrap(Empty) => 2"}
    );
    auto switch_mismatched_nested_constructor_success_result =
        run_parse(app, switch_mismatched_nested_constructor_success_path);

    assert_parse_success(switch_mismatched_nested_constructor_success_result);

    auto switch_duplicate_nested_zero_payload_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_nested_zero_payload_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_duplicate_nested_zero_payload_failure_path,
        {"Wrap(Empty) => 1", "Wrap(Empty) => 2"}
    );
    auto switch_duplicate_nested_zero_payload_failure_result =
        run_parse(app, switch_duplicate_nested_zero_payload_failure_path);

    assert_wrap_duplicate_parse_failure(switch_duplicate_nested_zero_payload_failure_result);

    auto switch_duplicate_nested_zero_payload_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_nested_zero_payload_no_cascade_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_duplicate_nested_zero_payload_no_cascade_failure_path,
        {"Wrap(Empty) => 1", "Wrap(Empty) => 2"},
        false
    );
    auto switch_duplicate_nested_zero_payload_no_cascade_failure_result =
        run_parse(app, switch_duplicate_nested_zero_payload_no_cascade_failure_path);

    assert_wrap_duplicate_parse_failure(switch_duplicate_nested_zero_payload_no_cascade_failure_result);

    auto switch_deep_nested_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_deep_nested_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_payload_overlap_failure_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(other))) => 2"}
    );
    auto switch_deep_nested_payload_overlap_failure_result =
        run_parse(app, switch_deep_nested_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_deep_nested_payload_overlap_failure_result);

    auto switch_disjoint_deep_nested_literal_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_disjoint_deep_nested_literal_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_disjoint_deep_nested_literal_payload_success_path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(2))) => 2"}
    );
    auto switch_disjoint_deep_nested_literal_payload_success_result =
        run_parse(app, switch_disjoint_deep_nested_literal_payload_success_path);

    assert_parse_success(switch_disjoint_deep_nested_literal_payload_success_result);

    auto switch_deep_nested_wildcard_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_deep_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_wildcard_literal_payload_overlap_failure_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(1))) => 2"}
    );
    auto switch_deep_nested_wildcard_literal_payload_overlap_failure_result =
        run_parse(app, switch_deep_nested_wildcard_literal_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_deep_nested_wildcard_literal_payload_overlap_failure_result);

    auto switch_deep_nested_literal_wildcard_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_deep_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_literal_wildcard_payload_overlap_failure_path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(value))) => 2"}
    );
    auto switch_deep_nested_literal_wildcard_payload_overlap_failure_result =
        run_parse(app, switch_deep_nested_literal_wildcard_payload_overlap_failure_path);

    assert_wrap_duplicate_parse_failure(switch_deep_nested_literal_wildcard_payload_overlap_failure_result);

    auto switch_mismatched_deep_nested_zero_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_mismatched_deep_nested_zero_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_mismatched_deep_nested_zero_payload_success_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Empty)) => 2"}
    );
    auto switch_mismatched_deep_nested_zero_payload_success_result =
        run_parse(app, switch_mismatched_deep_nested_zero_payload_success_path);

    assert_parse_success(switch_mismatched_deep_nested_zero_payload_success_result);

    auto switch_duplicate_deep_nested_zero_payload_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_deep_nested_zero_payload_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_duplicate_deep_nested_zero_payload_failure_path,
        {"Wrap(Hold(Empty)) => 1", "Wrap(Hold(Empty)) => 2"}
    );
    auto switch_duplicate_deep_nested_zero_payload_failure_result =
        run_parse(app, switch_duplicate_deep_nested_zero_payload_failure_path);

    assert_wrap_duplicate_parse_failure(switch_duplicate_deep_nested_zero_payload_failure_result);

    auto switch_nested_wrapped_payload_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_wrapped_payload_success.or";
    {
        std::ofstream output(switch_nested_wrapped_payload_success_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "unsafe function write_next(xs: List<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(next, tail)) => raw_write(out, next)\n";
        output << "        default => return\n";
    }

    auto switch_nested_wrapped_payload_success_path_text = switch_nested_wrapped_payload_success_path.string();
    std::array<char const*, 3> switch_nested_wrapped_payload_success_argv {
        "orisonc",
        "--parse",
        switch_nested_wrapped_payload_success_path_text.c_str()
    };
    auto switch_nested_wrapped_payload_success_result = app.run(
        std::span<char const* const>(
            switch_nested_wrapped_payload_success_argv.data(),
            switch_nested_wrapped_payload_success_argv.size()
        )
    );

    assert_parse_success(switch_nested_wrapped_payload_success_result);

    auto switch_nested_wrapped_payload_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_wrapped_payload_failure.or";
    {
        std::ofstream output(switch_nested_wrapped_payload_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "unsafe function write_next(xs: List<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(next, tail)) => raw_write(out, next)\n";
        output << "        default => return\n";
    }

    auto switch_nested_wrapped_payload_failure_path_text = switch_nested_wrapped_payload_failure_path.string();
    std::array<char const*, 3> switch_nested_wrapped_payload_failure_argv {
        "orisonc",
        "--parse",
        switch_nested_wrapped_payload_failure_path_text.c_str()
    };
    auto switch_nested_wrapped_payload_failure_result = app.run(
        std::span<char const* const>(
            switch_nested_wrapped_payload_failure_argv.data(),
            switch_nested_wrapped_payload_failure_argv.size()
        )
    );

    assert(switch_nested_wrapped_payload_failure_result.exit_code == 1);
    assert(switch_nested_wrapped_payload_failure_result.stdout_text.empty());
    assert(switch_nested_wrapped_payload_failure_result.stderr_text.find(
               "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto switch_generic_constructor_payload_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_generic_constructor_payload_success.or";
    {
        std::ofstream output(switch_generic_constructor_payload_success_path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    None\n";
        output << "    Some(value: T)\n";
        output << "unsafe function write_word(maybe: Maybe<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch maybe\n";
        output << "        Some(value) => raw_write(out, value)\n";
        output << "        default => return\n";
    }

    auto switch_generic_constructor_payload_success_path_text =
        switch_generic_constructor_payload_success_path.string();
    std::array<char const*, 3> switch_generic_constructor_payload_success_argv {
        "orisonc",
        "--parse",
        switch_generic_constructor_payload_success_path_text.c_str()
    };
    auto switch_generic_constructor_payload_success_result = app.run(
        std::span<char const* const>(
            switch_generic_constructor_payload_success_argv.data(),
            switch_generic_constructor_payload_success_argv.size()
        )
    );

    assert(switch_generic_constructor_payload_success_result.exit_code == 0);
    assert(switch_generic_constructor_payload_success_result.stderr_text.empty());
    assert(switch_generic_constructor_payload_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto switch_generic_constructor_payload_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_generic_constructor_payload_failure.or";
    {
        std::ofstream output(switch_generic_constructor_payload_failure_path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    None\n";
        output << "    Some(value: T)\n";
        output << "unsafe function write_word(maybe: Maybe<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch maybe\n";
        output << "        Some(value) => raw_write(out, value)\n";
        output << "        default => return\n";
    }

    auto switch_generic_constructor_payload_failure_path_text =
        switch_generic_constructor_payload_failure_path.string();
    std::array<char const*, 3> switch_generic_constructor_payload_failure_argv {
        "orisonc",
        "--parse",
        switch_generic_constructor_payload_failure_path_text.c_str()
    };
    auto switch_generic_constructor_payload_failure_result = app.run(
        std::span<char const* const>(
            switch_generic_constructor_payload_failure_argv.data(),
            switch_generic_constructor_payload_failure_argv.size()
        )
    );

    assert(switch_generic_constructor_payload_failure_result.exit_code == 1);
    assert(switch_generic_constructor_payload_failure_result.stdout_text.empty());
    assert(switch_generic_constructor_payload_failure_result.stderr_text.find(
               "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
           ) != std::string::npos);

    auto switch_wrong_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_wrong_choice_variant_failure.or";
    {
        std::ofstream output(switch_wrong_choice_variant_failure_path);
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
        output << "        default => 0\n";
    }

    auto switch_wrong_choice_variant_failure_path_text = switch_wrong_choice_variant_failure_path.string();
    std::array<char const*, 3> switch_wrong_choice_variant_failure_argv {
        "orisonc",
        "--parse",
        switch_wrong_choice_variant_failure_path_text.c_str()
    };
    auto switch_wrong_choice_variant_failure_result = app.run(
        std::span<char const* const>(
            switch_wrong_choice_variant_failure_argv.data(),
            switch_wrong_choice_variant_failure_argv.size()
        )
    );

    assert(switch_wrong_choice_variant_failure_result.exit_code == 1);
    assert(switch_wrong_choice_variant_failure_result.stdout_text.empty());
    assert(switch_wrong_choice_variant_failure_result.stderr_text.find(
               "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
           ) != std::string::npos);

    auto switch_wrong_choice_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_wrong_choice_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_wrong_choice_without_default_no_cascade_failure_path);
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
    }

    auto switch_wrong_choice_without_default_no_cascade_failure_result =
        run_parse(app, switch_wrong_choice_without_default_no_cascade_failure_path);
    assert(switch_wrong_choice_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_wrong_choice_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_wrong_choice_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
           ) != std::string::npos);
    assert(switch_wrong_choice_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing"
           ) == std::string::npos);

    auto switch_subject_specific_arity_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_subject_specific_arity_success.or";
    {
        std::ofstream output(switch_subject_specific_arity_success_path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    Some(value: T)\n";
        output << "choice Flag\n";
        output << "    Some\n";
        output << "function read(flag: Flag) -> Int64\n";
        output << "    switch flag\n";
        output << "        Some => 1\n";
    }

    auto switch_subject_specific_arity_success_path_text = switch_subject_specific_arity_success_path.string();
    std::array<char const*, 3> switch_subject_specific_arity_success_argv {
        "orisonc",
        "--parse",
        switch_subject_specific_arity_success_path_text.c_str()
    };
    auto switch_subject_specific_arity_success_result = app.run(
        std::span<char const* const>(
            switch_subject_specific_arity_success_argv.data(),
            switch_subject_specific_arity_success_argv.size()
        )
    );

    assert(switch_subject_specific_arity_success_result.exit_code == 0);
    assert(switch_subject_specific_arity_success_result.stderr_text.empty());
    assert(switch_subject_specific_arity_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto switch_nested_wrong_payload_choice_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_wrong_payload_choice_failure.or";
    {
        std::ofstream output(switch_nested_wrong_payload_choice_failure_path);
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

    auto switch_nested_wrong_payload_choice_failure_path_text =
        switch_nested_wrong_payload_choice_failure_path.string();
    std::array<char const*, 3> switch_nested_wrong_payload_choice_failure_argv {
        "orisonc",
        "--parse",
        switch_nested_wrong_payload_choice_failure_path_text.c_str()
    };
    auto switch_nested_wrong_payload_choice_failure_result = app.run(
        std::span<char const* const>(
            switch_nested_wrong_payload_choice_failure_argv.data(),
            switch_nested_wrong_payload_choice_failure_argv.size()
        )
    );

    assert(switch_nested_wrong_payload_choice_failure_result.exit_code == 1);
    assert(switch_nested_wrong_payload_choice_failure_result.stdout_text.empty());
    assert(switch_nested_wrong_payload_choice_failure_result.stderr_text.find(
               "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
           ) != std::string::npos);

    auto switch_nested_payload_specific_arity_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_nested_payload_specific_arity_success.or";
    {
        std::ofstream output(switch_nested_payload_specific_arity_success_path);
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

    auto switch_nested_payload_specific_arity_success_path_text =
        switch_nested_payload_specific_arity_success_path.string();
    std::array<char const*, 3> switch_nested_payload_specific_arity_success_argv {
        "orisonc",
        "--parse",
        switch_nested_payload_specific_arity_success_path_text.c_str()
    };
    auto switch_nested_payload_specific_arity_success_result = app.run(
        std::span<char const* const>(
            switch_nested_payload_specific_arity_success_argv.data(),
            switch_nested_payload_specific_arity_success_argv.size()
        )
    );

    assert(switch_nested_payload_specific_arity_success_result.exit_code == 0);
    assert(switch_nested_payload_specific_arity_success_result.stderr_text.empty());
    assert(switch_nested_payload_specific_arity_success_result.stdout_text.find("parsed ") != std::string::npos);

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
    {
        std::ofstream output(switch_unknown_constructor_without_default_no_cascade_failure_path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    Some(value: T)\n";
        output << "    Empty\n";
        output << "function read(item: Maybe<Int64>) -> Int64\n";
        output << "    switch item\n";
        output << "        Missing(value) => value\n";
    }

    auto switch_unknown_constructor_without_default_no_cascade_failure_result =
        run_parse(app, switch_unknown_constructor_without_default_no_cascade_failure_path);
    assert(switch_unknown_constructor_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_unknown_constructor_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_unknown_constructor_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern 'Missing' does not match any declared choice variant"
           ) != std::string::npos);
    assert(switch_unknown_constructor_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing"
           ) == std::string::npos);

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

    auto switch_constructor_payload_shape_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_payload_shape_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_constructor_payload_shape_without_default_no_cascade_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head + 1, tail) => 0\n";
    }

    auto switch_constructor_payload_shape_without_default_no_cascade_failure_result =
        run_parse(app, switch_constructor_payload_shape_without_default_no_cascade_failure_path);
    assert(switch_constructor_payload_shape_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_constructor_payload_shape_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_constructor_payload_shape_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
           ) != std::string::npos);
    assert(switch_constructor_payload_shape_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing"
           ) == std::string::npos);

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

    auto switch_constructor_duplicate_binding_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_constructor_duplicate_binding_without_default_no_cascade_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, head) => 0\n";
    }

    auto switch_constructor_duplicate_binding_without_default_no_cascade_failure_result =
        run_parse(app, switch_constructor_duplicate_binding_without_default_no_cascade_failure_path);
    assert(switch_constructor_duplicate_binding_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_constructor_duplicate_binding_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_constructor_duplicate_binding_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern cannot bind 'head' more than once"
           ) != std::string::npos);
    assert(switch_constructor_duplicate_binding_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing"
           ) == std::string::npos);

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

    auto switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(head, tail)) => 0\n";
    }

    auto switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_result =
        run_parse(app, switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path);
    assert(switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern cannot bind 'head' more than once"
           ) != std::string::npos);
    assert(switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing"
           ) == std::string::npos);

    auto switch_constructor_pattern_arity_missing_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_arity_missing_failure.or";
    {
        std::ofstream output(switch_constructor_pattern_arity_missing_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head) => 0\n";
        output << "        default => 0\n";
    }

    auto switch_constructor_pattern_arity_missing_failure_path_text =
        switch_constructor_pattern_arity_missing_failure_path.string();
    std::array<char const*, 3> switch_constructor_pattern_arity_missing_failure_argv {
        "orisonc",
        "--parse",
        switch_constructor_pattern_arity_missing_failure_path_text.c_str()
    };
    auto switch_constructor_pattern_arity_missing_failure_result = app.run(
        std::span<char const* const>(
            switch_constructor_pattern_arity_missing_failure_argv.data(),
            switch_constructor_pattern_arity_missing_failure_argv.size()
        )
    );

    assert(switch_constructor_pattern_arity_missing_failure_result.exit_code == 1);
    assert(switch_constructor_pattern_arity_missing_failure_result.stdout_text.empty());
    assert(switch_constructor_pattern_arity_missing_failure_result.stderr_text.find(
               "switch constructor pattern 'Node' expects 2 payload values but received 1"
           ) != std::string::npos);

    auto switch_constructor_pattern_arity_extra_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_arity_extra_failure.or";
    {
        std::ofstream output(switch_constructor_pattern_arity_extra_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Empty(value) => 0\n";
        output << "        default => 0\n";
    }

    auto switch_constructor_pattern_arity_extra_failure_path_text =
        switch_constructor_pattern_arity_extra_failure_path.string();
    std::array<char const*, 3> switch_constructor_pattern_arity_extra_failure_argv {
        "orisonc",
        "--parse",
        switch_constructor_pattern_arity_extra_failure_path_text.c_str()
    };
    auto switch_constructor_pattern_arity_extra_failure_result = app.run(
        std::span<char const* const>(
            switch_constructor_pattern_arity_extra_failure_argv.data(),
            switch_constructor_pattern_arity_extra_failure_argv.size()
        )
    );

    assert(switch_constructor_pattern_arity_extra_failure_result.exit_code == 1);
    assert(switch_constructor_pattern_arity_extra_failure_result.stdout_text.empty());
    assert(switch_constructor_pattern_arity_extra_failure_result.stderr_text.find(
               "switch constructor pattern 'Empty' expects 0 payload values but received 1"
           ) != std::string::npos);

    auto switch_constructor_pattern_arity_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_constructor_pattern_arity_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_constructor_pattern_arity_without_default_no_cascade_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head) => 0\n";
    }

    auto switch_constructor_pattern_arity_without_default_no_cascade_failure_result =
        run_parse(app, switch_constructor_pattern_arity_without_default_no_cascade_failure_path);
    assert(switch_constructor_pattern_arity_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_constructor_pattern_arity_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_constructor_pattern_arity_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern 'Node' expects 2 payload values but received 1"
           ) != std::string::npos);
    assert(switch_constructor_pattern_arity_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing"
           ) == std::string::npos);

    auto switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_zero_payload_constructor_arity_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Empty(value) => 0\n";
    }

    auto switch_zero_payload_constructor_arity_without_default_no_cascade_failure_result =
        run_parse(app, switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path);
    assert(switch_zero_payload_constructor_arity_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_zero_payload_constructor_arity_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_zero_payload_constructor_arity_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern 'Empty' expects 0 payload values but received 1"
           ) != std::string::npos);
    assert(switch_zero_payload_constructor_arity_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing"
           ) == std::string::npos);

    auto switch_pattern_mix_constructor_value_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_pattern_mix_constructor_value_failure.or";
    {
        std::ofstream output(switch_pattern_mix_constructor_value_failure_path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Empty => 0\n";
        output << "        1 => 1\n";
        output << "        default => 0\n";
    }

    auto switch_pattern_mix_constructor_value_failure_path_text =
        switch_pattern_mix_constructor_value_failure_path.string();
    std::array<char const*, 3> switch_pattern_mix_constructor_value_failure_argv {
        "orisonc",
        "--parse",
        switch_pattern_mix_constructor_value_failure_path_text.c_str()
    };
    auto switch_pattern_mix_constructor_value_failure_result = app.run(
        std::span<char const* const>(
            switch_pattern_mix_constructor_value_failure_argv.data(),
            switch_pattern_mix_constructor_value_failure_argv.size()
        )
    );

    assert(switch_pattern_mix_constructor_value_failure_result.exit_code == 1);
    assert(switch_pattern_mix_constructor_value_failure_result.stdout_text.empty());
    assert(switch_pattern_mix_constructor_value_failure_result.stderr_text.find(
               "switch cannot mix value patterns with constructor patterns"
           ) != std::string::npos);

    auto switch_pattern_mix_value_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_pattern_mix_value_constructor_failure.or";
    {
        std::ofstream output(switch_pattern_mix_value_constructor_failure_path);
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

    auto switch_pattern_mix_value_constructor_failure_path_text =
        switch_pattern_mix_value_constructor_failure_path.string();
    std::array<char const*, 3> switch_pattern_mix_value_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_pattern_mix_value_constructor_failure_path_text.c_str()
    };
    auto switch_pattern_mix_value_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_pattern_mix_value_constructor_failure_argv.data(),
            switch_pattern_mix_value_constructor_failure_argv.size()
        )
    );

    assert(switch_pattern_mix_value_constructor_failure_result.exit_code == 1);
    assert(switch_pattern_mix_value_constructor_failure_result.stdout_text.empty());
    assert(switch_pattern_mix_value_constructor_failure_result.stderr_text.find(
               "switch cannot mix value patterns with constructor patterns"
           ) != std::string::npos);

    auto switch_pattern_mix_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_pattern_mix_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_pattern_mix_without_default_no_cascade_failure_path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    Some(value: T)\n";
        output << "    Empty\n";
        output << "function classify(item: Maybe<Int64>) -> Int64\n";
        output << "    switch item\n";
        output << "        Some(value) => value\n";
        output << "        1 => 1\n";
    }

    auto switch_pattern_mix_without_default_no_cascade_failure_result =
        run_parse(app, switch_pattern_mix_without_default_no_cascade_failure_path);
    assert(switch_pattern_mix_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_pattern_mix_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_pattern_mix_without_default_no_cascade_failure_result.stderr_text.find(
               "switch cannot mix value patterns with constructor patterns"
           ) != std::string::npos);
    assert(switch_pattern_mix_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing choice variant"
           ) == std::string::npos);

    auto switch_value_pattern_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_value_pattern_type_failure.or";
    {
        std::ofstream output(switch_value_pattern_type_failure_path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        \"ready\" => 1\n";
        output << "        default => 0\n";
    }

    auto switch_value_pattern_type_failure_path_text = switch_value_pattern_type_failure_path.string();
    std::array<char const*, 3> switch_value_pattern_type_failure_argv {
        "orisonc",
        "--parse",
        switch_value_pattern_type_failure_path_text.c_str()
    };
    auto switch_value_pattern_type_failure_result = app.run(
        std::span<char const* const>(
            switch_value_pattern_type_failure_argv.data(),
            switch_value_pattern_type_failure_argv.size()
        )
    );

    assert(switch_value_pattern_type_failure_result.exit_code == 1);
    assert(switch_value_pattern_type_failure_result.stdout_text.empty());
    assert(switch_value_pattern_type_failure_result.stderr_text.find(
               "switch value pattern type 'Text' does not match switched expression type 'Bool'"
           ) != std::string::npos);

    auto switch_value_pattern_same_width_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_value_pattern_same_width_success.or";
    {
        std::ofstream output(switch_value_pattern_same_width_success_path);
        output << "package demo.switches\n";
        output << "function classify(value: UInt32) -> Int64\n";
        output << "    switch value\n";
        output << "        1 as Int32 => 1\n";
        output << "        default => 0\n";
    }

    auto switch_value_pattern_same_width_success_path_text = switch_value_pattern_same_width_success_path.string();
    std::array<char const*, 3> switch_value_pattern_same_width_success_argv {
        "orisonc",
        "--parse",
        switch_value_pattern_same_width_success_path_text.c_str()
    };
    auto switch_value_pattern_same_width_success_result = app.run(
        std::span<char const* const>(
            switch_value_pattern_same_width_success_argv.data(),
            switch_value_pattern_same_width_success_argv.size()
        )
    );

    assert(switch_value_pattern_same_width_success_result.exit_code == 0);
    assert(switch_value_pattern_same_width_success_result.stderr_text.empty());
    assert(switch_value_pattern_same_width_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto switch_duplicate_boolean_value_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_boolean_value_failure.or";
    {
        std::ofstream output(switch_duplicate_boolean_value_failure_path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        true => 2\n";
        output << "        default => 0\n";
    }

    auto switch_duplicate_boolean_value_failure_path_text = switch_duplicate_boolean_value_failure_path.string();
    std::array<char const*, 3> switch_duplicate_boolean_value_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_boolean_value_failure_path_text.c_str()
    };
    auto switch_duplicate_boolean_value_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_boolean_value_failure_argv.data(),
            switch_duplicate_boolean_value_failure_argv.size()
        )
    );

    assert(switch_duplicate_boolean_value_failure_result.exit_code == 1);
    assert(switch_duplicate_boolean_value_failure_result.stdout_text.empty());
    assert(switch_duplicate_boolean_value_failure_result.stderr_text.find(
               "switch value pattern 'true' is duplicated"
           ) != std::string::npos);

    auto switch_duplicate_bool_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_bool_without_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_duplicate_bool_without_default_no_cascade_failure_path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        true => 2\n";
    }

    auto switch_duplicate_bool_without_default_no_cascade_failure_path_text =
        switch_duplicate_bool_without_default_no_cascade_failure_path.string();
    std::array<char const*, 3> switch_duplicate_bool_without_default_no_cascade_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_bool_without_default_no_cascade_failure_path_text.c_str()
    };
    auto switch_duplicate_bool_without_default_no_cascade_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_bool_without_default_no_cascade_failure_argv.data(),
            switch_duplicate_bool_without_default_no_cascade_failure_argv.size()
        )
    );

    assert(switch_duplicate_bool_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_duplicate_bool_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_duplicate_bool_without_default_no_cascade_failure_result.stderr_text.find(
               "switch value pattern 'true' is duplicated"
           ) != std::string::npos);
    assert(switch_duplicate_bool_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing boolean value pattern"
           ) == std::string::npos);

    auto switch_duplicate_string_value_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_string_value_failure.or";
    {
        std::ofstream output(switch_duplicate_string_value_failure_path);
        output << "package demo.switches\n";
        output << "function classify(state: Text) -> Int64\n";
        output << "    switch state\n";
        output << "        \"ready\" => 1\n";
        output << "        \"ready\" => 2\n";
        output << "        default => 0\n";
    }

    auto switch_duplicate_string_value_failure_path_text = switch_duplicate_string_value_failure_path.string();
    std::array<char const*, 3> switch_duplicate_string_value_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_string_value_failure_path_text.c_str()
    };
    auto switch_duplicate_string_value_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_string_value_failure_argv.data(),
            switch_duplicate_string_value_failure_argv.size()
        )
    );

    assert(switch_duplicate_string_value_failure_result.exit_code == 1);
    assert(switch_duplicate_string_value_failure_result.stdout_text.empty());
    assert(switch_duplicate_string_value_failure_result.stderr_text.find(
               "switch value pattern '\"ready\"' is duplicated"
           ) != std::string::npos);

    auto switch_duplicate_integer_cast_value_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_integer_cast_value_failure.or";
    {
        std::ofstream output(switch_duplicate_integer_cast_value_failure_path);
        output << "package demo.switches\n";
        output << "function classify(value: UInt32) -> Int64\n";
        output << "    switch value\n";
        output << "        1 => 1\n";
        output << "        1 as Int32 => 2\n";
        output << "        default => 0\n";
    }

    auto switch_duplicate_integer_cast_value_failure_path_text =
        switch_duplicate_integer_cast_value_failure_path.string();
    std::array<char const*, 3> switch_duplicate_integer_cast_value_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_integer_cast_value_failure_path_text.c_str()
    };
    auto switch_duplicate_integer_cast_value_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_integer_cast_value_failure_argv.data(),
            switch_duplicate_integer_cast_value_failure_argv.size()
        )
    );

    assert(switch_duplicate_integer_cast_value_failure_result.exit_code == 1);
    assert(switch_duplicate_integer_cast_value_failure_result.stdout_text.empty());
    assert(switch_duplicate_integer_cast_value_failure_result.stderr_text.find(
               "switch value pattern '1 as Int32' is duplicated"
           ) != std::string::npos);

    auto switch_redundant_bool_default_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_redundant_bool_default_failure.or";
    {
        std::ofstream output(switch_redundant_bool_default_failure_path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
        output << "        default => 2\n";
    }

    auto switch_redundant_bool_default_failure_path_text = switch_redundant_bool_default_failure_path.string();
    std::array<char const*, 3> switch_redundant_bool_default_failure_argv {
        "orisonc",
        "--parse",
        switch_redundant_bool_default_failure_path_text.c_str()
    };
    auto switch_redundant_bool_default_failure_result = app.run(
        std::span<char const* const>(
            switch_redundant_bool_default_failure_argv.data(),
            switch_redundant_bool_default_failure_argv.size()
        )
    );

    assert(switch_redundant_bool_default_failure_result.exit_code == 1);
    assert(switch_redundant_bool_default_failure_result.stdout_text.empty());
    assert(switch_redundant_bool_default_failure_result.stderr_text.find(
               "switch default case is redundant after true and false value patterns"
           ) != std::string::npos);

    auto switch_duplicate_bool_redundant_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_bool_redundant_default_no_cascade_failure.or";
    {
        std::ofstream output(switch_duplicate_bool_redundant_default_no_cascade_failure_path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
        output << "        false => 2\n";
        output << "        default => 3\n";
    }

    auto switch_duplicate_bool_redundant_default_no_cascade_failure_result =
        run_parse(app, switch_duplicate_bool_redundant_default_no_cascade_failure_path);
    assert(switch_duplicate_bool_redundant_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_duplicate_bool_redundant_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_duplicate_bool_redundant_default_no_cascade_failure_result.stderr_text.find(
               "switch value pattern 'false' is duplicated"
           ) != std::string::npos);
    assert(switch_duplicate_bool_redundant_default_no_cascade_failure_result.stderr_text.find(
               "switch default case is redundant"
           ) == std::string::npos);

    auto switch_exhaustive_bool_without_default_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_exhaustive_bool_success.or";
    {
        std::ofstream output(switch_exhaustive_bool_without_default_success_path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
    }

    auto switch_exhaustive_bool_without_default_success_path_text =
        switch_exhaustive_bool_without_default_success_path.string();
    std::array<char const*, 3> switch_exhaustive_bool_without_default_success_argv {
        "orisonc",
        "--parse",
        switch_exhaustive_bool_without_default_success_path_text.c_str()
    };
    auto switch_exhaustive_bool_without_default_success_result = app.run(
        std::span<char const* const>(
            switch_exhaustive_bool_without_default_success_argv.data(),
            switch_exhaustive_bool_without_default_success_argv.size()
        )
    );

    assert(switch_exhaustive_bool_without_default_success_result.exit_code == 0);
    assert(switch_exhaustive_bool_without_default_success_result.stderr_text.empty());
    assert(switch_exhaustive_bool_without_default_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto switch_missing_bool_pattern_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_missing_bool_pattern_failure.or";
    {
        std::ofstream output(switch_missing_bool_pattern_failure_path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
    }

    auto switch_missing_bool_pattern_failure_path_text = switch_missing_bool_pattern_failure_path.string();
    std::array<char const*, 3> switch_missing_bool_pattern_failure_argv {
        "orisonc",
        "--parse",
        switch_missing_bool_pattern_failure_path_text.c_str()
    };
    auto switch_missing_bool_pattern_failure_result = app.run(
        std::span<char const* const>(
            switch_missing_bool_pattern_failure_argv.data(),
            switch_missing_bool_pattern_failure_argv.size()
        )
    );

    assert(switch_missing_bool_pattern_failure_result.exit_code == 1);
    assert(switch_missing_bool_pattern_failure_result.stdout_text.empty());
    assert(switch_missing_bool_pattern_failure_result.stderr_text.find(
               "switch is missing boolean value pattern 'false'"
           ) != std::string::npos);

    auto switch_redundant_choice_default_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_redundant_choice_default_failure.or";
    {
        std::ofstream output(switch_redundant_choice_default_failure_path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        PermissionDenied => 3\n";
        output << "        default => 0\n";
    }

    auto switch_redundant_choice_default_failure_path_text =
        switch_redundant_choice_default_failure_path.string();
    std::array<char const*, 3> switch_redundant_choice_default_failure_argv {
        "orisonc",
        "--parse",
        switch_redundant_choice_default_failure_path_text.c_str()
    };
    auto switch_redundant_choice_default_failure_result = app.run(
        std::span<char const* const>(
            switch_redundant_choice_default_failure_argv.data(),
            switch_redundant_choice_default_failure_argv.size()
        )
    );

    assert(switch_redundant_choice_default_failure_result.exit_code == 1);
    assert(switch_redundant_choice_default_failure_result.stdout_text.empty());
    assert(switch_redundant_choice_default_failure_result.stderr_text.find(
               "switch default case is redundant after all zero-payload choice variants are covered"
           ) != std::string::npos);

    auto switch_exhaustive_choice_without_default_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_exhaustive_choice_success.or";
    {
        std::ofstream output(switch_exhaustive_choice_without_default_success_path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        PermissionDenied => 3\n";
    }

    auto switch_exhaustive_choice_without_default_success_path_text =
        switch_exhaustive_choice_without_default_success_path.string();
    std::array<char const*, 3> switch_exhaustive_choice_without_default_success_argv {
        "orisonc",
        "--parse",
        switch_exhaustive_choice_without_default_success_path_text.c_str()
    };
    auto switch_exhaustive_choice_without_default_success_result = app.run(
        std::span<char const* const>(
            switch_exhaustive_choice_without_default_success_argv.data(),
            switch_exhaustive_choice_without_default_success_argv.size()
        )
    );

    assert(switch_exhaustive_choice_without_default_success_result.exit_code == 0);
    assert(switch_exhaustive_choice_without_default_success_result.stderr_text.empty());
    assert(switch_exhaustive_choice_without_default_success_result.stdout_text.find("parsed ") != std::string::npos);

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
    assert(switch_duplicate_multi_payload_choice_no_cascade_failure_result.exit_code == 1);
    assert(switch_duplicate_multi_payload_choice_no_cascade_failure_result.stdout_text.empty());
    assert(switch_duplicate_multi_payload_choice_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern 'First(...)' is duplicated"
           ) != std::string::npos);
    assert(switch_duplicate_multi_payload_choice_no_cascade_failure_result.stderr_text.find(
               "switch is missing choice variant"
           ) == std::string::npos);

    auto switch_duplicate_payload_choice_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_payload_choice_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_without_default_no_cascade_failure_path,
        {"Some(value) => value", "Some(other) => other"}
    );

    auto switch_duplicate_payload_choice_without_default_no_cascade_failure_result =
        run_parse(app, switch_duplicate_payload_choice_without_default_no_cascade_failure_path);
    assert(switch_duplicate_payload_choice_without_default_no_cascade_failure_result.exit_code == 1);
    assert(switch_duplicate_payload_choice_without_default_no_cascade_failure_result.stdout_text.empty());
    assert(switch_duplicate_payload_choice_without_default_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern 'Some(...)' is duplicated"
           ) != std::string::npos);
    assert(switch_duplicate_payload_choice_without_default_no_cascade_failure_result.stderr_text.find(
               "switch is missing choice variant"
           ) == std::string::npos);

    auto switch_duplicate_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_duplicate_choice_constructor_failure.or";
    {
        std::ofstream output(switch_duplicate_choice_constructor_failure_path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        Closed => 3\n";
    }

    auto switch_duplicate_choice_constructor_failure_path_text =
        switch_duplicate_choice_constructor_failure_path.string();
    std::array<char const*, 3> switch_duplicate_choice_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_choice_constructor_failure_path_text.c_str()
    };
    auto switch_duplicate_choice_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_choice_constructor_failure_argv.data(),
            switch_duplicate_choice_constructor_failure_argv.size()
        )
    );

    assert(switch_duplicate_choice_constructor_failure_result.exit_code == 1);
    assert(switch_duplicate_choice_constructor_failure_result.stdout_text.empty());
    assert(switch_duplicate_choice_constructor_failure_result.stderr_text.find(
               "switch constructor pattern 'Closed' is duplicated"
           ) != std::string::npos);
    assert(switch_duplicate_choice_constructor_failure_result.stderr_text.find(
               "switch is missing zero-payload choice variant"
           ) == std::string::npos);

    auto switch_duplicate_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_payload_choice_constructor_failure.or";
    {
        std::ofstream output(switch_duplicate_payload_choice_constructor_failure_path);
        output << "package demo.switches\n";
        output << "choice Maybe<T>\n";
        output << "    Some(value: T)\n";
        output << "    Empty\n";
        output << "function classify(item: Maybe<Int32>) -> Int64\n";
        output << "    switch item\n";
        output << "        Some(value) => 1\n";
        output << "        Some(other) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_duplicate_payload_choice_constructor_failure_path_text =
        switch_duplicate_payload_choice_constructor_failure_path.string();
    std::array<char const*, 3> switch_duplicate_payload_choice_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_payload_choice_constructor_failure_path_text.c_str()
    };
    auto switch_duplicate_payload_choice_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_payload_choice_constructor_failure_argv.data(),
            switch_duplicate_payload_choice_constructor_failure_argv.size()
        )
    );

    assert(switch_duplicate_payload_choice_constructor_failure_result.exit_code == 1);
    assert(switch_duplicate_payload_choice_constructor_failure_result.stdout_text.empty());
    assert(switch_duplicate_payload_choice_constructor_failure_result.stderr_text.find(
               "switch constructor pattern 'Some(...)' is duplicated"
           ) != std::string::npos);

    auto switch_duplicate_payload_choice_constructor_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_payload_choice_constructor_no_cascade_failure.or";
    {
        std::ofstream output(switch_duplicate_payload_choice_constructor_no_cascade_failure_path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int32, right: Int32)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(left, right) => 1\n";
        output << "        Both(value, value) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_duplicate_payload_choice_constructor_no_cascade_failure_path_text =
        switch_duplicate_payload_choice_constructor_no_cascade_failure_path.string();
    std::array<char const*, 3> switch_duplicate_payload_choice_constructor_no_cascade_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_payload_choice_constructor_no_cascade_failure_path_text.c_str()
    };
    auto switch_duplicate_payload_choice_constructor_no_cascade_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_payload_choice_constructor_no_cascade_failure_argv.data(),
            switch_duplicate_payload_choice_constructor_no_cascade_failure_argv.size()
        )
    );

    assert(switch_duplicate_payload_choice_constructor_no_cascade_failure_result.exit_code == 1);
    assert(switch_duplicate_payload_choice_constructor_no_cascade_failure_result.stdout_text.empty());
    assert(switch_duplicate_payload_choice_constructor_no_cascade_failure_result.stderr_text.find(
               "switch constructor pattern 'Both(...)' is duplicated"
           ) != std::string::npos);
    assert(switch_duplicate_payload_choice_constructor_no_cascade_failure_result.stderr_text.find(
               "cannot bind 'value' more than once"
           ) == std::string::npos);

    auto switch_duplicate_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_duplicate_literal_payload_choice_constructor_failure.or";
    {
        std::ofstream output(switch_duplicate_literal_payload_choice_constructor_failure_path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(1) => 1\n";
        output << "        Int(1) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_duplicate_literal_payload_choice_constructor_failure_path_text =
        switch_duplicate_literal_payload_choice_constructor_failure_path.string();
    std::array<char const*, 3> switch_duplicate_literal_payload_choice_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_duplicate_literal_payload_choice_constructor_failure_path_text.c_str()
    };
    auto switch_duplicate_literal_payload_choice_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_duplicate_literal_payload_choice_constructor_failure_argv.data(),
            switch_duplicate_literal_payload_choice_constructor_failure_argv.size()
        )
    );

    assert(switch_duplicate_literal_payload_choice_constructor_failure_result.exit_code == 1);
    assert(switch_duplicate_literal_payload_choice_constructor_failure_result.stdout_text.empty());
    assert(switch_duplicate_literal_payload_choice_constructor_failure_result.stderr_text.find(
               "switch constructor pattern 'Int(...)' is duplicated"
           ) != std::string::npos);
    assert(switch_duplicate_literal_payload_choice_constructor_failure_result.stderr_text.find(
               "switch value pattern"
           ) == std::string::npos);

    auto switch_equivalent_integer_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_equivalent_integer_literal_payload_choice_constructor_failure.or";
    {
        std::ofstream output(switch_equivalent_integer_literal_payload_choice_constructor_failure_path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(1) => 1\n";
        output << "        Int(1 as Int64) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_equivalent_integer_literal_payload_choice_constructor_failure_path_text =
        switch_equivalent_integer_literal_payload_choice_constructor_failure_path.string();
    std::array<char const*, 3> switch_equivalent_integer_literal_payload_choice_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_equivalent_integer_literal_payload_choice_constructor_failure_path_text.c_str()
    };
    auto switch_equivalent_integer_literal_payload_choice_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_equivalent_integer_literal_payload_choice_constructor_failure_argv.data(),
            switch_equivalent_integer_literal_payload_choice_constructor_failure_argv.size()
        )
    );

    assert(switch_equivalent_integer_literal_payload_choice_constructor_failure_result.exit_code == 1);
    assert(switch_equivalent_integer_literal_payload_choice_constructor_failure_result.stdout_text.empty());
    assert(switch_equivalent_integer_literal_payload_choice_constructor_failure_result.stderr_text.find(
               "switch constructor pattern 'Int(...)' is duplicated"
           ) != std::string::npos);

    auto switch_wildcard_then_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_wildcard_then_literal_payload_choice_constructor_failure.or";
    {
        std::ofstream output(switch_wildcard_then_literal_payload_choice_constructor_failure_path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(value) => 1\n";
        output << "        Int(1) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_wildcard_then_literal_payload_choice_constructor_failure_path_text =
        switch_wildcard_then_literal_payload_choice_constructor_failure_path.string();
    std::array<char const*, 3> switch_wildcard_then_literal_payload_choice_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_wildcard_then_literal_payload_choice_constructor_failure_path_text.c_str()
    };
    auto switch_wildcard_then_literal_payload_choice_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_wildcard_then_literal_payload_choice_constructor_failure_argv.data(),
            switch_wildcard_then_literal_payload_choice_constructor_failure_argv.size()
        )
    );

    assert(switch_wildcard_then_literal_payload_choice_constructor_failure_result.exit_code == 1);
    assert(switch_wildcard_then_literal_payload_choice_constructor_failure_result.stdout_text.empty());
    assert(switch_wildcard_then_literal_payload_choice_constructor_failure_result.stderr_text.find(
               "switch constructor pattern 'Int(...)' is duplicated"
           ) != std::string::npos);
    assert(switch_wildcard_then_literal_payload_choice_constructor_failure_result.stderr_text.find(
               "switch value pattern"
           ) == std::string::npos);

    auto switch_literal_then_wildcard_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_literal_then_wildcard_payload_choice_constructor_failure.or";
    {
        std::ofstream output(switch_literal_then_wildcard_payload_choice_constructor_failure_path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(1) => 1\n";
        output << "        Int(value) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_literal_then_wildcard_payload_choice_constructor_failure_path_text =
        switch_literal_then_wildcard_payload_choice_constructor_failure_path.string();
    std::array<char const*, 3> switch_literal_then_wildcard_payload_choice_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_literal_then_wildcard_payload_choice_constructor_failure_path_text.c_str()
    };
    auto switch_literal_then_wildcard_payload_choice_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_literal_then_wildcard_payload_choice_constructor_failure_argv.data(),
            switch_literal_then_wildcard_payload_choice_constructor_failure_argv.size()
        )
    );

    assert(switch_literal_then_wildcard_payload_choice_constructor_failure_result.exit_code == 1);
    assert(switch_literal_then_wildcard_payload_choice_constructor_failure_result.stdout_text.empty());
    assert(switch_literal_then_wildcard_payload_choice_constructor_failure_result.stderr_text.find(
               "switch constructor pattern 'Int(...)' is duplicated"
           ) != std::string::npos);

    auto switch_multi_payload_partial_overlap_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_multi_payload_partial_overlap_choice_constructor_failure.or";
    {
        std::ofstream output(switch_multi_payload_partial_overlap_choice_constructor_failure_path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int64, right: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(left, 1) => 1\n";
        output << "        Both(other, 1) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_multi_payload_partial_overlap_choice_constructor_failure_path_text =
        switch_multi_payload_partial_overlap_choice_constructor_failure_path.string();
    std::array<char const*, 3> switch_multi_payload_partial_overlap_choice_constructor_failure_argv {
        "orisonc",
        "--parse",
        switch_multi_payload_partial_overlap_choice_constructor_failure_path_text.c_str()
    };
    auto switch_multi_payload_partial_overlap_choice_constructor_failure_result = app.run(
        std::span<char const* const>(
            switch_multi_payload_partial_overlap_choice_constructor_failure_argv.data(),
            switch_multi_payload_partial_overlap_choice_constructor_failure_argv.size()
        )
    );

    assert(switch_multi_payload_partial_overlap_choice_constructor_failure_result.exit_code == 1);
    assert(switch_multi_payload_partial_overlap_choice_constructor_failure_result.stdout_text.empty());
    assert(switch_multi_payload_partial_overlap_choice_constructor_failure_result.stderr_text.find(
               "switch constructor pattern 'Both(...)' is duplicated"
           ) != std::string::npos);

    auto switch_multi_payload_disjoint_literal_choice_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_multi_payload_disjoint_literal_choice_constructor_success.or";
    {
        std::ofstream output(switch_multi_payload_disjoint_literal_choice_constructor_success_path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int64, right: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(left, 1) => 1\n";
        output << "        Both(other, 2) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_multi_payload_disjoint_literal_choice_constructor_success_path_text =
        switch_multi_payload_disjoint_literal_choice_constructor_success_path.string();
    std::array<char const*, 3> switch_multi_payload_disjoint_literal_choice_constructor_success_argv {
        "orisonc",
        "--parse",
        switch_multi_payload_disjoint_literal_choice_constructor_success_path_text.c_str()
    };
    auto switch_multi_payload_disjoint_literal_choice_constructor_success_result = app.run(
        std::span<char const* const>(
            switch_multi_payload_disjoint_literal_choice_constructor_success_argv.data(),
            switch_multi_payload_disjoint_literal_choice_constructor_success_argv.size()
        )
    );

    assert(switch_multi_payload_disjoint_literal_choice_constructor_success_result.exit_code == 0);
    assert(switch_multi_payload_disjoint_literal_choice_constructor_success_result.stderr_text.empty());
    assert(switch_multi_payload_disjoint_literal_choice_constructor_success_result.stdout_text.find("parsed ") !=
           std::string::npos);

    auto switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "orison_compiler_app_switch_multi_payload_disjoint_leading_literal_choice_constructor_success.or";
    {
        std::ofstream output(switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int64, right: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(1, left) => 1\n";
        output << "        Both(2, right) => 2\n";
        output << "        default => 0\n";
    }

    auto switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path_text =
        switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path.string();
    std::array<char const*, 3> switch_multi_payload_disjoint_leading_literal_choice_constructor_success_argv {
        "orisonc",
        "--parse",
        switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path_text.c_str()
    };
    auto switch_multi_payload_disjoint_leading_literal_choice_constructor_success_result = app.run(
        std::span<char const* const>(
            switch_multi_payload_disjoint_leading_literal_choice_constructor_success_argv.data(),
            switch_multi_payload_disjoint_leading_literal_choice_constructor_success_argv.size()
        )
    );

    assert(switch_multi_payload_disjoint_leading_literal_choice_constructor_success_result.exit_code == 0);
    assert(switch_multi_payload_disjoint_leading_literal_choice_constructor_success_result.stderr_text.empty());
    assert(switch_multi_payload_disjoint_leading_literal_choice_constructor_success_result.stdout_text.find("parsed ") !=
           std::string::npos);

    auto switch_missing_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_switch_missing_choice_variant_failure.or";
    {
        std::ofstream output(switch_missing_choice_variant_failure_path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
    }

    auto switch_missing_choice_variant_failure_path_text =
        switch_missing_choice_variant_failure_path.string();
    std::array<char const*, 3> switch_missing_choice_variant_failure_argv {
        "orisonc",
        "--parse",
        switch_missing_choice_variant_failure_path_text.c_str()
    };
    auto switch_missing_choice_variant_failure_result = app.run(
        std::span<char const* const>(
            switch_missing_choice_variant_failure_argv.data(),
            switch_missing_choice_variant_failure_argv.size()
        )
    );

    assert(switch_missing_choice_variant_failure_result.exit_code == 1);
    assert(switch_missing_choice_variant_failure_result.stdout_text.empty());
    assert(switch_missing_choice_variant_failure_result.stderr_text.find(
               "switch is missing zero-payload choice variant 'PermissionDenied'"
           ) != std::string::npos);

    auto break_outside_loop_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_break_outside_loop_failure.or";
    {
        std::ofstream output(break_outside_loop_failure_path);
        output << "package demo.loops\n";
        output << "function stop() -> Unit\n";
        output << "    break\n";
    }

    auto break_outside_loop_failure_path_text = break_outside_loop_failure_path.string();
    std::array<char const*, 3> break_outside_loop_failure_argv {
        "orisonc",
        "--parse",
        break_outside_loop_failure_path_text.c_str()
    };
    auto break_outside_loop_failure_result = app.run(
        std::span<char const* const>(
            break_outside_loop_failure_argv.data(),
            break_outside_loop_failure_argv.size()
        )
    );

    assert(break_outside_loop_failure_result.exit_code == 1);
    assert(break_outside_loop_failure_result.stdout_text.empty());
    assert(break_outside_loop_failure_result.stderr_text.find(
               "break statement is only valid inside loops"
           ) != std::string::npos);

    auto continue_inside_loop_success_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_continue_inside_loop_success.or";
    {
        std::ofstream output(continue_inside_loop_success_path);
        output << "package demo.loops\n";
        output << "function scan(items: shared View<Int64>) -> Unit\n";
        output << "    for item in items\n";
        output << "        continue\n";
    }

    auto continue_inside_loop_success_path_text = continue_inside_loop_success_path.string();
    std::array<char const*, 3> continue_inside_loop_success_argv {
        "orisonc",
        "--parse",
        continue_inside_loop_success_path_text.c_str()
    };
    auto continue_inside_loop_success_result = app.run(
        std::span<char const* const>(
            continue_inside_loop_success_argv.data(),
            continue_inside_loop_success_argv.size()
        )
    );

    assert(continue_inside_loop_success_result.exit_code == 0);
    assert(continue_inside_loop_success_result.stderr_text.empty());
    assert(continue_inside_loop_success_result.stdout_text.find("parsed ") != std::string::npos);

    auto this_outside_method_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_this_outside_method_failure.or";
    {
        std::ofstream output(this_outside_method_failure_path);
        output << "package demo.receiver\n";
        output << "function current() -> Int64\n";
        output << "    return this\n";
    }

    auto this_outside_method_failure_path_text = this_outside_method_failure_path.string();
    std::array<char const*, 3> this_outside_method_failure_argv {
        "orisonc",
        "--parse",
        this_outside_method_failure_path_text.c_str()
    };
    auto this_outside_method_failure_result = app.run(
        std::span<char const* const>(
            this_outside_method_failure_argv.data(),
            this_outside_method_failure_argv.size()
        )
    );

    assert(this_outside_method_failure_result.exit_code == 1);
    assert(this_outside_method_failure_result.stdout_text.empty());
    assert(this_outside_method_failure_result.stderr_text.find(
               "receiver 'this' is only valid inside implements or extend methods"
           ) != std::string::npos);

    auto this_type_signature_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_this_type_signature_failure.or";
    {
        std::ofstream output(this_type_signature_failure_path);
        output << "package demo.receiver\n";
        output << "function project(value: shared This) -> shared This\n";
        output << "    return value\n";
    }

    auto this_type_signature_failure_path_text = this_type_signature_failure_path.string();
    std::array<char const*, 3> this_type_signature_failure_argv {
        "orisonc",
        "--parse",
        this_type_signature_failure_path_text.c_str()
    };
    auto this_type_signature_failure_result = app.run(
        std::span<char const* const>(
            this_type_signature_failure_argv.data(),
            this_type_signature_failure_argv.size()
        )
    );

    assert(this_type_signature_failure_result.exit_code == 1);
    assert(this_type_signature_failure_result.stdout_text.empty());
    assert(this_type_signature_failure_result.stderr_text.find(
               "This type is only valid inside interface, implements, or extend methods"
           ) != std::string::npos);

    auto receiver_parameter_nonself_type_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_receiver_parameter_nonself_type_failure.or";
    {
        std::ofstream output(receiver_parameter_nonself_type_failure_path);
        output << "package demo.receiver\n";
        output << "extend Buffer\n";
        output << "    function reset(this: Int64) -> Unit\n";
        output << "        return\n";
    }

    auto receiver_parameter_nonself_type_failure_path_text =
        receiver_parameter_nonself_type_failure_path.string();
    std::array<char const*, 3> receiver_parameter_nonself_type_failure_argv {
        "orisonc",
        "--parse",
        receiver_parameter_nonself_type_failure_path_text.c_str()
    };
    auto receiver_parameter_nonself_type_failure_result = app.run(
        std::span<char const* const>(
            receiver_parameter_nonself_type_failure_argv.data(),
            receiver_parameter_nonself_type_failure_argv.size()
        )
    );

    assert(receiver_parameter_nonself_type_failure_result.exit_code == 1);
    assert(receiver_parameter_nonself_type_failure_result.stdout_text.empty());
    assert(receiver_parameter_nonself_type_failure_result.stderr_text.find(
               "receiver parameter 'this' must use This, shared This, or exclusive This"
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
