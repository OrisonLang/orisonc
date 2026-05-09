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

auto analyze_orison_fixture(std::filesystem::path const& path) -> orison::semantics::SemanticAnalysisResult {
    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

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

void assert_fixture_single_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view expected_message
) {
    assert_single_diagnostic(analyze_orison_fixture(path), expected_line, expected_message);
}

void test_await_inside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_async_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return await request_task\n";
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

void test_await_async_call_value_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_async_call_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let pending = request(url)\n";
        output << "    return await pending\n";
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

void test_await_outside_async_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_sync_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return await request(url)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "await expression is only valid inside async functions");
}

void test_await_outside_async_method_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_method_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(id: Int64) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(id)\n";
        output << "extend Worker\n";
        output << "    function poll(this: shared This) -> Outcome<Text, IOError>\n";
        output << "        return await request(this.id)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 6);
    assert(diagnostics.entries().front().message == "await expression is only valid inside async functions");
}

void test_await_plain_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_plain_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch() -> Int64\n";
        output << "    let count = 1\n";
        output << "    return await count\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "await expression currently requires a task value or declared async call result");
}

void test_await_thread_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_thread_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        1\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_await_non_async_call_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_non_async_call_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let pending = request(url)\n";
        output << "    return await pending\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 6);
    assert(diagnostics.entries().front().message ==
           "await expression currently requires a task value or declared async call result");
}

void test_await_member_call_not_marked_async_from_top_level_name_collision_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_await_member_name_collision_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function run(text: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(text)\n";
        output << "extend Printer\n";
        output << "    function run(this: shared This) -> Outcome<Text, IOError>\n";
        output << "        return render(this)\n";
        output << "async function fetch(printer: Printer) -> Outcome<Text, IOError>\n";
        output << "    let pending = printer.run()\n";
        output << "    return await pending\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 9);
    assert(diagnostics.entries().front().message ==
           "await expression currently requires a task value or declared async call result");
}

void test_task_inside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_async_success.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return await request_task\n";
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

void test_return_task_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_task_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return request_task\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "return cannot forward task or async-call values; use await instead");
}

void test_return_async_call_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_async_call_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let pending = request(url)\n";
        output << "    return pending\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 6);
    assert(diagnostics.entries().front().message ==
           "return cannot forward task or async-call values; use await instead");
}

void test_return_thread_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_thread_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function parallel_sum() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        1\n";
        output << "    return worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "return cannot forward thread values; use .join() instead");
}

void test_assignment_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_assignment_preserves_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    pending = request(url)\n";
        output << "    return await pending\n";
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

void test_assignment_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_assignment_preserves_thread_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "async function fetch() -> Int64\n";
        output << "    var worker = thread\n";
        output << "        1\n";
        output << "    worker = thread\n";
        output << "        2\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_ternary_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_ternary_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    let left = request(url)\n";
        output << "    let right = request(url)\n";
        output << "    let pending = flag ? left : right\n";
        output << "    return await pending\n";
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

void test_ternary_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_ternary_thread_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    let left = thread\n";
        output << "        1\n";
        output << "    let right = thread\n";
        output << "        2\n";
        output << "    let worker = flag ? left : right\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_return_ternary_async_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_ternary_async_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    let left = request(url)\n";
        output << "    let right = request(url)\n";
        output << "    return flag ? left : right\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "return cannot forward task or async-call values; use await instead");
}

void test_if_branch_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_if_branch_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    if flag\n";
        output << "        pending = request(url)\n";
        output << "    else\n";
        output << "        pending = request(url)\n";
        output << "    return await pending\n";
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

void test_if_branch_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_if_branch_thread_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    var worker = thread\n";
        output << "        1\n";
        output << "    if flag\n";
        output << "        worker = thread\n";
        output << "            2\n";
        output << "    else\n";
        output << "        worker = thread\n";
        output << "            3\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 11);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_switch_branch_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_branch_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    switch flag\n";
        output << "        true => pending = request(url)\n";
        output << "        false => pending = request(url)\n";
        output << "    return await pending\n";
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

void test_switch_branch_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_branch_thread_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    var worker = thread\n";
        output << "        1\n";
        output << "    switch flag\n";
        output << "        true => worker = thread\n";
        output << "            2\n";
        output << "        false => worker = thread\n";
        output << "            3\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 10);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_while_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_while_loop_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    while flag\n";
        output << "        pending = request(url)\n";
        output << "    return await pending\n";
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

void test_while_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_while_loop_thread_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    var worker = thread\n";
        output << "        1\n";
        output << "    while flag\n";
        output << "        worker = thread\n";
        output << "            2\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_repeat_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_repeat_loop_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = 0\n";
        output << "    repeat\n";
        output << "        pending = request(url)\n";
        output << "    while flag\n";
        output << "    return await pending\n";
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

void test_repeat_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_repeat_loop_thread_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch(flag: Bool) -> Int64\n";
        output << "    var worker = 0\n";
        output << "    repeat\n";
        output << "        worker = thread\n";
        output << "            2\n";
        output << "    while flag\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_for_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_for_loop_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(items: shared View<Int64>, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    for item in items\n";
        output << "        pending = request(url)\n";
        output << "    return await pending\n";
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

void test_for_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_for_loop_thread_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function fetch(items: shared View<Int64>) -> Int64\n";
        output << "    var worker = thread\n";
        output << "        1\n";
        output << "    for item in items\n";
        output << "        worker = thread\n";
        output << "            2\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "await cannot be used with thread values; use .join() instead");
}

void test_guard_failure_path_does_not_override_async_origin_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_guard_failure_path_async_origin_success.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = request(url)\n";
        output << "    guard flag else\n";
        output << "        pending = thread\n";
        output << "            2\n";
        output << "        return await request(url)\n";
        output << "    return await pending\n";
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

void test_guard_failure_path_does_not_create_async_origin_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_guard_failure_path_async_origin_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>\n";
        output << "    var pending = 0\n";
        output << "    guard flag else\n";
        output << "        pending = request(url)\n";
        output << "        return await request(url)\n";
        output << "    return await pending\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 9);
    assert(diagnostics.entries().front().message ==
           "await expression currently requires a task value or declared async call result");
}

void test_switch_constructor_pattern_binds_case_local_names_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_binding_success.or";
    {
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

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(!diagnostics.has_errors());
    assert(diagnostics.concurrency_captures.size() == 1);
    assert(diagnostics.concurrency_captures.front().name == "head");
    assert(
        diagnostics.concurrency_captures.front().capture_kind ==
        orison::semantics::ConcurrencyCaptureKind::immutable_outer_local
    );
}

void test_switch_top_level_name_pattern_rejects_unknown_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_name_pattern_binding_failure.or";
    {
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

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern 'head' does not match any declared choice variant");
}

void test_switch_call_pattern_rejects_unknown_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_call_pattern_unknown_variant_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "async function read(value: Int64) -> Int64\n";
        output << "    switch value\n";
        output << "        Missing(head) => 0\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern 'Missing' does not match any declared choice variant");
}

void test_switch_unknown_constructor_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_unknown_constructor_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    Some(value: T)\n";
        output << "    Empty\n";
        output << "function read(item: Maybe<Int64>) -> Int64\n";
        output << "    switch item\n";
        output << "        Missing(value) => value\n";
    }

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern 'Missing' does not match any declared choice variant"
    );
}

void test_switch_nested_constructor_pattern_binds_nested_names_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_constructor_pattern_success.or";
    {
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

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(!diagnostics.has_errors());
    assert(diagnostics.concurrency_captures.size() == 1);
    assert(diagnostics.concurrency_captures.front().name == "next");
}

void test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrapped_payload_success.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "unsafe function write_next(xs: List<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(next, tail)) => raw_write(out, next)\n";
        output << "        default => return\n";
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
}

void test_switch_accepts_mismatched_nested_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_mismatched_nested_constructor_success.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(value)) => 1", "Wrap(Empty) => 2"});

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "unsafe function write_next(xs: List<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(next, tail)) => raw_write(out, next)\n";
        output << "        default => return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_generic_constructor_payload_success.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    None\n";
        output << "    Some(value: T)\n";
        output << "unsafe function write_word(maybe: Maybe<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch maybe\n";
        output << "        Some(value) => raw_write(out, value)\n";
        output << "        default => return\n";
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

void test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_generic_constructor_payload_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    None\n";
        output << "    Some(value: T)\n";
        output << "unsafe function write_word(maybe: Maybe<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    switch maybe\n";
        output << "        Some(value) => raw_write(out, value)\n";
        output << "        default => return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_switch_constructor_pattern_rejects_variant_from_different_choice_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_wrong_choice_variant_failure.or";
    {
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
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 10);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'");
}

void test_switch_wrong_choice_constructor_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_wrong_choice_constructor_without_default_no_cascade_failure.or";
    {
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
    }

    assert_fixture_single_diagnostic(
        path,
        10,
        "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'"
    );
}

void test_switch_constructor_pattern_uses_subject_specific_arity_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_subject_specific_arity_success.or";
    {
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

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(!diagnostics.has_errors());
}

void test_switch_nested_constructor_pattern_rejects_variant_from_different_payload_choice_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrong_payload_choice_failure.or";
    {
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

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 12);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern 'Some' does not belong to switched choice type 'Result<Int64>'");
}

void test_switch_nested_constructor_pattern_uses_payload_specific_arity_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_specific_arity_success.or";
    {
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

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(!diagnostics.has_errors());
}

void test_switch_nested_constructor_pattern_rejects_invalid_payload_shape_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_constructor_pattern_shape_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head + 1, tail) => 0\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern");
}

void test_switch_constructor_payload_shape_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_payload_shape_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head + 1, tail) => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert_single_diagnostic(
        diagnostics,
        7,
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
    );
}

void test_switch_constructor_pattern_rejects_duplicate_binding_names_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_duplicate_binding_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, head) => 0\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_constructor_duplicate_binding_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, head) => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert_single_diagnostic(diagnostics, 7, "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_nested_constructor_pattern_rejects_duplicate_binding_names_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_switch_nested_constructor_pattern_duplicate_binding_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(head, tail)) => 0\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_nested_constructor_duplicate_binding_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head, Node(head, tail)) => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert_single_diagnostic(diagnostics, 7, "switch constructor pattern cannot bind 'head' more than once");
}

void test_switch_constructor_pattern_rejects_missing_payload_values_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_arity_missing_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head) => 0\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern 'Node' expects 2 payload values but received 1");
}

void test_switch_constructor_pattern_rejects_extra_payload_values_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_arity_extra_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "async function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Empty(value) => 0\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "switch constructor pattern 'Empty' expects 0 payload values but received 1");
}

void test_switch_constructor_pattern_arity_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_pattern_arity_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Node(head) => 0\n";
    }

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
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Empty(value) => 0\n";
    }

    assert_fixture_single_diagnostic(
        path,
        7,
        "switch constructor pattern 'Empty' expects 0 payload values but received 1"
    );
}

void test_switch_rejects_constructor_then_value_pattern_mix_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_pattern_mix_constructor_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
        output << "function sum(xs: List<Int64>) -> Int64\n";
        output << "    switch xs\n";
        output << "        Empty => 0\n";
        output << "        1 => 1\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "switch cannot mix value patterns with constructor patterns");
}

void test_switch_rejects_value_then_constructor_pattern_mix_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_pattern_mix_value_constructor_failure.or";
    {
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

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "switch cannot mix value patterns with constructor patterns");
}

void test_switch_pattern_mix_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_pattern_mix_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "choice Maybe<T>\n";
        output << "    Some(value: T)\n";
        output << "    Empty\n";
        output << "function classify(item: Maybe<Int64>) -> Int64\n";
        output << "    switch item\n";
        output << "        Some(value) => value\n";
        output << "        1 => 1\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert_single_diagnostic(diagnostics, 8, "switch cannot mix value patterns with constructor patterns");
}

void test_switch_rejects_mismatched_value_pattern_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_value_pattern_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        \"ready\" => 1\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "switch value pattern type 'Text' does not match switched expression type 'Bool'");
}

void test_switch_accepts_same_width_integer_cast_value_pattern_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_value_pattern_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(value: UInt32) -> Int64\n";
        output << "    switch value\n";
        output << "        1 as Int32 => 1\n";
        output << "        default => 0\n";
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

void test_switch_rejects_duplicate_boolean_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_boolean_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        true => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "switch value pattern 'true' is duplicated");
}

void test_switch_duplicate_bool_without_default_does_not_cascade_to_missing_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_bool_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        true => 2\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "switch value pattern 'true' is duplicated");
}

void test_switch_rejects_duplicate_string_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_string_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(state: Text) -> Int64\n";
        output << "    switch state\n";
        output << "        \"ready\" => 1\n";
        output << "        \"ready\" => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "switch value pattern '\"ready\"' is duplicated");
}

void test_switch_rejects_duplicate_integer_cast_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_integer_cast_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(value: UInt32) -> Int64\n";
        output << "    switch value\n";
        output << "        1 => 1\n";
        output << "        1 as Int32 => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "switch value pattern '1 as Int32' is duplicated");
}

void test_switch_rejects_redundant_bool_default_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_bool_default_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
        output << "        default => 2\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 6);
    assert(diagnostics.entries().front().message ==
           "switch default case is redundant after true and false value patterns");
}

void test_switch_duplicate_bool_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_bool_redundant_default_no_cascade.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
        output << "        false => 2\n";
        output << "        default => 3\n";
    }

    assert_fixture_single_diagnostic(path, 6, "switch value pattern 'false' is duplicated");
}

void test_switch_accepts_exhaustive_bool_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_bool_without_default_success.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
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

void test_switch_rejects_missing_bool_value_pattern_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_bool_pattern_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message == "switch is missing boolean value pattern 'false'");
}

void test_switch_rejects_redundant_zero_payload_choice_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_choice_default_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        PermissionDenied => 3\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 11);
    assert(diagnostics.entries().front().message ==
           "switch default case is redundant after all zero-payload choice variants are covered");
}

void test_switch_duplicate_zero_payload_choice_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_zero_payload_choice_redundant_default_no_cascade.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        PermissionDenied => 3\n";
        output << "        Closed => 4\n";
        output << "        default => 0\n";
    }

    assert_fixture_single_diagnostic(path, 11, "switch constructor pattern 'Closed' is duplicated");
}

void test_switch_accepts_exhaustive_zero_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_choice_without_default_success.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        PermissionDenied => 3\n";
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
}

void test_switch_accepts_reversed_exhaustive_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_reversed_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Empty => 0", "Some(value) => value"});

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
}

void test_switch_accepts_literal_payload_choice_arm_with_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_literal_payload_choice_default_success.or";
    write_maybe_int_exhaustiveness_fixture(path, {"Some(1) => 1", "Empty => 0"}, true);

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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

    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
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
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        Closed => 3\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 10);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Closed' is duplicated");
}

void test_switch_duplicate_choice_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_choice_without_default_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
        output << "        Closed => 3\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 10);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Closed' is duplicated");
}

void test_switch_rejects_duplicate_name_only_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_name_only_payload_choice_constructor_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice Maybe<T>\n";
        output << "    Some(value: T)\n";
        output << "    Empty\n";
        output << "function classify(item: Maybe<Int32>) -> Int64\n";
        output << "    switch item\n";
        output << "        Some(value) => 1\n";
        output << "        Some(other) => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Some(...)' is duplicated");
}

void test_switch_duplicate_payload_choice_constructor_does_not_cascade_to_binding_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_payload_choice_constructor_no_cascade_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int32, right: Int32)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(left, right) => 1\n";
        output << "        Both(value, value) => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Both(...)' is duplicated");
}

void test_switch_rejects_duplicate_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_literal_payload_choice_constructor_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(1) => 1\n";
        output << "        Int(1) => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_equivalent_integer_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_equivalent_integer_literal_payload_choice_constructor_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(1) => 1\n";
        output << "        Int(1 as Int64) => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_wildcard_then_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_wildcard_then_literal_payload_choice_constructor_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(value) => 1\n";
        output << "        Int(1) => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_literal_then_wildcard_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_literal_then_wildcard_payload_choice_constructor_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice Number\n";
        output << "    Int(value: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: Number) -> Int64\n";
        output << "    switch item\n";
        output << "        Int(1) => 1\n";
        output << "        Int(value) => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Int(...)' is duplicated");
}

void test_switch_rejects_multi_payload_partial_overlap_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_partial_overlap_choice_constructor_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int64, right: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(left, 1) => 1\n";
        output << "        Both(other, 1) => 2\n";
        output << "        default => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Both(...)' is duplicated");
}

void test_switch_accepts_multi_payload_disjoint_literal_choice_constructor_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_disjoint_literal_choice_constructor_success.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int64, right: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(left, 1) => 1\n";
        output << "        Both(other, 2) => 2\n";
        output << "        default => 0\n";
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

void test_switch_accepts_multi_payload_disjoint_leading_literal_choice_constructor_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_disjoint_leading_literal_choice_constructor_success.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice PairChoice\n";
        output << "    Both(left: Int64, right: Int64)\n";
        output << "    Empty\n";
        output << "function classify(item: PairChoice) -> Int64\n";
        output << "    switch item\n";
        output << "        Both(1, left) => 1\n";
        output << "        Both(2, right) => 2\n";
        output << "        default => 0\n";
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

void test_switch_rejects_missing_zero_payload_choice_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_choice_variant_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "choice IOError\n";
        output << "    Closed\n";
        output << "    EndOfInput\n";
        output << "    PermissionDenied\n";
        output << "function classify(error: IOError) -> Int64\n";
        output << "    switch error\n";
        output << "        Closed => 1\n";
        output << "        EndOfInput => 2\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "switch is missing zero-payload choice variant 'PermissionDenied'");
}

void test_switch_rejects_multiple_default_cases_semantically() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_multiple_default_semantic_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    auto& switch_statement = parse_result.module.functions.front().body_statements.front();
    assert(switch_statement.kind == orison::syntax::StatementKind::switch_statement);
    switch_statement.switch_cases.front().is_default = true;
    switch_statement.switch_cases.back().is_default = true;

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message == "switch default case must be the final case");
    assert(diagnostics.entries().back().line == 5);
    assert(diagnostics.entries().back().message == "switch statement may only contain one default case");
}

void test_switch_rejects_nonfinal_default_case_semantically() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_nonfinal_default_semantic_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    auto& switch_statement = parse_result.module.functions.front().body_statements.front();
    assert(switch_statement.kind == orison::syntax::StatementKind::switch_statement);
    switch_statement.switch_cases.front().is_default = true;

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message == "switch default case must be the final case");
}

void test_switch_nonfinal_default_suppresses_branch_analysis_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nonfinal_default_branch_no_cascade.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        default => await flag\n";
        output << "        true => 1\n";
    }

    assert_fixture_single_diagnostic(path, 4, "switch default case must be the final case");
}

void test_switch_multiple_default_suppresses_branch_analysis_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_multiple_default_branch_no_cascade.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(flag: Bool) -> Int64\n";
        output << "    switch flag\n";
        output << "        true => 1\n";
        output << "        false => await flag\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    auto& switch_statement = parse_result.module.functions.front().body_statements.front();
    assert(switch_statement.kind == orison::syntax::StatementKind::switch_statement);
    switch_statement.switch_cases.front().is_default = true;
    switch_statement.switch_cases.back().is_default = true;

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message == "switch default case must be the final case");
    assert(diagnostics.entries().back().line == 5);
    assert(diagnostics.entries().back().message == "switch statement may only contain one default case");
}

void test_break_outside_loop_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_break_outside_loop_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.loops\n";
        output << "function stop() -> Unit\n";
        output << "    break\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message == "break statement is only valid inside loops");
}

void test_continue_outside_loop_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_continue_outside_loop_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.loops\n";
        output << "function keep_going() -> Unit\n";
        output << "    continue\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message == "continue statement is only valid inside loops");
}

void test_break_inside_loop_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_break_inside_loop_success.or";
    {
        std::ofstream output(path);
        output << "package demo.loops\n";
        output << "function stop(flag: Bool) -> Unit\n";
        output << "    while flag\n";
        output << "        break\n";
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

void test_continue_inside_loop_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_continue_inside_loop_success.or";
    {
        std::ofstream output(path);
        output << "package demo.loops\n";
        output << "function scan(items: shared View<Int64>) -> Unit\n";
        output << "    for item in items\n";
        output << "        continue\n";
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

void test_this_outside_method_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_this_outside_method_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.receiver\n";
        output << "function current() -> Int64\n";
        output << "    return this\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "receiver 'this' is only valid inside implements or extend methods");
}

void test_receiver_parameter_outside_method_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_receiver_parameter_outside_method_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.receiver\n";
        output << "function current(this: Int64) -> Int64\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 2);
    assert(diagnostics.entries().front().message ==
           "receiver parameter 'this' is only valid in implements or extend methods");
}

void test_qualified_this_type_in_ordinary_function_signature_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_this_type_signature_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.receiver\n";
        output << "function project(value: shared This) -> shared This\n";
        output << "    return value\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 2);
    assert(diagnostics.entries().front().message ==
           "This type is only valid inside interface, implements, or extend methods");
    assert(diagnostics.entries().back().line == 2);
    assert(diagnostics.entries().back().message ==
           "This type is only valid inside interface, implements, or extend methods");
}

void test_qualified_this_type_in_local_annotation_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_this_type_local_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.receiver\n";
        output << "function cache() -> Unit\n";
        output << "    let current: exclusive This = unit\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "This type is only valid inside interface, implements, or extend methods");
}

void test_receiver_parameter_with_nonself_type_inside_method_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_receiver_parameter_nonself_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.receiver\n";
        output << "extend Buffer\n";
        output << "    function reset(this: Int64) -> Unit\n";
        output << "        return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "receiver parameter 'this' must use This, shared This, or exclusive This");
}

void test_unsafe_intrinsic_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_outside_unsafe_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function read_byte(p: Address) -> Byte\n";
        output << "    return raw_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "unsafe intrinsic 'raw_read' is only valid inside unsafe functions or unsafe blocks");
}

void test_unsafe_intrinsic_inside_unsafe_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_inside_unsafe_function.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(p: Address) -> Byte\n";
        output << "    return raw_read(p)\n";
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

void test_unsafe_intrinsic_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_inside_unsafe_block.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function zero_byte(p: Address) -> Unit\n";
        output << "    unsafe\n";
        output << "        raw_write(p, 0)\n";
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

void test_address_of_nonstorage_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_address_of_nonstorage_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function pointer() -> Address\n";
        output << "    return address_of(1)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "address_of currently requires an addressable storage operand");
}

void test_raw_read_nonaddress_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_read_nonaddress_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word() -> UInt32\n";
        output << "    return raw_read(1)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_read currently requires an address-like first argument");
}

void test_raw_offset_nonaddress_base_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_offset_nonaddress_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function advance() -> Address\n";
        output << "    return raw_offset(1, 2)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_offset currently requires an address-like first argument");
}

void test_raw_offset_noninteger_offset_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_offset_noninteger_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function advance(base: Address) -> Address\n";
        output << "    return raw_offset(base, \"one\")\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_offset currently requires an integer offset argument");
}

void test_volatile_read_nonaddress_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_nonaddress_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function uart_ready() -> UInt32\n";
        output << "    return volatile_read(1)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_read currently requires an address-like first argument");
}

void test_nested_address_of_and_raw_offset_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_nested_address_of_raw_offset_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function poke(buf: exclusive Buffer, value: Byte) -> Unit\n";
        output << "    let p = address_of(buf.data[0])\n";
        output << "    raw_write(raw_offset(p, 1), value)\n";
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

void test_index_access_noninteger_index_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_index_access_noninteger_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_byte(device: Device, value: Byte) -> Unit\n";
        output << "    raw_write(device.ptrs[\"one\"], value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "index access currently requires an integer index expression");
}

void test_index_access_integer_index_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_index_access_integer_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_byte(device: Device, index: UInt32, value: Byte) -> Unit\n";
        output << "    raw_write(device.ptrs[index], value)\n";
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

void test_call_unsafe_function_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_call_unsafe_function_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Address) -> UInt32\n";
        output << "    return raw_read(p)\n";
        output << "function read_twice(p: Address) -> UInt32\n";
        output << "    return read_word(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "call to unsafe function 'read_word' is only valid inside unsafe functions or unsafe blocks");
}

void test_call_unsafe_function_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_call_unsafe_function_block_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Address) -> UInt32\n";
        output << "    return raw_read(p)\n";
        output << "function copy_word(p: Address) -> UInt32\n";
        output << "    unsafe\n";
        output << "        return read_word(p)\n";
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

void test_pointer_construction_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function read_byte(addr: Address) -> Byte\n";
        output << "    let p = Pointer(addr)\n";
        output << "    return raw_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "Pointer construction is only valid inside unsafe functions or unsafe blocks");
    assert(diagnostics.entries().back().line == 4);
    assert(diagnostics.entries().back().message ==
           "unsafe intrinsic 'raw_read' is only valid inside unsafe functions or unsafe blocks");
}

void test_pointer_construction_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_block_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function scribble(addr: Address) -> Unit\n";
        output << "    unsafe\n";
        output << "        let p = Pointer(addr)\n";
        output << "        raw_write(p, 0)\n";
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

void test_pointer_construction_without_argument_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_noarg_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let p = Pointer()\n";
        output << "    return raw_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "Pointer construction currently requires exactly one source argument");
}

void test_pointer_construction_with_multiple_arguments_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_multiarg_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(addr: Address) -> Byte\n";
        output << "    let p = Pointer(addr, addr)\n";
        output << "    return raw_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "Pointer construction currently requires exactly one source argument");
}

void test_pointer_construction_with_nonaddress_argument_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_nonaddress_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let p = Pointer(\"text\")\n";
        output << "    return raw_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "Pointer construction currently requires an address-like source argument");
}

void test_pointer_construction_with_address_of_argument_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_addressof_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function first_ptr(buf: exclusive Buffer) -> Pointer<Byte>\n";
        output << "    let p = Pointer(address_of(buf.data[0]))\n";
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

void test_pointer_typed_binding_with_mismatched_address_of_source_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_addressof_source_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>\n";
        output << "    let p: Pointer<UInt32> = Pointer(address_of(buf.data[0]))\n";
        output << "    return p\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'");
}

void test_pointer_typed_binding_with_matching_address_of_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_addressof_source_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function first_byte_ptr(buf: Buffer) -> Pointer<Byte>\n";
        output << "    let p: Pointer<Byte> = Pointer(address_of(buf.data[0]))\n";
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

void test_pointer_return_with_same_width_address_of_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_addressof_source_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function status_ptr(device: Device) -> Pointer<UInt32>\n";
        output << "    return Pointer(address_of(device.registers.status))\n";
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

void test_pointer_typed_binding_with_nonpointer_initializer_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let p: Pointer<Byte> = \"text\"\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "pointer-typed binding initializer currently requires a structurally pointer-like expression");
}

void test_pointer_typed_binding_with_pointer_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_byte(base: Pointer<Byte>) -> Byte\n";
        output << "    let p: Pointer<Byte> = raw_offset(base, 1)\n";
        output << "    return raw_read(p)\n";
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

void test_pointer_typed_binding_with_mismatched_raw_offset_source_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_rawoffset_source_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>\n";
        output << "    let p: Pointer<UInt32> = raw_offset(base, 1)\n";
        output << "    return p\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'");
}

void test_pointer_typed_binding_with_matching_raw_offset_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_rawoffset_source_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    let p: Pointer<Byte> = raw_offset(base, 1)\n";
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

void test_pointer_return_with_same_width_raw_offset_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_rawoffset_source_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>\n";
        output << "    return raw_offset(base, 1)\n";
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

void test_raw_read_typed_binding_result_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_typed_binding_mismatch.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> UInt32\n";
        output << "    let value: UInt32 = raw_read(p)\n";
        output << "    return value\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_read result type 'Byte' does not match binding type 'UInt32'");
}

void test_raw_read_typed_binding_result_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_typed_binding_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(p: Pointer<Byte>) -> Byte\n";
        output << "    let value: Byte = raw_read(p)\n";
        output << "    return value\n";
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

void test_raw_read_typed_binding_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_typed_binding_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> Int32\n";
        output << "    let value: UInt32 = raw_read(p)\n";
        output << "    return value as Int32\n";
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

void test_raw_read_typed_binding_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_typed_binding_pointer_sized_mismatch.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> IntSize\n";
        output << "    let value: IntSize = raw_read(p)\n";
        output << "    return value\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_read result type 'Byte' does not match binding type 'IntSize'");
}

void test_pointer_typed_binding_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_name_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte() -> Byte\n";
        output << "    let source = \"text\"\n";
        output << "    let p: Pointer<Byte> = source\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "pointer-typed binding initializer currently requires a structurally pointer-like expression");
}

void test_pointer_typed_binding_with_mismatched_field_pointer_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_field_pointer_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptr: Pointer<Byte>\n";
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
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "pointer-typed binding initializer pointer element type 'Byte' does not match expected pointer element type 'UInt32'");
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
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_ptr() -> Pointer<Byte>\n";
        output << "    return \"text\"\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "pointer-returning function currently requires a structurally pointer-like expression");
}

void test_pointer_return_with_pointer_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_offset(base, 1)\n";
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

void test_pointer_return_with_mismatched_raw_offset_source_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_rawoffset_source_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>\n";
        output << "    return raw_offset(base, 1)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'");
}

void test_pointer_return_with_matching_raw_offset_source_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_rawoffset_source_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_offset(base, 1)\n";
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

void test_pointer_return_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_name_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_ptr() -> Pointer<Byte>\n";
        output << "    let source = \"text\"\n";
        output << "    return source\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "pointer-returning function currently requires a structurally pointer-like expression");
}

void test_pointer_return_with_mismatched_helper_pointer_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_helper_pointer_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return base\n";
        output << "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>\n";
        output << "    return byte_ptr(base)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "pointer-returning function pointer element type 'Byte' does not match expected pointer element type 'UInt32'");
}

void test_pointer_return_with_same_width_helper_pointer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_helper_pointer_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function wordish_ptr(base: Pointer<Int32>) -> Pointer<Int32>\n";
        output << "    return base\n";
        output << "unsafe function word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>\n";
        output << "    return wordish_ptr(base)\n";
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

void test_raw_write_generic_helper_returned_pointer_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_helper_returned_pointer_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>\n";
        output << "    return base\n";
        output << "unsafe function write_word(base: Pointer<Int32>, value: UInt32) -> Unit\n";
        output << "    raw_write(id_ptr(base), value)\n";
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

void test_raw_write_generic_helper_returned_pointer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_helper_returned_pointer_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>\n";
        output << "    return base\n";
        output << "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(id_ptr(base), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_address_return_with_generic_helper_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_with_generic_helper_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function id<T>(value: T) -> T\n";
        output << "    return value\n";
        output << "record Device\n";
        output << "    base: Address\n";
        output << "function read_base(device: Device) -> Address\n";
        output << "    return id(device.base)\n";
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

void test_raw_write_generic_receiver_method_pointer_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_receiver_method_pointer_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    id: Int64\n";
        output << "extend Device<T>\n";
        output << "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>\n";
        output << "        return base\n";
        output << "unsafe function write_word(device: Device<Int32>, base: Pointer<Int32>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr(base), value)\n";
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

void test_raw_write_generic_receiver_method_pointer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_receiver_method_pointer_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    id: Int64\n";
        output << "extend Device<T>\n";
        output << "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>\n";
        output << "        return base\n";
        output << "unsafe function write_word(device: Device<Byte>, base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr(base), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_raw_write_generic_record_pointer_field_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_pointer_field_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    ptr: Pointer<T>\n";
        output << "unsafe function write_word(device: Device<Int32>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr, value)\n";
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

void test_raw_write_generic_record_pointer_field_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_pointer_field_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device<T>\n";
        output << "    ptr: Pointer<T>\n";
        output << "unsafe function write_word(device: Device<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr, value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_raw_write_generic_record_scalar_field_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_scalar_field_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Box<T>\n";
        output << "    value: T\n";
        output << "unsafe function write_word(box: Box<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, box.value)\n";
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

void test_address_return_with_generic_record_field_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_with_generic_record_field_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Box<T>\n";
        output << "    value: T\n";
        output << "function read_base(box: Box<Address>) -> Address\n";
        output << "    return box.value\n";
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

void test_pointer_return_with_mismatched_address_of_source_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_addressof_source_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>\n";
        output << "    return Pointer(address_of(buf.data[0]))\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'");
}

void test_pointer_return_with_matching_address_of_source_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_addressof_source_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function first_byte_ptr(buf: Buffer) -> Pointer<Byte>\n";
        output << "    return Pointer(address_of(buf.data[0]))\n";
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

void test_raw_read_return_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_read result type 'Byte' does not match function return type 'Pointer<Byte>'");
}

void test_raw_read_return_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_type_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(p: Pointer<Byte>) -> Byte\n";
        output << "    return raw_read(p)\n";
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

void test_raw_read_return_same_width_integer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> UInt32\n";
        output << "    return raw_read(p)\n";
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

void test_raw_read_return_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_return_pointer_sized_mismatch.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> IntSize\n";
        output << "    return raw_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_read result type 'Byte' does not match function return type 'IntSize'");
}

void test_raw_write_value_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_raw_write_value_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_value_type_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: UInt32) -> Unit\n";
        output << "    raw_write(p, value)\n";
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

void test_raw_write_same_width_integer_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_same_width_integer_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    raw_write(p, value)\n";
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

void test_raw_write_pointer_sized_integer_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_pointer_sized_integer_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit\n";
        output << "    raw_write(p, value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_computed_integer_sum_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_computed_integer_sum_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(input) + 1)\n";
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

void test_raw_write_computed_bitwise_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_computed_bitwise_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    raw_write(out, value bit_or 1)\n";
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

void test_raw_write_computed_ternary_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_computed_ternary_pointer_sized_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    raw_write(out, flag ? left : right)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_rebound_computed_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_rebound_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    let masked = value bit_or 1\n";
        output << "    raw_write(out, masked)\n";
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

void test_raw_write_branch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_branch_merged_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left bit_or 1\n";
        output << "    else\n";
        output << "        selected = right + 1\n";
        output << "    raw_write(out, selected)\n";
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

void test_raw_write_branch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_branch_merged_pointer_sized_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left + 1\n";
        output << "    else\n";
        output << "        selected = right shift_left 1\n";
        output << "    raw_write(out, selected)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_switch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_switch_merged_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left bit_or 1\n";
        output << "        default => selected = right + 1\n";
        output << "    raw_write(out, selected)\n";
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

void test_raw_write_switch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_switch_merged_pointer_sized_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left + 1\n";
        output << "        default => selected = right shift_left 1\n";
        output << "    raw_write(out, selected)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_array_indexed_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_array_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit\n";
        output << "    raw_write(out, items[0])\n";
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

void test_raw_write_bound_array_literal_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_bound_array_literal_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit\n";
        output << "    let staged = [left, right]\n";
        output << "    raw_write(out, staged[0])\n";
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

void test_raw_write_array_indexed_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_array_indexed_pointer_sized_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit\n";
        output << "    raw_write(out, items[0])\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_helper_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_helper_returned_container_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function words() -> DynamicArray<Int32>\n";
        output << "    return []\n";
        output << "unsafe function write_word(out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, words()[0])\n";
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

void test_raw_write_member_container_field_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_container_field_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<Int32>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.words[0])\n";
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

void test_raw_write_nested_scalar_field_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_nested_scalar_field_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.registers.status)\n";
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

void test_raw_write_nested_scalar_field_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_nested_scalar_field_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.registers.status bit_or 1)\n";
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

void test_raw_write_nested_scalar_field_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_nested_scalar_field_pointer_sized_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: IntSize\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.registers.status)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_method_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_method_returned_container_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<Int32>\n";
        output << "extend Device\n";
        output << "    function words_view(this: shared This) -> DynamicArray<Int32>\n";
        output << "        this.words\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.words_view()[0])\n";
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

void test_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<IntSize>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    raw_write(out, device.words[0])\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_integer_literal_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_integer_literal_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(p, 0)\n";
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

void test_raw_write_integer_cast_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_integer_cast_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as UInt32)\n";
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

void test_raw_write_integer_cast_target_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_integer_cast_target_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as Int64)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Int64' does not match pointer element type 'UInt32'");
}

void test_raw_write_same_width_integer_cast_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_same_width_integer_cast_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as Int32)\n";
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

void test_raw_write_pointer_sized_integer_cast_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_pointer_sized_integer_cast_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    raw_write(p, value as IntSize)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_raw_write_recovered_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_recovered_raw_read_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_raw_write_recovered_raw_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_recovered_raw_read_integer_cast_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)) as Int32)\n";
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

void test_raw_write_helper_returned_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_helper_returned_raw_read_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_offset(base, 1)\n";
        output << "unsafe function write_word(base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(raw_offset(byte_ptr(base), 1)))\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_raw_write_member_returned_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_returned_raw_read_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(raw_offset(device.byte_ptr(base), 1)))\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 9);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_raw_write_member_returned_raw_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_returned_raw_read_integer_cast_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    raw_write(out, raw_read(raw_offset(device.byte_ptr(base), 1)) as UInt32)\n";
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

void test_raw_write_non_integer_cast_value_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_non_integer_cast_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Bool) -> Unit\n";
        output << "    raw_write(p, value as Bool)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'Bool' does not match pointer element type 'UInt32'");
}

void test_raw_write_helper_pointer_constructor_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_helper_pointer_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>\n";
        output << "    return Pointer(addr)\n";
        output << "unsafe function write_word(addr: Address, value: UInt32) -> Unit\n";
        output << "    raw_write(byte_ptr(addr), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_raw_write_helper_pointer_constructor_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_helper_pointer_type_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>\n";
        output << "    return Pointer(addr)\n";
        output << "unsafe function write_byte(addr: Address, value: Byte) -> Unit\n";
        output << "    raw_write(byte_ptr(addr), value)\n";
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

void test_raw_write_member_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_helper_pointer_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, addr: Address) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(addr)\n";
        output << "unsafe function write_word(device: Device, addr: Address, value: UInt32) -> Unit\n";
        output << "    raw_write(device.byte_ptr(addr), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 9);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_raw_write_member_helper_pointer_constructor_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_helper_pointer_type_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, addr: Address) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(addr)\n";
        output << "unsafe function write_byte(device: Device, addr: Address, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(addr), value)\n";
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

void test_raw_write_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_raw_offset_helper_pointer_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_offset(base, 1)\n";
        output << "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(next_byte_ptr(base), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_raw_write_raw_offset_helper_pointer_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_raw_offset_helper_pointer_type_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_offset(base, 1)\n";
        output << "unsafe function write_byte(base: Pointer<Byte>, value: Byte) -> Unit\n";
        output << "    raw_write(next_byte_ptr(base), value)\n";
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

void test_raw_write_member_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_raw_offset_helper_pointer_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function next_byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, value: UInt32) -> Unit\n";
        output << "    raw_write(device.next_byte_ptr(base), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 9);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_raw_write_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_record_pointer_field_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptr: Pointer<Byte>\n";
        output << "unsafe function write_word(device: Device, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptr, value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_member_field_address_pointer_constructor_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    base: Address\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.base)\n";
        output << "unsafe function write_byte(device: Device, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(), value)\n";
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

void test_raw_write_indexed_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_indexed_record_pointer_field_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit\n";
        output << "    raw_write(device.ptrs[index], value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_indexed_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_indexed_member_field_address_pointer_constructor_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.bases[index])\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
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

void test_rebound_indexed_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_rebound_indexed_record_pointer_field_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit\n";
        output << "    let p = device.ptrs[index]\n";
        output << "    raw_write(p, value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 6);
    assert(diagnostics.entries().front().message ==
           "raw_write value type 'UInt32' does not match pointer element type 'Byte'");
}

void test_rebound_indexed_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_rebound_indexed_member_field_address_pointer_constructor_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            let base = this.bases[index]\n";
        output << "            return Pointer(base)\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
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

void test_return_rebound_indexed_record_pointer_field_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_record_pointer_field_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>\n";
        output << "    let p = device.ptrs[index]\n";
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

void test_return_rebound_indexed_member_field_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_member_field_address_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function base_at(this: shared This, index: Int64) -> Address\n";
        output << "        let base = this.bases[index]\n";
        output << "        return base\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.base_at(index))\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
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

void test_return_rebound_indexed_pointer_used_by_helper_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_pointer_used_by_helper_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    ptrs: Pointer<Pointer<Byte>>\n";
        output << "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>\n";
        output << "    let p = device.ptrs[index]\n";
        output << "    return p\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(byte_ptr(device, index), value)\n";
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

void test_return_rebound_indexed_address_used_by_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_address_pointer_constructor_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function base_at(this: shared This, index: Int64) -> Address\n";
        output << "        let base = this.bases[index]\n";
        output << "        return base\n";
        output << "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return Pointer(this.base_at(index))\n";
        output << "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit\n";
        output << "    raw_write(device.byte_ptr(index), value)\n";
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

void test_volatile_read_return_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_return_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return volatile_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_read result type 'Byte' does not match function return type 'Pointer<Byte>'");
}

void test_volatile_read_return_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_return_type_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(p: Pointer<Byte>) -> Byte\n";
        output << "    return volatile_read(p)\n";
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

void test_volatile_read_return_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_return_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> UInt32\n";
        output << "    return volatile_read(p)\n";
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

void test_volatile_read_return_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_return_pointer_sized_mismatch.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> IntSize\n";
        output << "    return volatile_read(p)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_read result type 'Byte' does not match function return type 'IntSize'");
}

void test_volatile_read_typed_binding_result_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_typed_binding_mismatch.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> UInt32\n";
        output << "    let value: UInt32 = volatile_read(p)\n";
        output << "    return value\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_read result type 'Byte' does not match binding type 'UInt32'");
}

void test_volatile_read_typed_binding_result_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_typed_binding_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_byte(p: Pointer<Byte>) -> Byte\n";
        output << "    let value: Byte = volatile_read(p)\n";
        output << "    return value\n";
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

void test_volatile_read_typed_binding_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_typed_binding_same_width_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Int32>) -> Int32\n";
        output << "    let value: UInt32 = volatile_read(p)\n";
        output << "    return value as Int32\n";
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

void test_volatile_read_typed_binding_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_typed_binding_pointer_sized_mismatch.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function read_word(p: Pointer<Byte>) -> IntSize\n";
        output << "    let value: IntSize = volatile_read(p)\n";
        output << "    return value\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_read result type 'Byte' does not match binding type 'IntSize'");
}

void test_volatile_write_value_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_volatile_write_value_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_value_type_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: UInt32) -> Unit\n";
        output << "    volatile_write(p, value)\n";
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

void test_volatile_write_same_width_integer_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_same_width_integer_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    volatile_write(p, value)\n";
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

void test_volatile_write_pointer_sized_integer_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_pointer_sized_integer_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit\n";
        output << "    volatile_write(p, value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_computed_integer_sum_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_integer_sum_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(input) + 1)\n";
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

void test_volatile_write_computed_bitwise_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_bitwise_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    volatile_write(out, value bit_or 1)\n";
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

void test_volatile_write_computed_ternary_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_ternary_pointer_sized_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    volatile_write(out, flag ? left : right)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_rebound_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_rebound_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit\n";
        output << "    let masked = value bit_or 1\n";
        output << "    volatile_write(out, masked)\n";
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

void test_volatile_write_branch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_branch_merged_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left bit_or 1\n";
        output << "    else\n";
        output << "        selected = right + 1\n";
        output << "    volatile_write(out, selected)\n";
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

void test_volatile_write_branch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_branch_merged_pointer_sized_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    if flag\n";
        output << "        selected = left + 1\n";
        output << "    else\n";
        output << "        selected = right shift_left 1\n";
        output << "    volatile_write(out, selected)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 8);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_switch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_switch_merged_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left bit_or 1\n";
        output << "        default => selected = right + 1\n";
        output << "    volatile_write(out, selected)\n";
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

void test_volatile_write_switch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_switch_merged_pointer_sized_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit\n";
        output << "    var selected = left\n";
        output << "    switch selector\n";
        output << "        0 => selected = left + 1\n";
        output << "        default => selected = right shift_left 1\n";
        output << "    volatile_write(out, selected)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_array_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_array_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit\n";
        output << "    volatile_write(out, items[0])\n";
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

void test_volatile_write_bound_array_literal_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_bound_array_literal_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit\n";
        output << "    let staged = [left, right]\n";
        output << "    volatile_write(out, staged[0])\n";
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

void test_volatile_write_array_indexed_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_array_indexed_pointer_sized_value_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit\n";
        output << "    volatile_write(out, items[0])\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_helper_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_returned_container_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function words() -> DynamicArray<Int32>\n";
        output << "    return []\n";
        output << "unsafe function write_word(out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, words()[0])\n";
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

void test_volatile_write_member_container_field_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_container_field_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<Int32>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.words[0])\n";
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

void test_volatile_write_nested_scalar_field_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.registers.status)\n";
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

void test_volatile_write_nested_scalar_field_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_computed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: Int32\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.registers.status bit_or 1)\n";
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

void test_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Registers\n";
        output << "    status: IntSize\n";
        output << "record Device\n";
        output << "    registers: Registers\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.registers.status)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 7);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_method_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_method_returned_container_indexed_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<Int32>\n";
        output << "extend Device\n";
        output << "    function words_view(this: shared This) -> DynamicArray<Int32>\n";
        output << "        this.words\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.words_view()[0])\n";
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

void test_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    words: DynamicArray<IntSize>\n";
        output << "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit\n";
        output << "    volatile_write(out, device.words[0])\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_integer_literal_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_integer_literal_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(p, 0)\n";
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

void test_volatile_write_integer_cast_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_integer_cast_value_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as UInt32)\n";
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

void test_volatile_write_integer_cast_target_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_integer_cast_target_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as Int64)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Int64' does not match pointer element type 'UInt32'");
}

void test_volatile_write_same_width_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_same_width_integer_cast_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as Int32)\n";
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

void test_volatile_write_pointer_sized_integer_cast_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_pointer_sized_integer_cast_mismatch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(p, value as IntSize)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'IntSize' does not match pointer element type 'UInt32'");
}

void test_volatile_write_recovered_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_recovered_volatile_read_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_volatile_write_recovered_volatile_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_recovered_volatile_read_integer_cast_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Buffer\n";
        output << "    data: Pointer<Byte>\n";
        output << "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)) as Int32)\n";
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

void test_volatile_write_helper_returned_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_returned_volatile_read_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "    return raw_offset(base, 1)\n";
        output << "unsafe function write_word(base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(raw_offset(word_ptr(base), 1)))\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_volatile_write_member_returned_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_returned_volatile_read_value_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)))\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 9);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_volatile_write_member_returned_volatile_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_returned_volatile_read_integer_cast_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>\n";
        output << "        unsafe\n";
        output << "            return raw_offset(base, 1)\n";
        output << "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit\n";
        output << "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)) as UInt32)\n";
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

void test_volatile_write_non_integer_cast_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_non_integer_cast_value_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function write_word(p: Pointer<UInt32>, value: Bool) -> Unit\n";
        output << "    volatile_write(p, value as Bool)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Bool' does not match pointer element type 'UInt32'");
}

void test_volatile_write_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_pointer_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function word_ptr(addr: Address) -> Pointer<UInt32>\n";
        output << "    return Pointer(addr)\n";
        output << "unsafe function write_word(addr: Address, value: Byte) -> Unit\n";
        output << "    volatile_write(word_ptr(addr), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_volatile_write_member_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_helper_pointer_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    id: Int64\n";
        output << "extend Device\n";
        output << "    function word_ptr(this: shared This, addr: Address) -> Pointer<UInt32>\n";
        output << "        unsafe\n";
        output << "            return Pointer(addr)\n";
        output << "unsafe function write_word(device: Device, addr: Address, value: Byte) -> Unit\n";
        output << "    volatile_write(device.word_ptr(addr), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 9);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_volatile_write_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_raw_offset_helper_pointer_type_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function next_word_ptr(base: Pointer<UInt32>) -> Pointer<UInt32>\n";
        output << "    return raw_offset(base, 1)\n";
        output << "unsafe function write_word(base: Pointer<UInt32>, value: Byte) -> Unit\n";
        output << "    volatile_write(next_word_ptr(base), value)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "volatile_write value type 'Byte' does not match pointer element type 'UInt32'");
}

void test_address_typed_binding_with_nonaddress_initializer_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function read_base() -> Address\n";
        output << "    let base: Address = \"text\"\n";
        output << "    return 0x4000_1000\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "address-typed binding initializer currently requires a structurally address-like expression");
}

void test_address_typed_binding_with_address_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function first_addr(buf: exclusive Buffer) -> Address\n";
        output << "    let base: Address = address_of(buf.data[0])\n";
        output << "    return base\n";
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

void test_address_typed_binding_with_field_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_typed_binding_field_address_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    base: Address\n";
        output << "function read_base(device: Device) -> Address\n";
        output << "    let base: Address = device.base\n";
        output << "    return base\n";
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

void test_address_typed_binding_with_indexed_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_typed_binding_indexed_address_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "function read_base(device: Device, index: Int64) -> Address\n";
        output << "    let base: Address = device.bases[index]\n";
        output << "    return base\n";
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

void test_address_typed_binding_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_name_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function read_base() -> Address\n";
        output << "    let source = \"text\"\n";
        output << "    let base: Address = source\n";
        output << "    return 0x4000_1000\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "address-typed binding initializer currently requires a structurally address-like expression");
}

void test_address_return_with_nonaddress_expression_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function base() -> Address\n";
        output << "    return \"text\"\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message ==
           "address-returning function currently requires a structurally address-like expression");
}

void test_address_return_with_address_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "unsafe function base(buf: exclusive Buffer) -> Address\n";
        output << "    return address_of(buf.data[0])\n";
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

void test_address_return_with_helper_returned_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_helper_returned_address_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "record Device\n";
        output << "    bases: Pointer<Address>\n";
        output << "extend Device\n";
        output << "    function base_at(this: shared This, index: Int64) -> Address\n";
        output << "        let base = this.bases[index]\n";
        output << "        return base\n";
        output << "function read_base(device: Device, index: Int64) -> Address\n";
        output << "    return device.base_at(index)\n";
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

void test_address_return_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_name_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function base() -> Address\n";
        output << "    let source = \"text\"\n";
        output << "    return source\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "address-returning function currently requires a structurally address-like expression");
}

void test_task_outside_async_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_sync_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return request(url)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message == "task expression is only valid inside async functions");
}

void test_thread_outside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_sync_success.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        sum(data)\n";
        output << "    return worker.join()\n";
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

void test_thread_join_receiver_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_join_receiver_success.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function parallel_sum() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        1\n";
        output << "    return worker.join()\n";
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

void test_join_non_thread_receiver_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_join_non_thread_receiver_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "async function fetch() -> Int64\n";
        output << "    let request_task = task\n";
        output << "        1\n";
        output << "    return request_task.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "join() cannot be used with task values; use await instead");
}

void test_join_async_call_receiver_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_join_async_call_receiver_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "async function request(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return fetch_remote(url)\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let pending = request(url)\n";
        output << "    return pending.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 6);
    assert(diagnostics.entries().front().message ==
           "join() cannot be used with declared async call results; use await instead");
}

void test_thread_value_without_join_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_without_join_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function parallel_sum() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        1\n";
        output << "    return worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message == "return cannot forward thread values; use .join() instead");
}

void test_concurrency_capture_classification_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_capture_classification_success.or";
    {
        std::ofstream output(path);
        output << "package demo.capture\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let cached = url\n";
        output << "    let request_task = task\n";
        output << "        cached\n";
        output << "    return await request_task\n";
        output << "function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        sum(data)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
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
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function launch_processing(buffer: Buffer) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(buffer)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 4);
    assert(analysis.entries().front().message ==
           "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis");
    assert(analysis.concurrency_captures.size() == 1);
    assert(analysis.concurrency_captures[0].name == "buffer");
    assert(analysis.concurrency_captures[0].type_name == "Buffer");
    assert(analysis.concurrency_captures[0].capture_kind ==
           orison::semantics::ConcurrencyCaptureKind::parameter);
}

void test_thread_capture_transferable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_transferable_generic_success.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function launch<T>(item: T) -> Int64\n";
        output << "where T: Transferable\n";
        output << "    let worker = thread\n";
        output << "        process(item)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert(analysis.concurrency_captures[0].name == "item");
    assert(analysis.concurrency_captures[0].type_name == "T");
    assert(analysis.concurrency_captures[0].capture_kind ==
           orison::semantics::ConcurrencyCaptureKind::parameter);
}

void test_thread_capture_unconstrained_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_unconstrained_generic_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function launch<T>(item: T) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(item)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 4);
    assert(analysis.entries().front().message ==
           "thread capture 'item' of type 'T' requires future Transferable analysis");
}

void test_thread_capture_transferable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_transferable_concrete_success.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "implements Transferable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "function launch_processing(buffer: Buffer) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(buffer)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert(analysis.concurrency_captures[0].name == "buffer");
    assert(analysis.concurrency_captures[0].type_name == "Buffer");
}

void test_thread_capture_shareable_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_shareable_generic_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function launch<T>(item: T) -> Int64\n";
        output << "where T: Shareable\n";
        output << "    let worker = thread\n";
        output << "        process(item)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 5);
    assert(analysis.entries().front().message ==
           "thread capture 'item' of type 'T' requires future Transferable analysis");
}

void test_task_capture_shareable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_shareable_generic_success.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "async function launch<T>(item: T) -> T\n";
        output << "where T: Shareable\n";
        output << "    let worker = task\n";
        output << "        item\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert(analysis.concurrency_captures[0].name == "item");
    assert(analysis.concurrency_captures[0].type_name == "T");
    assert(analysis.concurrency_captures[0].expression_kind ==
           orison::semantics::ConcurrencyExpressionKind::task);
}

void test_task_capture_shareable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_shareable_concrete_success.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "implements Shareable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "async function launch_processing(buffer: Buffer) -> Buffer\n";
        output << "    let worker = task\n";
        output << "        buffer\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 1);
    assert(analysis.concurrency_captures[0].name == "buffer");
    assert(analysis.concurrency_captures[0].type_name == "Buffer");
    assert(analysis.concurrency_captures[0].expression_kind ==
           orison::semantics::ConcurrencyExpressionKind::task);
}

void test_thread_capture_shareable_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_shareable_concrete_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "implements Shareable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "function launch_processing(buffer: Buffer) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        process(buffer)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 7);
    assert(analysis.entries().front().message ==
           "thread capture 'buffer' of type 'Buffer' requires future Transferable analysis");
}

void test_thread_result_owned_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_owned_concrete_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function launch_processing() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        let produced: Buffer = make_buffer()\n";
        output << "        produced\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 5);
    assert(analysis.entries().front().message ==
           "thread result type 'Buffer' requires future Transferable analysis");
}

void test_thread_result_transferable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_transferable_concrete_success.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "implements Transferable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "function launch_processing() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        let produced: Buffer = make_buffer()\n";
        output << "        produced\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(!analysis.has_errors());
}

void test_thread_result_shareable_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_shareable_generic_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function launch<T>() -> Int64\n";
        output << "where T: Shareable\n";
        output << "    let worker = thread\n";
        output << "        let produced: T = build()\n";
        output << "        produced\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 6);
    assert(analysis.entries().front().message ==
           "thread result type 'T' requires future Transferable analysis");
}

void test_task_result_shareable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_result_shareable_generic_success.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "async function launch<T>() -> T\n";
        output << "where T: Shareable\n";
        output << "    let worker = task\n";
        output << "        let produced: T = build()\n";
        output << "        produced\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(!analysis.has_errors());
}

void test_task_result_shareable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_result_shareable_concrete_success.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "implements Shareable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "async function launch_processing() -> Buffer\n";
        output << "    let worker = task\n";
        output << "        let produced: Buffer = make_buffer()\n";
        output << "        produced\n";
        output << "    return await worker\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(!analysis.has_errors());
}

void test_thread_result_shareable_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_shareable_concrete_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "implements Shareable for Buffer\n";
        output << "    function placeholder(this: shared This) -> Unit\n";
        output << "        return\n";
        output << "function launch_processing() -> Int64\n";
        output << "    let worker = thread\n";
        output << "        let produced: Buffer = make_buffer()\n";
        output << "        produced\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto analysis = analyzer.analyze(parse_result.module);
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == 8);
    assert(analysis.entries().front().message ==
           "thread result type 'Buffer' requires future Transferable analysis");
}

void test_task_expression_value_boundary_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_value_boundary_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        let response = request(url)\n";
        output << "    return await request_task\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "task expression body must end with an expression statement or value return");
}

void test_task_expression_value_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_value_return_success.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        return request(url)\n";
        output << "    return await request_task\n";
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

void test_thread_expression_value_boundary_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_boundary_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        let total = sum(data)\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message ==
           "thread expression body must end with an expression statement or value return");
}

void test_thread_expression_value_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_return_success.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        return sum(data)\n";
        output << "    return worker.join()\n";
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

void test_task_capture_mutable_outer_local_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_capture_mutable_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.task\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    var attempts = 0\n";
        output << "    let request_task = task\n";
        output << "        attempts\n";
        output << "    return await request_task\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "concurrency expression cannot capture mutable outer local 'attempts'");
}

void test_thread_capture_mutable_outer_local_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_capture_mutable_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    var total = 0\n";
        output << "    let worker = thread\n";
        output << "        total\n";
        output << "    return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "concurrency expression cannot capture mutable outer local 'total'");
}

void test_thread_capture_receiver_this_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_capture_this_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.thread\n";
        output << "extend Worker\n";
        output << "    function spawn(this: shared This) -> Int64\n";
        output << "        let worker = thread\n";
        output << "            this.id\n";
        output << "        return worker.join()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto diagnostics = analyzer.analyze(parse_result.module);
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == 5);
    assert(diagnostics.entries().front().message ==
           "concurrency expression cannot capture receiver 'this'");
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
