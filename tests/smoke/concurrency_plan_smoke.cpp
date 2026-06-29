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
#include <memory>

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_concurrency_plan_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.concurrency\n"
                  "\n"
                  "record Payload\n"
                  "    public value: Int64\n"
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
    assert(task_plan->thunk_symbol_name == "__orison_task_thunk.compute.7.0");
    assert(task_plan->cleanup_symbol_name == "__orison_task_cleanup.compute.7.0");
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
    assert(task_plan->cleanup.drop_candidates.empty());
    assert(task_plan->cleanup.drop_cleanup.cleanup_symbol_name == "__orison_task_cleanup.compute.7.0");
    assert(task_plan->cleanup.drop_cleanup.actions.empty());
    assert(!task_plan->cleanup.drop_cleanup.emit_drop_calls);
    auto empty_drop_cleanup_report =
        orison::lowering::format_concurrency_drop_cleanup_plan(task_plan->cleanup.drop_cleanup);
    assert(empty_drop_cleanup_report.size() == 1);
    assert(
        empty_drop_cleanup_report.front() ==
        "drop cleanup plan __orison_task_cleanup.compute.7.0 actions 0 drop calls disabled (metadata only)"
    );
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
    assert(thread_plan->thunk_symbol_name == "__orison_thread_thunk.on_thread.13.0");
    assert(thread_plan->cleanup_symbol_name == "__orison_thread_cleanup.on_thread.13.0");
    assert(thread_plan->result_type.type == "i64");
    assert(thread_plan->captures.size() == 1);
    assert(thread_plan->captures.front().name == "value");
    assert(thread_plan->captures.front().capture_kind == orison::semantics::ConcurrencyCaptureKind::parameter);
    assert(thread_plan->captures.front().field_index == 0);
    assert(thread_plan->environment_layout.llvm_type == "{ i64 }");
    assert(thread_plan->environment_layout.size_bytes == 8);
    assert(thread_plan->environment_layout.fields.size() == 1);
    assert(thread_plan->cleanup.drop_candidates.empty());
    assert(thread_plan->cleanup.drop_cleanup.cleanup_symbol_name == "__orison_thread_cleanup.on_thread.13.0");
    assert(thread_plan->cleanup.drop_cleanup.actions.empty());
    assert(!thread_plan->cleanup.drop_cleanup.emit_drop_calls);
    assert(thread_plan->result_storage.llvm_type == "i64");
    assert(thread_plan->result_storage.size_bytes == 8);

    auto record_thread_expression = orison::syntax::ExpressionSyntax {};
    record_thread_expression.kind = orison::syntax::ExpressionKind::thread;
    record_thread_expression.line = 20;
    auto record_thread_result = std::make_unique<orison::syntax::StatementSyntax>();
    record_thread_result->kind = orison::syntax::StatementKind::expression_statement;
    record_thread_result->line = 21;
    record_thread_result->expression.kind = orison::syntax::ExpressionKind::integer_literal;
    record_thread_result->expression.line = 21;
    record_thread_result->expression.text = "1";
    record_thread_expression.nested_statements.push_back(std::move(record_thread_result));

    auto record_semantics = orison::semantics::SemanticAnalysisResult {};
    record_semantics.concurrency_captures.push_back(orison::semantics::ConcurrencyCapture {
        .line = 21,
        .name = "payload",
        .type_name = "Payload",
        .expression_kind = orison::semantics::ConcurrencyExpressionKind::thread,
        .capture_kind = orison::semantics::ConcurrencyCaptureKind::immutable_outer_local,
    });

    auto record_plan = orison::lowering::plan_concurrency_expression(
        record_thread_expression,
        "record_worker",
        2,
        context,
        state,
        record_semantics
    );
    assert(record_plan.has_value());
    assert(record_plan->captures.size() == 1);
    assert(record_plan->captures.front().name == "payload");
    assert(record_plan->captures.front().source_type_name == "Payload");
    assert(record_plan->captures.front().llvm_type == "%record.Payload");
    assert(record_plan->environment_layout.llvm_type == "{ %record.Payload }");
    assert(record_plan->environment_layout.size_bytes == 8);
    assert(record_plan->cleanup.drop_candidates.size() == 1);
    assert(record_plan->cleanup.drop_candidates.front().name == "payload");
    assert(record_plan->cleanup.drop_candidates.front().source_type_name == "Payload");
    assert(record_plan->cleanup.drop_candidates.front().llvm_type == "%record.Payload");
    assert(record_plan->cleanup.drop_candidates.front().drop_symbol_name == "__orison_drop.Payload");
    assert(record_plan->cleanup.drop_candidates.front().field_index == 0);
    assert(
        record_plan->cleanup.drop_candidates.front().capture_kind ==
        orison::semantics::ConcurrencyCaptureKind::immutable_outer_local
    );
    assert(record_plan->cleanup.drop_cleanup.cleanup_symbol_name == "__orison_thread_cleanup.record_worker.20.2");
    assert(record_plan->cleanup.drop_cleanup.actions.size() == 1);
    assert(record_plan->cleanup.drop_cleanup.actions.front().capture_name == "payload");
    assert(record_plan->cleanup.drop_cleanup.actions.front().source_type_name == "Payload");
    assert(record_plan->cleanup.drop_cleanup.actions.front().symbol_name == "__orison_drop.Payload");
    assert(record_plan->cleanup.drop_cleanup.actions.front().field_index == 0);
    assert(record_plan->cleanup.drop_cleanup.actions.front().discovery_line == 20);
    assert(!record_plan->cleanup.drop_cleanup.emit_drop_calls);
    auto record_drop_cleanup_report =
        orison::lowering::format_concurrency_drop_cleanup_plan(record_plan->cleanup.drop_cleanup);
    assert(record_drop_cleanup_report.size() == 2);
    assert(
        record_drop_cleanup_report[0] ==
        "drop cleanup plan __orison_thread_cleanup.record_worker.20.2 actions 1 "
        "drop calls disabled (metadata only)"
    );
    assert(
        record_drop_cleanup_report[1] ==
        "planned drop action __orison_drop.Payload for capture payload: Payload field 0 "
        "discovered at line 20 (metadata only)"
    );

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
