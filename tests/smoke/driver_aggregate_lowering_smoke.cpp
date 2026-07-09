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

auto run_emit_llvm(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "--emit-llvm",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_failure_with_no_stdout_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_message) != std::string::npos);
}

void assert_success_with_stdout_contains(
    orison::driver::CompileResult const& result,
    std::initializer_list<std::string_view> expected_fragments
) {
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    for (auto expected_fragment : expected_fragments) {
        assert(result.stdout_text.find(expected_fragment) != std::string::npos);
    }
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_aggregate_lowering_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto emit_scalar_member_assignment_failure_path =
        std::filesystem::temp_directory_path() / "emit_scalar_member_assignment_failure.or";
    write_fixture(
        emit_scalar_member_assignment_failure_path,
        "demo.emit",
        {
            "unsafe function main() -> Unit",
            "    var total: UInt32 = 0 as UInt32",
            "    total.status = 1 as UInt32",
            "    return",
        }
    );
    auto emit_scalar_member_assignment_failure =
        run_emit_llvm(app, emit_scalar_member_assignment_failure_path);
    assert_failure_with_no_stdout_contains(
        emit_scalar_member_assignment_failure,
        "lowering aggregate assignment member target is unsupported"
    );

    auto emit_scalar_index_assignment_failure_path =
        std::filesystem::temp_directory_path() / "emit_scalar_index_assignment_failure.or";
    write_fixture(
        emit_scalar_index_assignment_failure_path,
        "demo.emit",
        {
            "unsafe function main(index: UInt64) -> Unit",
            "    var total: UInt32 = 0 as UInt32",
            "    total[index] = 1 as UInt32",
            "    return",
        }
    );
    auto emit_scalar_index_assignment_failure =
        run_emit_llvm(app, emit_scalar_index_assignment_failure_path);
    assert_failure_with_no_stdout_contains(
        emit_scalar_index_assignment_failure,
        "lowering aggregate assignment index target is unsupported"
    );

    auto emit_local_record_address_path = std::filesystem::temp_directory_path() / "emit_local_record_address.or";
    write_fixture(
        emit_local_record_address_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "unsafe function main() -> Address",
            "    var regs = Registers(0 as UInt32, 1 as UInt32)",
            "    address_of(regs.status)",
        }
    );
    auto emit_local_record_address = run_emit_llvm(app, emit_local_record_address_path);
    assert_success_with_stdout_contains(
        emit_local_record_address,
        {
            "%tmp0 = insertvalue %record.Registers undef, i32 0, 0",
            "getelementptr %record.Registers, ptr %regs.addr, i32 0, i32 1",
        }
    );

    auto emit_local_array_address_path = std::filesystem::temp_directory_path() / "emit_local_array_address.or";
    write_fixture(
        emit_local_array_address_path,
        "demo.emit",
        {
            "record Buffer",
            "    bytes: Array<Byte, 4>",
            "unsafe function main(index: UInt64) -> Address",
            "    var buffer = Buffer([1, 2, 3, 4])",
            "    address_of(buffer.bytes[index])",
        }
    );
    auto emit_local_array_address = run_emit_llvm(app, emit_local_array_address_path);
    assert_success_with_stdout_contains(
        emit_local_array_address,
        {
            "%tmp0 = insertvalue [4 x i8] undef, i8 1, 0",
            "getelementptr [4 x i8], ptr %tmp5, i64 0, i64 %index",
        }
    );

    auto emit_aggregate_assignment_path = std::filesystem::temp_directory_path() / "emit_aggregate_assignment.or";
    write_fixture(
        emit_aggregate_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "unsafe function main(index: UInt64) -> Address",
            "    var regs = Registers(0 as UInt32, 1 as UInt32)",
            "    regs = Registers(2 as UInt32, 3 as UInt32)",
            "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]",
            "    bytes = [5, 6, 7, 8]",
            "    address_of(bytes[index])",
        }
    );
    auto emit_aggregate_assignment = run_emit_llvm(app, emit_aggregate_assignment_path);
    assert_success_with_stdout_contains(
        emit_aggregate_assignment,
        {
            "store %record.Registers %tmp",
            "store [4 x i8] %tmp",
        }
    );

    auto emit_aggregate_element_assignment_path =
        std::filesystem::temp_directory_path() / "emit_aggregate_element_assignment.or";
    write_fixture(
        emit_aggregate_element_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "unsafe function main(index: UInt64) -> Unit",
            "    var regs = Registers(0 as UInt32, 1 as UInt32)",
            "    regs.status = 4 as UInt32",
            "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]",
            "    bytes[index] = 9",
            "    return",
        }
    );
    auto emit_aggregate_element_assignment = run_emit_llvm(app, emit_aggregate_element_assignment_path);
    assert_success_with_stdout_contains(
        emit_aggregate_element_assignment,
        {
            "store i32 4, ptr %tmp",
            "store i8 9, ptr %tmp",
        }
    );

    auto emit_pointer_aggregate_assignment_path =
        std::filesystem::temp_directory_path() / "emit_pointer_aggregate_assignment.or";
    write_fixture(
        emit_pointer_aggregate_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "record Buffer",
            "    bytes: Array<Byte, 4>",
            "unsafe function main(regs: Pointer<Registers>, buffer: Pointer<Buffer>, index: UInt64) -> Unit",
            "    regs.status = 4 as UInt32",
            "    buffer.bytes[index] = 7",
            "    return",
        }
    );
    auto emit_pointer_aggregate_assignment = run_emit_llvm(app, emit_pointer_aggregate_assignment_path);
    assert_success_with_stdout_contains(
        emit_pointer_aggregate_assignment,
        {
            "getelementptr %record.Registers, ptr %regs, i32 0, i32 1",
            "getelementptr %record.Buffer, ptr %buffer, i32 0, i32 0",
            "store i32 4, ptr %tmp",
            "store i8 7, ptr %tmp",
        }
    );

    auto emit_pointer_nested_aggregate_assignment_path =
        std::filesystem::temp_directory_path() / "emit_pointer_nested_aggregate_assignment.or";
    write_fixture(
        emit_pointer_nested_aggregate_assignment_path,
        "demo.emit",
        {
            "record Registers",
            "    data: UInt32",
            "    status: UInt32",
            "record Buffer",
            "    bytes: Array<Byte, 4>",
            "record Log",
            "    entries: Array<Registers, 2>",
            "record Matrix",
            "    rows: Array<Array<Byte, 4>, 2>",
            "unsafe function main(log: Pointer<Log>, matrix: Pointer<Matrix>, index: UInt64, inner: UInt64) -> Unit",
            "    log.entries[index].status = 8 as UInt32",
            "    matrix.rows[index][inner] = 1 as Byte",
            "    return",
        }
    );
    auto emit_pointer_nested_aggregate_assignment =
        run_emit_llvm(app, emit_pointer_nested_aggregate_assignment_path);
    assert_success_with_stdout_contains(
        emit_pointer_nested_aggregate_assignment,
        {
            "getelementptr %record.Log, ptr %log, i32 0, i32 0",
            "getelementptr [2 x %record.Registers], ptr %tmp",
            "getelementptr %record.Matrix, ptr %matrix, i32 0, i32 0",
            "getelementptr [2 x [4 x i8]], ptr %tmp",
            "store i32 8, ptr %tmp",
            "store i8 1, ptr %tmp",
        }
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
