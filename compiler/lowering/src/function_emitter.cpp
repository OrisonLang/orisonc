#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/for_loop_lowering.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/lowering_failure_lifecycle.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/nonvalue_if_lowering.hpp"
#include "orison/lowering/nonvalue_switch_lowering.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/repeat_loop_lowering.hpp"
#include "orison/lowering/statement_body_lowering.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/statement_pointer_adapter.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/unit_deferred_cleanup.hpp"
#include "orison/lowering/unsafe_block_lowering.hpp"
#include "orison/lowering/while_emitter.hpp"

#include <algorithm>
#include <optional>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

using EmissionContext = LoweringEmissionContext;

auto lower_unit_statement_block(
    std::span<syntax::StatementSyntax const*> statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement(
    syntax::StatementSyntax const& statement,
    bool is_last_statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto append_generic_record_constructor_inference_detail(
    std::string message,
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::string {
    if (auto detail = generic_record_constructor_inference_failure_detail(
            expression,
            context.lowering,
            session.state
        )) {
        message += ": ";
        message += *detail;
    }
    return message;
}

auto lower_unit_if_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_switch_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_repeat_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_for_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_unsafe_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_guard_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_nonvoid_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_nonvoid_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto is_empty_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.text.empty() && expression.arguments.empty() && expression.nested_statements.empty() &&
           expression.left == nullptr && expression.right == nullptr && expression.alternate == nullptr;
}

auto is_concurrency_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task ||
           expression.kind == syntax::ExpressionKind::thread ||
           (expression.kind == syntax::ExpressionKind::unary && expression.text == "await");
}

auto concurrency_expression_name(syntax::ExpressionSyntax const& expression) -> std::string {
    if (expression.kind == syntax::ExpressionKind::task) {
        return "task";
    }
    if (expression.kind == syntax::ExpressionKind::thread) {
        return "thread";
    }
    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "await") {
        return "await";
    }
    return expression.text;
}

auto is_thread_join_expression(
    syntax::ExpressionSyntax const& expression,
    FunctionLoweringState const& state
) -> bool {
    if (expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::member_access ||
        expression.left->text != "join" ||
        expression.left->left == nullptr ||
        expression.left->left->kind != syntax::ExpressionKind::name) {
        return false;
    }
    return state.thread_bindings.contains(expression.left->left->text);
}

auto emit_function_return_cleanup(
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    emit_abandoned_concurrency_handle_cleanup(session, output);
    if (!emit_deferred_cleanup_to_depth(
        0,
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block
    )) {
        return false;
    }
    if (!emit_local_dynamic_array_cleanups(context, session, output)) {
        return false;
    }
    if (!emit_choice_dynamic_array_payload_cleanups(context, session, output)) {
        return false;
    }
    return emit_bound_dynamic_array_parameter_cleanups(context, session, output);
}

void release_returned_dynamic_array_local_cleanup(
    syntax::ExpressionSyntax const& expression,
    std::optional<std::string_view> return_source_type_name,
    FunctionLoweringState& state
) {
    if (!return_source_type_name.has_value() ||
        dynamic_array_element_source_type_name(*return_source_type_name) == std::nullopt ||
        expression.kind != syntax::ExpressionKind::name) {
        return;
    }

    auto source_type = state.source_type_names.find(expression.text);
    if (source_type == state.source_type_names.end() || source_type->second != *return_source_type_name) {
        return;
    }

    for (auto cleanup_plan = state.dynamic_array_local_cleanup_plans.begin();
         cleanup_plan != state.dynamic_array_local_cleanup_plans.end();
         ++cleanup_plan) {
        if (cleanup_plan->owner_name == expression.text &&
            cleanup_plan->source_type_name == *return_source_type_name &&
            cleanup_plan->descriptor_storage_status ==
                DynamicArrayDescriptorStorageStatus::lowered_local_descriptor) {
            state.dynamic_array_local_cleanup_plans.erase(cleanup_plan);
            return;
        }
    }
}

void release_returned_choice_dynamic_array_payload_cleanup(
    syntax::ExpressionSyntax const& expression,
    std::optional<std::string_view> return_source_type_name,
    LoweringContext const& context,
    FunctionLoweringState& state
) {
    if (!return_source_type_name.has_value() ||
        expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return;
    }

    auto choice = context.choices.find(std::string {*return_source_type_name});
    if (choice == context.choices.end()) {
        return;
    }

    auto const& variant_name = expression.left->text;
    for (auto const& variant : choice->second.variants) {
        if (variant.name != variant_name || variant.payloads.size() != expression.arguments.size()) {
            continue;
        }
        for (auto index = std::size_t {0}; index < variant.payloads.size(); ++index) {
            auto const& argument = expression.arguments[index];
            auto const& payload = variant.payloads[index];
            if (argument.kind != syntax::ExpressionKind::name ||
                dynamic_array_element_source_type_name(payload.source_type_name) == std::nullopt) {
                continue;
            }
            auto source_type = state.source_type_names.find(argument.text);
            if (source_type != state.source_type_names.end() &&
                source_type->second == payload.source_type_name) {
                mark_owned_binding_consumed(state.ownership_transfers, argument.text);
            }
        }
        return;
    }
}

void mark_dynamic_array_cleanup_descendants_consumed(
    std::string_view owner_name,
    std::string_view source_type_name,
    LoweringContext const& context,
    OwnershipTransferState& transfers,
    std::size_t depth = 0
) {
    if (depth > 16) {
        return;
    }

    if (dynamic_array_element_source_type_name(source_type_name).has_value()) {
        mark_owned_binding_consumed(transfers, std::string {owner_name});
        return;
    }

    auto record = context.records.find(std::string {source_type_name});
    if (record == context.records.end()) {
        return;
    }

    for (auto const& field : record->second.fields) {
        auto field_owner_name = std::string {owner_name};
        field_owner_name += ".";
        field_owner_name += field.name;
        mark_dynamic_array_cleanup_descendants_consumed(
            field_owner_name,
            field.source_type_name,
            context,
            transfers,
            depth + 1
        );
    }
}

void release_returned_record_constructor_dynamic_array_field_cleanups(
    syntax::ExpressionSyntax const& expression,
    std::optional<std::string_view> return_source_type_name,
    LoweringContext const& context,
    FunctionLoweringState& state
) {
    if (!return_source_type_name.has_value() ||
        expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return;
    }

    auto record = context.records.find(std::string {*return_source_type_name});
    if (record == context.records.end() ||
        record->second.name != expression.left->text ||
        record->second.fields.size() != expression.arguments.size()) {
        return;
    }

    for (auto index = std::size_t {0}; index < record->second.fields.size(); ++index) {
        auto const& argument = expression.arguments[index];
        auto const& field = record->second.fields[index];
        if (argument.kind != syntax::ExpressionKind::name ||
            !is_owned_transfer_source_type(field.source_type_name, context)) {
            continue;
        }

        auto source_type = state.source_type_names.find(argument.text);
        if (source_type != state.source_type_names.end() &&
            source_type->second == field.source_type_name) {
            mark_dynamic_array_cleanup_descendants_consumed(
                argument.text,
                field.source_type_name,
                context,
                state.ownership_transfers
            );
        }
    }
}

auto infer_unit_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
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
    if (expression.kind == syntax::ExpressionKind::boolean_literal) {
        return LoweredType {
            .type = "i1",
            .signedness = IntegerSignedness::not_integer,
        };
    }
    if (expression.kind == syntax::ExpressionKind::string_literal) {
        return LoweredType {
            .type = "ptr",
            .signedness = IntegerSignedness::not_integer,
        };
    }
    return std::nullopt;
}

auto is_receiver_self_source_type(std::string_view type_name) -> bool {
    return type_name == "This" || type_name == "shared.This" || type_name == "exclusive.This";
}

auto is_exclusive_receiver_parameter(
    syntax::FunctionSyntax const& function,
    syntax::ParameterSyntax const& parameter
) -> bool {
    return parameter.name == "this" &&
        (parameter.type.name == "exclusive.This" || function.has_exclusive_receiver_parameter);
}

auto owned_aggregate_projection_value_read_diagnostic(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    LoweringEmissionContext const& emission_context,
    FunctionLoweringState& state
) -> std::string {
    auto plan = describe_named_aggregate_projection_access(
        expression,
        context,
        state,
        AggregateProjectionAccessIntent::value_read
    );
    if (emission_context.options.collect_aggregate_projection_access_metadata &&
        plan.status != AggregateProjectionAccessStatus::not_named_aggregate_path) {
        state.aggregate_projection_access_plans.push_back(plan);
    }
    return aggregate_projection_access_diagnostic(plan);
}

void seed_bound_dynamic_array_parameter_cleanup_owner(
    std::string_view parameter_name,
    std::string_view source_type_name,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session
) {
    if (parameter_name == "this") {
        return;
    }

    auto sequence = dynamic_sequence_source_type(source_type_name);
    if (!sequence.has_value() ||
        sequence->kind != DynamicSequenceKind::dynamic_array) {
        return;
    }

    auto lowerable_parameter =
        is_scalar_or_nonowning_source_type(sequence->element_source_type_name) ||
        std::ranges::any_of(context.options.semantic_drop_lowering_authorizations, [&](auto const& authorization) {
            return authorization.authorized &&
                authorization.site.owner_name == std::string {parameter_name} + ".element" &&
                authorization.site.source_type_name == sequence->element_source_type_name &&
                authorization.site.abi_symbol_name == "__orison_drop." + sequence->element_source_type_name;
        });
    if (!lowerable_parameter) {
        return;
    }

    auto storage = aggregate_storage_for_name(parameter_name, session.state);
    if (!storage.has_value()) {
        return;
    }

    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        std::string {parameter_name},
        std::string {source_type_name},
        context.lowering
    );
    if (!cleanup_plan.has_value()) {
        return;
    }

    cleanup_plan->descriptor_storage_name = std::move(*storage);
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor;
    session.state.dynamic_array_iterable_cleanup_owner_plans.push_back(std::move(*cleanup_plan));
}

auto unsupported_dynamic_array_parameter_diagnostic(
    syntax::ParameterSyntax const& parameter
) -> std::optional<std::string> {
    auto source_type_name = render_source_type_name(parameter.type);
    auto sequence = dynamic_sequence_source_type(source_type_name);
    if (!sequence.has_value() ||
        sequence->kind != DynamicSequenceKind::dynamic_array ||
        is_scalar_or_nonowning_source_type(sequence->element_source_type_name)) {
        return std::nullopt;
    }

    return "lowering DynamicArray parameter '" + parameter.name + "' with owned element type " +
        sequence->element_source_type_name + " requires ownership/drop proof before production lowering";
}

auto dynamic_array_owned_parameter_has_drop_proof(
    syntax::ParameterSyntax const& parameter,
    LlvmIrEmissionOptions const& options
) -> bool {
    if (parameter.name == "this") {
        return true;
    }

    auto source_type_name = render_source_type_name(parameter.type);
    auto sequence = dynamic_sequence_source_type(source_type_name);
    if (!sequence.has_value() ||
        sequence->kind != DynamicSequenceKind::dynamic_array ||
        is_scalar_or_nonowning_source_type(sequence->element_source_type_name)) {
        return true;
    }
    if (options.fixture_enable_dynamic_array_parameter_descriptors) {
        return true;
    }

    auto const expected_owner_name = parameter.name + ".element";
    auto const expected_symbol_name = "__orison_drop." + sequence->element_source_type_name;
    return std::ranges::any_of(options.semantic_drop_lowering_authorizations, [&](auto const& authorization) {
        return authorization.authorized &&
            authorization.site.owner_name == expected_owner_name &&
            authorization.site.source_type_name == sequence->element_source_type_name &&
            authorization.site.abi_symbol_name == expected_symbol_name;
    });
}

auto infer_unit_binding_type(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (!statement.annotated_type.name.empty()) {
        return lowered_type_for_source_type_name(
            render_source_type_name(statement.annotated_type),
            context.lowering
        );
    }

    auto inferred_type = infer_unit_expression_type(statement.expression, context, state);
    if (!inferred_type.has_value() || inferred_type->type.empty() || inferred_type->type == "void") {
        return std::nullopt;
    }
    return inferred_type;
}

auto lower_unit_statement_block(
    std::span<syntax::StatementSyntax const*> statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_nonvalue_statement_block(
        statements,
        "lowering does not yet support statements after a terminating Unit statement",
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block,
        [&](syntax::StatementSyntax const& statement, bool is_last_statement) {
            return lower_unit_statement(statement, is_last_statement, context, session, diagnostics, output);
        }
    );
}

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = statement_pointers_for(statements);
    return lower_unit_statement_block(statement_pointers, context, session, diagnostics, output);
}

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_nonvalue_statement_block(
        statement_pointer_span(statements),
        "lowering does not yet support statements after a terminating Unit statement",
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block,
        [&](syntax::StatementSyntax const& statement, bool is_last_statement) {
            return lower_unit_statement(statement, is_last_statement, context, session, diagnostics, output);
        }
    );
}

auto lower_unit_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = statement_pointers_for(statements);
    return lower_unit_statement_block(statement_pointers, context, session, diagnostics, output);
}

auto lower_unit_if_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_nonvalue_if_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        "lowering does not yet support this Unit if condition",
        true,
        [&]() {
            return lower_unit_statement_block(statement.nested_statements, context, session, diagnostics, output);
        },
        [&]() {
            return lower_unit_statement_block(
                statement.alternate_statements,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
}

auto lower_unit_switch_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto subject_type = infer_unit_expression_type(statement.expression, context, session.state);
    if (!subject_type.has_value()) {
        diagnostics.error(statement.line, "lowering does not yet support this Unit switch subject");
        return StatementFlow::failed;
    }

    return lower_nonvalue_switch_statement(
        statement,
        *subject_type,
        context,
        session,
        diagnostics,
        output,
        "lowering does not yet support this Unit switch subject",
        "lowering does not yet support this Unit switch statement",
        [&](LoweredSwitchCasePlan const& planned_case) {
            return lower_unit_statement_block(
                planned_case.syntax->statements,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
}

auto lower_unit_repeat_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_repeat_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        [&]() {
            return lower_unit_statement_block(statement.nested_statements, context, session, diagnostics, output);
        }
    );
}

auto lower_unit_for_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (statement.expression.kind != syntax::ExpressionKind::array_literal) {
        auto source_type_name =
            source_type_name_for_expression(statement.expression, context.lowering, session.state);
        auto dynamic_sequence = source_type_name.has_value()
            ? dynamic_sequence_source_type(*source_type_name)
            : std::nullopt;
        if (dynamic_sequence.has_value() &&
            (dynamic_sequence_for_lowering_enabled(*dynamic_sequence, context.options) ||
             dynamic_sequence->kind == DynamicSequenceKind::dynamic_array)) {
            return lower_sequence_for_statement(
                statement,
                context,
                session,
                diagnostics,
                output,
                [&]() {
                    return lower_unit_statement_block(
                        statement.nested_statements,
                        context,
                        session,
                        diagnostics,
                        output
                    );
                }
            );
        }
        return lower_fixed_array_for_statement(
            statement,
            context,
            session,
            diagnostics,
            output,
            [&]() {
                return lower_unit_statement_block(
                    statement.nested_statements,
                    context,
                    session,
                    diagnostics,
                    output
                );
            }
        );
    }

    return lower_array_literal_for_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        infer_unit_expression_type,
        [&]() {
            return lower_unit_statement_block(
                statement.nested_statements,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
}

auto lower_unit_unsafe_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_unsafe_block(session, [&]() {
        return lower_unit_statement_block(statement.nested_statements, context, session, diagnostics, output);
    });
}

auto lower_guard_return_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (return_llvm_type == "void") {
        if (!is_empty_expression(statement.expression)) {
            diagnostics.error(statement.line, "lowering does not yet support return expressions in Unit guard failure blocks");
            return StatementFlow::failed;
        }
        if (!emit_function_return_cleanup(context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        output << "  ret void\n";
        return StatementFlow::terminated;
    }

    if (is_empty_expression(statement.expression)) {
        diagnostics.error(statement.line, "lowering does not yet support bare return statements in non-Unit guard failure blocks");
        return StatementFlow::failed;
    }

    auto lowered = lower_expression(
        statement.expression,
        return_llvm_type,
        return_signedness,
        context,
        session,
        output,
        return_source_type_name
    );
    if (!lowered.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this guard failure return",
                session.failures.expression
            )
        );
        return StatementFlow::failed;
    }

    release_returned_dynamic_array_local_cleanup(statement.expression, return_source_type_name, session.state);
    release_returned_choice_dynamic_array_payload_cleanup(
        statement.expression,
        return_source_type_name,
        context.lowering,
        session.state
    );
    release_returned_record_constructor_dynamic_array_field_cleanups(
        statement.expression,
        return_source_type_name,
        context.lowering,
        session.state
    );
    if (!emit_function_return_cleanup(context, session, diagnostics, output)) {
        return StatementFlow::failed;
    }
    output << "  ret " << lowered->type << " " << lowered->value << "\n";
    return StatementFlow::terminated;
}

auto lower_guard_statement_block(
    std::span<syntax::StatementSyntax const*> statements,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_nonvalue_statement_block(
        statements,
        "lowering does not yet support statements after a terminating guard failure statement",
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block,
        [&](syntax::StatementSyntax const& statement, bool is_last_statement) {
            if (statement.kind == syntax::StatementKind::return_statement) {
                return lower_guard_return_statement(
                    statement,
                    return_llvm_type,
                    return_signedness,
                    return_source_type_name,
                    context,
                    session,
                    diagnostics,
                    output
                );
            }
            if (statement.kind == syntax::StatementKind::if_statement) {
                return lower_nonvoid_if_statement(
                    statement,
                    return_llvm_type,
                    return_signedness,
                    return_source_type_name,
                    context,
                    session,
                    diagnostics,
                    output
                );
            }
            if (statement.kind == syntax::StatementKind::switch_statement) {
                return lower_nonvoid_switch_statement(
                    statement,
                    return_llvm_type,
                    return_signedness,
                    return_source_type_name,
                    context,
                    session,
                    diagnostics,
                    output
                );
            }
            if (statement.kind == syntax::StatementKind::guard_statement) {
                return lower_guard_statement(
                    statement,
                    return_llvm_type,
                    return_signedness,
                    return_source_type_name,
                    context,
                    session,
                    diagnostics,
                    output
                );
            }
            return lower_unit_statement(statement, is_last_statement, context, session, diagnostics, output);
        }
    );
}

auto lower_guard_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = statement_pointers_for(statements);
    return lower_guard_statement_block(
        statement_pointers,
        return_llvm_type,
        return_signedness,
        return_source_type_name,
        context,
        session,
        diagnostics,
        output
    );
}

auto lower_guard_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = statement_pointers_for(statements);
    return lower_guard_statement_block(
        statement_pointers,
        return_llvm_type,
        return_signedness,
        return_source_type_name,
        context,
        session,
        diagnostics,
        output
    );
}

auto lower_guard_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto condition = lower_expression(
        statement.expression,
        "i1",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!condition.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this guard condition",
                session.failures.expression
            )
        );
        return StatementFlow::failed;
    }

    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const failure_block = llvm_block_name("guard.failure", block_index);
    auto const merge_block = llvm_block_name("guard.merge", block_index);
    emit_llvm_conditional_branch(output, condition->value, merge_block, failure_block);

    emit_llvm_block_label(output, failure_block);
    session.state.current_block = failure_block;
    [[maybe_unused]] auto failure_scope = BranchBindingScope(session.state);
    auto failure_flow = lower_guard_statement_block(
        statement.nested_statements,
        return_llvm_type,
        return_signedness,
        return_source_type_name,
        context,
        session,
        diagnostics,
        output
    );
    if (failure_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (failure_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, merge_block);
    }

    failure_scope.reset();
    emit_llvm_block_label(output, merge_block);
    session.state.current_block = merge_block;
    return StatementFlow::falls_through;
}

auto lower_nonvoid_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_nonvalue_if_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        "lowering does not yet support this non-void if condition",
        true,
        [&]() {
            return lower_guard_statement_block(
                statement.nested_statements,
                return_llvm_type,
                return_signedness,
                return_source_type_name,
                context,
                session,
                diagnostics,
                output
            );
        },
        [&]() {
            return lower_guard_statement_block(
                statement.alternate_statements,
                return_llvm_type,
                return_signedness,
                return_source_type_name,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
}

auto lower_nonvoid_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    std::optional<std::string_view> return_source_type_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto subject_type = infer_unit_expression_type(statement.expression, context, session.state);
    if (!subject_type.has_value()) {
        diagnostics.error(statement.line, "lowering does not yet support this non-void switch subject");
        return StatementFlow::failed;
    }

    return lower_nonvalue_switch_statement(
        statement,
        *subject_type,
        context,
        session,
        diagnostics,
        output,
        "lowering does not yet support this non-void switch subject",
        "lowering does not yet support this non-void switch statement",
        [&](LoweredSwitchCasePlan const& planned_case) {
            return lower_guard_statement_block(
                planned_case.syntax->statements,
                return_llvm_type,
                return_signedness,
                return_source_type_name,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
}

auto lower_unit_statement(
    syntax::StatementSyntax const& statement,
    bool is_last_statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto common_flow = lower_common_nonvalue_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block,
        infer_unit_binding_type,
        "lowering does not yet support this Unit let binding",
        "lowering does not yet support this Unit var binding",
        [&](syntax::StatementSyntax const& nested_statement) {
            return lower_unit_repeat_statement(nested_statement, context, session, diagnostics, output);
        },
        [&](syntax::StatementSyntax const& nested_statement) {
            return lower_unit_for_statement(nested_statement, context, session, diagnostics, output);
        },
        [&](syntax::StatementSyntax const& nested_statement) {
            return lower_unit_unsafe_statement(nested_statement, context, session, diagnostics, output);
        }
    );
    if (common_flow.has_value()) {
        return *common_flow;
    }
    if (statement.kind == syntax::StatementKind::return_statement) {
        if (!is_empty_expression(statement.expression)) {
            diagnostics.error(statement.line, "lowering does not yet support return expressions in Unit functions");
            return StatementFlow::failed;
        }
        if (!emit_function_return_cleanup(context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        output << "  ret void\n";
        return StatementFlow::terminated;
    }
    if (statement.kind == syntax::StatementKind::switch_statement) {
        return lower_unit_switch_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::guard_statement) {
        return lower_guard_statement(
            statement,
            "void",
            IntegerSignedness::not_integer,
            std::nullopt,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_unit_if_statement(statement, context, session, diagnostics, output);
    }
    (void)is_last_statement;

    diagnostics.error(statement.line, "lowering does not yet support this statement");
    return StatementFlow::failed;
}

void preserve_function_emission_metadata(
    FunctionLoweringState const& state,
    FunctionEmissionResult* result,
    std::vector<ConsumedDescriptorFinalizationPlan>* consumed_descriptor_finalization_plans
) {
    if (consumed_descriptor_finalization_plans == nullptr) {
        return;
    }
    if (result != nullptr) {
        result->emitted_dynamic_array_cleanup_obligations =
            state.emitted_dynamic_array_cleanup_obligations;
        result->emitted_dynamic_array_cleanup_sequence_plans =
            state.emitted_dynamic_array_cleanup_sequence_plans;
        result->emitted_dynamic_array_cleanup_sequence_verifications =
            state.emitted_dynamic_array_cleanup_sequence_verifications;
        result->emitted_dynamic_array_cleanup_emission_capabilities =
            state.emitted_dynamic_array_cleanup_emission_capabilities;
        result->aggregate_projection_access_plans =
            state.aggregate_projection_access_plans;
        result->computed_dynamic_array_inserted_cleanup_handoffs =
            state.computed_dynamic_array_inserted_cleanup_handoffs;
        result->computed_dynamic_array_cleanup_call_operands =
            state.computed_dynamic_array_cleanup_call_operands;
        result->generated_module_symbols =
            state.pending_generated_module_symbols;
    }
    consumed_descriptor_finalization_plans->insert(
        consumed_descriptor_finalization_plans->end(),
        state.consumed_descriptor_finalization_plans.begin(),
        state.consumed_descriptor_finalization_plans.end()
    );
}

void finish_function_emission(
    FunctionLoweringState const& state,
    FunctionEmissionResult* result,
    std::vector<ConsumedDescriptorFinalizationPlan>* consumed_descriptor_finalization_plans,
    std::ostringstream& output
) {
    output << "}\n";
    for (auto const& definition : state.pending_function_definitions) {
        output << "\n" << definition;
    }
    preserve_function_emission_metadata(state, result, consumed_descriptor_finalization_plans);
}

void emit_function_body(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    EmissionContext const& context,
    semantics::SemanticAnalysisResult const* semantic_result,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FunctionEmissionResult* result,
    std::vector<ConsumedDescriptorFinalizationPlan>* consumed_descriptor_finalization_plans
) {
    if (!function.generic_parameters.empty()) {
        diagnostics.error(function.line, "lowering does not yet support generic functions");
        return;
    }
    if (signature.return_type.empty()) {
        if (auto choice_diagnostic = unsupported_choice_abi_diagnostic(
                function.return_type,
                context.lowering,
                "function return type"
            )) {
            diagnostics.error(function.line, *choice_diagnostic);
            return;
        }
        diagnostics.error(function.line, "lowering does not yet support this function return type");
        return;
    }
    if (signature.parameter_types.size() != function.parameters.size() ||
        signature.parameter_signedness.size() != function.parameters.size()) {
        diagnostics.error(function.line, "lowering does not yet support this function parameter type");
        return;
    }
    for (auto index = std::size_t {0}; index < signature.parameter_types.size(); ++index) {
        auto const& parameter_type = signature.parameter_types[index];
        if (parameter_type.empty() || parameter_type == "void") {
            if (index < function.parameters.size()) {
                if (auto diagnostic = unsupported_dynamic_array_parameter_diagnostic(function.parameters[index])) {
                    diagnostics.error(function.line, *diagnostic);
                    return;
                }
                if (auto choice_diagnostic = unsupported_choice_abi_diagnostic(
                        function.parameters[index].type,
                        context.lowering,
                        "function parameter type"
                    )) {
                    diagnostics.error(function.line, *choice_diagnostic);
                    return;
                }
            }
            diagnostics.error(function.line, "lowering does not yet support this function parameter type");
            return;
        }
        if (index < function.parameters.size() &&
            !dynamic_array_owned_parameter_has_drop_proof(function.parameters[index], context.options)) {
            if (auto diagnostic = unsupported_dynamic_array_parameter_diagnostic(function.parameters[index])) {
                diagnostics.error(function.line, *diagnostic);
                return;
            }
        }
    }

    output << "define " << signature.return_type << " @" << signature.symbol_name << "(";
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << signature.parameter_types[index] << " "
               << llvm_local_value_name(function.parameters[index].name);
    }
    output << ") {\n";
    output << "entry:\n";

    auto state = FunctionLoweringState {};
    auto failures = LoweringFailures {};
    auto session = FunctionLoweringSession {
        .state = state,
        .failures = failures,
        .semantics = semantic_result,
        .enclosing_symbol_name = signature.symbol_name,
    };
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        state.parameter_names.push_back(function.parameters[index].name);
        state.local_name_counts[function.parameters[index].name] = 1;
        state.immutable_bindings.emplace(function.parameters[index].name, LoweredExpression {
            .type = signature.parameter_types[index],
            .value = llvm_local_value_name(function.parameters[index].name),
            .signedness = signature.parameter_signedness[index],
        });
        auto source_type_name = render_source_type_name(function.parameters[index].type);
        if (is_receiver_self_source_type(source_type_name)) {
            if (auto concrete_source_type =
                    source_type_name_for_llvm_type(signature.parameter_types[index], context.lowering)) {
                source_type_name = std::move(*concrete_source_type);
            }
        }
        state.source_type_names.emplace(function.parameters[index].name, std::move(source_type_name));
        if (is_exclusive_receiver_parameter(function, function.parameters[index])) {
            state.exclusive_receiver_bindings.insert(function.parameters[index].name);
        }
        if (is_exclusive_receiver_parameter(function, function.parameters[index]) &&
            signature.parameter_types[index] == "ptr") {
            state.addressable_bindings[function.parameters[index].name] = AddressableBinding {
                .type = LoweredType {
                    .type = std::string {dynamic_array_descriptor_llvm_type()},
                    .signedness = IntegerSignedness::not_integer,
                },
                .storage = llvm_local_value_name(function.parameters[index].name),
            };
        } else {
            bind_addressable_aggregate_value(
                function.parameters[index].name,
                LoweredExpression {
                    .type = signature.parameter_types[index],
                    .value = llvm_local_value_name(function.parameters[index].name),
                    .signedness = signature.parameter_signedness[index],
                },
                session,
                output
            );
        }
        seed_bound_dynamic_array_parameter_cleanup_owner(
            function.parameters[index].name,
            state.source_type_names.at(function.parameters[index].name),
            context,
            session
        );
    }
    [[maybe_unused]] auto function_scope = DeferredCleanupScope {state};

    if (signature.return_type == "void") {
        auto flow = lower_unit_statement_block(function.body_statements, context, session, diagnostics, output);
        if (flow == StatementFlow::failed) {
            return;
        }

        if (flow == StatementFlow::falls_through) {
            if (!emit_function_return_cleanup(context, session, diagnostics, output)) {
                return;
            }
            output << "  ret void\n";
        }
        finish_function_emission(state, result, consumed_descriptor_finalization_plans, output);
        return;
    }

    auto const* expression = static_cast<syntax::ExpressionSyntax const*>(nullptr);
    auto lowered_final_statement = std::optional<LoweredExpression> {};
    auto return_source_type_name = signature.source_return_type_name.empty()
        ? std::optional<std::string_view> {}
        : std::optional<std::string_view> {signature.source_return_type_name};
    auto attempted_final_control_flow = false;
    auto final_statement_line = function.line;
    auto leading_statement_flow = StatementFlow::falls_through;
    auto propagate_leading_statement_flow = [&leading_statement_flow](StatementFlow flow) -> bool {
        if (flow == StatementFlow::failed) {
            return false;
        }
        if (flow == StatementFlow::terminated) {
            leading_statement_flow = StatementFlow::terminated;
        }
        return true;
    };
    auto body_statement_pointers = statement_pointers_for(function.body_statements);
    for (auto index = std::size_t {0}; index < function.body_statements.size(); ++index) {
        auto const& statement = function.body_statements[index];
        auto is_last_statement = index + 1 == function.body_statements.size();
        auto tail_scope = SiblingStatementTailScope {
            session.state,
            statement_pointer_span(body_statement_pointers),
            index,
        };
        auto function_tail_scope = FunctionStatementTailScope {
            session.state,
            statement_pointer_span(body_statement_pointers),
            index,
        };
        if (leading_statement_flow == StatementFlow::terminated) {
            diagnostics.error(
                statement.line,
                "lowering does not yet support statements after a terminating non-Unit statement"
            );
            return;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::let_binding) {
            if (statement.expression.kind == syntax::ExpressionKind::thread ||
                statement.expression.kind == syntax::ExpressionKind::task) {
                if (!lower_let_statement(
                        statement,
                        std::string(concurrency_handle_llvm_type()),
                        IntegerSignedness::not_integer,
                        context,
                        session,
                        diagnostics,
                        output
                    )) {
                    return;
                }
                continue;
            }
            auto type = infer_unit_binding_type(statement, context, session.state);
            if (!type.has_value()) {
                if (!statement.annotated_type.name.empty()) {
                    if (!lower_let_statement(
                            statement,
                            "",
                            IntegerSignedness::not_integer,
                            context,
                            session,
                            diagnostics,
                            output
                        )) {
                        preserve_function_emission_metadata(
                            state,
                            result,
                            consumed_descriptor_finalization_plans
                        );
                        return;
                    }
                    continue;
                } else if (is_concurrency_expression(statement.expression)) {
                    diagnostics.error(
                        statement.line,
                        "lowering does not yet support " +
                            concurrency_expression_name(statement.expression) + " expressions"
                    );
                } else {
                    diagnostics.error(
                        statement.line,
                        append_generic_record_constructor_inference_detail(
                            "lowering does not yet support this let binding",
                            statement.expression,
                            context,
                            session
                        )
                    );
                }
                return;
            }
            if (!lower_let_statement(
                    statement,
                    type->type,
                    type->signedness,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                preserve_function_emission_metadata(
                    state,
                    result,
                    consumed_descriptor_finalization_plans
                );
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::var_binding) {
            if (!lower_var_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::assignment_statement) {
            if (!lower_assignment_statement(
                    statement,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                return;
            }
            continue;
        }
        if (statement.kind == syntax::StatementKind::defer_statement) {
            if (!record_deferred_cleanup(statement, session, diagnostics)) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::while_statement) {
            if (!lower_while_statement(statement, context, session, diagnostics, output)) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::repeat_statement) {
            if (!propagate_leading_statement_flow(
                    lower_unit_repeat_statement(statement, context, session, diagnostics, output)
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::for_statement) {
            if (!propagate_leading_statement_flow(
                    lower_unit_for_statement(statement, context, session, diagnostics, output)
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::unsafe_statement) {
            if (!propagate_leading_statement_flow(
                    lower_unit_unsafe_statement(statement, context, session, diagnostics, output)
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::expression_statement &&
            statement.expression.kind == syntax::ExpressionKind::call) {
            if (!lower_call_statement(statement, context, session, diagnostics, output)) {
                return;
            }
            continue;
        }
        if (statement.kind == syntax::StatementKind::guard_statement) {
            if (!propagate_leading_statement_flow(lower_guard_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    return_source_type_name,
                    context,
                    session,
                    diagnostics,
                    output
                ))) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::if_statement) {
            if (!propagate_leading_statement_flow(lower_nonvoid_if_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    return_source_type_name,
                    context,
                    session,
                    diagnostics,
                    output
                ))) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::switch_statement) {
            if (!propagate_leading_statement_flow(lower_nonvoid_switch_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    return_source_type_name,
                    context,
                    session,
                    diagnostics,
                    output
                ))) {
                return;
            }
            continue;
        }

        if (is_last_statement) {
            if (statement.kind == syntax::StatementKind::if_statement ||
                statement.kind == syntax::StatementKind::switch_statement) {
                attempted_final_control_flow = true;
                final_statement_line = statement.line;
                lowered_final_statement = lower_final_control_flow_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    context,
                    session,
                    diagnostics,
                    output,
                    return_source_type_name
                );
                break;
            }
            expression = value_expression_for(statement);
            break;
        }

        diagnostics.error(statement.line, "lowering does not yet support this statement");
        return;
    }

    if (attempted_final_control_flow && !lowered_final_statement.has_value()) {
        diagnostics.error(
            final_statement_line,
            append_control_flow_lowering_failure(
                "lowering does not yet support this final control-flow statement",
                failures.control_flow
            )
        );
        return;
    }

    if (!lowered_final_statement.has_value() && expression == nullptr) {
        diagnostics.error(
            function.line,
            "lowering requires supported leading statements followed by a return or final expression"
        );
        return;
    }

    auto lowered = std::move(lowered_final_statement);
    if (!lowered.has_value()) {
        if (auto diagnostic = owned_aggregate_projection_value_read_diagnostic(
                *expression,
                context.lowering,
                context,
                session.state
            ); !diagnostic.empty()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                std::move(diagnostic)
            );
            diagnostics.error(
                expression->line,
                append_expression_lowering_failure(
                    "lowering does not yet support this return expression",
                    failures.expression
                )
            );
            preserve_function_emission_metadata(
                state,
                result,
                consumed_descriptor_finalization_plans
            );
            return;
        }
        lowered = lower_expression(
            *expression,
            signature.return_type,
            signature.return_signedness,
            context,
            session,
            output,
            return_source_type_name
        );
    }
    if (!lowered.has_value()) {
        if (expression != nullptr && is_thread_join_expression(*expression, session.state)) {
            diagnostics.error(
                expression->line,
                "lowering does not yet support thread join expressions"
            );
            return;
        }
        diagnostics.error(
            expression != nullptr ? expression->line : function.line,
            append_expression_lowering_failure(
                "lowering does not yet support this return expression",
                failures.expression
            )
        );
        return;
    }

    if (expression != nullptr) {
        release_returned_dynamic_array_local_cleanup(*expression, return_source_type_name, session.state);
        release_returned_choice_dynamic_array_payload_cleanup(
            *expression,
            return_source_type_name,
            context.lowering,
            session.state
        );
        release_returned_record_constructor_dynamic_array_field_cleanups(
            *expression,
            return_source_type_name,
            context.lowering,
            session.state
        );
    }
    if (!emit_function_return_cleanup(context, session, diagnostics, output)) {
        return;
    }
    output << "  ret " << lowered->type << " " << lowered->value << "\n";
    finish_function_emission(state, result, consumed_descriptor_finalization_plans, output);
}

}  // namespace

auto lower_unit_deferred_cleanup_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_unit_statement_block(statements, context, session, diagnostics, output);
}

auto emit_function_with_metadata(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    semantics::SemanticAnalysisResult const& semantic_result,
    diagnostics::DiagnosticBag& diagnostics,
    LlvmIrEmissionOptions const& options
) -> FunctionEmissionResult {
    auto output = std::ostringstream {};
    auto context = EmissionContext {
        .lowering = lowering_context,
        .string_constants = string_constants,
        .options = options,
    };
    auto result = FunctionEmissionResult {};
    result.function_symbol_name = signature.symbol_name;
    emit_function_body(
        function,
        signature,
        context,
        &semantic_result,
        diagnostics,
        output,
        &result,
        &result.consumed_descriptor_finalization_plans
    );
    result.ir_text = output.str();
    return result;
}

auto emit_function(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    semantics::SemanticAnalysisResult const& semantic_result,
    diagnostics::DiagnosticBag& diagnostics,
    LlvmIrEmissionOptions const& options
) -> std::string {
    return emit_function_with_metadata(
        function,
        signature,
        lowering_context,
        string_constants,
        semantic_result,
        diagnostics,
        options
    ).ir_text;
}

auto emit_function(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    diagnostics::DiagnosticBag& diagnostics,
    LlvmIrEmissionOptions const& options
) -> std::string {
    auto semantic_result = semantics::SemanticAnalysisResult {};
    return emit_function(
        function,
        signature,
        lowering_context,
        string_constants,
        semantic_result,
        diagnostics,
        options
    );
}

}  // namespace orison::lowering
