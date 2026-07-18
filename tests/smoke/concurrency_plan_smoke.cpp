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
#include <memory>
#include <string>
#include <unistd.h>

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_concurrency_plan_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

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
    assert(!orison::lowering::drop_calls_enabled(task_plan->cleanup.drop_cleanup));
    auto empty_drop_cleanup_report =
        orison::lowering::format_concurrency_drop_cleanup_plan(task_plan->cleanup.drop_cleanup);
    assert(empty_drop_cleanup_report.size() == 1);
    assert(
        empty_drop_cleanup_report.front() ==
        "drop cleanup plan __orison_task_cleanup.compute.7.0 actions 0 descriptor deallocation not-required "
        "drop calls disabled (metadata only)"
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
    assert(!orison::lowering::drop_calls_enabled(thread_plan->cleanup.drop_cleanup));
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
    assert(!orison::lowering::drop_calls_enabled(record_plan->cleanup.drop_cleanup));
    auto authorized_plan = record_plan->cleanup.drop_cleanup;
    assert(!orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(authorized_plan, {}));
    assert(!orison::lowering::drop_calls_enabled(authorized_plan));
    auto missing_authorization = orison::lowering::plan_drop_cleanup_authorization(authorized_plan, {});
    assert(!missing_authorization.authorized);
    assert(missing_authorization.missing_declarations.size() == 1);
    assert(missing_authorization.missing_declarations.front().symbol_name == "__orison_drop.Payload");
    auto missing_authorization_report = orison::lowering::format_drop_cleanup_authorization_report(
        authorized_plan,
        missing_authorization
    );
    assert(missing_authorization_report.size() == 2);
    assert(
        missing_authorization_report[0] ==
        "drop cleanup authorization __orison_thread_cleanup.record_worker.20.2 blocked "
        "semantic blockers 0 missing declarations 1"
    );
    assert(
        missing_authorization_report[1] ==
        "missing drop declaration __orison_drop.Payload for Payload capture payload field 0 discovered at line 20"
    );
    auto semantic_blocked_authorization = orison::lowering::plan_drop_cleanup_authorization(
        authorized_plan,
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 20,
                .emit_declaration = true,
            },
        },
        {
            orison::semantics::DropLoweringAuthorization {
                .site = orison::semantics::PlannedDropSite {
                    .source_type_name = "Payload",
                    .abi_symbol_name = "__orison_drop.Payload",
                    .owner_name = "payload",
                    .site_line = 20,
                },
                .semantic_resolved = true,
                .source_drop_lowering_enabled = false,
                .authorized = false,
            },
        }
    );
    assert(!semantic_blocked_authorization.authorized);
    assert(semantic_blocked_authorization.semantic_lowering_blockers.size() == 1);
    assert(semantic_blocked_authorization.semantic_unresolved_blockers.empty());
    assert(semantic_blocked_authorization.source_drop_lowering_blockers.size() == 1);
    assert(semantic_blocked_authorization.missing_declarations.empty());
    auto semantic_blocked_authorization_report = orison::lowering::format_drop_cleanup_authorization_report(
        authorized_plan,
        semantic_blocked_authorization
    );
    assert(semantic_blocked_authorization_report.size() == 3);
    assert(
        semantic_blocked_authorization_report[0] ==
        "drop cleanup authorization __orison_thread_cleanup.record_worker.20.2 blocked "
        "semantic blockers 1 missing declarations 0"
    );
    assert(
        semantic_blocked_authorization_report[1] ==
        "semantic drop lowering blocked __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    assert(
        semantic_blocked_authorization_report[2] ==
        "source drop lowering not accepted __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    auto semantic_blocked_readiness_snapshot = orison::lowering::plan_drop_readiness_snapshot(
        {
            orison::semantics::DropLoweringAuthorization {
                .site = orison::semantics::PlannedDropSite {
                    .source_type_name = "Payload",
                    .abi_symbol_name = "__orison_drop.Payload",
                    .owner_name = "payload",
                    .site_line = 20,
                },
                .semantic_resolved = true,
                .source_drop_lowering_enabled = false,
                .authorized = false,
            },
        },
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 20,
                .emit_declaration = true,
            },
        },
        {authorized_plan}
    );
    auto semantic_blocked_relation_report =
        orison::lowering::format_drop_readiness_relation_report(semantic_blocked_readiness_snapshot);
    assert(semantic_blocked_relation_report.size() == 2);
    assert(
        semantic_blocked_relation_report[0] ==
        "drop readiness relation __orison_thread_cleanup.record_worker.20.2 blocked "
        "semantic blockers 1 emitted declarations 1 missing declarations 0"
    );
    assert(
        semantic_blocked_relation_report[1] ==
        "drop readiness relation semantic blocker __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    auto readiness_snapshot = orison::lowering::plan_drop_readiness_snapshot(
        {
            orison::semantics::DropLoweringAuthorization {
                .site = orison::semantics::PlannedDropSite {
                    .source_type_name = "Payload",
                    .abi_symbol_name = "__orison_drop.Payload",
                    .owner_name = "payload",
                    .site_line = 20,
                },
                .semantic_resolved = true,
                .source_drop_lowering_enabled = true,
                .authorized = true,
            },
        },
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 20,
                .emit_declaration = true,
            },
        },
        {authorized_plan}
    );
    assert(readiness_snapshot.semantic_authorizations.size() == 1);
    assert(readiness_snapshot.emitted_declarations.size() == 1);
    assert(readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(readiness_snapshot.cleanup_authorizations.front().authorization.authorized);
    auto readiness_snapshot_report = orison::lowering::format_drop_readiness_snapshot_report(readiness_snapshot);
    assert(readiness_snapshot_report.size() == 4);
    assert(
        readiness_snapshot_report[0] ==
        "drop readiness snapshot semantic authorizations 1 emitted declarations 1 cleanup authorizations 1"
    );
    assert(readiness_snapshot_report[1] == "semantic readiness __orison_drop.Payload for Payload authorized");
    assert(readiness_snapshot_report[2] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(
        readiness_snapshot_report[3] ==
        "cleanup readiness __orison_thread_cleanup.record_worker.20.2 authorized"
    );
    auto readiness_summary = orison::lowering::summarize_drop_readiness(readiness_snapshot);
    assert(readiness_summary.semantic_authorized == 1);
    assert(readiness_summary.semantic_blocked == 0);
    assert(readiness_summary.emitted_declarations == 1);
    assert(readiness_summary.cleanup_authorized == 1);
    assert(readiness_summary.cleanup_blocked == 0);
    assert(
        orison::lowering::format_drop_readiness_summary(readiness_summary) ==
        "drop readiness summary semantic authorized 1 blocked 0 emitted declarations 1 cleanup authorized 1 blocked 0"
    );
    auto readiness_relation_report = orison::lowering::format_drop_readiness_relation_report(readiness_snapshot);
    assert(readiness_relation_report.size() == 1);
    assert(
        readiness_relation_report.front() ==
        "drop readiness relation __orison_thread_cleanup.record_worker.20.2 authorized "
        "semantic blockers 0 emitted declarations 1 missing declarations 0"
    );
    assert(!orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(
        authorized_plan,
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 20,
            },
        }
    ));
    assert(!orison::lowering::drop_calls_enabled(authorized_plan));
    assert(orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(
        authorized_plan,
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 20,
                .emit_declaration = true,
            },
        }
    ));
    assert(orison::lowering::drop_calls_enabled(authorized_plan));
    auto successful_authorization = orison::lowering::plan_drop_cleanup_authorization(
        authorized_plan,
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 20,
                .emit_declaration = true,
            },
        }
    );
    assert(successful_authorization.authorized);
    assert(successful_authorization.missing_declarations.empty());
    auto successful_authorization_report = orison::lowering::format_drop_cleanup_authorization_report(
        authorized_plan,
        successful_authorization
    );
    assert(successful_authorization_report.size() == 1);
    assert(
        successful_authorization_report.front() ==
        "drop cleanup authorization __orison_thread_cleanup.record_worker.20.2 authorized"
    );
    auto authorized_drop_cleanup_report =
        orison::lowering::format_concurrency_drop_cleanup_plan(authorized_plan);
    assert(authorized_drop_cleanup_report.size() == 2);
    assert(
        authorized_drop_cleanup_report[0] ==
        "drop cleanup plan __orison_thread_cleanup.record_worker.20.2 actions 1 "
        "descriptor deallocation not-required drop calls enabled"
    );
    auto record_drop_cleanup_report =
        orison::lowering::format_concurrency_drop_cleanup_plan(record_plan->cleanup.drop_cleanup);
    assert(record_drop_cleanup_report.size() == 2);
    assert(
        record_drop_cleanup_report[0] ==
        "drop cleanup plan __orison_thread_cleanup.record_worker.20.2 actions 1 "
        "descriptor deallocation not-required drop calls disabled (metadata only)"
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
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
