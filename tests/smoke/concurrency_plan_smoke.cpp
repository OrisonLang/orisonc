#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_concurrency_plan_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.concurrency\n"
                  "\n"
                  "async function compute(value: Int64) -> Int64\n"
                  "    let pending = task\n"
                  "        value + 1\n"
                  "\n"
                  "    await pending\n"
                  "\n"
                  "function on_thread(value: Int64) -> Int64\n"
                  "    let worker = thread\n"
                  "        value + 1\n"
                  "\n"
                  "    worker.join()\n";
    }

    auto source = orison::source::SourceFile::read(path);
    assert(source.has_value());
    auto parser = orison::syntax::ModuleParser {};
    auto parse_result = parser.parse(*source);
    assert(!parse_result.diagnostics.has_errors());

    auto semantic_result = orison::semantics::ModuleSemanticAnalyzer {}.analyze(parse_result.module);
    assert(!semantic_result.has_errors());

    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    auto lowering = orison::lowering::build_lowering_context(parse_result.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = orison::lowering::collect_string_constants(parse_result.module),
    };

    auto state = orison::lowering::FunctionLoweringState {};
    state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i64",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::signed_integer,
    });
    state.source_type_names.emplace("value", "Int64");

    auto const& task_expression = parse_result.module.functions[0].body_statements[0].expression;
    auto task_plan = orison::lowering::plan_concurrency_expression(
        task_expression,
        "compute",
        0,
        context,
        state,
        semantic_result
    );
    assert(task_plan.has_value());
    assert(task_plan->kind == orison::lowering::ConcurrencyPlanKind::task);
    assert(task_plan->spawn_operation == orison::lowering::ConcurrencyRuntimeOperation::spawn_task);
    assert(task_plan->thunk_symbol_name == "__orison_task_thunk.compute.4.0");
    assert(task_plan->result_type.type == "i64");
    assert(task_plan->result_type.signedness == orison::lowering::IntegerSignedness::signed_integer);
    assert(task_plan->captures.size() == 1);
    assert(task_plan->captures.front().name == "value");
    assert(task_plan->captures.front().source_type_name == "Int64");
    assert(task_plan->captures.front().llvm_type == "i64");
    assert(task_plan->captures.front().field_index == 0);
    assert(task_plan->environment_layout.llvm_type == "{ i64 }");
    assert(task_plan->environment_layout.size_bytes == 8);
    assert(task_plan->environment_layout.fields.size() == 1);
    assert(task_plan->environment_layout.fields.front().name == "value");
    assert(task_plan->environment_layout.fields.front().field_index == 0);
    assert(task_plan->result_storage.llvm_type == "i64");
    assert(task_plan->result_storage.size_bytes == 8);

    auto const& thread_expression = parse_result.module.functions[1].body_statements[0].expression;
    auto thread_plan = orison::lowering::plan_concurrency_expression(
        thread_expression,
        "on_thread",
        0,
        context,
        state,
        semantic_result
    );
    assert(thread_plan.has_value());
    assert(thread_plan->kind == orison::lowering::ConcurrencyPlanKind::thread);
    assert(thread_plan->spawn_operation == orison::lowering::ConcurrencyRuntimeOperation::spawn_thread);
    assert(thread_plan->thunk_symbol_name == "__orison_thread_thunk.on_thread.10.0");
    assert(thread_plan->result_type.type == "i64");
    assert(thread_plan->captures.size() == 1);
    assert(thread_plan->captures.front().name == "value");
    assert(thread_plan->captures.front().capture_kind == orison::semantics::ConcurrencyCaptureKind::parameter);
    assert(thread_plan->captures.front().field_index == 0);
    assert(thread_plan->environment_layout.llvm_type == "{ i64 }");
    assert(thread_plan->environment_layout.size_bytes == 8);
    assert(thread_plan->environment_layout.fields.size() == 1);
    assert(thread_plan->result_storage.llvm_type == "i64");
    assert(thread_plan->result_storage.size_bytes == 8);

    auto not_concurrency = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "value",
    };
    assert(!orison::lowering::plan_concurrency_expression(
        not_concurrency,
        "on_thread",
        1,
        context,
        state,
        semantic_result
    ).has_value());
    return 0;
}
