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

auto run_single_file_command(
    orison::driver::CompilerApp const& app,
    std::string_view command,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        command.data(),
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_emit_llvm(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    return run_single_file_command(app, "--emit-llvm", path);
}

void assert_failure_with_no_stdout_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_message) != std::string::npos);
}

}  // namespace

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_basic_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::driver::CompilerApp app;

    std::array<char const*, 2> version_argv {"orisonc", "--version"};
    auto result = app.run(std::span<char const* const>(version_argv.data(), version_argv.size()));

    assert(result.exit_code == 0);
    assert(result.stdout_text == "orisonc 0.1.0-dev\n");
    assert(result.stderr_text.empty());

    auto emit_path = std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm.or";
    write_concurrency_fixture(
        emit_path,
        "demo.emit",
        {
            "function main() -> UInt32",
            "    42 as UInt32",
        }
    );
    auto emit_result = run_emit_llvm(app, emit_path);
    assert(emit_result.exit_code == 0);
    assert(emit_result.stderr_text.empty());
    assert(
        emit_result.stdout_text ==
        "; Orison LLVM IR scaffold\n"
        "; package demo.emit\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 42\n"
        "}\n"
        "\n"
    );

    auto first_interface_path = smoke_temp_root / "external_types.opi";
    {
        auto output = std::ofstream(first_interface_path);
        output << "orison-package-interface 1\n"
                  "package external.types\n"
                  "type record Pair 2\n";
    }
    auto second_interface_path = smoke_temp_root / "external_values.opi";
    {
        auto output = std::ofstream(second_interface_path);
        output << "orison-package-interface 1\n"
                  "package external.values\n"
                  "type choice Result 1\n";
    }
    auto imported_type_path = smoke_temp_root / "imported_type.or";
    write_concurrency_fixture(
        imported_type_path,
        "demo.imported",
        {
            "import",
            "    Pair from external.types",
            "    Result from external.values",
            "function consume(value: Result<UInt32, UInt64>) -> Unit",
            "    return",
        }
    );
    auto first_interface_path_text = first_interface_path.string();
    auto second_interface_path_text = second_interface_path.string();
    auto imported_type_path_text = imported_type_path.string();
    std::array<char const*, 7> imported_type_argv {
        "orisonc",
        "--package-interface",
        first_interface_path_text.c_str(),
        "--package-interface",
        second_interface_path_text.c_str(),
        "--emit-llvm",
        imported_type_path_text.c_str(),
    };
    auto imported_type_result = app.run(
        std::span<char const* const>(imported_type_argv.data(), imported_type_argv.size())
    );
    assert_failure_with_no_stdout_contains(
        imported_type_result,
        "type 'Result<UInt32, UInt64>' supplies 2 type arguments; type 'Result' requires 1"
    );

    std::array<char const*, 2> missing_interface_value_argv {
        "orisonc",
        "--package-interface",
    };
    auto missing_interface_value = app.run(
        std::span<char const* const>(missing_interface_value_argv.data(), missing_interface_value_argv.size())
    );
    assert_failure_with_no_stdout_contains(
        missing_interface_value,
        "--package-interface requires a file path"
    );

    auto missing_interface_path = smoke_temp_root / "missing.opi";
    auto missing_interface_path_text = missing_interface_path.string();
    auto emit_path_text = emit_path.string();
    std::array<char const*, 5> missing_interface_argv {
        "orisonc",
        "--package-interface",
        missing_interface_path_text.c_str(),
        "--emit-llvm",
        emit_path_text.c_str(),
    };
    auto missing_interface = app.run(
        std::span<char const* const>(missing_interface_argv.data(), missing_interface_argv.size())
    );
    assert_failure_with_no_stdout_contains(missing_interface, "unable to read package interface");

    auto invalid_interface_path = smoke_temp_root / "invalid.opi";
    {
        auto output = std::ofstream(invalid_interface_path);
        output << "orison-package-interface 2\npackage external.types\n";
    }
    auto invalid_interface_path_text = invalid_interface_path.string();
    std::array<char const*, 5> invalid_interface_argv {
        "orisonc",
        "--package-interface",
        invalid_interface_path_text.c_str(),
        "--emit-llvm",
        emit_path_text.c_str(),
    };
    auto invalid_interface = app.run(
        std::span<char const* const>(invalid_interface_argv.data(), invalid_interface_argv.size())
    );
    assert_failure_with_no_stdout_contains(
        invalid_interface,
        "invalid package interface '" + invalid_interface_path_text +
            "': unsupported package interface version '2'"
    );

    auto view_descriptor_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "view_descriptor_reads.or";
    auto view_descriptor = run_emit_llvm(app, view_descriptor_path);
    assert(view_descriptor.exit_code == 0);
    assert(view_descriptor.stderr_text.empty());
    assert(
        view_descriptor.stdout_text.find("define i32 @sum({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        view_descriptor.stdout_text.find(
            "%values.sequence_for0.value = load i32, ptr %values.sequence_for0.element.addr"
        ) != std::string::npos
    );

    std::array<char const*, 2> unknown_flag_argv {
        "orisonc",
        "--unknown-option"
    };
    auto unknown_flag =
        app.run(std::span<char const* const>(
            unknown_flag_argv.data(),
            unknown_flag_argv.size()
        ));
    assert_failure_with_no_stdout_contains(
        unknown_flag,
        "usage: orisonc [--package-interface <file>]..."
    );

    auto emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm_failure.or";
    write_concurrency_fixture(
        emit_failure_path,
        "demo.emit",
        {
            "function same(left: Bool, right: Bool) -> Bool",
            "    left < right",
        }
    );
    auto emit_failure = run_emit_llvm(app, emit_failure_path);
    assert_failure_with_no_stdout_contains(
        emit_failure,
        "lowering does not yet support this return expression: unsupported operator: <"
    );

    auto unary_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm_unary_failure.or";
    write_concurrency_fixture(
        unary_emit_failure_path,
        "demo.emit",
        {
            "function negate(value: UInt32) -> UInt32",
            "    -value",
        }
    );
    auto unary_emit_failure = run_emit_llvm(app, unary_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        unary_emit_failure,
        "lowering does not yet support this return expression: unsupported operator: -"
    );

    auto cast_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm_cast_failure.or";
    write_concurrency_fixture(
        cast_emit_failure_path,
        "demo.emit",
        {
            "function main() -> UInt32",
            "    -1 as UInt32",
        }
    );
    auto cast_emit_failure = run_emit_llvm(app, cast_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        cast_emit_failure,
        "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
    );

    auto final_if_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm_final_if_failure.or";
    write_concurrency_fixture(
        final_if_emit_failure_path,
        "demo.emit",
        {
            "function same(flag: Bool, left: Bool, right: Bool) -> Bool",
            "    if flag",
            "        left < right",
            "    else",
            "        false",
        }
    );
    auto final_if_emit_failure = run_emit_llvm(app, final_if_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_if_emit_failure,
        "lowering does not yet support this final control-flow statement: "
        "if then arm lowering failed: unsupported operator: <"
    );

    auto final_switch_emit_failure_path =
        std::filesystem::temp_directory_path() / "orison_compiler_app_emit_llvm_final_switch_failure.or";
    write_concurrency_fixture(
        final_switch_emit_failure_path,
        "demo.emit",
        {
            "function same(flag: Bool, left: Bool, right: Bool) -> Bool",
            "    switch flag",
            "        true => left < right",
            "        false => false",
        }
    );
    auto final_switch_emit_failure = run_emit_llvm(app, final_switch_emit_failure_path);
    assert_failure_with_no_stdout_contains(
        final_switch_emit_failure,
        "lowering does not yet support this final control-flow statement: "
        "switch case lowering failed: unsupported operator: <"
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
