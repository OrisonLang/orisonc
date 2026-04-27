#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {

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

void test_await_outside_async_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_sync_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
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
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message == "await expression is only valid inside async functions");
}

void test_await_outside_async_method_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_method_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
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
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message == "await expression is only valid inside async functions");
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

}  // namespace

int main() {
    test_await_inside_async_function_success();
    test_await_outside_async_function_failure();
    test_await_outside_async_method_failure();
    test_task_inside_async_function_success();
    test_task_outside_async_function_failure();
    test_thread_outside_async_function_success();
    return 0;
}
