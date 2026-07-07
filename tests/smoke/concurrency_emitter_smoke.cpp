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
    auto thread_result = orison::lowering::emit_thread_join_result(
        thread_binding,
        state,
        thread_output
    );
    assert(
        thread_output.str() ==
        "  call void @__orison_thread_join(ptr %worker)\n"
        "  %tmp0 = load i64, ptr %worker.thread.result\n"
        "  call void @__orison_concurrency_handle_destroy(ptr %worker)\n"
    );
    assert(thread_binding.handle_destroyed);
    assert(thread_result.type == "i64");
    assert(thread_result.value == "%tmp0");
    assert(thread_result.signedness == orison::lowering::IntegerSignedness::signed_integer);
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
    auto task_result = orison::lowering::emit_task_await_result(
        task_binding,
        state,
        task_output
    );
    assert(
        task_output.str() ==
        "  call void @__orison_task_await(ptr %pending)\n"
        "  %tmp1 = load i1, ptr %pending.task.result\n"
        "  call void @__orison_concurrency_handle_destroy(ptr %pending)\n"
    );
    assert(task_binding.handle_destroyed);
    assert(task_result.type == "i1");
    assert(task_result.value == "%tmp1");
    assert(task_result.signedness == orison::lowering::IntegerSignedness::not_integer);
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
