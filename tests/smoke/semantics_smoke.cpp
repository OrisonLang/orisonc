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
           "concurrency capture 'buffer' of type 'Buffer' requires future Transferable/Shareable analysis");
    assert(analysis.concurrency_captures.size() == 1);
    assert(analysis.concurrency_captures[0].name == "buffer");
    assert(analysis.concurrency_captures[0].type_name == "Buffer");
    assert(analysis.concurrency_captures[0].capture_kind ==
           orison::semantics::ConcurrencyCaptureKind::parameter);
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
    test_await_outside_async_function_failure();
    test_await_outside_async_method_failure();
    test_task_inside_async_function_success();
    test_task_outside_async_function_failure();
    test_thread_outside_async_function_success();
    test_concurrency_capture_classification_success();
    test_thread_capture_owned_parameter_type_failure();
    test_task_expression_value_boundary_failure();
    test_task_expression_value_return_success();
    test_thread_expression_value_boundary_failure();
    test_thread_expression_value_return_success();
    test_task_capture_mutable_outer_local_failure();
    test_thread_capture_mutable_outer_local_failure();
    test_thread_capture_receiver_this_failure();
    return 0;
}
