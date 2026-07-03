#include "orison/lowering/concurrency_plan.hpp"

#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/syntax_traversal.hpp"
#include "orison/lowering/target_layout.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <string_view>

namespace orison::lowering {
namespace {

auto is_concurrency_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task ||
           expression.kind == syntax::ExpressionKind::thread;
}

auto expression_plan_kind(syntax::ExpressionSyntax const& expression) -> ConcurrencyPlanKind {
    return expression.kind == syntax::ExpressionKind::thread
        ? ConcurrencyPlanKind::thread
        : ConcurrencyPlanKind::task;
}

auto spawn_operation_for(ConcurrencyPlanKind kind) -> ConcurrencyRuntimeOperation {
    return kind == ConcurrencyPlanKind::thread
        ? ConcurrencyRuntimeOperation::spawn_thread
        : ConcurrencyRuntimeOperation::spawn_task;
}

auto semantic_kind_for(ConcurrencyPlanKind kind) -> semantics::ConcurrencyExpressionKind {
    return kind == ConcurrencyPlanKind::thread
        ? semantics::ConcurrencyExpressionKind::thread
        : semantics::ConcurrencyExpressionKind::task;
}

auto append_sanitized_symbol_part(std::string& symbol, std::string_view text) -> void {
    for (auto character : text) {
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_') {
            symbol.push_back(character);
            continue;
        }
        symbol.push_back('_');
    }
}

auto concurrency_thunk_symbol_name(
    ConcurrencyPlanKind kind,
    std::string_view enclosing_symbol_name,
    std::size_t expression_line,
    std::size_t ordinal
) -> std::string {
    auto symbol = kind == ConcurrencyPlanKind::thread
        ? std::string {"__orison_thread_thunk."}
        : std::string {"__orison_task_thunk."};
    append_sanitized_symbol_part(symbol, enclosing_symbol_name);
    symbol += "." + std::to_string(expression_line) + "." + std::to_string(ordinal);
    return symbol;
}

auto concurrency_cleanup_symbol_name(
    ConcurrencyPlanKind kind,
    std::string_view enclosing_symbol_name,
    std::size_t expression_line,
    std::size_t ordinal
) -> std::string {
    auto symbol = kind == ConcurrencyPlanKind::thread
        ? std::string {"__orison_thread_cleanup."}
        : std::string {"__orison_task_cleanup."};
    append_sanitized_symbol_part(symbol, enclosing_symbol_name);
    symbol += "." + std::to_string(expression_line) + "." + std::to_string(ordinal);
    return symbol;
}

auto max_expression_line(syntax::ExpressionSyntax const& expression) -> std::size_t;

auto max_statement_line(syntax::StatementSyntax const& statement) -> std::size_t {
    auto line = statement.line;
    line = std::max(line, max_expression_line(statement.expression));
    line = std::max(line, max_expression_line(statement.assignment_target));
    for (auto const& nested_statement : statement.nested_statements) {
        line = std::max(line, max_statement_line(nested_statement));
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        line = std::max(line, max_statement_line(alternate_statement));
    }
    for (auto const& switch_case : statement.switch_cases) {
        line = std::max(line, max_expression_line(switch_case.pattern));
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                line = std::max(line, max_statement_line(*case_statement));
            }
        }
    }
    return line;
}

auto max_expression_line(syntax::ExpressionSyntax const& expression) -> std::size_t {
    auto line = expression.line;
    for (auto const& argument : expression.arguments) {
        line = std::max(line, max_expression_line(argument));
    }
    for (auto const& nested_statement : expression.nested_statements) {
        if (nested_statement != nullptr) {
            line = std::max(line, max_statement_line(*nested_statement));
        }
    }
    if (expression.left != nullptr) {
        line = std::max(line, max_expression_line(*expression.left));
    }
    if (expression.right != nullptr) {
        line = std::max(line, max_expression_line(*expression.right));
    }
    if (expression.alternate != nullptr) {
        line = std::max(line, max_expression_line(*expression.alternate));
    }
    return line;
}

auto final_value_expression(
    syntax::ExpressionSyntax const& expression
) -> syntax::ExpressionSyntax const* {
    if (expression.nested_statements.empty() ||
        expression.nested_statements.back() == nullptr) {
        return nullptr;
    }

    auto const& final_statement = *expression.nested_statements.back();
    if (final_statement.kind == syntax::StatementKind::expression_statement ||
        final_statement.kind == syntax::StatementKind::return_statement) {
        return &final_statement.expression;
    }
    return nullptr;
}

auto infer_concurrency_result_type(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    auto inferred = infer_expression_type(expression, context, state);
    if (inferred.has_value()) {
        return inferred;
    }

    if (expression.kind == syntax::ExpressionKind::integer_literal) {
        return LoweredType {
            .type = "i64",
            .signedness = IntegerSignedness::signed_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::binary && expression.left != nullptr &&
        expression.right != nullptr) {
        auto left = infer_concurrency_result_type(*expression.left, context, state);
        auto right = infer_concurrency_result_type(*expression.right, context, state);
        if (left.has_value() && right.has_value() && left->type == right->type &&
            left->signedness == right->signedness) {
            return left;
        }
    }

    return std::nullopt;
}

auto is_capture_in_expression_range(
    semantics::ConcurrencyCapture const& capture,
    semantics::ConcurrencyExpressionKind expected_kind,
    std::size_t first_line,
    std::size_t last_line
) -> bool {
    return capture.expression_kind == expected_kind &&
           capture.line >= first_line &&
           capture.line <= last_line;
}

auto environment_llvm_type_for(
    std::vector<ConcurrencyCapturePlan> const& captures
) -> std::string {
    auto output = std::ostringstream {};
    output << "{ ";
    for (auto index = std::size_t {0}; index < captures.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << captures[index].llvm_type;
    }
    output << " }";
    return output.str();
}

auto capture_llvm_types(
    std::vector<ConcurrencyCapturePlan> const& captures
) -> std::vector<std::string_view> {
    auto types = std::vector<std::string_view> {};
    types.reserve(captures.size());
    for (auto const& capture : captures) {
        types.push_back(capture.llvm_type);
    }
    return types;
}

auto is_scalar_or_nonowning_source_type(std::string_view source_type_name) -> bool {
    constexpr auto scalar_names = std::array<std::string_view, 25> {
        "Address",
        "Bool",
        "Byte",
        "Char",
        "Float32",
        "Float64",
        "Int8",
        "Int16",
        "Int32",
        "Int64",
        "Int128",
        "IntSize",
        "UInt8",
        "UInt16",
        "UInt32",
        "UInt64",
        "UInt128",
        "UIntSize",
        "Unit",
        "Pointer",
        "Pointer<",
        "View",
        "View<",
        "shared.",
        "exclusive.",
    };

    for (auto scalar_name : scalar_names) {
        if (source_type_name == scalar_name || source_type_name.starts_with(scalar_name)) {
            return true;
        }
    }
    return false;
}

auto drop_symbol_name_for(std::string_view source_type_name) -> std::string {
    auto symbol = std::string {"__orison_drop."};
    append_sanitized_symbol_part(symbol, source_type_name);
    return symbol;
}

auto cleanup_plan_for(
    std::vector<ConcurrencyCapturePlan> const& captures
) -> ConcurrencyCleanupPlan {
    auto cleanup = ConcurrencyCleanupPlan {};
    for (auto const& capture : captures) {
        if (capture.llvm_type.empty() || is_scalar_or_nonowning_source_type(capture.source_type_name)) {
            continue;
        }
        cleanup.drop_candidates.push_back(ConcurrencyCleanupFieldPlan {
            .name = capture.name,
            .source_type_name = capture.source_type_name,
            .llvm_type = capture.llvm_type,
            .drop_symbol_name = drop_symbol_name_for(capture.source_type_name),
            .field_index = capture.field_index,
            .capture_kind = capture.capture_kind,
        });
    }
    return cleanup;
}

auto planned_drop_action_for_candidate(
    ConcurrencyCleanupFieldPlan const& candidate,
    std::size_t discovery_line
) -> PlannedDropAction {
    return PlannedDropAction {
        .capture_name = candidate.name,
        .source_type_name = candidate.source_type_name,
        .symbol_name = candidate.drop_symbol_name,
        .field_index = candidate.field_index,
        .discovery_line = discovery_line,
    };
}

auto drop_cleanup_plan_for(
    std::string_view cleanup_symbol_name,
    std::size_t discovery_line,
    std::vector<ConcurrencyCleanupFieldPlan> const& candidates
) -> ConcurrencyDropCleanupPlan {
    auto plan = ConcurrencyDropCleanupPlan {
        .cleanup_symbol_name = std::string {cleanup_symbol_name},
    };
    plan.actions.reserve(candidates.size());
    for (auto const& candidate : candidates) {
        plan.actions.push_back(planned_drop_action_for_candidate(candidate, discovery_line));
    }
    return plan;
}

void collect_planned_drop_actions_from_expression(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    semantics::SemanticAnalysisResult const& semantics,
    std::string_view enclosing_symbol_name,
    std::size_t ordinal,
    std::vector<ConcurrencyDropCleanupPlan>& drop_cleanups
) {
    if (is_concurrency_expression(expression)) {
        auto captures = std::vector<ConcurrencyCapturePlan> {};
        auto semantic_kind = semantic_kind_for(expression_plan_kind(expression));
        auto expression_last_line = max_expression_line(expression);
        for (auto const& capture : semantics.concurrency_captures) {
            if (!is_capture_in_expression_range(capture, semantic_kind, expression.line, expression_last_line)) {
                continue;
            }

            auto lowered_capture_type = lowered_type_for_source_type_name(
                capture.type_name,
                context.lowering
            );
            captures.push_back(ConcurrencyCapturePlan {
                .name = capture.name,
                .source_type_name = capture.type_name,
                .llvm_type = lowered_capture_type.has_value() ? lowered_capture_type->type : std::string {},
                .field_index = captures.size(),
                .capture_kind = capture.capture_kind,
            });
        }

        auto cleanup = cleanup_plan_for(captures);
        auto drop_cleanup = drop_cleanup_plan_for(
            concurrency_cleanup_symbol_name(
                expression_plan_kind(expression),
                enclosing_symbol_name,
                expression.line,
                ordinal
            ),
            expression.line,
            cleanup.drop_candidates
        );
        if (!drop_cleanup.actions.empty()) {
            drop_cleanups.push_back(std::move(drop_cleanup));
        }
    }
}

void collect_planned_drop_cleanups_from_function(
    syntax::FunctionSyntax const& function,
    std::string_view enclosing_symbol_name,
    LoweringEmissionContext const& context,
    semantics::SemanticAnalysisResult const& semantics,
    std::vector<ConcurrencyDropCleanupPlan>& drop_cleanups
) {
    auto ordinal = std::size_t {0};
    walk_function_expressions(function, [&](syntax::ExpressionSyntax const& expression) {
        if (!is_concurrency_expression(expression)) {
            return;
        }
        collect_planned_drop_actions_from_expression(
            expression,
            context,
            semantics,
            enclosing_symbol_name,
            ordinal,
            drop_cleanups
        );
        ++ordinal;
    });
}

}  // namespace

auto format_concurrency_drop_cleanup_plan(
    ConcurrencyDropCleanupPlan const& plan
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    auto header = std::ostringstream {};
    header << "drop cleanup plan";
    if (!plan.cleanup_symbol_name.empty()) {
        header << " " << plan.cleanup_symbol_name;
    }
    header << " actions " << plan.actions.size()
           << " drop calls " << (drop_calls_enabled(plan) ? "enabled" : "disabled");
    if (!drop_calls_enabled(plan)) {
        header << " (metadata only)";
    }
    report.push_back(header.str());

    auto action_report = format_planned_drop_action_report(plan.actions);
    report.insert(report.end(), action_report.begin(), action_report.end());
    return report;
}

auto drop_calls_enabled(ConcurrencyDropCleanupPlan const& plan) -> bool {
    return plan.drop_call_emission == DropCallEmissionEligibility::declared_drop_abi;
}

auto plan_drop_cleanup_authorization(
    ConcurrencyDropCleanupPlan const& plan,
    std::vector<PlannedDropDeclaration> const& declarations
) -> DropCleanupAuthorizationReport {
    return plan_drop_cleanup_authorization(plan, declarations, {});
}

auto plan_drop_cleanup_authorization(
    ConcurrencyDropCleanupPlan const& plan,
    std::vector<PlannedDropDeclaration> const& declarations,
    std::vector<semantics::DropLoweringAuthorization> const& semantic_authorizations
) -> DropCleanupAuthorizationReport {
    auto report = DropCleanupAuthorizationReport {};
    if (plan.actions.empty()) {
        return report;
    }

    for (auto const& action : plan.actions) {
        auto const semantic_authorization = std::find_if(
            semantic_authorizations.begin(),
            semantic_authorizations.end(),
            [&](semantics::DropLoweringAuthorization const& candidate) {
                return candidate.site.abi_symbol_name == action.symbol_name &&
                       candidate.site.source_type_name == action.source_type_name &&
                       !candidate.authorized;
            }
        );
        if (semantic_authorization != semantic_authorizations.end()) {
            report.semantic_lowering_blockers.push_back(action);
        }

        auto const declaration = std::find_if(
            declarations.begin(),
            declarations.end(),
            [&](PlannedDropDeclaration const& candidate) {
                return candidate.symbol_name == action.symbol_name && candidate.emit_declaration;
            }
        );
        if (declaration == declarations.end()) {
            report.missing_declarations.push_back(action);
        }
    }

    report.authorized =
        report.semantic_lowering_blockers.empty() && report.missing_declarations.empty();
    return report;
}

auto format_drop_cleanup_authorization_report(
    ConcurrencyDropCleanupPlan const& plan,
    DropCleanupAuthorizationReport const& report
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto header = std::ostringstream {};
    header << "drop cleanup authorization";
    if (!plan.cleanup_symbol_name.empty()) {
        header << " " << plan.cleanup_symbol_name;
    }
    header << (report.authorized ? " authorized" : " blocked");
    if (!report.authorized) {
        header << " semantic blockers " << report.semantic_lowering_blockers.size();
        header << " missing declarations " << report.missing_declarations.size();
    }
    lines.push_back(header.str());

    for (auto const& blocker : report.semantic_lowering_blockers) {
        auto line = std::ostringstream {};
        line << "semantic drop lowering blocked " << blocker.symbol_name;
        if (!blocker.source_type_name.empty()) {
            line << " for " << blocker.source_type_name;
        }
        if (!blocker.capture_name.empty()) {
            line << " capture " << blocker.capture_name;
        }
        line << " field " << blocker.field_index;
        if (blocker.discovery_line > 0) {
            line << " discovered at line " << blocker.discovery_line;
        }
        lines.push_back(line.str());
    }

    for (auto const& missing : report.missing_declarations) {
        auto line = std::ostringstream {};
        line << "missing drop declaration " << missing.symbol_name;
        if (!missing.source_type_name.empty()) {
            line << " for " << missing.source_type_name;
        }
        if (!missing.capture_name.empty()) {
            line << " capture " << missing.capture_name;
        }
        line << " field " << missing.field_index;
        if (missing.discovery_line > 0) {
            line << " discovered at line " << missing.discovery_line;
        }
        lines.push_back(line.str());
    }
    return lines;
}

auto plan_drop_readiness_snapshot(
    std::vector<semantics::DropLoweringAuthorization> const& semantic_authorizations,
    std::vector<PlannedDropDeclaration> const& declarations,
    std::vector<ConcurrencyDropCleanupPlan> const& cleanups
) -> DropReadinessSnapshot {
    auto snapshot = DropReadinessSnapshot {
        .semantic_authorizations = semantic_authorizations,
    };
    for (auto const& declaration : declarations) {
        if (declaration.emit_declaration) {
            snapshot.emitted_declarations.push_back(declaration);
        }
    }
    snapshot.cleanup_authorizations.reserve(cleanups.size());
    for (auto const& cleanup : cleanups) {
        snapshot.cleanup_authorizations.push_back(DropCleanupReadiness {
            .cleanup_symbol_name = cleanup.cleanup_symbol_name,
            .authorization = plan_drop_cleanup_authorization(cleanup, declarations, semantic_authorizations),
        });
    }
    return snapshot;
}

auto format_drop_readiness_snapshot_report(
    DropReadinessSnapshot const& snapshot
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto header = std::ostringstream {};
    header << "drop readiness snapshot semantic authorizations " << snapshot.semantic_authorizations.size()
           << " emitted declarations " << snapshot.emitted_declarations.size()
           << " cleanup authorizations " << snapshot.cleanup_authorizations.size();
    lines.push_back(header.str());

    for (auto const& authorization : snapshot.semantic_authorizations) {
        auto line = std::ostringstream {};
        line << "semantic readiness " << authorization.site.abi_symbol_name;
        if (!authorization.site.source_type_name.empty()) {
            line << " for " << authorization.site.source_type_name;
        }
        line << (authorization.authorized ? " authorized" : " blocked");
        lines.push_back(line.str());
    }

    for (auto const& declaration : snapshot.emitted_declarations) {
        auto line = std::ostringstream {};
        line << "emitted declaration readiness " << declaration.symbol_name;
        if (!declaration.source_type_name.empty()) {
            line << " for " << declaration.source_type_name;
        }
        lines.push_back(line.str());
    }

    for (auto const& cleanup : snapshot.cleanup_authorizations) {
        auto line = std::ostringstream {};
        line << "cleanup readiness";
        if (!cleanup.cleanup_symbol_name.empty()) {
            line << " " << cleanup.cleanup_symbol_name;
        }
        line << (cleanup.authorization.authorized ? " authorized" : " blocked");
        if (!cleanup.authorization.authorized) {
            line << " semantic blockers " << cleanup.authorization.semantic_lowering_blockers.size()
                 << " missing declarations " << cleanup.authorization.missing_declarations.size();
        }
        lines.push_back(line.str());
    }

    return lines;
}

auto summarize_drop_readiness(
    DropReadinessSnapshot const& snapshot
) -> DropReadinessSummary {
    auto summary = DropReadinessSummary {
        .emitted_declarations = snapshot.emitted_declarations.size(),
    };
    for (auto const& authorization : snapshot.semantic_authorizations) {
        if (authorization.authorized) {
            ++summary.semantic_authorized;
        } else {
            ++summary.semantic_blocked;
        }
    }
    for (auto const& cleanup : snapshot.cleanup_authorizations) {
        if (cleanup.authorization.authorized) {
            ++summary.cleanup_authorized;
        } else {
            ++summary.cleanup_blocked;
        }
    }
    return summary;
}

auto format_drop_readiness_summary(
    DropReadinessSummary const& summary
) -> std::string {
    auto output = std::ostringstream {};
    output << "drop readiness summary semantic authorized " << summary.semantic_authorized
           << " blocked " << summary.semantic_blocked
           << " emitted declarations " << summary.emitted_declarations
           << " cleanup authorized " << summary.cleanup_authorized
           << " blocked " << summary.cleanup_blocked;
    return output.str();
}

auto format_drop_readiness_relation_report(
    DropReadinessSnapshot const& snapshot
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    for (auto const& cleanup : snapshot.cleanup_authorizations) {
        auto line = std::ostringstream {};
        line << "drop readiness relation";
        if (!cleanup.cleanup_symbol_name.empty()) {
            line << " " << cleanup.cleanup_symbol_name;
        }
        line << (cleanup.authorization.authorized ? " authorized" : " blocked")
             << " semantic blockers " << cleanup.authorization.semantic_lowering_blockers.size()
             << " emitted declarations " << snapshot.emitted_declarations.size()
             << " missing declarations " << cleanup.authorization.missing_declarations.size();
        lines.push_back(line.str());

        for (auto const& blocker : cleanup.authorization.semantic_lowering_blockers) {
            auto detail = std::ostringstream {};
            detail << "drop readiness relation semantic blocker " << blocker.symbol_name;
            if (!blocker.source_type_name.empty()) {
                detail << " for " << blocker.source_type_name;
            }
            detail << " capture " << blocker.capture_name << " field " << blocker.field_index;
            if (blocker.discovery_line != 0) {
                detail << " discovered at line " << blocker.discovery_line;
            }
            lines.push_back(detail.str());
        }

        for (auto const& missing : cleanup.authorization.missing_declarations) {
            auto detail = std::ostringstream {};
            detail << "drop readiness relation missing declaration " << missing.symbol_name;
            if (!missing.source_type_name.empty()) {
                detail << " for " << missing.source_type_name;
            }
            detail << " capture " << missing.capture_name << " field " << missing.field_index;
            if (missing.discovery_line != 0) {
                detail << " discovered at line " << missing.discovery_line;
            }
            lines.push_back(detail.str());
        }
    }
    return lines;
}

auto authorize_drop_cleanup_calls_for_declared_abi(
    ConcurrencyDropCleanupPlan& plan,
    std::vector<PlannedDropDeclaration> const& declarations
) -> bool {
    auto report = plan_drop_cleanup_authorization(plan, declarations);
    plan.drop_call_emission = report.authorized
        ? DropCallEmissionEligibility::declared_drop_abi
        : DropCallEmissionEligibility::metadata_only;
    return report.authorized;
}

auto plan_concurrency_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view enclosing_symbol_name,
    std::size_t ordinal,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state,
    semantics::SemanticAnalysisResult const& semantics
) -> std::optional<ConcurrencyExpressionPlan> {
    if (!is_concurrency_expression(expression)) {
        return std::nullopt;
    }

    auto const* result_expression = final_value_expression(expression);
    if (result_expression == nullptr) {
        return std::nullopt;
    }

    auto result_type = infer_concurrency_result_type(*result_expression, context, state);
    if (!result_type.has_value() || result_type->type.empty() || result_type->type == "void") {
        return std::nullopt;
    }

    auto kind = expression_plan_kind(expression);
    auto expression_last_line = max_expression_line(expression);
    auto plan = ConcurrencyExpressionPlan {
        .kind = kind,
        .spawn_operation = spawn_operation_for(kind),
        .thunk_symbol_name = concurrency_thunk_symbol_name(
            kind,
            enclosing_symbol_name,
            expression.line,
            ordinal
        ),
        .cleanup_symbol_name = concurrency_cleanup_symbol_name(
            kind,
            enclosing_symbol_name,
            expression.line,
            ordinal
        ),
        .result_type = *result_type,
    };

    auto semantic_kind = semantic_kind_for(kind);
    for (auto const& capture : semantics.concurrency_captures) {
        if (!is_capture_in_expression_range(capture, semantic_kind, expression.line, expression_last_line)) {
            continue;
        }

        auto lowered_capture_type = lowered_type_for_source_type_name(capture.type_name, context.lowering);
        auto field_index = plan.captures.size();
        plan.captures.push_back(ConcurrencyCapturePlan {
            .name = capture.name,
            .source_type_name = capture.type_name,
            .llvm_type = lowered_capture_type.has_value() ? lowered_capture_type->type : std::string {},
            .field_index = field_index,
            .capture_kind = capture.capture_kind,
        });
    }

    plan.environment_layout = ConcurrencyEnvironmentLayout {
        .llvm_type = environment_llvm_type_for(plan.captures),
        .size_bytes = lowered_struct_size_bytes(capture_llvm_types(plan.captures), context.lowering).value_or(0),
        .fields = plan.captures,
    };
    plan.cleanup = cleanup_plan_for(plan.captures);
    plan.cleanup.drop_cleanup = drop_cleanup_plan_for(
        plan.cleanup_symbol_name,
        expression.line,
        plan.cleanup.drop_candidates
    );
    plan.result_storage = ConcurrencyResultStorageLayout {
        .llvm_type = plan.result_type.type,
        .size_bytes = lowered_type_size_bytes(plan.result_type.type, context.lowering).value_or(0),
    };

    return plan;
}

auto plan_concurrency_planned_drops(
    syntax::ModuleSyntax const& module,
    LoweringEmissionContext const& context,
    semantics::SemanticAnalysisResult const& semantics
) -> std::vector<PlannedDropDeclaration> {
    auto planned_drops = std::vector<PlannedDropDeclaration> {};
    for (auto const& action : plan_concurrency_planned_drop_actions(module, context, semantics)) {
        add_planned_drop_declaration(planned_drops, planned_drop_declaration_for_action(action));
    }
    return planned_drops;
}

auto plan_concurrency_drop_cleanups(
    syntax::ModuleSyntax const& module,
    LoweringEmissionContext const& context,
    semantics::SemanticAnalysisResult const& semantics
) -> std::vector<ConcurrencyDropCleanupPlan> {
    auto drop_cleanups = std::vector<ConcurrencyDropCleanupPlan> {};
    for (auto const& function : module.functions) {
        auto signature = context.lowering.functions.find(function.name);
        auto symbol_name = signature == context.lowering.functions.end()
            ? std::string_view {function.name}
            : std::string_view {signature->second.symbol_name};
        collect_planned_drop_cleanups_from_function(
            function,
            symbol_name,
            context,
            semantics,
            drop_cleanups
        );
    }

    auto method_index = std::size_t {0};
    auto collect_method = [&](syntax::FunctionSyntax const& method) {
        auto symbol_name = method_index >= context.lowering.methods.size()
            ? std::string_view {method.name}
            : std::string_view {context.lowering.methods[method_index].signature.symbol_name};
        ++method_index;
        collect_planned_drop_cleanups_from_function(
            method,
            symbol_name,
            context,
            semantics,
            drop_cleanups
        );
    };
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            collect_method(method);
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            collect_method(method);
        }
    }
    return drop_cleanups;
}

auto plan_concurrency_planned_drop_actions(
    syntax::ModuleSyntax const& module,
    LoweringEmissionContext const& context,
    semantics::SemanticAnalysisResult const& semantics
) -> std::vector<PlannedDropAction> {
    auto planned_drop_actions = std::vector<PlannedDropAction> {};
    for (auto const& cleanup : plan_concurrency_drop_cleanups(module, context, semantics)) {
        planned_drop_actions.insert(
            planned_drop_actions.end(),
            cleanup.actions.begin(),
            cleanup.actions.end()
        );
    }
    return planned_drop_actions;
}

}  // namespace orison::lowering
