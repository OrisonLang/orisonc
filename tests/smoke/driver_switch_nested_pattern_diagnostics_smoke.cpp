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

void write_boxed_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: Maybe<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_boxed_pair_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice PairMaybe<T>\n";
    output << "    PairSome(left: T, right: Int64)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: PairMaybe<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    output << "        default => 0\n";
}

void write_boxed_outer_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Outer<T>\n";
    output << "    Hold(inner: Maybe<T>)\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: Outer<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    output << "        default => 0\n";
}

void write_result_switch_with_maybe_variant_fixture(
    std::filesystem::path const& path,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "choice Result<T>\n";
    output << "    Ok(value: T)\n";
    output << "    Error\n";
    output << "function read(result: Result<Int64>) -> Int64\n";
    output << "    switch result\n";
    output << "        Some(value) => value\n";
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_envelope_result_switch_with_maybe_variant_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "choice Result<T>\n";
    output << "    Ok(value: T)\n";
    output << "    Error\n";
    output << "choice Envelope<T>\n";
    output << "    Wrap(inner: Result<T>)\n";
    output << "function read(env: Envelope<Int64>) -> Int64\n";
    output << "    switch env\n";
    output << "        Wrap(Some(value)) => value\n";
    output << "        default => 0\n";
}

void write_flag_switch_with_maybe_same_name_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "choice Flag\n";
    output << "    Some\n";
    output << "function read(flag: Flag) -> Int64\n";
    output << "    switch flag\n";
    output << "        Some => 1\n";
}

void write_envelope_pair_flag_switch_with_maybe_same_name_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "choice PairFlag\n";
    output << "    Some(left: Int64, right: Int64)\n";
    output << "choice Envelope\n";
    output << "    Wrap(inner: PairFlag)\n";
    output << "function read(env: Envelope) -> Int64\n";
    output << "    switch env\n";
    output << "        Wrap(Some(left, right)) => left\n";
    output << "        default => 0\n";
}

void write_maybe_raw_write_fixture(std::filesystem::path const& path, std::string_view maybe_payload_type) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "unsafe function write_word(maybe: Maybe<" << maybe_payload_type;
    output << ">, out: Pointer<UInt32>) -> Unit\n";
    output << "    switch maybe\n";
    output << "        Some(value) => raw_write(out, value)\n";
    output << "        default => return\n";
}

void write_nested_list_raw_write_fixture(std::filesystem::path const& path, std::string_view list_payload_type) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << "unsafe function write_next(xs: List<" << list_payload_type;
    output << ">, out: Pointer<UInt32>) -> Unit\n";
    output << "    switch xs\n";
    output << "        Node(head, Node(next, tail)) => raw_write(out, next)\n";
    output << "        default => return\n";
}

void write_nested_list_async_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << "async function sum(xs: List<Int64>) -> Int64\n";
    output << "    switch xs\n";
    output << "        Node(head, Node(next, tail)) =>\n";
    output << "            let request_task = task\n";
    output << "                next\n";
    output << "            return await request_task\n";
    output << "        default => 0\n";
}

void write_list_async_head_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << "async function sum(xs: List<Int64>) -> Int64\n";
    output << "    var head = 0\n";
    output << "    switch xs\n";
    output << "        Empty => 0\n";
    output << "        Node(head, tail) =>\n";
    output << "            let request_task = task\n";
    output << "                head\n";
    output << "            return await request_task\n";
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

void assert_wrap_duplicate_parse_failure(orison::driver::CompileResult const& result) {
    assert_parse_failure_contains(result, "switch constructor pattern 'Wrap(...)' is duplicated");
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_switch_nested_pattern_diagnostics_smoke_" +
         std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto switch_constructor_pattern_binding_path =
        std::filesystem::temp_directory_path() / "switch_constructor_pattern_binding_success.or";
    write_list_async_head_capture_fixture(switch_constructor_pattern_binding_path);
    assert_parse_success(run_parse(app, switch_constructor_pattern_binding_path));

    auto switch_nested_constructor_pattern_binding_path =
        std::filesystem::temp_directory_path() / "switch_nested_constructor_pattern_success.or";
    write_nested_list_async_capture_fixture(switch_nested_constructor_pattern_binding_path);
    assert_parse_success(run_parse(app, switch_nested_constructor_pattern_binding_path));

    auto switch_nested_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_payload_overlap_failure_path,
        {"Wrap(Some(value)) => 1", "Wrap(Some(other)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_nested_payload_overlap_failure_path));

    auto switch_nested_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_literal_payload_overlap_failure_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(1)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_nested_literal_payload_overlap_failure_path));

    auto switch_disjoint_nested_literal_payload_success_path =
        std::filesystem::temp_directory_path() / "switch_disjoint_nested_literal_payload_success.or";
    write_boxed_maybe_switch_fixture(
        switch_disjoint_nested_literal_payload_success_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(2)) => 2"}
    );
    assert_parse_success(run_parse(app, switch_disjoint_nested_literal_payload_success_path));

    auto switch_nested_wildcard_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_wildcard_literal_payload_overlap_failure_path,
        {"Wrap(Some(value)) => 1", "Wrap(Some(1)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_nested_wildcard_literal_payload_overlap_failure_path)
    );

    auto switch_nested_literal_wildcard_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_nested_literal_wildcard_payload_overlap_failure_path,
        {"Wrap(Some(1)) => 1", "Wrap(Some(value)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_nested_literal_wildcard_payload_overlap_failure_path)
    );

    auto switch_nested_multi_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_multi_payload_overlap_failure.or";
    write_boxed_pair_maybe_switch_fixture(
        switch_nested_multi_payload_overlap_failure_path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 1)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_nested_multi_payload_overlap_failure_path));

    auto switch_disjoint_nested_multi_payload_success_path =
        std::filesystem::temp_directory_path() / "switch_disjoint_nested_multi_payload_success.or";
    write_boxed_pair_maybe_switch_fixture(
        switch_disjoint_nested_multi_payload_success_path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 2)) => 2"}
    );
    assert_parse_success(run_parse(app, switch_disjoint_nested_multi_payload_success_path));

    auto switch_mismatched_nested_constructor_success_path =
        std::filesystem::temp_directory_path() / "switch_mismatched_nested_constructor_success.or";
    write_boxed_maybe_switch_fixture(
        switch_mismatched_nested_constructor_success_path,
        {"Wrap(Some(value)) => 1", "Wrap(Empty) => 2"}
    );
    assert_parse_success(run_parse(app, switch_mismatched_nested_constructor_success_path));

    auto switch_duplicate_nested_zero_payload_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_nested_zero_payload_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_duplicate_nested_zero_payload_failure_path,
        {"Wrap(Empty) => 1", "Wrap(Empty) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_duplicate_nested_zero_payload_failure_path));

    auto switch_duplicate_nested_zero_payload_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_nested_zero_payload_no_cascade_failure.or";
    write_boxed_maybe_switch_fixture(
        switch_duplicate_nested_zero_payload_no_cascade_failure_path,
        {"Wrap(Empty) => 1", "Wrap(Empty) => 2"},
        false
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_duplicate_nested_zero_payload_no_cascade_failure_path)
    );

    auto switch_deep_nested_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_deep_nested_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_payload_overlap_failure_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(other))) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_deep_nested_payload_overlap_failure_path));

    auto switch_disjoint_deep_nested_literal_payload_success_path =
        std::filesystem::temp_directory_path() / "switch_disjoint_deep_nested_literal_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_disjoint_deep_nested_literal_payload_success_path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(2))) => 2"}
    );
    assert_parse_success(run_parse(app, switch_disjoint_deep_nested_literal_payload_success_path));

    auto switch_deep_nested_wildcard_literal_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_deep_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_wildcard_literal_payload_overlap_failure_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(1))) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_deep_nested_wildcard_literal_payload_overlap_failure_path)
    );

    auto switch_deep_nested_literal_wildcard_payload_overlap_failure_path =
        std::filesystem::temp_directory_path() / "switch_deep_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_deep_nested_literal_wildcard_payload_overlap_failure_path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(value))) => 2"}
    );
    assert_wrap_duplicate_parse_failure(
        run_parse(app, switch_deep_nested_literal_wildcard_payload_overlap_failure_path)
    );

    auto switch_mismatched_deep_nested_zero_payload_success_path =
        std::filesystem::temp_directory_path() / "switch_mismatched_deep_nested_zero_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_mismatched_deep_nested_zero_payload_success_path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Empty)) => 2"}
    );
    assert_parse_success(run_parse(app, switch_mismatched_deep_nested_zero_payload_success_path));

    auto switch_duplicate_deep_nested_zero_payload_failure_path =
        std::filesystem::temp_directory_path() / "switch_duplicate_deep_nested_zero_payload_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        switch_duplicate_deep_nested_zero_payload_failure_path,
        {"Wrap(Hold(Empty)) => 1", "Wrap(Hold(Empty)) => 2"}
    );
    assert_wrap_duplicate_parse_failure(run_parse(app, switch_duplicate_deep_nested_zero_payload_failure_path));

    auto switch_nested_wrapped_payload_success_path =
        std::filesystem::temp_directory_path() / "switch_nested_wrapped_payload_success.or";
    write_nested_list_raw_write_fixture(switch_nested_wrapped_payload_success_path, "Int32");
    assert_parse_success(run_parse(app, switch_nested_wrapped_payload_success_path));

    auto switch_nested_wrapped_payload_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_wrapped_payload_failure.or";
    write_nested_list_raw_write_fixture(switch_nested_wrapped_payload_failure_path, "Byte");
    assert_parse_failure_contains(
        run_parse(app, switch_nested_wrapped_payload_failure_path),
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto switch_generic_constructor_payload_success_path =
        std::filesystem::temp_directory_path() / "switch_generic_constructor_payload_success.or";
    write_maybe_raw_write_fixture(switch_generic_constructor_payload_success_path, "Int32");
    assert_parse_success(run_parse(app, switch_generic_constructor_payload_success_path));

    auto switch_generic_constructor_payload_failure_path =
        std::filesystem::temp_directory_path() / "switch_generic_constructor_payload_failure.or";
    write_maybe_raw_write_fixture(switch_generic_constructor_payload_failure_path, "Byte");
    assert_parse_failure_contains(
        run_parse(app, switch_generic_constructor_payload_failure_path),
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );

    auto switch_wrong_choice_variant_failure_path =
        std::filesystem::temp_directory_path() / "switch_wrong_choice_variant_failure.or";
    write_result_switch_with_maybe_variant_fixture(switch_wrong_choice_variant_failure_path);
    assert_parse_failure_contains(
        run_parse(app, switch_wrong_choice_variant_failure_path),
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );

    auto switch_wrong_choice_without_default_no_cascade_failure_path =
        std::filesystem::temp_directory_path() / "switch_wrong_choice_without_default_no_cascade_failure.or";
    write_result_switch_with_maybe_variant_fixture(
        switch_wrong_choice_without_default_no_cascade_failure_path,
        false
    );
    assert_parse_failure_contains_without(
        run_parse(app, switch_wrong_choice_without_default_no_cascade_failure_path),
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'",
        "switch is missing"
    );

    auto switch_subject_specific_arity_success_path =
        std::filesystem::temp_directory_path() / "switch_subject_specific_arity_success.or";
    write_flag_switch_with_maybe_same_name_fixture(switch_subject_specific_arity_success_path);
    assert_parse_success(run_parse(app, switch_subject_specific_arity_success_path));

    auto switch_nested_wrong_payload_choice_failure_path =
        std::filesystem::temp_directory_path() / "switch_nested_wrong_payload_choice_failure.or";
    write_envelope_result_switch_with_maybe_variant_fixture(switch_nested_wrong_payload_choice_failure_path);
    assert_parse_failure_contains(
        run_parse(app, switch_nested_wrong_payload_choice_failure_path),
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );

    auto switch_nested_payload_specific_arity_success_path =
        std::filesystem::temp_directory_path() / "switch_nested_payload_specific_arity_success.or";
    write_envelope_pair_flag_switch_with_maybe_same_name_fixture(
        switch_nested_payload_specific_arity_success_path
    );
    assert_parse_success(run_parse(app, switch_nested_payload_specific_arity_success_path));

    return 0;
}
