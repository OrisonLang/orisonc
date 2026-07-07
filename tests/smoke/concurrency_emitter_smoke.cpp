#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

auto scalar_capture_plan(std::string llvm_type = "i64") -> orison::lowering::ConcurrencyExpressionPlan {
    return orison::lowering::ConcurrencyExpressionPlan {
        .environment_layout = orison::lowering::ConcurrencyEnvironmentLayout {
            .llvm_type = "{ i64 }",
        },
        .captures = {
            orison::lowering::ConcurrencyCapturePlan {
                .name = "value",
                .source_type_name = "Int64",
                .llvm_type = std::move(llvm_type),
                .field_index = 0,
            },
        },
    };
}

auto test_capture_environment_store_emitter() -> void {
    auto state = orison::lowering::FunctionLoweringState {};
    state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i64",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::signed_integer,
    });
    auto output = std::ostringstream {};
    auto plan = scalar_capture_plan();
    auto result = orison::lowering::emit_concurrency_capture_environment_stores(
        plan,
        "%worker.thread.env",
        state,
        output
    );
    assert(result == orison::lowering::ConcurrencyCaptureStoreEmissionResult::emitted);
    assert(
        output.str() ==
        "  %tmp0 = getelementptr { i64 }, ptr %worker.thread.env, i32 0, i32 0\n"
        "  store i64 %value, ptr %tmp0\n"
    );
    assert(state.next_temporary_index == 1);

    auto unsupported_plan = scalar_capture_plan("");
    auto unsupported_output = std::ostringstream {};
    auto unsupported_result = orison::lowering::emit_concurrency_capture_environment_stores(
        unsupported_plan,
        "%worker.thread.env",
        state,
        unsupported_output
    );
    assert(unsupported_result == orison::lowering::ConcurrencyCaptureStoreEmissionResult::unsupported_capture_type);
    assert(unsupported_output.str().empty());
    assert(state.next_temporary_index == 1);

    auto missing_state = orison::lowering::FunctionLoweringState {};
    auto missing_output = std::ostringstream {};
    auto missing_result = orison::lowering::emit_concurrency_capture_environment_stores(
        plan,
        "%worker.thread.env",
        missing_state,
        missing_output
    );
    assert(missing_result == orison::lowering::ConcurrencyCaptureStoreEmissionResult::missing_capture_source);
    assert(missing_output.str().empty());
    assert(missing_state.next_temporary_index == 0);
}

auto test_result_completion_emitters() -> void {
    auto state = orison::lowering::FunctionLoweringState {};
    auto thread_binding = orison::lowering::ConcurrencyBinding {
        .handle = "%worker",
        .result_storage = "%worker.thread.result",
        .result_type = orison::lowering::LoweredType {
            .type = "i64",
            .signedness = orison::lowering::IntegerSignedness::signed_integer,
        },
    };
    auto thread_output = std::ostringstream {};
    auto thread_failures = orison::lowering::LoweringFailures {};
    auto thread_result = orison::lowering::emit_thread_join_result(
        thread_binding,
        "i64",
        state,
        thread_failures,
        thread_output
    );
    assert(thread_result.has_value());
    assert(
        thread_output.str() ==
        "  call void @__orison_thread_join(ptr %worker)\n"
        "  %tmp0 = load i64, ptr %worker.thread.result\n"
        "  call void @__orison_concurrency_handle_destroy(ptr %worker)\n"
    );
    assert(thread_binding.handle_destroyed);
    assert(thread_result->type == "i64");
    assert(thread_result->value == "%tmp0");
    assert(thread_result->signedness == orison::lowering::IntegerSignedness::signed_integer);
    assert(thread_failures.expression.reason == orison::lowering::ExpressionLoweringFailureReason::none);
    assert(state.next_temporary_index == 1);

    auto mismatched_thread_binding = thread_binding;
    mismatched_thread_binding.handle_destroyed = false;
    auto mismatched_thread_output = std::ostringstream {};
    auto mismatched_thread_failures = orison::lowering::LoweringFailures {};
    auto mismatched_thread_result = orison::lowering::emit_thread_join_result(
        mismatched_thread_binding,
        "i32",
        state,
        mismatched_thread_failures,
        mismatched_thread_output
    );
    assert(!mismatched_thread_result.has_value());
    assert(mismatched_thread_output.str().empty());
    assert(!mismatched_thread_binding.handle_destroyed);
    assert(
        mismatched_thread_failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::call_return_type_mismatch
    );
    assert(mismatched_thread_failures.expression.detail == "thread join returns i64, expected i32");
    assert(state.next_temporary_index == 1);

    auto task_binding = orison::lowering::ConcurrencyBinding {
        .handle = "%pending",
        .result_storage = "%pending.task.result",
        .result_type = orison::lowering::LoweredType {
            .type = "i1",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
    };
    auto task_output = std::ostringstream {};
    auto task_failures = orison::lowering::LoweringFailures {};
    auto task_result = orison::lowering::emit_task_await_result(
        task_binding,
        "i1",
        state,
        task_failures,
        task_output
    );
    assert(task_result.has_value());
    assert(
        task_output.str() ==
        "  call void @__orison_task_await(ptr %pending)\n"
        "  %tmp1 = load i1, ptr %pending.task.result\n"
        "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
    );
    assert(task_binding.handle_destroyed);
    assert(task_result->type == "i1");
    assert(task_result->value == "%tmp1");
    assert(task_result->signedness == orison::lowering::IntegerSignedness::not_integer);
    assert(task_failures.expression.reason == orison::lowering::ExpressionLoweringFailureReason::none);
    assert(state.next_temporary_index == 2);

    auto mismatched_task_binding = task_binding;
    mismatched_task_binding.handle_destroyed = false;
    auto mismatched_task_output = std::ostringstream {};
    auto mismatched_task_failures = orison::lowering::LoweringFailures {};
    auto mismatched_task_result = orison::lowering::emit_task_await_result(
        mismatched_task_binding,
        "i64",
        state,
        mismatched_task_failures,
        mismatched_task_output
    );
    assert(!mismatched_task_result.has_value());
    assert(mismatched_task_output.str().empty());
    assert(!mismatched_task_binding.handle_destroyed);
    assert(
        mismatched_task_failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::call_return_type_mismatch
    );
    assert(mismatched_task_failures.expression.detail == "task await returns i1, expected i64");
    assert(state.next_temporary_index == 2);
}

auto test_abandoned_handle_cleanup() -> void {
    auto state = orison::lowering::FunctionLoweringState {};
    state.thread_bindings.emplace("joined", orison::lowering::ConcurrencyBinding {
        .handle = "%joined",
        .result_storage = "%joined.result",
        .result_type = orison::lowering::LoweredType {
            .type = "i64",
            .signedness = orison::lowering::IntegerSignedness::signed_integer,
        },
        .handle_destroyed = true,
    });
    state.thread_bindings.emplace("worker", orison::lowering::ConcurrencyBinding {
        .handle = "%worker",
        .result_storage = "%worker.result",
        .result_type = orison::lowering::LoweredType {
            .type = "i64",
            .signedness = orison::lowering::IntegerSignedness::signed_integer,
        },
    });
    state.task_bindings.emplace("pending", orison::lowering::ConcurrencyBinding {
        .handle = "%pending",
        .result_storage = "%pending.result",
        .result_type = orison::lowering::LoweredType {
            .type = "i1",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
    });
    state.thread_binding_order = {"joined", "worker"};
    state.task_binding_order = {"missing", "pending"};

    auto failures = orison::lowering::LoweringFailures {};
    auto session = orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };
    auto output = std::ostringstream {};
    orison::lowering::emit_abandoned_concurrency_handle_cleanup(session, output);

    assert(
        output.str() ==
        "  call void @__orison_concurrency_handle_destroy(ptr %worker)\n"
        "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
    );
    assert(state.thread_bindings.at("joined").handle_destroyed);
    assert(state.thread_bindings.at("worker").handle_destroyed);
    assert(state.task_bindings.at("pending").handle_destroyed);
}

}  // namespace

int main() {
    test_capture_environment_store_emitter();
    test_result_completion_emitters();
    test_abandoned_handle_cleanup();

    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_concurrency_emitter_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto path = std::filesystem::temp_directory_path() / "orison_concurrency_emitter_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.concurrency_emitter\n"
                  "\n"
                  "async function compute(value: Int64) -> Int64\n"
                  "    let pending = task\n"
                  "        value + 1\n"
                  "\n"
                  "    await pending\n";
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
        .options = {},
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

    auto parent_failures = orison::lowering::LoweringFailures {};
    auto parent_session = orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = parent_failures,
        .semantics = &semantic_result,
        .enclosing_symbol_name = "compute",
    };
    auto thunk = orison::lowering::emit_concurrency_entry_thunk(
        *task_plan,
        task_expression,
        context,
        parent_session,
        diagnostics
    );
    assert(thunk.has_value());
    assert(
        *thunk ==
        "define private void @__orison_task_thunk.compute.4.0(ptr %environment, ptr %result_storage) {\n"
        "entry:\n"
        "  %tmp0 = getelementptr { i64 }, ptr %environment, i32 0, i32 0\n"
        "  %tmp1 = load i64, ptr %tmp0\n"
        "  %tmp2 = add i64 %tmp1, 1\n"
        "  store i64 %tmp2, ptr %result_storage\n"
        "  ret void\n"
        "}\n"
    );

    auto empty_expression = orison::syntax::ExpressionSyntax {};
    empty_expression.kind = orison::syntax::ExpressionKind::task;
    empty_expression.line = 20;
    auto missing_body_result = orison::lowering::emit_concurrency_entry_thunk(
        *task_plan,
        empty_expression,
        context,
        parent_session,
        diagnostics
    );
    assert(!missing_body_result.has_value());
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().back().message == "lowering does not yet support this task body");

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
