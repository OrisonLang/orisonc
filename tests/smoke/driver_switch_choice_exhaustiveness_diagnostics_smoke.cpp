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

void write_zero_payload_choice_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice IOError\n";
    output << "    Closed\n";
    output << "    EndOfInput\n";
    output << "    PermissionDenied\n";
    output << "function classify(error: IOError) -> Int64\n";
    output << "    switch error\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
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

void write_number_choice_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice Number\n";
    output << "    Int(value: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: Number) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
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
        ("orison_driver_switch_choice_exhaustiveness_diagnostics_smoke_" +
         std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto switch_redundant_choice_default_failure_path =
        std::filesystem::temp_directory_path() / "switch_redundant_choice_default_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_redundant_choice_default_failure_path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"},
        true
    );
    assert_parse_failure_contains(
        run_parse(app, switch_redundant_choice_default_failure_path),
        "switch default case is redundant after all zero-payload choice variants are covered"
    );

    auto switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure_path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3", "Closed => 4"},
        true
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_zero_payload_choice_redundant_default_no_cascade_failure_path),
        "switch constructor pattern 'Closed' is duplicated",
        "switch default case is redundant"
    );

    auto switch_exhaustive_choice_without_default_success_path =
        std::filesystem::temp_directory_path() / "switch_exhaustive_choice_success.or";
    write_zero_payload_choice_switch_fixture(
        switch_exhaustive_choice_without_default_success_path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"}
    );
    assert_parse_success(run_parse(app, switch_exhaustive_choice_without_default_success_path));

    auto switch_redundant_payload_choice_default_failure_path =
        std::filesystem::temp_directory_path() / "switch_redundant_payload_choice_default_failure.or";
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
        std::filesystem::temp_directory_path() / "switch_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_exhaustive_payload_choice_without_default_success_path,
        {"Some(value) => value", "Empty => 0"}
    );
    assert_parse_success(run_parse(app, switch_exhaustive_payload_choice_without_default_success_path));

    auto switch_reversed_exhaustive_payload_choice_without_default_success_path =
        std::filesystem::temp_directory_path() / "switch_reversed_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_reversed_exhaustive_payload_choice_without_default_success_path,
        {"Empty => 0", "Some(value) => value"}
    );
    assert_parse_success(run_parse(app, switch_reversed_exhaustive_payload_choice_without_default_success_path));

    auto switch_literal_payload_choice_default_success_path =
        std::filesystem::temp_directory_path() / "switch_literal_payload_choice_default_success.or";
    write_maybe_int_exhaustiveness_fixture(
        switch_literal_payload_choice_default_success_path,
        {"Some(1) => 1", "Empty => 0"},
        true
    );
    assert_parse_success(run_parse(app, switch_literal_payload_choice_default_success_path));

    auto switch_literal_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() / "switch_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(
        switch_literal_payload_choice_missing_failure_path,
        {"Some(1) => 1", "Empty => 0"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_literal_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Some'"
    );

    auto switch_reversed_literal_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() / "switch_reversed_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(
        switch_reversed_literal_payload_choice_missing_failure_path,
        {"Empty => 0", "Some(1) => 1"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_reversed_literal_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Some'"
    );

    auto switch_nested_payload_choice_default_success_path =
        std::filesystem::temp_directory_path() / "switch_nested_payload_choice_default_success.or";
    write_boxed_maybe_exhaustiveness_fixture(
        switch_nested_payload_choice_default_success_path,
        {"Wrap(Some(value)) => value", "Blank => 0"},
        true
    );
    assert_parse_success(run_parse(app, switch_nested_payload_choice_default_success_path));

    auto switch_nested_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_payload_choice_missing_failure.or";
    write_boxed_maybe_exhaustiveness_fixture(
        switch_nested_payload_choice_missing_failure_path,
        {"Wrap(Some(value)) => value", "Blank => 0"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_nested_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Wrap'"
    );

    auto switch_partial_multi_payload_choice_default_success_path =
        std::filesystem::temp_directory_path() / "switch_partial_multi_payload_choice_default_success.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_partial_multi_payload_choice_default_success_path,
        {"Both(left, 1) => left", "Empty => 0"},
        true
    );
    assert_parse_success(run_parse(app, switch_partial_multi_payload_choice_default_success_path));

    auto switch_partial_multi_payload_choice_missing_failure_path =
        std::filesystem::temp_directory_path() / "switch_partial_multi_payload_choice_missing_failure.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_partial_multi_payload_choice_missing_failure_path,
        {"Both(left, 1) => left", "Empty => 0"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_partial_multi_payload_choice_missing_failure_path),
        "switch is missing choice variant 'Both'"
    );

    auto switch_missing_payload_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "switch_missing_payload_choice_variant_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_missing_payload_choice_variant_failure_path,
        {"Some(value) => value"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_missing_payload_choice_variant_failure_path),
        "switch is missing choice variant 'Empty'"
    );

    auto switch_exhaustive_multi_payload_choice_without_default_success_path =
        std::filesystem::temp_directory_path() / "switch_exhaustive_multi_payload_choice_success.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_exhaustive_multi_payload_choice_without_default_success_path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"}
    );
    assert_parse_success(run_parse(app, switch_exhaustive_multi_payload_choice_without_default_success_path));

    auto switch_redundant_multi_payload_choice_default_failure_path =
        std::filesystem::temp_directory_path() / "switch_redundant_multi_payload_choice_default_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_redundant_multi_payload_choice_default_failure_path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"},
        true
    );
    assert_parse_failure_contains(
        run_parse(app, switch_redundant_multi_payload_choice_default_failure_path),
        "switch default case is redundant after all choice variants are covered"
    );

    auto switch_duplicate_payload_choice_redundant_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() /
        "switch_duplicate_payload_choice_redundant_default_no_cascade_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_redundant_default_no_cascade_failure_path,
        {"First(value) => value", "Second(value) => value", "Empty => 0", "First(other) => other"},
        true
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_payload_choice_redundant_default_no_cascade_failure_path),
        "switch constructor pattern 'First(...)' is duplicated",
        "switch default case is redundant"
    );

    auto switch_first_missing_multi_payload_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "switch_first_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_first_missing_multi_payload_choice_variant_failure_path,
        {"Second(value) => value", "Empty => 0"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_first_missing_multi_payload_choice_variant_failure_path),
        "switch is missing choice variant 'First'"
    );

    auto switch_second_missing_multi_payload_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "switch_second_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_second_missing_multi_payload_choice_variant_failure_path,
        {"First(value) => value", "Empty => 0"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_second_missing_multi_payload_choice_variant_failure_path),
        "switch is missing choice variant 'Second'"
    );

    auto switch_duplicate_multi_payload_choice_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_multi_payload_choice_no_cascade_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        switch_duplicate_multi_payload_choice_no_cascade_failure_path,
        {"First(value) => value", "First(other) => other"}
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_multi_payload_choice_no_cascade_failure_path),
        "switch constructor pattern 'First(...)' is duplicated",
        "switch is missing choice variant"
    );

    auto switch_duplicate_payload_choice_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_payload_choice_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_without_default_no_cascade_failure_path,
        {"Some(value) => value", "Some(other) => other"}
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_payload_choice_without_default_no_cascade_failure_path),
        "switch constructor pattern 'Some(...)' is duplicated",
        "switch is missing choice variant"
    );

    auto switch_duplicate_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_choice_constructor_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_duplicate_choice_constructor_failure_path,
        {"Closed => 1", "EndOfInput => 2", "Closed => 3"}
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_choice_constructor_failure_path),
        "switch constructor pattern 'Closed' is duplicated",
        "switch is missing zero-payload choice variant"
    );

    auto switch_duplicate_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_payload_choice_constructor_failure.or";
    write_maybe_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_constructor_failure_path,
        {"Some(value) => 1", "Some(other) => 2"},
        true
    );
    assert_parse_failure_contains(
        run_parse(app, switch_duplicate_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Some(...)' is duplicated"
    );

    auto switch_duplicate_payload_choice_constructor_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_payload_choice_constructor_no_cascade_failure.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_duplicate_payload_choice_constructor_no_cascade_failure_path,
        {"Both(left, right) => 1", "Both(value, value) => 2"},
        true
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_payload_choice_constructor_no_cascade_failure_path),
        "switch constructor pattern 'Both(...)' is duplicated",
        "cannot bind 'value' more than once"
    );

    auto switch_duplicate_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_duplicate_literal_payload_choice_constructor_failure_path,
        {"Int(1) => 1", "Int(1) => 2"}
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_duplicate_literal_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated",
        "switch value pattern"
    );

    auto switch_equivalent_integer_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() /
        "switch_equivalent_integer_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_equivalent_integer_literal_payload_choice_constructor_failure_path,
        {"Int(1) => 1", "Int(1 as Int64) => 2"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_equivalent_integer_literal_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated"
    );

    auto switch_wildcard_then_literal_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "switch_wildcard_then_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_wildcard_then_literal_payload_choice_constructor_failure_path,
        {"Int(value) => 1", "Int(1) => 2"}
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_wildcard_then_literal_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated",
        "switch value pattern"
    );

    auto switch_literal_then_wildcard_payload_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "switch_literal_then_wildcard_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(
        switch_literal_then_wildcard_payload_choice_constructor_failure_path,
        {"Int(1) => 1", "Int(value) => 2"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_literal_then_wildcard_payload_choice_constructor_failure_path),
        "switch constructor pattern 'Int(...)' is duplicated"
    );

    auto switch_multi_payload_partial_overlap_choice_constructor_failure_path =
        std::filesystem::temp_directory_path() / "switch_multi_payload_partial_overlap_choice_constructor_failure.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_multi_payload_partial_overlap_choice_constructor_failure_path,
        {"Both(left, 1) => 1", "Both(other, 1) => 2"},
        true
    );
    assert_parse_failure_contains(
        run_parse(app, switch_multi_payload_partial_overlap_choice_constructor_failure_path),
        "switch constructor pattern 'Both(...)' is duplicated"
    );

    auto switch_multi_payload_disjoint_literal_choice_constructor_success_path =
        std::filesystem::temp_directory_path() / "switch_multi_payload_disjoint_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_multi_payload_disjoint_literal_choice_constructor_success_path,
        {"Both(left, 1) => 1", "Both(other, 2) => 2"},
        true
    );
    assert_parse_success(run_parse(app, switch_multi_payload_disjoint_literal_choice_constructor_success_path));

    auto switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path =
        std::filesystem::temp_directory_path() /
        "switch_multi_payload_disjoint_leading_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(
        switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path,
        {"Both(1, left) => 1", "Both(2, right) => 2"},
        true
    );
    assert_parse_success(run_parse(app, switch_multi_payload_disjoint_leading_literal_choice_constructor_success_path));

    auto switch_missing_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "switch_missing_choice_variant_failure.or";
    write_zero_payload_choice_switch_fixture(
        switch_missing_choice_variant_failure_path,
        {"Closed => 1", "EndOfInput => 2"}
    );
    assert_parse_failure_contains(
        run_parse(app, switch_missing_choice_variant_failure_path),
        "switch is missing zero-payload choice variant 'PermissionDenied'"
    );

    return 0;
}
