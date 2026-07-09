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
        ("orison_driver_raw_helper_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto raw_write_integer_literal_success_path =
        std::filesystem::temp_directory_path() / "raw_write_integer_literal_success.or";
    write_fixture(
        raw_write_integer_literal_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    raw_write(p, 0)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_integer_literal_success_path));

    auto raw_write_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "raw_write_integer_cast_success.or";
    write_fixture(
        raw_write_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as UInt32)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_integer_cast_success_path));

    auto raw_write_same_width_integer_cast_success_path =
        std::filesystem::temp_directory_path() / "raw_write_same_width_integer_cast_success.or";
    write_fixture(
        raw_write_same_width_integer_cast_success_path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as Int32)",
        }
    );
    assert_parse_success(run_parse(app, raw_write_same_width_integer_cast_success_path));

    auto raw_write_integer_cast_target_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_integer_cast_target_failure.or";
    write_fixture(
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

    auto raw_write_pointer_sized_integer_cast_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_pointer_sized_integer_cast_failure.or";
    write_fixture(
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

    auto raw_write_recovered_raw_read_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_recovered_raw_read_failure.or";
    write_fixture(
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

    auto raw_write_member_returned_raw_read_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_member_returned_raw_read_failure.or";
    write_fixture(
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

    auto raw_write_helper_type_failure_path = std::filesystem::temp_directory_path() / "raw_write_helper_type_failure.or";
    write_fixture(
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

    auto raw_write_member_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_member_helper_type_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_raw_offset_helper_type_failure.or";
    write_fixture(
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

    auto raw_write_member_raw_offset_helper_type_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_member_raw_offset_helper_type_failure.or";
    write_fixture(
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

    auto raw_write_record_pointer_field_type_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_record_pointer_field_type_failure.or";
    write_fixture(
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

    auto raw_write_generic_helper_returned_pointer_same_width_success_path =
        std::filesystem::temp_directory_path() / "raw_write_generic_helper_returned_pointer_same_width_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_generic_helper_returned_pointer_mismatch_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_generic_receiver_method_pointer_same_width_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_generic_receiver_method_pointer_mismatch_failure.or";
    write_fixture(
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

    auto raw_write_generic_record_pointer_field_same_width_success_path =
        std::filesystem::temp_directory_path() / "raw_write_generic_record_pointer_field_same_width_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_generic_record_pointer_field_mismatch_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "raw_write_generic_record_scalar_field_same_width_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "address_return_generic_record_field_success.or";
    write_fixture(
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

    auto member_field_address_pointer_constructor_success_path =
        std::filesystem::temp_directory_path() / "member_field_address_pointer_constructor_success.or";
    write_fixture(
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

    auto raw_write_indexed_record_pointer_field_type_failure_path =
        std::filesystem::temp_directory_path() / "raw_write_indexed_record_pointer_field_type_failure.or";
    write_fixture(
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

    auto indexed_member_field_address_pointer_constructor_success_path =
        std::filesystem::temp_directory_path() / "indexed_member_field_address_pointer_constructor_success.or";
    write_fixture(
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

    auto rebound_indexed_record_pointer_field_type_failure_path =
        std::filesystem::temp_directory_path() / "rebound_indexed_record_pointer_field_type_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "rebound_indexed_member_field_address_pointer_constructor_success.or";
    write_fixture(
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

    auto return_rebound_indexed_record_pointer_field_success_path =
        std::filesystem::temp_directory_path() / "return_rebound_indexed_record_pointer_field_success.or";
    write_fixture(
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

    auto return_rebound_indexed_member_field_address_success_path =
        std::filesystem::temp_directory_path() / "return_rebound_indexed_member_field_address_success.or";
    write_fixture(
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

    auto return_rebound_indexed_pointer_used_by_helper_success_path =
        std::filesystem::temp_directory_path() / "return_rebound_indexed_pointer_used_by_helper_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "return_rebound_indexed_address_pointer_constructor_success.or";
    write_fixture(
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

    return 0;
}
