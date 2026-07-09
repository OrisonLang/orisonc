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

void write_maybe_choice_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "function classify(item: Maybe<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
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
        ("orison_driver_switch_pattern_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto switch_name_pattern_binding_failure_path =
        std::filesystem::temp_directory_path() / "switch_name_pattern_binding_failure.or";
    write_fixture(
        switch_name_pattern_binding_failure_path,
        "demo.patterns",
        {
            "async function read(value: Int64) -> Int64",
            "    var head = 0",
            "    switch value",
            "        head =>",
            "            let request_task = task",
            "                head",
            "            return await request_task",
            "        default => 0",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, switch_name_pattern_binding_failure_path),
        "switch constructor pattern 'head' does not match any declared choice variant"
    );

    auto switch_call_pattern_unknown_variant_failure_path =
        std::filesystem::temp_directory_path() / "switch_call_pattern_unknown_variant_failure.or";
    write_fixture(
        switch_call_pattern_unknown_variant_failure_path,
        "demo.patterns",
        {
            "async function read(value: Int64) -> Int64",
            "    switch value",
            "        Missing(head) => 0",
            "        default => 0",
        }
    );
    assert_parse_failure_contains(
        run_parse(app, switch_call_pattern_unknown_variant_failure_path),
        "switch constructor pattern 'Missing' does not match any declared choice variant"
    );

    auto switch_unknown_constructor_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_unknown_constructor_without_default_no_cascade_failure.or";
    write_maybe_unknown_constructor_fixture(switch_unknown_constructor_without_default_no_cascade_failure_path);
    assert_parse_failure_contains_without(
        run_parse(app, switch_unknown_constructor_without_default_no_cascade_failure_path),
        "switch constructor pattern 'Missing' does not match any declared choice variant",
        "switch is missing"
    );

    auto switch_nested_constructor_pattern_shape_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_constructor_pattern_shape_failure.or";
    write_list_switch_fixture(switch_nested_constructor_pattern_shape_failure_path, {"Node(head + 1, tail) => 0"}, true);
    assert_parse_failure_contains(
        run_parse(app, switch_nested_constructor_pattern_shape_failure_path),
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
    );

    auto switch_constructor_payload_shape_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_constructor_payload_shape_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_constructor_payload_shape_without_default_no_cascade_failure_path,
        {"Node(head + 1, tail) => 0"},
        false,
        false
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_constructor_payload_shape_without_default_no_cascade_failure_path),
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern",
        "switch is missing"
    );

    auto switch_constructor_pattern_duplicate_binding_failure_path =
        std::filesystem::temp_directory_path() / "switch_constructor_pattern_duplicate_binding_failure.or";
    write_list_switch_fixture(switch_constructor_pattern_duplicate_binding_failure_path, {"Node(head, head) => 0"}, true);
    assert_parse_failure_contains(
        run_parse(app, switch_constructor_pattern_duplicate_binding_failure_path),
        "switch constructor pattern cannot bind 'head' more than once"
    );

    auto switch_constructor_duplicate_binding_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "switch_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_constructor_duplicate_binding_without_default_no_cascade_failure_path,
        {"Node(head, head) => 0"},
        false,
        false
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_constructor_duplicate_binding_without_default_no_cascade_failure_path),
        "switch constructor pattern cannot bind 'head' more than once",
        "switch is missing"
    );

    auto switch_nested_constructor_pattern_duplicate_binding_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_constructor_pattern_duplicate_binding_failure.or";
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
        "switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path,
        {"Node(head, Node(head, tail)) => 0"},
        false,
        false
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure_path),
        "switch constructor pattern cannot bind 'head' more than once",
        "switch is missing"
    );

    auto switch_constructor_pattern_arity_missing_failure_path =
        std::filesystem::temp_directory_path() / "switch_constructor_pattern_arity_missing_failure.or";
    write_list_switch_fixture(switch_constructor_pattern_arity_missing_failure_path, {"Node(head) => 0"}, true);
    assert_parse_failure_contains(
        run_parse(app, switch_constructor_pattern_arity_missing_failure_path),
        "switch constructor pattern 'Node' expects 2 payload values but received 1"
    );

    auto switch_constructor_pattern_arity_extra_failure_path =
        std::filesystem::temp_directory_path() / "switch_constructor_pattern_arity_extra_failure.or";
    write_list_switch_fixture(switch_constructor_pattern_arity_extra_failure_path, {"Empty(value) => 0"}, true);
    assert_parse_failure_contains(
        run_parse(app, switch_constructor_pattern_arity_extra_failure_path),
        "switch constructor pattern 'Empty' expects 0 payload values but received 1"
    );

    auto switch_constructor_pattern_arity_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_constructor_pattern_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_constructor_pattern_arity_without_default_no_cascade_failure_path,
        {"Node(head) => 0"},
        false,
        false
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_constructor_pattern_arity_without_default_no_cascade_failure_path),
        "switch constructor pattern 'Node' expects 2 payload values but received 1",
        "switch is missing"
    );

    auto switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "switch_zero_payload_constructor_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(
        switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path,
        {"Empty(value) => 0"},
        false,
        false
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_zero_payload_constructor_arity_without_default_no_cascade_failure_path),
        "switch constructor pattern 'Empty' expects 0 payload values but received 1",
        "switch is missing"
    );

    auto switch_pattern_mix_constructor_value_failure_path =
        std::filesystem::temp_directory_path() / "switch_pattern_mix_constructor_value_failure.or";
    write_list_switch_fixture(switch_pattern_mix_constructor_value_failure_path, {"Empty => 0", "1 => 1"});
    assert_parse_failure_contains(
        run_parse(app, switch_pattern_mix_constructor_value_failure_path),
        "switch cannot mix value patterns with constructor patterns"
    );

    auto switch_pattern_mix_value_constructor_failure_path =
        std::filesystem::temp_directory_path() / "switch_pattern_mix_value_constructor_failure.or";
    write_value_then_constructor_pattern_mix_fixture(switch_pattern_mix_value_constructor_failure_path);
    assert_parse_failure_contains(
        run_parse(app, switch_pattern_mix_value_constructor_failure_path),
        "switch cannot mix value patterns with constructor patterns"
    );

    auto switch_pattern_mix_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_pattern_mix_without_default_no_cascade_failure.or";
    write_maybe_choice_fixture(
        switch_pattern_mix_without_default_no_cascade_failure_path,
        {"Some(value) => value", "1 => 1"}
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_pattern_mix_without_default_no_cascade_failure_path),
        "switch cannot mix value patterns with constructor patterns",
        "switch is missing choice variant"
    );

    return 0;
}
