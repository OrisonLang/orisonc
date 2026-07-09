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

void assert_parse_failure_contains_without(
    orison::driver::CompileResult const& result,
    std::string_view expected_message,
    std::string_view unexpected_message
) {
    assert_parse_failure_contains(result, expected_message);
    assert(result.stderr_text.find(unexpected_message) == std::string_view::npos);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_switch_value_pattern_diagnostics_smoke_" +
         std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto switch_value_pattern_type_failure_path =
        std::filesystem::temp_directory_path() / "switch_value_pattern_type_failure.or";
    write_bool_switch_text_value_pattern_fixture(switch_value_pattern_type_failure_path);
    assert_parse_failure_contains(
        run_parse(app, switch_value_pattern_type_failure_path),
        "switch value pattern type 'Text' does not match switched expression type 'Bool'"
    );

    auto switch_value_pattern_same_width_success_path =
        std::filesystem::temp_directory_path() / "switch_value_pattern_same_width_success.or";
    write_same_width_integer_value_pattern_fixture(switch_value_pattern_same_width_success_path);
    assert_parse_success(run_parse(app, switch_value_pattern_same_width_success_path));

    auto switch_duplicate_boolean_value_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_boolean_value_failure.or";
    write_duplicate_bool_value_pattern_fixture(switch_duplicate_boolean_value_failure_path);
    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_boolean_value_failure_path),
        "switch value pattern 'true' is duplicated"
    );

    auto switch_duplicate_bool_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_bool_without_default_no_cascade_failure.or";
    write_duplicate_bool_value_pattern_fixture(switch_duplicate_bool_without_default_no_cascade_failure_path, false);
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_bool_without_default_no_cascade_failure_path),
        "switch value pattern 'true' is duplicated",
        "switch is missing boolean value pattern"
    );

    auto switch_duplicate_string_value_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_string_value_failure.or";
    write_duplicate_text_value_pattern_fixture(switch_duplicate_string_value_failure_path);
    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_string_value_failure_path),
        "switch value pattern '\"ready\"' is duplicated"
    );

    auto switch_duplicate_integer_cast_value_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_integer_cast_value_failure.or";
    write_duplicate_integer_cast_value_pattern_fixture(switch_duplicate_integer_cast_value_failure_path);
    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_integer_cast_value_failure_path),
        "switch value pattern '1 as Int32' is duplicated"
    );

    auto switch_redundant_bool_default_failure_path =
        std::filesystem::temp_directory_path() / "switch_redundant_bool_default_failure.or";
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
        std::filesystem::temp_directory_path() / "switch_duplicate_bool_redundant_default_no_cascade_failure.or";
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
        std::filesystem::temp_directory_path() / "switch_exhaustive_bool_success.or";
    write_bool_value_pattern_switch_fixture(
        switch_exhaustive_bool_without_default_success_path,
        {"true => 1", "false => 0"}
    );
    assert_parse_success(run_parse(app, switch_exhaustive_bool_without_default_success_path));

    auto switch_missing_bool_pattern_failure_path =
        std::filesystem::temp_directory_path() / "switch_missing_bool_pattern_failure.or";
    write_bool_value_pattern_switch_fixture(switch_missing_bool_pattern_failure_path, {"true => 1"});
    assert_parse_failure_contains(
        run_parse(app, switch_missing_bool_pattern_failure_path),
        "switch is missing boolean value pattern 'false'"
    );

    return 0;
}
