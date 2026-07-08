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
        ("orison_driver_raw_value_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto raw_read_typed_binding_failure_path = std::filesystem::temp_directory_path() / "raw_read_typed_binding_failure.or";
    write_fixture(
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

    auto raw_read_typed_binding_success_path = std::filesystem::temp_directory_path() / "raw_read_typed_binding_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_read_same_width_binding_success.or";
    write_fixture(
        raw_read_same_width_binding_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> Int32",
            "    let value: UInt32 = raw_read(p)",
            "    return value as Int32",
        }
    );
    assert_parse_success(run_parse(app, raw_read_same_width_binding_success_path));

    auto raw_read_return_type_failure_path = std::filesystem::temp_directory_path() / "raw_read_return_type_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_read_same_width_return_success.or";
    write_fixture(
        raw_read_same_width_return_success_path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return raw_read(p)",
        }
    );
    assert_parse_success(run_parse(app, raw_read_same_width_return_success_path));

    auto raw_write_value_type_failure_path = std::filesystem::temp_directory_path() / "raw_write_value_type_failure.or";
    write_fixture(
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

    auto raw_write_same_width_integer_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_same_width_integer_value_success.or";
    write_fixture(
        raw_write_same_width_integer_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(p, value)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_same_width_integer_value_success_path));

    auto raw_write_pointer_sized_integer_value_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_pointer_sized_integer_value_failure.or";
    write_fixture(
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

    auto raw_write_computed_integer_sum_success_path =
        std::filesystem::temp_directory_path() / "raw_write_computed_integer_sum_success.or";
    write_fixture(
        raw_write_computed_integer_sum_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(input) + 1)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_computed_integer_sum_success_path));

    auto raw_write_computed_bitwise_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_computed_bitwise_value_success.or";
    write_fixture(
        raw_write_computed_bitwise_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(out, value bit_or 1)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_computed_bitwise_value_success_path));

    auto raw_write_computed_ternary_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_computed_ternary_pointer_sized_failure.or";
    write_fixture(
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

    auto raw_write_rebound_computed_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_rebound_computed_value_success.or";
    write_fixture(
        raw_write_rebound_computed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    raw_write(out, masked)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_rebound_computed_value_success_path));

    auto raw_write_branch_merged_computed_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_branch_merged_computed_value_success.or";
    write_fixture(
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

    auto raw_write_branch_merged_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_branch_merged_pointer_sized_failure.or";
    write_fixture(
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

    auto raw_write_switch_merged_computed_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_switch_merged_computed_value_success.or";
    write_fixture(
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

    auto raw_write_switch_merged_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_switch_merged_pointer_sized_failure.or";
    write_fixture(
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

    auto raw_write_array_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_array_indexed_value_success.or";
    write_fixture(
        raw_write_array_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    raw_write(out, items[0])",
        }
    );
    assert_parse_success(run_parse(app, raw_write_array_indexed_value_success_path));

    auto raw_write_bound_array_literal_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_bound_array_literal_indexed_value_success.or";
    write_fixture(
        raw_write_bound_array_literal_indexed_value_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    raw_write(out, staged[0])",
        }
    );
    assert_parse_success(run_parse(app, raw_write_bound_array_literal_indexed_value_success_path));

    auto raw_write_array_indexed_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_array_indexed_pointer_sized_failure.or";
    write_fixture(
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

    auto raw_write_member_container_field_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_member_container_field_indexed_value_success.or";
    write_fixture(
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

    auto raw_write_nested_scalar_field_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_nested_scalar_field_value_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_nested_scalar_field_computed_value_success.or";
    write_fixture(
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

    auto raw_write_helper_returned_container_indexed_value_success_path =
        std::filesystem::temp_directory_path() / "raw_write_helper_returned_container_indexed_value_success.or";
    write_fixture(
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

    auto raw_write_member_container_field_indexed_pointer_sized_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_member_container_field_indexed_pointer_sized_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_nested_scalar_field_pointer_sized_failure.or";
    write_fixture(
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

    return 0;
}
