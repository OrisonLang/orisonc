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
        ("orison_driver_unsafe_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto unsafe_intrinsic_failure_path = std::filesystem::temp_directory_path() / "unsafe_intrinsic_failure.or";
    write_fixture(
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

    auto unsafe_intrinsic_success_path = std::filesystem::temp_directory_path() / "unsafe_intrinsic_success.or";
    write_fixture(
        unsafe_intrinsic_success_path,
        "demo.unsafe",
        {
            "function zero_byte(p: Address) -> Unit",
            "    unsafe",
            "        raw_write(p, 0)",
        }
    );
    assert_parse_success(run_parse(app, unsafe_intrinsic_success_path));

    auto address_of_shape_failure_path = std::filesystem::temp_directory_path() / "address_of_shape_failure.or";
    write_fixture(
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

    auto raw_offset_shape_failure_path = std::filesystem::temp_directory_path() / "raw_offset_shape_failure.or";
    write_fixture(
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

    auto raw_offset_noninteger_failure_path = std::filesystem::temp_directory_path() / "raw_offset_noninteger_failure.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "nested_unsafe_operand_success.or";
    write_fixture(
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
        std::filesystem::temp_directory_path() / "index_access_noninteger_failure.or";
    write_fixture(
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

    auto index_access_integer_success_path = std::filesystem::temp_directory_path() / "index_access_integer_success.or";
    write_fixture(
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

    auto unsafe_call_failure_path = std::filesystem::temp_directory_path() / "unsafe_call_failure.or";
    write_fixture(
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

    auto unsafe_call_block_success_path = std::filesystem::temp_directory_path() / "unsafe_call_block_success.or";
    write_fixture(
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

    return 0;
}
