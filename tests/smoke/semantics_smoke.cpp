#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string_view>

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

void write_switch_name_pattern_async_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "async function read(value: Int64) -> Int64\n";
    output << "    var head = 0\n";
    output << "    switch value\n";
    output << "        head =>\n";
    output << "            let request_task = task\n";
    output << "                head\n";
    output << "            return await request_task\n";
    output << "        default => 0\n";
}

void write_switch_unknown_call_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "async function read(value: Int64) -> Int64\n";
    output << "    switch value\n";
    output << "        Missing(head) => 0\n";
    output << "        default => 0\n";
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

void write_loop_control_fixture(
    std::filesystem::path const& path,
    std::string_view function_header,
    std::initializer_list<std::string_view> body_lines
) {
    std::ofstream output(path);
    output << "package demo.loops\n";
    output << "function " << function_header << "\n";
    for (auto line : body_lines) {
        output << "    " << line << "\n";
    }
}

void write_receiver_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package demo.receiver\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

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

auto analyze_orison_fixture(std::filesystem::path const& path) -> orison::semantics::SemanticAnalysisResult {
    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    return analyzer.analyze(parse_result.module);
}

template <typename MutateSwitch>
auto analyze_mutated_first_switch_fixture(
    std::filesystem::path const& path,
    MutateSwitch mutate_switch
) -> orison::semantics::SemanticAnalysisResult {
    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    auto& switch_statement = parse_result.module.functions.front().body_statements.front();
    assert(switch_statement.kind == orison::syntax::StatementKind::switch_statement);
    mutate_switch(switch_statement);

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    return analyzer.analyze(parse_result.module);
}

void assert_wrap_duplicate_diagnostic(
    orison::semantics::SemanticAnalysisResult const& diagnostics,
    std::size_t expected_line = 10
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == expected_line);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Wrap(...)' is duplicated");
}

void assert_single_diagnostic(
    orison::semantics::SemanticAnalysisResult const& diagnostics,
    std::size_t expected_line,
    std::string_view expected_message
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == expected_line);
    assert(diagnostics.entries().front().message == expected_message);
}

void assert_nonfinal_switch_default_diagnostic(orison::semantics::SemanticAnalysisResult const& diagnostics) {
    assert_single_diagnostic(diagnostics, 4, "switch default case must be the final case");
}

void assert_multiple_switch_default_diagnostics(orison::semantics::SemanticAnalysisResult const& diagnostics) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message == "switch default case must be the final case");
    assert(diagnostics.entries().back().line == 5);
    assert(diagnostics.entries().back().message == "switch statement may only contain one default case");
}

void assert_this_type_context_diagnostics(
    orison::semantics::SemanticAnalysisResult const& diagnostics,
    std::size_t expected_line,
    std::size_t expected_count
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == expected_count);
    for (auto const& diagnostic : diagnostics.entries()) {
        assert(diagnostic.line == expected_line);
        assert(diagnostic.message == "This type is only valid inside interface, implements, or extend methods");
    }
}

void assert_fixture_single_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view expected_message
) {
    assert_single_diagnostic(analyze_orison_fixture(path), expected_line, expected_message);
}

void assert_fixture_success(std::filesystem::path const& path) {
    assert(!analyze_orison_fixture(path).has_errors());
}

void assert_fixture_single_capture(std::filesystem::path const& path, std::string_view expected_name) {
    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
    assert(diagnostics.concurrency_captures.size() == 1);
    assert(diagnostics.concurrency_captures.front().name == expected_name);
}

void assert_fixture_single_capture_kind(
    std::filesystem::path const& path,
    std::string_view expected_name,
    orison::semantics::ConcurrencyCaptureKind expected_kind
) {
    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
    assert(diagnostics.concurrency_captures.size() == 1);
    assert(diagnostics.concurrency_captures.front().name == expected_name);
    assert(diagnostics.concurrency_captures.front().capture_kind == expected_kind);
}

void assert_thread_result_transferable_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type_name
) {
    std::string const message =
        "thread result type '" + std::string(result_type_name) + "' requires future Transferable analysis";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_concurrency_value_boundary_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view expression_kind
) {
    std::string const message =
        std::string(expression_kind) + " expression body must end with an expression statement or value return";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_mutable_capture_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view local_name
) {
    std::string const message =
        "concurrency expression cannot capture mutable outer local '" + std::string(local_name) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_receiver_capture_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, "concurrency expression cannot capture receiver 'this'");
}

void assert_fixture_this_type_context_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_this_type_context_diagnostics(analyze_orison_fixture(path), expected_line, 1);
}

void assert_pointer_construction_unsafe_boundary_diagnostics(
    orison::semantics::SemanticAnalysisResult const& diagnostics
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "Pointer construction is only valid inside unsafe functions or unsafe blocks");
    assert(diagnostics.entries().back().line == 4);
    assert(diagnostics.entries().back().message ==
           "unsafe intrinsic 'raw_read' is only valid inside unsafe functions or unsafe blocks");
}

void assert_pointer_typed_binding_initializer_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "pointer-typed binding initializer currently requires a structurally pointer-like expression"
    );
}

void assert_pointer_typed_binding_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    std::string const message = "pointer-typed binding initializer pointer element type '" +
                                std::string(actual_type) +
                                "' does not match expected pointer element type '" +
                                std::string(expected_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_pointer_return_structural_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "pointer-returning function currently requires a structurally pointer-like expression"
    );
}

void assert_raw_offset_source_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    std::string const message = "raw_offset source pointer element type '" +
                                std::string(actual_type) +
                                "' does not match expected pointer element type '" +
                                std::string(expected_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_pointer_return_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    std::string const message = "pointer-returning function pointer element type '" +
                                std::string(actual_type) +
                                "' does not match expected pointer element type '" +
                                std::string(expected_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_raw_write_value_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view value_type,
    std::string_view pointer_element_type
) {
    std::string const message = "raw_write value type '" +
                                std::string(value_type) +
                                "' does not match pointer element type '" +
                                std::string(pointer_element_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_pointer_construction_source_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view source_type,
    std::string_view expected_type
) {
    std::string const message = "Pointer construction source type '" +
                                std::string(source_type) +
                                "' does not match expected pointer element type '" +
                                std::string(expected_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_raw_read_result_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type,
    std::string_view expected_type
) {
    std::string const message = "raw_read result type '" +
                                std::string(result_type) +
                                "' does not match function return type '" +
                                std::string(expected_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_volatile_read_result_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type,
    std::string_view expected_type
) {
    std::string const message = "volatile_read result type '" +
                                std::string(result_type) +
                                "' does not match function return type '" +
                                std::string(expected_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_volatile_read_binding_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type,
    std::string_view expected_type
) {
    std::string const message = "volatile_read result type '" +
                                std::string(result_type) +
                                "' does not match binding type '" +
                                std::string(expected_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_volatile_write_value_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view value_type,
    std::string_view pointer_element_type
) {
    std::string const message = "volatile_write value type '" +
                                std::string(value_type) +
                                "' does not match pointer element type '" +
                                std::string(pointer_element_type) + "'";
    assert_fixture_single_diagnostic(path, expected_line, message);
}

void assert_address_binding_initializer_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "address-typed binding initializer currently requires a structurally address-like expression"
    );
}

void assert_address_return_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "address-returning function currently requires a structurally address-like expression"
    );
}

void assert_task_inside_async_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, "task expression is only valid inside async functions");
}

void assert_join_task_receiver_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, "join() cannot be used with task values; use await instead");
}

void assert_join_async_call_receiver_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "join() cannot be used with declared async call results; use await instead"
    );
}

void assert_thread_return_forward_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, "return cannot forward thread values; use .join() instead");
}

void assert_concurrency_capture(
    orison::semantics::SemanticAnalysisResult const& analysis,
    std::size_t index,
    std::string_view expected_name,
    std::string_view expected_type_name,
    orison::semantics::ConcurrencyExpressionKind expected_expression_kind,
    orison::semantics::ConcurrencyCaptureKind expected_capture_kind
) {
    assert(analysis.concurrency_captures.size() > index);
    auto const& capture = analysis.concurrency_captures[index];
    assert(capture.name == expected_name);
    assert(capture.type_name == expected_type_name);
    assert(capture.expression_kind == expected_expression_kind);
    assert(capture.capture_kind == expected_capture_kind);
}

void test_await_inside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_async_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return await request_task",
        }
    );

    assert_fixture_success(path);
}

void test_await_async_call_value_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_async_call_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_await_outside_async_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_sync_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "function fetch(url: Text) -> Outcome<Text, IOError>",
            "    return await request(url)",
        }
    );

    assert_fixture_single_diagnostic(path, 5, "await expression is only valid inside async functions");
}

void test_await_outside_async_method_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_method_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(id: Int64) -> Outcome<Text, IOError>",
            "    return fetch_remote(id)",
            "extend Worker",
            "    function poll(this: shared This) -> Outcome<Text, IOError>",
            "        return await request(this.id)",
        }
    );

    assert_fixture_single_diagnostic(path, 6, "await expression is only valid inside async functions");
}

void test_await_plain_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_plain_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch() -> Int64",
            "    let count = 1",
            "    return await count",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        4,
        "await expression currently requires a task value or declared async call result"
    );
}

void test_await_thread_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_thread_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch() -> Int64",
            "    let worker = thread",
            "        1",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 5, "await cannot be used with thread values; use .join() instead");
}

void test_await_non_async_call_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_non_async_call_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        6,
        "await expression currently requires a task value or declared async call result"
    );
}

void test_await_member_call_not_marked_async_from_top_level_name_collision_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_await_member_name_collision_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function run(text: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(text)",
            "extend Printer",
            "    function run(this: shared This) -> Outcome<Text, IOError>",
            "        return render(this)",
            "async function fetch(printer: Printer) -> Outcome<Text, IOError>",
            "    let pending = printer.run()",
            "    return await pending",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        9,
        "await expression currently requires a task value or declared async call result"
    );
}

void test_task_inside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_async_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return await request_task",
        }
    );

    assert_fixture_success(path);
}

void test_return_task_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_task_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return request_task",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        5,
        "return cannot forward task or async-call values; use await instead"
    );
}

void test_return_async_call_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_async_call_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return pending",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        6,
        "return cannot forward task or async-call values; use await instead"
    );
}

void test_return_thread_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_thread_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker",
        }
    );

    assert_fixture_single_diagnostic(path, 5, "return cannot forward thread values; use .join() instead");
}

void test_assignment_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_assignment_preserves_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_assignment_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_assignment_preserves_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "async function fetch() -> Int64",
            "    var worker = thread",
            "        1",
            "    worker = thread",
            "        2",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 7, "await cannot be used with thread values; use .join() instead");
}

void test_ternary_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_ternary_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    let left = request(url)",
            "    let right = request(url)",
            "    let pending = flag ? left : right",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_ternary_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_ternary_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    let left = thread",
            "        1",
            "    let right = thread",
            "        2",
            "    let worker = flag ? left : right",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 8, "await cannot be used with thread values; use .join() instead");
}

void test_return_ternary_async_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_ternary_async_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    let left = request(url)",
            "    let right = request(url)",
            "    return flag ? left : right",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        7,
        "return cannot forward task or async-call values; use await instead"
    );
}

void test_if_branch_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_if_branch_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    if flag",
            "        pending = request(url)",
            "    else",
            "        pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_if_branch_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_if_branch_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = thread",
            "        1",
            "    if flag",
            "        worker = thread",
            "            2",
            "    else",
            "        worker = thread",
            "            3",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 11, "await cannot be used with thread values; use .join() instead");
}

void test_switch_branch_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_branch_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    switch flag",
            "        true => pending = request(url)",
            "        false => pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_switch_branch_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_branch_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = thread",
            "        1",
            "    switch flag",
            "        true => worker = thread",
            "            2",
            "        false => worker = thread",
            "            3",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 10, "await cannot be used with thread values; use .join() instead");
}

void test_while_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_while_loop_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    while flag",
            "        pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_while_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_while_loop_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = thread",
            "        1",
            "    while flag",
            "        worker = thread",
            "            2",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 8, "await cannot be used with thread values; use .join() instead");
}

void test_repeat_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_repeat_loop_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = 0",
            "    repeat",
            "        pending = request(url)",
            "    while flag",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_repeat_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_repeat_loop_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = 0",
            "    repeat",
            "        worker = thread",
            "            2",
            "    while flag",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 8, "await cannot be used with thread values; use .join() instead");
}

void test_for_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_for_loop_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(items: shared View<Int64>, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    for item in items",
            "        pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_for_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_for_loop_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(items: shared View<Int64>) -> Int64",
            "    var worker = thread",
            "        1",
            "    for item in items",
            "        worker = thread",
            "            2",
            "    return await worker",
        }
    );

    assert_fixture_single_diagnostic(path, 8, "await cannot be used with thread values; use .join() instead");
}

void test_guard_failure_path_does_not_override_async_origin_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_guard_failure_path_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    guard flag else",
            "        pending = thread",
            "            2",
            "        return await request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_guard_failure_path_does_not_create_async_origin_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_guard_failure_path_async_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = 0",
            "    guard flag else",
            "        pending = request(url)",
            "        return await request(url)",
            "    return await pending",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        9,
        "await expression currently requires a task value or declared async call result"
    );
}

void test_switch_constructor_pattern_binds_case_local_names_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_binding_success.or";
    write_list_async_head_capture_fixture(path);

    assert_fixture_single_capture_kind(
        path,
        "head",
        orison::semantics::ConcurrencyCaptureKind::immutable_outer_local
    );
}

void test_switch_top_level_name_pattern_rejects_unknown_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_name_pattern_binding_failure.or";
    write_switch_name_pattern_async_capture_fixture(path);

    assert_fixture_single_diagnostic(
        path,
        5,
        "switch constructor pattern 'head' does not match any declared choice variant"
    );
}

void test_switch_call_pattern_rejects_unknown_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_call_pattern_unknown_variant_failure.or";
    write_switch_unknown_call_pattern_fixture(path);

    assert_fixture_single_diagnostic(
        path,
        4,
        "switch constructor pattern 'Missing' does not match any declared choice variant"
    );
}

void test_switch_unknown_constructor_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_unknown_constructor_without_default_no_cascade_failure.or";
    write_maybe_unknown_constructor_fixture(path);

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern 'Missing' does not match any declared choice variant"
    );
}

void test_switch_nested_constructor_pattern_binds_nested_names_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_constructor_pattern_success.or";
    write_nested_list_async_capture_fixture(path);

    assert_fixture_single_capture(path, "next");
}

void test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrapped_payload_success.or";
    write_nested_list_raw_write_fixture(path, "Int32");

    assert_fixture_success(path);
}

void test_switch_rejects_nested_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(value)) => 1", "Wrap(Some(other)) => 2"});

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path));
}

void test_switch_rejects_nested_literal_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(1)) => 1", "Wrap(Some(1)) => 2"});

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path));
}

void test_switch_accepts_disjoint_nested_literal_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_disjoint_nested_literal_payload_success.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(1)) => 1", "Wrap(Some(2)) => 2"});

    assert_fixture_success(path);
}

void test_switch_rejects_nested_wildcard_literal_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(value)) => 1", "Wrap(Some(1)) => 2"});

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path));
}

void test_switch_rejects_nested_literal_wildcard_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(1)) => 1", "Wrap(Some(value)) => 2"});

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path));
}

void test_switch_rejects_nested_multi_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_multi_payload_overlap_failure.or";
    write_boxed_pair_maybe_switch_fixture(
        path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 1)) => 2"}
    );

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path));
}

void test_switch_accepts_disjoint_nested_multi_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_disjoint_nested_multi_payload_success.or";
    write_boxed_pair_maybe_switch_fixture(
        path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 2)) => 2"}
    );

    assert_fixture_success(path);
}

void test_switch_accepts_mismatched_nested_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_mismatched_nested_constructor_success.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(value)) => 1", "Wrap(Empty) => 2"});

    assert_fixture_success(path);
}

void test_switch_rejects_duplicate_nested_zero_payload_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_nested_zero_payload_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Empty) => 1", "Wrap(Empty) => 2"});

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path));
}

void test_switch_rejects_duplicate_nested_zero_payload_constructor_no_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_nested_zero_payload_no_cascade_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Empty) => 1", "Wrap(Empty) => 2"}, false);

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path));
}

void test_switch_rejects_deep_nested_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_deep_nested_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(other))) => 2"}
    );

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path), 12);
}

void test_switch_accepts_disjoint_deep_nested_literal_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_disjoint_deep_nested_literal_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(2))) => 2"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_deep_nested_wildcard_literal_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_deep_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(1))) => 2"}
    );

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path), 12);
}

void test_switch_rejects_deep_nested_literal_wildcard_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_deep_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(value))) => 2"}
    );

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path), 12);
}

void test_switch_accepts_mismatched_deep_nested_zero_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_mismatched_deep_nested_zero_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Empty)) => 2"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_duplicate_deep_nested_zero_payload_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_deep_nested_zero_payload_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Empty)) => 1", "Wrap(Hold(Empty)) => 2"}
    );

    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path), 12);
}

void test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrapped_payload_failure.or";
    write_nested_list_raw_write_fixture(path, "Byte");

    assert_fixture_single_diagnostic(
        path,
        7,
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );
}

void test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_generic_constructor_payload_success.or";
    write_maybe_raw_write_fixture(path, "Int32");

    assert_fixture_success(path);
}

void test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_generic_constructor_payload_failure.or";
    write_maybe_raw_write_fixture(path, "Byte");

    assert_fixture_single_diagnostic(
        path,
        7,
        "raw_write value type 'Byte' does not match pointer element type 'UInt32'"
    );
}

void test_switch_constructor_pattern_rejects_variant_from_different_choice_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_wrong_choice_variant_failure.or";
    write_result_switch_with_maybe_variant_fixture(path);

    assert_fixture_single_diagnostic(
        path,
        10,
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );
}

void test_switch_wrong_choice_constructor_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_wrong_choice_constructor_without_default_no_cascade_failure.or";
    write_result_switch_with_maybe_variant_fixture(path, false);

    assert_fixture_single_diagnostic(
        path,
        10,
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );
}

void test_switch_constructor_pattern_uses_subject_specific_arity_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_subject_specific_arity_success.or";
    write_flag_switch_with_maybe_same_name_fixture(path);

    assert_fixture_success(path);
}

void test_switch_nested_constructor_pattern_rejects_variant_from_different_payload_choice_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrong_payload_choice_failure.or";
    write_envelope_result_switch_with_maybe_variant_fixture(path);

    assert_fixture_single_diagnostic(
        path,
        12,
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );
}

void test_switch_nested_constructor_pattern_uses_payload_specific_arity_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_specific_arity_success.or";
    write_envelope_pair_flag_switch_with_maybe_same_name_fixture(path);

    assert_fixture_success(path);
}

void test_switch_nested_constructor_pattern_rejects_invalid_payload_shape_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_constructor_pattern_shape_failure.or";
    write_list_switch_fixture(path, {"Node(head + 1, tail) => 0"}, true);

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
    );
}

void test_switch_constructor_payload_shape_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_payload_shape_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head + 1, tail) => 0"}, false, false);

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
    );
}

void test_switch_constructor_pattern_rejects_duplicate_binding_names_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_duplicate_binding_failure.or";
    write_list_switch_fixture(path, {"Node(head, head) => 0"}, true);

    assert_fixture_single_diagnostic(path, 7, "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_constructor_duplicate_binding_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head, head) => 0"}, false, false);

    assert_fixture_single_diagnostic(path, 7, "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_nested_constructor_pattern_rejects_duplicate_binding_names_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_switch_nested_constructor_pattern_duplicate_binding_failure.or";
    write_list_switch_fixture(path, {"Node(head, Node(head, tail)) => 0"}, true);

    assert_fixture_single_diagnostic(path, 7, "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_nested_constructor_duplicate_binding_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head, Node(head, tail)) => 0"}, false, false);

    assert_fixture_single_diagnostic(path, 7, "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_constructor_pattern_rejects_missing_payload_values_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_arity_missing_failure.or";
    write_list_switch_fixture(path, {"Node(head) => 0"}, true);

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern 'Node' expects 2 payload values but received 1"
    );
}

void test_switch_constructor_pattern_rejects_extra_payload_values_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_arity_extra_failure.or";
    write_list_switch_fixture(path, {"Empty(value) => 0"}, true);

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern 'Empty' expects 0 payload values but received 1"
    );
}

void test_switch_constructor_pattern_arity_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_pattern_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head) => 0"}, false, false);

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern 'Node' expects 2 payload values but received 1"
    );
}

void test_switch_zero_payload_constructor_arity_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_zero_payload_constructor_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Empty(value) => 0"}, false, false);

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern 'Empty' expects 0 payload values but received 1"
    );
}

void test_switch_rejects_constructor_then_value_pattern_mix_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_pattern_mix_constructor_value_failure.or";
    write_list_switch_fixture(path, {"Empty => 0", "1 => 1"});

    assert_fixture_single_diagnostic(path, 8, "switch cannot mix value patterns with constructor patterns");
}

void test_switch_rejects_value_then_constructor_pattern_mix_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_pattern_mix_value_constructor_failure.or";
    write_value_then_constructor_pattern_mix_fixture(path);

    assert_fixture_single_diagnostic(path, 8, "switch cannot mix value patterns with constructor patterns");
}

void test_switch_pattern_mix_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_pattern_mix_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "1 => 1"});

    assert_fixture_single_diagnostic(path, 8, "switch cannot mix value patterns with constructor patterns");
}

void test_switch_rejects_mismatched_value_pattern_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_value_pattern_type_failure.or";
    write_bool_switch_text_value_pattern_fixture(path);

    assert_fixture_single_diagnostic(
        path,
        4,
        "switch value pattern type 'Text' does not match switched expression type 'Bool'"
    );
}

void test_switch_accepts_same_width_integer_cast_value_pattern_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_value_pattern_same_width_success.or";
    write_same_width_integer_value_pattern_fixture(path);

    assert_fixture_success(path);
}

void test_switch_rejects_duplicate_boolean_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_boolean_value_failure.or";
    write_duplicate_bool_value_pattern_fixture(path);

    assert_fixture_single_diagnostic(path, 5, "switch value pattern 'true' is duplicated");
}

void test_switch_duplicate_bool_without_default_does_not_cascade_to_missing_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_bool_without_default_no_cascade_failure.or";
    write_duplicate_bool_value_pattern_fixture(path, false);

    assert_fixture_single_diagnostic(path, 5, "switch value pattern 'true' is duplicated");
}

void test_switch_rejects_duplicate_string_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_string_value_failure.or";
    write_duplicate_text_value_pattern_fixture(path);

    assert_fixture_single_diagnostic(path, 5, "switch value pattern '\"ready\"' is duplicated");
}

void test_switch_rejects_duplicate_integer_cast_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_integer_cast_value_failure.or";
    write_duplicate_integer_cast_value_pattern_fixture(path);

    assert_fixture_single_diagnostic(path, 5, "switch value pattern '1 as Int32' is duplicated");
}

void test_switch_rejects_redundant_bool_default_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_bool_default_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"}, true);

    assert_fixture_single_diagnostic(
        path,
        6,
        "switch default case is redundant after true and false value patterns"
    );
}

void test_switch_duplicate_bool_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_bool_redundant_default_no_cascade.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0", "false => 2"}, true);

    assert_fixture_single_diagnostic(path, 6, "switch value pattern 'false' is duplicated");
}

void test_switch_accepts_exhaustive_bool_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_bool_without_default_success.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"});

    assert_fixture_success(path);
}

void test_switch_rejects_missing_bool_value_pattern_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_bool_pattern_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1"});

    assert_fixture_single_diagnostic(path, 3, "switch is missing boolean value pattern 'false'");
}

void test_switch_rejects_redundant_zero_payload_choice_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_choice_default_failure.or";
    write_zero_payload_choice_switch_fixture(
        path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"},
        true
    );

    assert_fixture_single_diagnostic(
        path,
        11,
        "switch default case is redundant after all zero-payload choice variants are covered"
    );
}

void test_switch_duplicate_zero_payload_choice_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_zero_payload_choice_redundant_default_no_cascade.or";
    write_zero_payload_choice_switch_fixture(
        path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3", "Closed => 4"},
        true
    );

    assert_fixture_single_diagnostic(path, 11, "switch constructor pattern 'Closed' is duplicated");
}

void test_switch_accepts_exhaustive_zero_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_choice_without_default_success.or";
    write_zero_payload_choice_switch_fixture(
        path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_redundant_payload_choice_default_after_full_cover_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_payload_choice_default_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "Empty => 0"}, true);

    assert_fixture_single_diagnostic(
        path,
        9,
        "switch default case is redundant after all choice variants are covered"
    );
}

void test_switch_accepts_exhaustive_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "Empty => 0"});

    assert_fixture_success(path);
}

void test_switch_accepts_reversed_exhaustive_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_reversed_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Empty => 0", "Some(value) => value"});

    assert_fixture_success(path);
}

void test_switch_accepts_literal_payload_choice_arm_with_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_literal_payload_choice_default_success.or";
    write_maybe_int_exhaustiveness_fixture(path, {"Some(1) => 1", "Empty => 0"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_literal_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(path, {"Some(1) => 1", "Empty => 0"});

    assert_fixture_single_diagnostic(path, 6, "switch is missing choice variant 'Some'");
}

void test_switch_rejects_reversed_literal_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_reversed_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(path, {"Empty => 0", "Some(1) => 1"});

    assert_fixture_single_diagnostic(path, 6, "switch is missing choice variant 'Some'");
}

void test_switch_accepts_nested_payload_choice_arm_with_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_choice_default_success.or";
    write_boxed_maybe_exhaustiveness_fixture(path, {"Wrap(Some(value)) => value", "Blank => 0"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_nested_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_choice_missing_failure.or";
    write_boxed_maybe_exhaustiveness_fixture(path, {"Wrap(Some(value)) => value", "Blank => 0"});

    assert_fixture_single_diagnostic(path, 9, "switch is missing choice variant 'Wrap'");
}

void test_switch_accepts_partial_multi_payload_choice_arm_with_default_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_partial_multi_payload_choice_default_success.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => left", "Empty => 0"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_partial_multi_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_partial_multi_payload_choice_missing_failure.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => left", "Empty => 0"});

    assert_fixture_single_diagnostic(path, 6, "switch is missing choice variant 'Both'");
}

void test_switch_rejects_missing_payload_choice_variant_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_payload_choice_variant_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value"});

    assert_fixture_single_diagnostic(path, 6, "switch is missing choice variant 'Empty'");
}

void test_switch_accepts_exhaustive_multi_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_exhaustive_multi_payload_choice_success.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_redundant_multi_payload_choice_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_redundant_multi_payload_choice_default_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"},
        true
    );

    assert_fixture_single_diagnostic(
        path,
        11,
        "switch default case is redundant after all choice variants are covered"
    );
}

void test_switch_duplicate_payload_choice_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_payload_choice_redundant_default_no_cascade.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        path,
        {"First(value) => value", "Second(value) => value", "Empty => 0", "First(other) => other"},
        true
    );

    assert_fixture_single_diagnostic(path, 11, "switch constructor pattern 'First(...)' is duplicated");
}

void test_switch_rejects_first_missing_multi_payload_choice_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_first_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(path, {"Second(value) => value", "Empty => 0"});

    assert_fixture_single_diagnostic(path, 7, "switch is missing choice variant 'First'");
}

void test_switch_rejects_second_missing_multi_payload_choice_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_second_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(path, {"First(value) => value", "Empty => 0"});

    assert_fixture_single_diagnostic(path, 7, "switch is missing choice variant 'Second'");
}

void test_switch_duplicate_multi_payload_choice_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_multi_payload_choice_no_cascade_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(path, {"First(value) => value", "First(other) => other"});

    assert_fixture_single_diagnostic(path, 9, "switch constructor pattern 'First(...)' is duplicated");
}

void test_switch_duplicate_payload_choice_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_payload_choice_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "Some(other) => other"});

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Some(...)' is duplicated");
}

void test_switch_rejects_duplicate_zero_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_choice_constructor_failure.or";
    write_zero_payload_choice_switch_fixture(path, {"Closed => 1", "EndOfInput => 2", "Closed => 3"});

    assert_fixture_single_diagnostic(path, 10, "switch constructor pattern 'Closed' is duplicated");
}

void test_switch_duplicate_choice_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_choice_without_default_no_cascade_failure.or";
    write_zero_payload_choice_switch_fixture(path, {"Closed => 1", "EndOfInput => 2", "Closed => 3"});

    assert_fixture_single_diagnostic(path, 10, "switch constructor pattern 'Closed' is duplicated");
}

void test_switch_rejects_duplicate_name_only_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_name_only_payload_choice_constructor_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => 1", "Some(other) => 2"}, true);

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Some(...)' is duplicated");
}

void test_switch_duplicate_payload_choice_constructor_does_not_cascade_to_binding_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_payload_choice_constructor_no_cascade_failure.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, right) => 1", "Both(value, value) => 2"}, true);

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Both(...)' is duplicated");
}

void test_switch_rejects_duplicate_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(1) => 1", "Int(1) => 2"});

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_equivalent_integer_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_equivalent_integer_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(1) => 1", "Int(1 as Int64) => 2"});

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_wildcard_then_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_wildcard_then_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(value) => 1", "Int(1) => 2"});

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_literal_then_wildcard_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_literal_then_wildcard_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(1) => 1", "Int(value) => 2"});

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_multi_payload_partial_overlap_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_partial_overlap_choice_constructor_failure.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => 1", "Both(other, 1) => 2"}, true);

    assert_fixture_single_diagnostic(path, 8, "switch constructor pattern 'Both(...)' is duplicated");
}

void test_switch_accepts_multi_payload_disjoint_literal_choice_constructor_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_disjoint_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => 1", "Both(other, 2) => 2"}, true);

    assert_fixture_success(path);
}

void test_switch_accepts_multi_payload_disjoint_leading_literal_choice_constructor_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_disjoint_leading_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(1, left) => 1", "Both(2, right) => 2"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_missing_zero_payload_choice_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_choice_variant_failure.or";
    write_zero_payload_choice_switch_fixture(path, {"Closed => 1", "EndOfInput => 2"});

    assert_fixture_single_diagnostic(path, 7, "switch is missing zero-payload choice variant 'PermissionDenied'");
}

void test_switch_rejects_multiple_default_cases_semantically() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_multiple_default_semantic_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"});

    auto diagnostics = analyze_mutated_first_switch_fixture(path, [](auto& switch_statement) {
        switch_statement.switch_cases.front().is_default = true;
        switch_statement.switch_cases.back().is_default = true;
    });
    assert_multiple_switch_default_diagnostics(diagnostics);
}

void test_switch_rejects_nonfinal_default_case_semantically() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_nonfinal_default_semantic_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"});

    auto diagnostics = analyze_mutated_first_switch_fixture(path, [](auto& switch_statement) {
        switch_statement.switch_cases.front().is_default = true;
    });
    assert_nonfinal_switch_default_diagnostic(diagnostics);
}

void test_switch_nonfinal_default_suppresses_branch_analysis_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nonfinal_default_branch_no_cascade.or";
    write_bool_value_pattern_switch_fixture(path, {"default => await flag", "true => 1"});

    assert_fixture_single_diagnostic(path, 4, "switch default case must be the final case");
}

void test_switch_multiple_default_suppresses_branch_analysis_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_multiple_default_branch_no_cascade.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => await flag"});

    auto diagnostics = analyze_mutated_first_switch_fixture(path, [](auto& switch_statement) {
        switch_statement.switch_cases.front().is_default = true;
        switch_statement.switch_cases.back().is_default = true;
    });
    assert_multiple_switch_default_diagnostics(diagnostics);
}

void test_break_outside_loop_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_break_outside_loop_failure.or";
    write_loop_control_fixture(path, "stop() -> Unit", {"break"});

    assert_fixture_single_diagnostic(path, 3, "break statement is only valid inside loops");
}

void test_continue_outside_loop_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_continue_outside_loop_failure.or";
    write_loop_control_fixture(path, "keep_going() -> Unit", {"continue"});

    assert_fixture_single_diagnostic(path, 3, "continue statement is only valid inside loops");
}

void test_break_inside_loop_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_break_inside_loop_success.or";
    write_loop_control_fixture(path, "stop(flag: Bool) -> Unit", {"while flag", "    break"});

    assert_fixture_success(path);
}

void test_continue_inside_loop_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_continue_inside_loop_success.or";
    write_loop_control_fixture(
        path,
        "scan(items: shared View<Int64>) -> Unit",
        {"for item in items", "    continue"}
    );

    assert_fixture_success(path);
}

void test_this_outside_method_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_this_outside_method_failure.or";
    write_receiver_fixture(path, {"function current() -> Int64", "    return this"});

    assert_fixture_single_diagnostic(
        path,
        3,
        "receiver 'this' is only valid inside implements or extend methods"
    );
}

void test_receiver_parameter_outside_method_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_receiver_parameter_outside_method_failure.or";
    write_receiver_fixture(path, {"function current(this: Int64) -> Int64", "    return 0"});

    assert_fixture_single_diagnostic(
        path,
        2,
        "receiver parameter 'this' is only valid in implements or extend methods"
    );
}

void test_qualified_this_type_in_ordinary_function_signature_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_this_type_signature_failure.or";
    write_receiver_fixture(path, {"function project(value: shared This) -> shared This", "    return value"});

    auto diagnostics = analyze_orison_fixture(path);
    assert_this_type_context_diagnostics(diagnostics, 2, 2);
}

void test_qualified_this_type_in_local_annotation_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_this_type_local_failure.or";
    write_receiver_fixture(path, {"function cache() -> Unit", "    let current: exclusive This = unit"});

    assert_fixture_this_type_context_diagnostic(path, 3);
}

void test_receiver_parameter_with_nonself_type_inside_method_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_receiver_parameter_nonself_type_failure.or";
    write_receiver_fixture(path, {"extend Buffer", "    function reset(this: Int64) -> Unit", "        return"});

    assert_fixture_single_diagnostic(
        path,
        3,
        "receiver parameter 'this' must use This, shared This, or exclusive This"
    );
}

void test_unsafe_intrinsic_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_outside_unsafe_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_byte(p: Address) -> Byte",
            "    return raw_read(p)",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        3,
        "unsafe intrinsic 'raw_read' is only valid inside unsafe functions or unsafe blocks"
    );
}

void test_unsafe_intrinsic_inside_unsafe_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_inside_unsafe_function.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Address) -> Byte",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_unsafe_intrinsic_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_inside_unsafe_block.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function zero_byte(p: Address) -> Unit",
            "    unsafe",
            "        raw_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_address_of_nonstorage_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_address_of_nonstorage_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function pointer() -> Address",
            "    return address_of(1)",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "address_of currently requires an addressable storage operand");
}

void test_raw_read_nonaddress_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_read_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word() -> UInt32",
            "    return raw_read(1)",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "raw_read currently requires an address-like first argument");
}

void test_raw_offset_nonaddress_base_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_offset_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function advance() -> Address",
            "    return raw_offset(1, 2)",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "raw_offset currently requires an address-like first argument");
}

void test_raw_offset_noninteger_offset_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_offset_noninteger_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function advance(base: Address) -> Address",
            "    return raw_offset(base, \"one\")",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "raw_offset currently requires an integer offset argument");
}

void test_volatile_read_nonaddress_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function uart_ready() -> UInt32",
            "    return volatile_read(1)",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "volatile_read currently requires an address-like first argument");
}

void test_nested_address_of_and_raw_offset_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_nested_address_of_raw_offset_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function poke(buf: exclusive Buffer, value: Byte) -> Unit",
            "    let p = address_of(buf.data[0])",
            "    raw_write(raw_offset(p, 1), value)",
        }
    );

    assert_fixture_success(path);
}

void test_index_access_noninteger_index_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_index_access_noninteger_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_byte(device: Device, value: Byte) -> Unit",
            "    raw_write(device.ptrs[\"one\"], value)",
        }
    );

    assert_fixture_single_diagnostic(path, 5, "index access currently requires an integer index expression");
}

void test_index_access_integer_index_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_index_access_integer_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_byte(device: Device, index: UInt32, value: Byte) -> Unit",
            "    raw_write(device.ptrs[index], value)",
        }
    );

    assert_fixture_success(path);
}

void test_call_unsafe_function_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_call_unsafe_function_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Address) -> UInt32",
            "    return raw_read(p)",
            "function read_twice(p: Address) -> UInt32",
            "    return read_word(p)",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        5,
        "call to unsafe function 'read_word' is only valid inside unsafe functions or unsafe blocks"
    );
}

void test_call_unsafe_function_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_call_unsafe_function_block_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Address) -> UInt32",
            "    return raw_read(p)",
            "function copy_word(p: Address) -> UInt32",
            "    unsafe",
            "        return read_word(p)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_construction_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_byte(addr: Address) -> Byte",
            "    let p = Pointer(addr)",
            "    return raw_read(p)",
        }
    );

    auto diagnostics = analyze_orison_fixture(path);
    assert_pointer_construction_unsafe_boundary_diagnostics(diagnostics);
}

void test_pointer_construction_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_block_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function scribble(addr: Address) -> Unit",
            "    unsafe",
            "        let p = Pointer(addr)",
            "        raw_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_construction_without_argument_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_noarg_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p = Pointer()",
            "    return raw_read(p)",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "Pointer construction currently requires exactly one source argument");
}

void test_pointer_construction_with_multiple_arguments_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_multiarg_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(addr: Address) -> Byte",
            "    let p = Pointer(addr, addr)",
            "    return raw_read(p)",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "Pointer construction currently requires exactly one source argument");
}

void test_pointer_construction_with_nonaddress_argument_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p = Pointer(\"text\")",
            "    return raw_read(p)",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        3,
        "Pointer construction currently requires an address-like source argument"
    );
}

void test_pointer_construction_with_address_of_argument_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_addressof_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function first_ptr(buf: exclusive Buffer) -> Pointer<Byte>",
            "    let p = Pointer(address_of(buf.data[0]))",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_typed_binding_with_mismatched_address_of_source_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_addressof_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = Pointer(address_of(buf.data[0]))",
            "    return p",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        5,
        "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'"
    );
}

void test_pointer_typed_binding_with_matching_address_of_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_addressof_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_byte_ptr(buf: Buffer) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = Pointer(address_of(buf.data[0]))",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_same_width_address_of_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_addressof_source_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_pointer_typed_binding_with_nonpointer_initializer_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p: Pointer<Byte> = \"text\"",
            "    return 0",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        3,
        "pointer-typed binding initializer currently requires a structurally pointer-like expression"
    );
}

void test_pointer_typed_binding_with_pointer_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte(base: Pointer<Byte>) -> Byte",
            "    let p: Pointer<Byte> = raw_offset(base, 1)",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_typed_binding_with_mismatched_raw_offset_source_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_rawoffset_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = raw_offset(base, 1)",
            "    return p",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        3,
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
}

void test_pointer_typed_binding_with_matching_raw_offset_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_rawoffset_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = raw_offset(base, 1)",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_same_width_raw_offset_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_rawoffset_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_typed_binding_result_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_typed_binding_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value: UInt32 = raw_read(p)",
            "    return value",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "raw_read result type 'Byte' does not match binding type 'UInt32'");
}

void test_raw_read_typed_binding_result_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value: Byte = raw_read(p)",
            "    return value",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_typed_binding_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_typed_binding_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> Int32",
            "    let value: UInt32 = raw_read(p)",
            "    return value as Int32",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_typed_binding_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_typed_binding_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    let value: IntSize = raw_read(p)",
            "    return value",
        }
    );

    assert_fixture_single_diagnostic(path, 3, "raw_read result type 'Byte' does not match binding type 'IntSize'");
}

void test_pointer_typed_binding_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let source = \"text\"",
            "    let p: Pointer<Byte> = source",
            "    return 0",
        }
    );

    assert_pointer_typed_binding_initializer_diagnostic(path, 4);
}

void test_pointer_typed_binding_with_mismatched_field_pointer_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_field_pointer_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Byte>",
            "unsafe function next_ptr(device: Device) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = device.ptr",
            "    return p",
        }
    );

    assert_pointer_typed_binding_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_pointer_typed_binding_with_same_width_field_pointer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_same_width_field_pointer_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptr: Pointer<Int32>\n";
        output << "unsafe function next_ptr(device: Device) -> Pointer<UInt32>\n";
        output << "    let p: Pointer<UInt32> = device.ptr\n";
        output << "    return p\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(!diagnostics.has_errors());
}

void test_pointer_return_with_nonpointer_expression_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    return \"text\"",
        }
    );

    assert_pointer_return_structural_diagnostic(path, 3);
}

void test_pointer_return_with_pointer_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_mismatched_raw_offset_source_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_rawoffset_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_pointer_return_with_matching_raw_offset_source_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_rawoffset_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    let source = \"text\"",
            "    return source",
        }
    );

    assert_pointer_return_structural_diagnostic(path, 4);
}

void test_pointer_return_with_mismatched_helper_pointer_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_helper_pointer_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return base",
            "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    return byte_ptr(base)",
        }
    );

    assert_pointer_return_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_pointer_return_with_same_width_helper_pointer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_helper_pointer_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function wordish_ptr(base: Pointer<Int32>) -> Pointer<Int32>",
            "    return base",
            "unsafe function word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>",
            "    return wordish_ptr(base)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_helper_returned_pointer_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_helper_returned_pointer_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Int32>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_helper_returned_pointer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_helper_returned_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_address_return_with_generic_helper_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_with_generic_helper_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_raw_write_generic_receiver_method_pointer_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_receiver_method_pointer_same_width_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_raw_write_generic_receiver_method_pointer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_receiver_method_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        path,
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

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 8, "UInt32", "Byte");
}

void test_raw_write_generic_record_pointer_field_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_pointer_field_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Int32>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_record_pointer_field_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_pointer_field_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_raw_write_generic_record_scalar_field_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_scalar_field_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "unsafe function write_word(box: Box<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, box.value)",
        }
    );

    assert_fixture_success(path);
}

void test_address_return_with_generic_record_field_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_with_generic_record_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "function read_base(box: Box<Address>) -> Address",
            "    return box.value",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_mismatched_address_of_source_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_addressof_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>",
            "    return Pointer(address_of(buf.data[0]))",
        }
    );

    assert_pointer_construction_source_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_pointer_return_with_matching_address_of_source_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_addressof_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_byte_ptr(buf: Buffer) -> Pointer<Byte>",
            "    return Pointer(address_of(buf.data[0]))",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_return_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_read(p)",
        }
    );

    assert_raw_read_result_mismatch_diagnostic(path, 3, "Byte", "Pointer<Byte>");
}

void test_raw_read_return_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_return_same_width_integer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_return_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_return_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    return raw_read(p)",
        }
    );

    assert_raw_read_result_mismatch_diagnostic(path, 3, "Byte", "IntSize");
}

void test_raw_write_value_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_raw_write_value_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_value_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: UInt32) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_same_width_integer_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_same_width_integer_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_pointer_sized_integer_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_pointer_sized_integer_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_computed_integer_sum_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_computed_integer_sum_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(input) + 1)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_computed_bitwise_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_computed_bitwise_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(out, value bit_or 1)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_computed_ternary_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_computed_ternary_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    raw_write(out, flag ? left : right)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_rebound_computed_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_rebound_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    raw_write(out, masked)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_branch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_branch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_raw_write_branch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_branch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
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

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 8, "IntSize", "UInt32");
}

void test_raw_write_switch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_switch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_raw_write_switch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_switch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
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

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_raw_write_array_indexed_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_array_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    raw_write(out, items[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_bound_array_literal_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_bound_array_literal_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    raw_write(out, staged[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_array_indexed_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_array_indexed_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit",
            "    raw_write(out, items[0])",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_helper_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_helper_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function words() -> DynamicArray<Int32>",
            "    return []",
            "unsafe function write_word(out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, words()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_container_field_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_container_field_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_nested_scalar_field_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_nested_scalar_field_value_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_raw_write_nested_scalar_field_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_nested_scalar_field_computed_value_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_raw_write_nested_scalar_field_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_nested_scalar_field_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
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

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_raw_write_method_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_method_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "extend Device",
            "    function words_view(this: shared This) -> DynamicArray<Int32>",
            "        this.words",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words_view()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<IntSize>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words[0])",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "IntSize", "UInt32");
}

void test_raw_write_integer_literal_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_integer_literal_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    raw_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_integer_cast_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_integer_cast_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_integer_cast_target_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_integer_cast_target_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as Int64)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "Int64", "UInt32");
}

void test_raw_write_same_width_integer_cast_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_same_width_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_pointer_sized_integer_cast_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_pointer_sized_integer_cast_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as IntSize)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_recovered_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_recovered_raw_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_raw_write_recovered_raw_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_recovered_raw_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)) as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_helper_returned_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_helper_returned_raw_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(byte_ptr(base), 1)))",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_raw_write_member_returned_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_returned_raw_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
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

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 9, "Byte", "UInt32");
}

void test_raw_write_member_returned_raw_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_returned_raw_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(device.byte_ptr(base), 1)) as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_non_integer_cast_value_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_non_integer_cast_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Bool) -> Unit",
            "    raw_write(p, value as Bool)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "Bool", "UInt32");
}

void test_raw_write_helper_pointer_constructor_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>",
            "    return Pointer(addr)",
            "unsafe function write_word(addr: Address, value: UInt32) -> Unit",
            "    raw_write(byte_ptr(addr), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_raw_write_helper_pointer_constructor_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_helper_pointer_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>",
            "    return Pointer(addr)",
            "unsafe function write_byte(addr: Address, value: Byte) -> Unit",
            "    raw_write(byte_ptr(addr), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
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

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 9, "UInt32", "Byte");
}

void test_raw_write_member_helper_pointer_constructor_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_helper_pointer_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, addr: Address) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(addr)",
            "unsafe function write_byte(device: Device, addr: Address, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(addr), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_raw_offset_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(next_byte_ptr(base), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_raw_write_raw_offset_helper_pointer_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_raw_offset_helper_pointer_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_byte(base: Pointer<Byte>, value: Byte) -> Unit",
            "    raw_write(next_byte_ptr(base), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_raw_offset_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
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

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 9, "UInt32", "Byte");
}

void test_raw_write_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Byte>",
            "unsafe function write_word(device: Device, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_raw_write_indexed_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_indexed_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit",
            "    raw_write(device.ptrs[index], value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_indexed_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_indexed_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_rebound_indexed_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_rebound_indexed_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit",
            "    let p = device.ptrs[index]",
            "    raw_write(p, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 6, "UInt32", "Byte");
}

void test_rebound_indexed_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_rebound_indexed_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_return_rebound_indexed_record_pointer_field_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_record_pointer_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>",
            "    let p = device.ptrs[index]",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_return_rebound_indexed_member_field_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_member_field_address_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_return_rebound_indexed_pointer_used_by_helper_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_pointer_used_by_helper_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_return_rebound_indexed_address_used_by_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_volatile_read_return_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    return volatile_read(p)",
        }
    );

    assert_volatile_read_result_mismatch_diagnostic(path, 3, "Byte", "Pointer<Byte>");
}

void test_volatile_read_return_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_return_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    return volatile_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_return_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_return_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return volatile_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_return_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_return_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    return volatile_read(p)",
        }
    );

    assert_volatile_read_result_mismatch_diagnostic(path, 3, "Byte", "IntSize");
}

void test_volatile_read_typed_binding_result_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_typed_binding_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value: UInt32 = volatile_read(p)",
            "    return value",
        }
    );

    assert_volatile_read_binding_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_volatile_read_typed_binding_result_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value: Byte = volatile_read(p)",
            "    return value",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_typed_binding_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_typed_binding_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> Int32",
            "    let value: UInt32 = volatile_read(p)",
            "    return value as Int32",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_typed_binding_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_typed_binding_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    let value: IntSize = volatile_read(p)",
            "    return value",
        }
    );

    assert_volatile_read_binding_mismatch_diagnostic(path, 3, "Byte", "IntSize");
}

void test_volatile_write_value_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_volatile_write_value_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_value_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: UInt32) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_same_width_integer_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_same_width_integer_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_pointer_sized_integer_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_pointer_sized_integer_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_computed_integer_sum_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_integer_sum_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(input) + 1)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_computed_bitwise_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_bitwise_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(out, value bit_or 1)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_computed_ternary_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_ternary_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    volatile_write(out, flag ? left : right)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_rebound_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_rebound_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    volatile_write(out, masked)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_branch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_branch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left bit_or 1",
            "    else",
            "        selected = right + 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_branch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_branch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left + 1",
            "    else",
            "        selected = right shift_left 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 8, "IntSize", "UInt32");
}

void test_volatile_write_switch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_switch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left bit_or 1",
            "        default => selected = right + 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_switch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_switch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left + 1",
            "        default => selected = right shift_left 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_volatile_write_array_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_array_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    volatile_write(out, items[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_bound_array_literal_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_bound_array_literal_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    volatile_write(out, staged[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_array_indexed_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_array_indexed_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit",
            "    volatile_write(out, items[0])",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_helper_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function words() -> DynamicArray<Int32>",
            "    return []",
            "unsafe function write_word(out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, words()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_member_container_field_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_container_field_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_nested_scalar_field_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_nested_scalar_field_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status bit_or 1)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: IntSize",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_volatile_write_method_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_method_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "extend Device",
            "    function words_view(this: shared This) -> DynamicArray<Int32>",
            "        this.words",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words_view()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<IntSize>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words[0])",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "IntSize", "UInt32");
}

void test_volatile_write_integer_literal_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_integer_literal_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    volatile_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_integer_cast_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_integer_cast_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_integer_cast_target_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_integer_cast_target_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as Int64)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "Int64", "UInt32");
}

void test_volatile_write_same_width_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_same_width_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_pointer_sized_integer_cast_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_pointer_sized_integer_cast_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as IntSize)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_recovered_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_recovered_volatile_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_volatile_write_recovered_volatile_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_recovered_volatile_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)) as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_helper_returned_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_returned_volatile_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(word_ptr(base), 1)))",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_volatile_write_member_returned_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_returned_volatile_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)))",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 9, "Byte", "UInt32");
}

void test_volatile_write_member_returned_volatile_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_returned_volatile_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)) as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_non_integer_cast_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_non_integer_cast_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Bool) -> Unit",
            "    volatile_write(p, value as Bool)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "Bool", "UInt32");
}

void test_volatile_write_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function word_ptr(addr: Address) -> Pointer<UInt32>",
            "    return Pointer(addr)",
            "unsafe function write_word(addr: Address, value: Byte) -> Unit",
            "    volatile_write(word_ptr(addr), value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_volatile_write_member_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, addr: Address) -> Pointer<UInt32>",
            "        unsafe",
            "            return Pointer(addr)",
            "unsafe function write_word(device: Device, addr: Address, value: Byte) -> Unit",
            "    volatile_write(device.word_ptr(addr), value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 9, "Byte", "UInt32");
}

void test_volatile_write_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_raw_offset_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(next_word_ptr(base), value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_address_typed_binding_with_nonaddress_initializer_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_base() -> Address",
            "    let base: Address = \"text\"",
            "    return 0x4000_1000",
        }
    );

    assert_address_binding_initializer_diagnostic(path, 3);
}

void test_address_typed_binding_with_address_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function first_addr(buf: exclusive Buffer) -> Address",
            "    let base: Address = address_of(buf.data[0])",
            "    return base",
        }
    );

    assert_fixture_success(path);
}

void test_address_typed_binding_with_field_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_typed_binding_field_address_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    base: Address",
            "function read_base(device: Device) -> Address",
            "    let base: Address = device.base",
            "    return base",
        }
    );

    assert_fixture_success(path);
}

void test_address_typed_binding_with_indexed_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_typed_binding_indexed_address_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "function read_base(device: Device, index: Int64) -> Address",
            "    let base: Address = device.bases[index]",
            "    return base",
        }
    );

    assert_fixture_success(path);
}

void test_address_typed_binding_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_base() -> Address",
            "    let source = \"text\"",
            "    let base: Address = source",
            "    return 0x4000_1000",
        }
    );

    assert_address_binding_initializer_diagnostic(path, 4);
}

void test_address_return_with_nonaddress_expression_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function base() -> Address",
            "    return \"text\"",
        }
    );

    assert_address_return_diagnostic(path, 3);
}

void test_address_return_with_address_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function base(buf: exclusive Buffer) -> Address",
            "    return address_of(buf.data[0])",
        }
    );

    assert_fixture_success(path);
}

void test_address_return_with_helper_returned_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_helper_returned_address_success.or";
    write_concurrency_fixture(
        path,
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

    assert_fixture_success(path);
}

void test_address_return_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function base() -> Address",
            "    let source = \"text\"",
            "    return source",
        }
    );

    assert_address_return_diagnostic(path, 4);
}

void test_task_outside_async_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_sync_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return request(url)",
        }
    );

    assert_task_inside_async_diagnostic(path, 3);
}

void test_thread_outside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_sync_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        sum(data)",
            "    return worker.join()",
        }
    );

    assert_fixture_success(path);
}

void test_thread_join_receiver_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_join_receiver_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker.join()",
        }
    );

    assert_fixture_success(path);
}

void test_join_non_thread_receiver_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_join_non_thread_receiver_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "async function fetch() -> Int64",
            "    let request_task = task",
            "        1",
            "    return request_task.join()",
        }
    );

    assert_join_task_receiver_diagnostic(path, 5);
}

void test_join_async_call_receiver_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_join_async_call_receiver_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return pending.join()",
        }
    );

    assert_join_async_call_receiver_diagnostic(path, 6);
}

void test_thread_value_without_join_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_without_join_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker",
        }
    );

    assert_thread_return_forward_diagnostic(path, 5);
}

void test_concurrency_capture_classification_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_capture_classification_success.or";
    write_concurrency_fixture(
        path,
        "demo.capture",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let cached = url",
            "    let request_task = task",
            "        cached",
            "    return await request_task",
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        sum(data)",
            "    return worker.join()",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 2);
    assert(analysis.concurrency_captures[0].line == 5);
    assert(analysis.concurrency_captures[0].name == "cached");
    assert(analysis.concurrency_captures[0].type_name == "Text");
    assert(analysis.concurrency_captures[0].expression_kind ==
           orison::semantics::ConcurrencyExpressionKind::task);
    assert(analysis.concurrency_captures[0].capture_kind ==
           orison::semantics::ConcurrencyCaptureKind::immutable_outer_local);
    assert(analysis.concurrency_captures[1].line == 9);
    assert(analysis.concurrency_captures[1].name == "data");
    assert(analysis.concurrency_captures[1].type_name == "shared.View<Int64>");
    assert(analysis.concurrency_captures[1].expression_kind ==
           orison::semantics::ConcurrencyExpressionKind::thread);
    assert(analysis.concurrency_captures[1].capture_kind ==
           orison::semantics::ConcurrencyCaptureKind::parameter);
}

void test_thread_capture_owned_parameter_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_owned_parameter_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 4);
    assert(analysis.entries().front().message ==
           "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis");
    assert(analysis.concurrency_captures.size() == 1);
    assert_concurrency_capture(
        analysis,
        0,
        "buffer",
        "Buffer",
        orison::semantics::ConcurrencyExpressionKind::thread,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_transferable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_transferable_generic_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>(item: T) -> Int64",
            "where T: Transferable",
            "    let worker = thread",
            "        process(item)",
            "    return worker.join()",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert_concurrency_capture(
        analysis,
        0,
        "item",
        "T",
        orison::semantics::ConcurrencyExpressionKind::thread,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_unconstrained_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_unconstrained_generic_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>(item: T) -> Int64",
            "    let worker = thread",
            "        process(item)",
            "    return worker.join()",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        4,
        "thread capture 'item' of type 'T' requires future Transferable analysis"
    );
}

void test_thread_capture_transferable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_transferable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Transferable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert_concurrency_capture(
        analysis,
        0,
        "buffer",
        "Buffer",
        orison::semantics::ConcurrencyExpressionKind::thread,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_shareable_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_shareable_generic_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>(item: T) -> Int64",
            "where T: Shareable",
            "    let worker = thread",
            "        process(item)",
            "    return worker.join()",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        5,
        "thread capture 'item' of type 'T' requires future Transferable analysis"
    );
}

void test_task_capture_shareable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_shareable_generic_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function launch<T>(item: T) -> T",
            "where T: Shareable",
            "    let worker = task",
            "        item",
            "    return await worker",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert_concurrency_capture(
        analysis,
        0,
        "item",
        "T",
        orison::semantics::ConcurrencyExpressionKind::task,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_task_capture_shareable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_shareable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "async function launch_processing(buffer: Buffer) -> Buffer",
            "    let worker = task",
            "        buffer",
            "    return await worker",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert_concurrency_capture(
        analysis,
        0,
        "buffer",
        "Buffer",
        orison::semantics::ConcurrencyExpressionKind::task,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_shareable_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_shareable_concrete_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );

    assert_fixture_single_diagnostic(
        path,
        7,
        "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis"
    );
}

void test_thread_result_owned_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_owned_concrete_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch_processing() -> Int64",
            "    let worker = thread",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_thread_result_transferable_diagnostic(path, 5, "Buffer");
}

void test_thread_result_transferable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_transferable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Transferable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing() -> Int64",
            "    let worker = thread",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_fixture_success(path);
}

void test_thread_result_shareable_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_shareable_generic_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>() -> Int64",
            "where T: Shareable",
            "    let worker = thread",
            "        let produced: T = build()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_thread_result_transferable_diagnostic(path, 6, "T");
}

void test_task_result_shareable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_result_shareable_generic_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function launch<T>() -> T",
            "where T: Shareable",
            "    let worker = task",
            "        let produced: T = build()",
            "        produced",
            "    return await worker",
        }
    );

    assert_fixture_success(path);
}

void test_task_result_shareable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_result_shareable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "async function launch_processing() -> Buffer",
            "    let worker = task",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return await worker",
        }
    );

    assert_fixture_success(path);
}

void test_thread_result_shareable_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_shareable_concrete_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing() -> Int64",
            "    let worker = thread",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_thread_result_transferable_diagnostic(path, 8, "Buffer");
}

void test_task_expression_value_boundary_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_value_boundary_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        let response = request(url)",
            "    return await request_task",
        }
    );

    assert_concurrency_value_boundary_diagnostic(path, 4, "task");
}

void test_task_expression_value_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_value_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        return request(url)",
            "    return await request_task",
        }
    );

    assert_fixture_success(path);
}

void test_thread_expression_value_boundary_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_boundary_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        let total = sum(data)",
            "    return worker.join()",
        }
    );

    assert_concurrency_value_boundary_diagnostic(path, 4, "thread");
}

void test_thread_expression_value_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        return sum(data)",
            "    return worker.join()",
        }
    );

    assert_fixture_success(path);
}

void test_task_capture_mutable_outer_local_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_capture_mutable_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    var attempts = 0",
            "    let request_task = task",
            "        attempts",
            "    return await request_task",
        }
    );

    assert_mutable_capture_diagnostic(path, 5, "attempts");
}

void test_thread_capture_mutable_outer_local_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_capture_mutable_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    var total = 0",
            "    let worker = thread",
            "        total",
            "    return worker.join()",
        }
    );

    assert_mutable_capture_diagnostic(path, 5, "total");
}

void test_thread_capture_receiver_this_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_capture_this_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "extend Worker",
            "    function spawn(this: shared This) -> Int64",
            "        let worker = thread",
            "            this.id",
            "        return worker.join()",
        }
    );

    assert_receiver_capture_diagnostic(path, 5);
}

}  // namespace

int main() {
    test_await_inside_async_function_success();
    test_await_async_call_value_success();
    test_await_outside_async_function_failure();
    test_await_outside_async_method_failure();
    test_await_plain_value_failure();
    test_await_thread_value_failure();
    test_await_non_async_call_value_failure();
    test_await_member_call_not_marked_async_from_top_level_name_collision_failure();
    test_task_inside_async_function_success();
    test_return_task_value_failure();
    test_return_async_call_value_failure();
    test_return_thread_value_failure();
    test_assignment_preserves_async_call_origin_success();
    test_assignment_preserves_thread_origin_failure();
    test_ternary_preserves_async_call_origin_success();
    test_ternary_preserves_thread_origin_failure();
    test_return_ternary_async_origin_failure();
    test_if_branch_preserves_async_call_origin_success();
    test_if_branch_preserves_thread_origin_failure();
    test_switch_branch_preserves_async_call_origin_success();
    test_switch_branch_preserves_thread_origin_failure();
    test_while_loop_preserves_async_call_origin_success();
    test_while_loop_preserves_thread_origin_failure();
    test_repeat_loop_preserves_async_call_origin_success();
    test_repeat_loop_preserves_thread_origin_failure();
    test_for_loop_preserves_async_call_origin_success();
    test_for_loop_preserves_thread_origin_failure();
    test_guard_failure_path_does_not_override_async_origin_success();
    test_guard_failure_path_does_not_create_async_origin_failure();
    test_switch_constructor_pattern_binds_case_local_names_success();
    test_switch_top_level_name_pattern_rejects_unknown_variant_failure();
    test_switch_call_pattern_rejects_unknown_variant_failure();
    test_switch_unknown_constructor_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_nested_constructor_pattern_binds_nested_names_success();
    test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_success();
    test_switch_rejects_nested_payload_constructor_overlap_failure();
    test_switch_rejects_nested_literal_payload_constructor_overlap_failure();
    test_switch_accepts_disjoint_nested_literal_payload_constructor_patterns_success();
    test_switch_rejects_nested_wildcard_literal_payload_constructor_overlap_failure();
    test_switch_rejects_nested_literal_wildcard_payload_constructor_overlap_failure();
    test_switch_rejects_nested_multi_payload_constructor_overlap_failure();
    test_switch_accepts_disjoint_nested_multi_payload_constructor_patterns_success();
    test_switch_accepts_mismatched_nested_constructor_patterns_success();
    test_switch_rejects_duplicate_nested_zero_payload_constructor_failure();
    test_switch_rejects_duplicate_nested_zero_payload_constructor_no_cascade_failure();
    test_switch_rejects_deep_nested_payload_constructor_overlap_failure();
    test_switch_accepts_disjoint_deep_nested_literal_payload_constructor_patterns_success();
    test_switch_rejects_deep_nested_wildcard_literal_payload_constructor_overlap_failure();
    test_switch_rejects_deep_nested_literal_wildcard_payload_constructor_overlap_failure();
    test_switch_accepts_mismatched_deep_nested_zero_payload_constructor_patterns_success();
    test_switch_rejects_duplicate_deep_nested_zero_payload_constructor_failure();
    test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_failure();
    test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_success();
    test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_failure();
    test_switch_constructor_pattern_rejects_variant_from_different_choice_failure();
    test_switch_wrong_choice_constructor_without_default_does_not_cascade_failure();
    test_switch_constructor_pattern_uses_subject_specific_arity_success();
    test_switch_nested_constructor_pattern_rejects_variant_from_different_payload_choice_failure();
    test_switch_nested_constructor_pattern_uses_payload_specific_arity_success();
    test_switch_nested_constructor_pattern_rejects_invalid_payload_shape_failure();
    test_switch_constructor_payload_shape_without_default_does_not_cascade_failure();
    test_switch_constructor_pattern_rejects_duplicate_binding_names_failure();
    test_switch_constructor_duplicate_binding_without_default_does_not_cascade_failure();
    test_switch_nested_constructor_pattern_rejects_duplicate_binding_names_failure();
    test_switch_nested_constructor_duplicate_binding_without_default_does_not_cascade_failure();
    test_switch_constructor_pattern_rejects_missing_payload_values_failure();
    test_switch_constructor_pattern_rejects_extra_payload_values_failure();
    test_switch_constructor_pattern_arity_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_zero_payload_constructor_arity_without_default_does_not_cascade_failure();
    test_switch_rejects_constructor_then_value_pattern_mix_failure();
    test_switch_rejects_value_then_constructor_pattern_mix_failure();
    test_switch_pattern_mix_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_rejects_mismatched_value_pattern_type_failure();
    test_switch_accepts_same_width_integer_cast_value_pattern_success();
    test_switch_rejects_duplicate_boolean_value_pattern_failure();
    test_switch_duplicate_bool_without_default_does_not_cascade_to_missing_pattern_failure();
    test_switch_rejects_duplicate_string_value_pattern_failure();
    test_switch_rejects_duplicate_integer_cast_value_pattern_failure();
    test_switch_rejects_redundant_bool_default_failure();
    test_switch_duplicate_bool_suppresses_redundant_default_failure();
    test_switch_accepts_exhaustive_bool_without_default_success();
    test_switch_rejects_missing_bool_value_pattern_failure();
    test_switch_rejects_redundant_zero_payload_choice_default_failure();
    test_switch_duplicate_zero_payload_choice_suppresses_redundant_default_failure();
    test_switch_accepts_exhaustive_zero_payload_choice_without_default_success();
    test_switch_rejects_redundant_payload_choice_default_after_full_cover_failure();
    test_switch_accepts_exhaustive_payload_choice_without_default_success();
    test_switch_accepts_reversed_exhaustive_payload_choice_without_default_success();
    test_switch_accepts_literal_payload_choice_arm_with_default_success();
    test_switch_rejects_literal_payload_choice_arm_without_default_failure();
    test_switch_rejects_reversed_literal_payload_choice_arm_without_default_failure();
    test_switch_accepts_nested_payload_choice_arm_with_default_success();
    test_switch_rejects_nested_payload_choice_arm_without_default_failure();
    test_switch_accepts_partial_multi_payload_choice_arm_with_default_success();
    test_switch_rejects_partial_multi_payload_choice_arm_without_default_failure();
    test_switch_rejects_missing_payload_choice_variant_without_default_failure();
    test_switch_accepts_exhaustive_multi_payload_choice_without_default_success();
    test_switch_rejects_redundant_multi_payload_choice_default_failure();
    test_switch_duplicate_payload_choice_suppresses_redundant_default_failure();
    test_switch_rejects_first_missing_multi_payload_choice_variant_failure();
    test_switch_rejects_second_missing_multi_payload_choice_variant_failure();
    test_switch_duplicate_multi_payload_choice_without_default_does_not_cascade_failure();
    test_switch_duplicate_payload_choice_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_rejects_duplicate_zero_payload_choice_constructor_failure();
    test_switch_duplicate_choice_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_rejects_duplicate_name_only_payload_choice_constructor_failure();
    test_switch_duplicate_payload_choice_constructor_does_not_cascade_to_binding_failure();
    test_switch_rejects_duplicate_literal_payload_choice_constructor_failure();
    test_switch_rejects_equivalent_integer_literal_payload_choice_constructor_failure();
    test_switch_rejects_wildcard_then_literal_payload_choice_constructor_failure();
    test_switch_rejects_literal_then_wildcard_payload_choice_constructor_failure();
    test_switch_rejects_multi_payload_partial_overlap_choice_constructor_failure();
    test_switch_accepts_multi_payload_disjoint_literal_choice_constructor_success();
    test_switch_accepts_multi_payload_disjoint_leading_literal_choice_constructor_success();
    test_switch_rejects_missing_zero_payload_choice_variant_failure();
    test_switch_rejects_multiple_default_cases_semantically();
    test_switch_rejects_nonfinal_default_case_semantically();
    test_switch_nonfinal_default_suppresses_branch_analysis_failure();
    test_switch_multiple_default_suppresses_branch_analysis_failure();
    test_break_outside_loop_failure();
    test_continue_outside_loop_failure();
    test_break_inside_loop_success();
    test_continue_inside_loop_success();
    test_this_outside_method_failure();
    test_receiver_parameter_outside_method_failure();
    test_qualified_this_type_in_ordinary_function_signature_failure();
    test_qualified_this_type_in_local_annotation_failure();
    test_receiver_parameter_with_nonself_type_inside_method_failure();
    test_unsafe_intrinsic_outside_unsafe_context_failure();
    test_unsafe_intrinsic_inside_unsafe_function_success();
    test_unsafe_intrinsic_inside_unsafe_block_success();
    test_address_of_nonstorage_operand_failure();
    test_raw_read_nonaddress_operand_failure();
    test_raw_offset_nonaddress_base_failure();
    test_raw_offset_noninteger_offset_failure();
    test_volatile_read_nonaddress_operand_failure();
    test_nested_address_of_and_raw_offset_success();
    test_index_access_noninteger_index_failure();
    test_index_access_integer_index_success();
    test_call_unsafe_function_outside_unsafe_context_failure();
    test_call_unsafe_function_inside_unsafe_block_success();
    test_pointer_construction_outside_unsafe_context_failure();
    test_pointer_construction_inside_unsafe_block_success();
    test_pointer_construction_without_argument_failure();
    test_pointer_construction_with_multiple_arguments_failure();
    test_pointer_construction_with_nonaddress_argument_failure();
    test_pointer_construction_with_address_of_argument_success();
    test_pointer_typed_binding_with_mismatched_address_of_source_failure();
    test_pointer_typed_binding_with_matching_address_of_source_success();
    test_pointer_return_with_same_width_address_of_source_success();
    test_pointer_typed_binding_with_nonpointer_initializer_failure();
    test_pointer_typed_binding_with_pointer_initializer_success();
    test_pointer_typed_binding_with_mismatched_raw_offset_source_failure();
    test_pointer_typed_binding_with_matching_raw_offset_source_success();
    test_pointer_return_with_same_width_raw_offset_source_success();
    test_raw_read_typed_binding_result_mismatch_failure();
    test_raw_read_typed_binding_result_match_success();
    test_raw_read_typed_binding_same_width_integer_success();
    test_raw_read_typed_binding_pointer_sized_integer_mismatch_failure();
    test_pointer_typed_binding_with_wrong_typed_name_failure();
    test_pointer_typed_binding_with_mismatched_field_pointer_failure();
    test_pointer_typed_binding_with_same_width_field_pointer_success();
    test_pointer_return_with_nonpointer_expression_failure();
    test_pointer_return_with_pointer_expression_success();
    test_pointer_return_with_mismatched_raw_offset_source_failure();
    test_pointer_return_with_matching_raw_offset_source_success();
    test_pointer_return_with_wrong_typed_name_failure();
    test_pointer_return_with_mismatched_helper_pointer_failure();
    test_pointer_return_with_same_width_helper_pointer_success();
    test_pointer_return_with_mismatched_address_of_source_failure();
    test_pointer_return_with_matching_address_of_source_success();
    test_raw_write_generic_helper_returned_pointer_same_width_success();
    test_raw_write_generic_helper_returned_pointer_mismatch_failure();
    test_raw_write_generic_receiver_method_pointer_same_width_success();
    test_raw_write_generic_receiver_method_pointer_mismatch_failure();
    test_raw_read_return_type_mismatch_failure();
    test_raw_read_return_type_match_success();
    test_raw_read_return_same_width_integer_success();
    test_raw_read_return_pointer_sized_integer_mismatch_failure();
    test_raw_write_value_type_mismatch_failure();
    test_raw_write_value_type_match_success();
    test_raw_write_same_width_integer_value_success();
    test_raw_write_pointer_sized_integer_value_mismatch_failure();
    test_raw_write_computed_integer_sum_success();
    test_raw_write_computed_bitwise_value_success();
    test_raw_write_computed_ternary_pointer_sized_mismatch_failure();
    test_raw_write_rebound_computed_value_success();
    test_raw_write_branch_merged_computed_value_success();
    test_raw_write_branch_merged_pointer_sized_value_mismatch_failure();
    test_raw_write_switch_merged_computed_value_success();
    test_raw_write_switch_merged_pointer_sized_value_mismatch_failure();
    test_raw_write_array_indexed_value_success();
    test_raw_write_bound_array_literal_indexed_value_success();
    test_raw_write_array_indexed_pointer_sized_value_mismatch_failure();
    test_raw_write_member_container_field_indexed_value_success();
    test_raw_write_nested_scalar_field_value_success();
    test_raw_write_nested_scalar_field_computed_value_success();
    test_raw_write_helper_returned_container_indexed_value_success();
    test_raw_write_nested_scalar_field_pointer_sized_mismatch_failure();
    test_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure();
    test_raw_write_integer_literal_value_success();
    test_raw_write_integer_cast_value_success();
    test_raw_write_same_width_integer_cast_success();
    test_raw_write_integer_cast_target_mismatch_failure();
    test_raw_write_pointer_sized_integer_cast_mismatch_failure();
    test_raw_write_recovered_raw_read_value_type_mismatch_failure();
    test_raw_write_recovered_raw_read_integer_cast_success();
    test_raw_write_helper_returned_raw_read_value_type_mismatch_failure();
    test_raw_write_member_returned_raw_read_value_type_mismatch_failure();
    test_raw_write_member_returned_raw_read_integer_cast_success();
    test_raw_write_non_integer_cast_value_mismatch_failure();
    test_raw_write_helper_pointer_constructor_type_mismatch_failure();
    test_raw_write_helper_pointer_constructor_type_match_success();
    test_raw_write_member_helper_pointer_constructor_type_mismatch_failure();
    test_raw_write_member_helper_pointer_constructor_type_match_success();
    test_raw_write_raw_offset_helper_pointer_type_mismatch_failure();
    test_raw_write_raw_offset_helper_pointer_type_match_success();
    test_raw_write_member_raw_offset_helper_pointer_type_mismatch_failure();
    test_raw_write_record_pointer_field_type_mismatch_failure();
    test_member_field_address_inference_enables_pointer_constructor_success();
    test_raw_write_indexed_record_pointer_field_type_mismatch_failure();
    test_indexed_member_field_address_inference_enables_pointer_constructor_success();
    test_rebound_indexed_record_pointer_field_type_mismatch_failure();
    test_rebound_indexed_member_field_address_inference_enables_pointer_constructor_success();
    test_return_rebound_indexed_record_pointer_field_success();
    test_return_rebound_indexed_member_field_address_success();
    test_return_rebound_indexed_pointer_used_by_helper_success();
    test_return_rebound_indexed_address_used_by_pointer_constructor_success();
    test_volatile_read_return_type_mismatch_failure();
    test_volatile_read_return_type_match_success();
    test_volatile_read_return_same_width_integer_success();
    test_volatile_read_return_pointer_sized_integer_mismatch_failure();
    test_volatile_read_typed_binding_result_mismatch_failure();
    test_volatile_read_typed_binding_result_match_success();
    test_volatile_read_typed_binding_same_width_integer_success();
    test_volatile_read_typed_binding_pointer_sized_integer_mismatch_failure();
    test_volatile_write_value_type_mismatch_failure();
    test_volatile_write_value_type_match_success();
    test_volatile_write_same_width_integer_value_success();
    test_volatile_write_pointer_sized_integer_value_mismatch_failure();
    test_volatile_write_computed_integer_sum_success();
    test_volatile_write_computed_bitwise_value_success();
    test_volatile_write_computed_ternary_pointer_sized_mismatch_failure();
    test_volatile_write_rebound_computed_value_success();
    test_volatile_write_branch_merged_computed_value_success();
    test_volatile_write_branch_merged_pointer_sized_value_mismatch_failure();
    test_volatile_write_switch_merged_computed_value_success();
    test_volatile_write_switch_merged_pointer_sized_value_mismatch_failure();
    test_volatile_write_array_indexed_value_success();
    test_volatile_write_bound_array_literal_indexed_value_success();
    test_volatile_write_array_indexed_pointer_sized_value_mismatch_failure();
    test_volatile_write_member_container_field_indexed_value_success();
    test_volatile_write_nested_scalar_field_value_success();
    test_volatile_write_nested_scalar_field_computed_value_success();
    test_volatile_write_helper_returned_container_indexed_value_success();
    test_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure();
    test_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure();
    test_volatile_write_integer_literal_value_success();
    test_volatile_write_integer_cast_value_success();
    test_volatile_write_same_width_integer_cast_success();
    test_volatile_write_integer_cast_target_mismatch_failure();
    test_volatile_write_pointer_sized_integer_cast_mismatch_failure();
    test_volatile_write_recovered_volatile_read_value_type_mismatch_failure();
    test_volatile_write_recovered_volatile_read_integer_cast_success();
    test_volatile_write_helper_returned_volatile_read_value_type_mismatch_failure();
    test_volatile_write_member_returned_volatile_read_value_type_mismatch_failure();
    test_volatile_write_member_returned_volatile_read_integer_cast_success();
    test_volatile_write_non_integer_cast_value_mismatch_failure();
    test_volatile_write_helper_pointer_constructor_type_mismatch_failure();
    test_volatile_write_member_helper_pointer_constructor_type_mismatch_failure();
    test_volatile_write_raw_offset_helper_pointer_type_mismatch_failure();
    test_address_typed_binding_with_nonaddress_initializer_failure();
    test_address_typed_binding_with_address_initializer_success();
    test_address_typed_binding_with_wrong_typed_name_failure();
    test_address_return_with_nonaddress_expression_failure();
    test_address_return_with_address_expression_success();
    test_address_return_with_wrong_typed_name_failure();
    test_address_typed_binding_with_field_address_success();
    test_address_typed_binding_with_indexed_address_success();
    test_address_return_with_helper_returned_address_success();
    test_address_return_with_generic_helper_success();
    test_raw_write_generic_record_pointer_field_same_width_success();
    test_raw_write_generic_record_pointer_field_mismatch_failure();
    test_raw_write_generic_record_scalar_field_same_width_success();
    test_address_return_with_generic_record_field_success();
    test_task_outside_async_function_failure();
    test_thread_outside_async_function_success();
    test_thread_join_receiver_success();
    test_join_non_thread_receiver_failure();
    test_join_async_call_receiver_failure();
    test_thread_value_without_join_failure();
    test_concurrency_capture_classification_success();
    test_thread_capture_owned_parameter_type_failure();
    test_thread_capture_transferable_generic_success();
    test_thread_capture_unconstrained_generic_failure();
    test_thread_capture_transferable_concrete_type_success();
    test_thread_capture_shareable_generic_failure();
    test_task_capture_shareable_generic_success();
    test_task_capture_shareable_concrete_type_success();
    test_thread_capture_shareable_concrete_type_failure();
    test_thread_result_owned_concrete_type_failure();
    test_thread_result_transferable_concrete_type_success();
    test_thread_result_shareable_generic_failure();
    test_task_result_shareable_generic_success();
    test_task_result_shareable_concrete_type_success();
    test_thread_result_shareable_concrete_type_failure();
    test_task_expression_value_boundary_failure();
    test_task_expression_value_return_success();
    test_thread_expression_value_boundary_failure();
    test_thread_expression_value_return_success();
    test_task_capture_mutable_outer_local_failure();
    test_thread_capture_mutable_outer_local_failure();
    test_thread_capture_receiver_this_failure();
    return 0;
}
