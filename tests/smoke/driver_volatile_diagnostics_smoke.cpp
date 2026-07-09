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
        ("orison_driver_volatile_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto volatile_read_return_type_failure_path =
        std::filesystem::temp_directory_path() / "volatile_read_return_type_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_read_same_width_return_success.or";
    write_fixture(
        volatile_read_same_width_return_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return volatile_read(p)",
        }
    );
    assert_parse_success(run_parse(app, volatile_read_same_width_return_success_path));

    auto volatile_read_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "volatile_read_typed_binding_failure.or";
    write_fixture(
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

    auto volatile_read_same_width_binding_success_path =
        std::filesystem::temp_directory_path() / "volatile_read_same_width_binding_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_read_typed_binding_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_write_value_type_failure.or";
    write_fixture(
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

    auto volatile_write_same_width_integer_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_same_width_integer_value_success.or";
    write_fixture(
        volatile_write_same_width_integer_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(p, value)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_same_width_integer_value_success_path));

    auto volatile_write_pointer_sized_integer_value_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_pointer_sized_integer_value_failure.or";
    write_fixture(
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

    auto volatile_write_computed_integer_sum_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_computed_integer_sum_success.or";
    write_fixture(
        volatile_write_computed_integer_sum_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(input) + 1)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_computed_integer_sum_success_path));

    auto volatile_write_computed_bitwise_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_computed_bitwise_value_success.or";
    write_fixture(
        volatile_write_computed_bitwise_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(out, value bit_or 1)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_computed_bitwise_value_success_path));

    auto volatile_write_computed_ternary_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_computed_ternary_pointer_sized_failure.or";
    write_fixture(
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

    auto volatile_write_rebound_computed_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_rebound_computed_value_success.or";
    write_fixture(
        volatile_write_rebound_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    volatile_write(out, masked)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_rebound_computed_value_success_path));

    auto volatile_write_branch_merged_computed_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_branch_merged_computed_value_success.or";
    write_fixture(
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

    auto volatile_write_branch_merged_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_branch_merged_pointer_sized_failure.or";
    write_fixture(
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

    auto volatile_write_switch_merged_computed_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_switch_merged_computed_value_success.or";
    write_fixture(
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

    auto volatile_write_switch_merged_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_switch_merged_pointer_sized_failure.or";
    write_fixture(
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

    auto volatile_write_array_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_array_indexed_value_success.or";
    write_fixture(
        volatile_write_array_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    volatile_write(out, items[0])",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_array_indexed_value_success_path));

    auto volatile_write_bound_array_literal_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_bound_array_literal_indexed_value_success.or";
    write_fixture(
        volatile_write_bound_array_literal_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    volatile_write(out, staged[0])",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_bound_array_literal_indexed_value_success_path));

    auto volatile_write_array_indexed_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_array_indexed_pointer_sized_failure.or";
    write_fixture(
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

    auto volatile_write_member_container_field_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_member_container_field_indexed_value_success.or";
    write_fixture(
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

    auto volatile_write_nested_scalar_field_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_nested_scalar_field_value_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_write_nested_scalar_field_computed_value_success.or";
    write_fixture(
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

    auto volatile_write_helper_returned_container_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_helper_returned_container_indexed_value_success.or";
    write_fixture(
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

    auto volatile_write_member_container_field_indexed_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_member_container_field_indexed_pointer_sized_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_write_nested_scalar_field_pointer_sized_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_write_integer_literal_success.or";
    write_fixture(
        volatile_write_integer_literal_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    volatile_write(p, 0)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_integer_literal_success_path));

    auto volatile_write_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_integer_cast_success.or";
    write_fixture(
        volatile_write_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as UInt32)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_integer_cast_success_path));

    auto volatile_write_same_width_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "volatile_write_same_width_integer_cast_success.or";
    write_fixture(
        volatile_write_same_width_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as Int32)",
        }
    );
    assert_parse_success(run_parse(app, volatile_write_same_width_integer_cast_success_path));

    auto volatile_write_integer_cast_target_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_integer_cast_target_failure.or";
    write_fixture(
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

    auto volatile_write_pointer_sized_integer_cast_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_pointer_sized_integer_cast_failure.or";
    write_fixture(
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

    auto volatile_write_recovered_volatile_read_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_recovered_volatile_read_failure.or";
    write_fixture(
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

    auto volatile_write_member_returned_volatile_read_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_member_returned_volatile_read_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_write_helper_type_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "volatile_write_member_helper_type_failure.or";
    write_fixture(
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

    auto volatile_write_raw_offset_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "volatile_write_raw_offset_helper_type_failure.or";
    write_fixture(
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

    return 0;
}
