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
        ("orison_driver_pointer_address_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto pointer_construction_failure_path = std::filesystem::temp_directory_path() / "pointer_construction_failure.or";
    write_fixture(
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

    auto pointer_construction_success_path = std::filesystem::temp_directory_path() / "pointer_construction_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_construction_addressof_typed_failure.or";
    write_fixture(
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

    auto pointer_construction_addressof_same_width_success_path =
        std::filesystem::temp_directory_path() / "pointer_construction_addressof_same_width_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_construction_noarg_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_construction_multiarg_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_construction_nonaddress_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_construction_addressof_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_typed_binding_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_typed_binding_name_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_typed_binding_field_pointer_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_typed_binding_same_width_field_pointer_success.or";
    write_fixture(
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

    auto pointer_return_failure_path = std::filesystem::temp_directory_path() / "pointer_return_failure.or";
    write_fixture(
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

    auto pointer_return_name_failure_path = std::filesystem::temp_directory_path() / "pointer_return_name_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_return_helper_pointer_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_return_same_width_helper_pointer_success.or";
    write_fixture(
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

    auto pointer_typed_binding_success_path =
        std::filesystem::temp_directory_path() / "pointer_typed_binding_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "pointer_rawoffset_typed_failure.or";
    write_fixture(
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

    auto pointer_raw_offset_same_width_success_path =
        std::filesystem::temp_directory_path() / "pointer_rawoffset_same_width_success.or";
    write_fixture(
        pointer_raw_offset_same_width_success_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
        }
    );
    assert_parse_success(run_parse(app, pointer_raw_offset_same_width_success_path));

    auto address_typed_binding_failure_path =
        std::filesystem::temp_directory_path() / "address_typed_binding_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "address_typed_binding_name_failure.or";
    write_fixture(
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

    auto address_return_failure_path = std::filesystem::temp_directory_path() / "address_return_failure.or";
    write_fixture(
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

    auto address_return_name_failure_path = std::filesystem::temp_directory_path() / "address_return_name_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "address_typed_binding_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "address_typed_binding_field_address_success.or";
    write_fixture(
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

    auto address_typed_binding_indexed_address_success_path =
        std::filesystem::temp_directory_path() / "address_typed_binding_indexed_address_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "address_return_helper_returned_address_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "address_return_generic_helper_success.or";
    write_fixture(
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

    return 0;
}
