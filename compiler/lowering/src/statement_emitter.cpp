#include "orison/lowering/statement_emitter.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/cleanup_plan_owner.hpp"
#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/direct_dynamic_array_receiver.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/for_loop_lowering.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_failure_lifecycle.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/maybe_value_emitter.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/statement_body_lowering.hpp"
#include "orison/lowering/statement_pointer_adapter.hpp"

#include "orison/semantics/drop_model.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

struct FixedArraySourceType {
    std::string element_source_type_name;
    std::size_t length = 0;
};

auto lower_prefix_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

auto lower_prefix_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    for (auto index = std::size_t {0}; index < statements.size(); ++index) {
        auto const* statement = statements[index];
        auto tail_scope = SiblingStatementTailScope {
            session.state,
            statements,
            index,
        };
        if (statement == nullptr ||
            !lower_prefix_statement(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return false;
        }
    }
    return true;
}

auto is_thread_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::thread;
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

auto reject_owned_aggregate_projection_value_read(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session
) -> bool {
    auto diagnostic = owned_aggregate_projection_value_read_diagnostic(
        expression,
        context.lowering,
        context,
        session.state
    );
    if (diagnostic.empty()) {
        return false;
    }

    record_expression_lowering_failure(
        session.failures,
        ExpressionLoweringFailureReason::unsupported_expression,
        std::move(diagnostic)
    );
    return true;
}

auto is_task_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task;
}

auto is_dynamic_array_default_constructor(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::call &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name &&
        expression.left->text == "DynamicArray" &&
        expression.arguments.empty();
}

auto is_moved_owned_dynamic_array_binding(
    std::string_view owner_name,
    FunctionLoweringState const& state
) -> bool {
    auto name = std::string(owner_name);
    auto source_type = state.source_type_names.find(name);
    return source_type != state.source_type_names.end() &&
        dynamic_array_element_source_type_name(source_type->second).has_value() &&
        is_owned_binding_consumed(state.ownership_transfers, name);
}

auto is_dynamic_array_source_type(std::string_view source_type_name) -> bool {
    auto sequence = dynamic_sequence_source_type(source_type_name);
    return sequence.has_value() && sequence->kind == DynamicSequenceKind::dynamic_array;
}

auto is_bound_dynamic_array_parameter(
    std::string_view owner_name,
    FunctionLoweringState const& state
) -> bool {
    auto const name = std::string {owner_name};
    if (std::ranges::find(state.parameter_names, name) == state.parameter_names.end()) {
        return false;
    }

    auto source_type = state.source_type_names.find(name);
    return source_type != state.source_type_names.end() &&
        is_dynamic_array_source_type(source_type->second);
}

auto consumed_owned_push_argument_name(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::optional<std::string> {
    if (!is_owned_transfer_source_type(expected_source_type, context.lowering)) {
        return std::nullopt;
    }

    if (argument.kind == syntax::ExpressionKind::name) {
        auto actual_source_type = session.state.source_type_names.find(argument.text);
        if (actual_source_type != session.state.source_type_names.end() &&
            actual_source_type->second == expected_source_type) {
            return argument.text;
        }
        return std::nullopt;
    }

    auto path = collect_named_aggregate_path(argument);
    if (!path.has_value() || path->base_expression == nullptr) {
        return std::nullopt;
    }

    auto field_names = std::vector<std::string> {};
    field_names.reserve(path->steps.size());
    for (auto const& step : path->steps) {
        if (step.kind != AggregatePathStepKind::member) {
            return std::nullopt;
        }
        field_names.push_back(step.field_name);
    }

    auto owner_source_type = session.state.source_type_names.find(path->base_expression->text);
    if (owner_source_type == session.state.source_type_names.end()) {
        return std::nullopt;
    }

    auto transfer = owned_record_member_path_transfer(
        path->base_expression->text,
        owner_source_type->second,
        field_names,
        context.lowering
    );
    if (!transfer.has_value() || transfer->source_type_name != expected_source_type) {
        return std::nullopt;
    }

    return transfer->binding_name;
}

auto normalize_fixed_array_element_owner_name(std::string_view owner_name) -> std::string {
    auto output = std::string {};
    output.reserve(owner_name.size());

    for (auto index = std::size_t {0}; index < owner_name.size();) {
        auto const segment_start = index == 0 || owner_name[index - 1] == '.';
        if (segment_start && owner_name.substr(index, 7) == "element") {
            auto cursor = index + 7;
            auto saw_digit = false;
            while (cursor < owner_name.size() &&
                   std::isdigit(static_cast<unsigned char>(owner_name[cursor])) != 0) {
                saw_digit = true;
                ++cursor;
            }
            if (saw_digit && (cursor == owner_name.size() || owner_name[cursor] == '.')) {
                output += "element";
                index = cursor;
                continue;
            }
        }

        output.push_back(owner_name[index]);
        ++index;
    }

    return output;
}

auto authorized_dynamic_array_element_drop_symbol_name(
    std::string_view owner_name,
    std::string_view element_source_type_name,
    LoweringEmissionContext const& context
) -> std::optional<std::string> {
    auto symbol_name = semantics::drop_abi_symbol_name(element_source_type_name);
    auto element_owner_name = std::string {owner_name};
    element_owner_name += ".element";
    auto const normalized_element_owner_name = normalize_fixed_array_element_owner_name(element_owner_name);
    for (auto const& authorization : context.options.semantic_drop_lowering_authorizations) {
        if (authorization.authorized &&
            authorization.site.source_type_name == element_source_type_name &&
            authorization.site.abi_symbol_name == symbol_name &&
            (authorization.site.owner_name == element_owner_name ||
             normalize_fixed_array_element_owner_name(authorization.site.owner_name) == normalized_element_owner_name ||
             owner_name == "this")) {
            return symbol_name;
        }
    }
    return std::nullopt;
}

auto has_authorized_dynamic_array_element_drop_type(
    std::string_view element_source_type_name,
    LoweringEmissionContext const& context
) -> bool {
    auto symbol_name = semantics::drop_abi_symbol_name(element_source_type_name);
    for (auto const& authorization : context.options.semantic_drop_lowering_authorizations) {
        if (authorization.authorized &&
            authorization.site.source_type_name == element_source_type_name &&
            authorization.site.abi_symbol_name == symbol_name) {
            return true;
        }
    }
    return false;
}

auto aggregate_assignment_target_failure(
    std::string_view operation,
    AggregatePathError error
) -> std::string {
    return "lowering aggregate assignment " + std::string(operation) + " failed: " +
        std::string(render_aggregate_path_error(error));
}

auto emit_view_descriptor_field_projection(
    std::string_view result_name,
    std::string_view descriptor_value_name,
    std::size_t field_index
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_name << " = extractvalue ";
    output << view_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", " << field_index << "\n";
    return output.str();
}

auto lower_dynamic_array_default_construction(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    bool mutable_binding
) -> bool {
    if (!context.options.enable_dynamic_array_construction_lowering ||
        statement.annotated_type.name.empty()) {
        return false;
    }

    auto source_type_name = render_source_type_name(statement.annotated_type);
    if (!is_dynamic_array_source_type(source_type_name) ||
        !is_dynamic_array_default_constructor(statement.expression)) {
        return false;
    }

    auto plan = plan_dynamic_array_construction(source_type_name, 0, context.lowering);
    if (!plan.has_value()) {
        diagnostics.error(statement.line, "source dynamic array construction could not be planned");
        return true;
    }
    plan->owner_name = statement.name;

    auto allocation = next_llvm_local_value_name(
        statement.name + ".dynamic_array_alloc",
        session.state.local_name_counts
    );
    auto storage = next_llvm_local_value_name(
        statement.name + ".addr",
        session.state.local_name_counts
    );
    auto storage_name = storage;
    output << emit_dynamic_array_allocation_call(*plan, allocation);
    output << emit_dynamic_array_descriptor_binding(*plan, storage, allocation);

    auto descriptor_type = std::string {dynamic_array_descriptor_llvm_type()};
    auto lowered_type = LoweredType {
        .type = descriptor_type,
        .signedness = IntegerSignedness::not_integer,
    };
    if (mutable_binding) {
        session.state.mutable_bindings[statement.name] = MutableBinding {
            .type = std::move(lowered_type),
            .storage = storage_name,
        };
    } else {
        session.state.immutable_bindings[statement.name] = LoweredExpression {
            .type = descriptor_type,
            .value = allocation,
            .signedness = IntegerSignedness::not_integer,
        };
        session.state.addressable_bindings[statement.name] = AddressableBinding {
            .type = std::move(lowered_type),
            .storage = storage_name,
        };
    }
    session.state.source_type_names[statement.name] = std::move(source_type_name);
    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        statement.name,
        session.state.source_type_names[statement.name],
        context.lowering
    );
    if (!cleanup_plan.has_value()) {
        diagnostics.error(statement.line, "source dynamic array cleanup could not be planned");
        return true;
    }
    cleanup_plan->descriptor_storage_name = std::move(storage_name);
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    cleanup_plan->source_line = statement.line;
    session.state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));
    return true;
}

auto seed_dynamic_array_local_cleanup_plan(
    std::string_view owner_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    auto const source_type = session.state.source_type_names.find(std::string(owner_name));
    if (source_type == session.state.source_type_names.end() ||
        !is_dynamic_array_source_type(source_type->second)) {
        return true;
    }
    if (auto existing = std::find_if(
            session.state.dynamic_array_local_cleanup_plans.begin(),
            session.state.dynamic_array_local_cleanup_plans.end(),
            [&](DynamicArrayDescriptorCleanupPlan const& plan) {
                return plan.owner_name == owner_name &&
                    plan.source_type_name == source_type->second &&
                    plan.descriptor_storage_status ==
                        DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
            }
        ); existing != session.state.dynamic_array_local_cleanup_plans.end()) {
        return true;
    }

    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        return true;
    }

    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        owner_name,
        source_type->second,
        context.lowering
    );
    if (!cleanup_plan.has_value()) {
        diagnostics.error(source_line, "source dynamic array cleanup could not be planned");
        return false;
    }
    cleanup_plan->descriptor_storage_name = std::move(*storage);
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    cleanup_plan->source_line = source_line;
    session.state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));
    return true;
}

auto parse_size_literal(std::string const& text) -> std::optional<std::size_t> {
    if (text.empty()) {
        return std::nullopt;
    }

    auto value = std::size_t {0};
    for (auto const character : text) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        auto const digit = static_cast<std::size_t>(character - '0');
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }
    return value;
}

auto fixed_array_source_type(std::string_view type_name) -> std::optional<FixedArraySourceType> {
    constexpr auto prefix = std::string_view {"Array<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    auto arguments = split_top_level_generic_arguments(
        type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
    );
    if (arguments.size() != 2 || arguments[0].empty() || arguments[1].empty()) {
        return std::nullopt;
    }

    auto length = parse_size_literal(arguments[1]);
    if (!length.has_value()) {
        return std::nullopt;
    }

    return FixedArraySourceType {
        .element_source_type_name = std::move(arguments[0]),
        .length = *length,
    };
}

auto seed_dynamic_array_cleanup_plan_for_storage(
    std::string_view owner_name,
    std::string_view source_type_name,
    std::string descriptor_storage_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    auto existing = std::find_if(
        session.state.dynamic_array_local_cleanup_plans.begin(),
        session.state.dynamic_array_local_cleanup_plans.end(),
        [&](DynamicArrayDescriptorCleanupPlan const& plan) {
            return plan.owner_name == owner_name &&
                plan.source_type_name == source_type_name &&
                plan.descriptor_storage_status == DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
        }
    );
    if (existing != session.state.dynamic_array_local_cleanup_plans.end()) {
        return true;
    }

    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        owner_name,
        source_type_name,
        context.lowering
    );
    if (!cleanup_plan.has_value()) {
        diagnostics.error(source_line, "source nested dynamic array cleanup could not be planned");
        return false;
    }
    cleanup_plan->descriptor_storage_name = std::move(descriptor_storage_name);
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    cleanup_plan->source_line = source_line;
    session.state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));
    return true;
}

auto source_type_has_dynamic_array_cleanup_descendant(
    std::string_view source_type_name,
    LoweringEmissionContext const& context,
    std::size_t depth = 0
) -> bool {
    if (depth > 16) {
        return false;
    }
    if (is_dynamic_array_source_type(source_type_name)) {
        return true;
    }
    if (auto array = fixed_array_source_type(source_type_name)) {
        return source_type_has_dynamic_array_cleanup_descendant(
            array->element_source_type_name,
            context,
            depth + 1
        );
    }
    auto const record = context.lowering.records.find(std::string(source_type_name));
    if (record == context.lowering.records.end()) {
        return false;
    }
    return std::any_of(
        record->second.fields.begin(),
        record->second.fields.end(),
        [&](LoweredRecordField const& field) {
            return source_type_has_dynamic_array_cleanup_descendant(
                field.source_type_name,
                context,
                depth + 1
            );
        }
    );
}

auto seed_record_type_dynamic_array_local_cleanup_plans(
    std::string_view owner_name,
    std::string_view source_type_name,
    std::string_view storage_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::size_t depth = 0
) -> bool {
    if (depth > 16) {
        return true;
    }

    if (auto array = fixed_array_source_type(source_type_name)) {
        auto array_type = lowered_type_for_source_type_name(source_type_name, context.lowering);
        auto element_type = lowered_type_for_source_type_name(array->element_source_type_name, context.lowering);
        if (!array_type.has_value() || !element_type.has_value()) {
            diagnostics.error(source_line, "source record-field array cleanup could not be planned");
            return false;
        }

        auto const element_is_dynamic_array = is_dynamic_array_source_type(array->element_source_type_name);
        auto const element_needs_cleanup = source_type_has_dynamic_array_cleanup_descendant(
            array->element_source_type_name,
            context,
            depth + 1
        );
        if (!element_needs_cleanup) {
            return true;
        }

        for (auto index = std::size_t {0}; index < array->length; ++index) {
            auto element_owner_name = std::string {owner_name};
            element_owner_name += ".element";
            element_owner_name += std::to_string(index);
            auto element_pointer = "%" + element_owner_name + ".addr" +
                std::to_string(session.state.next_temporary_index++);
            output << "  " << element_pointer << " = getelementptr " << array_type->type;
            output << ", ptr " << storage_name << ", i64 0, i64 " << index << "\n";

            if (element_is_dynamic_array) {
                if (!seed_dynamic_array_cleanup_plan_for_storage(
                        element_owner_name,
                        array->element_source_type_name,
                        element_pointer,
                        source_line,
                        context,
                        session,
                        diagnostics
                    )) {
                    return false;
                }
                continue;
            }

            if (!seed_record_type_dynamic_array_local_cleanup_plans(
                    element_owner_name,
                    array->element_source_type_name,
                    element_pointer,
                    source_line,
                    context,
                    session,
                    diagnostics,
                    output,
                    depth + 1
                )) {
                return false;
            }
        }
        return true;
    }

    auto const record = context.lowering.records.find(std::string(source_type_name));
    if (record == context.lowering.records.end()) {
        return true;
    }

    for (auto const& field : record->second.fields) {
        auto const field_is_dynamic_array = is_dynamic_array_source_type(field.source_type_name);
        auto const field_needs_cleanup = source_type_has_dynamic_array_cleanup_descendant(
            field.source_type_name,
            context,
            depth + 1
        );
        if (!field_needs_cleanup) {
            continue;
        }

        auto field_owner_name = std::string {owner_name};
        field_owner_name += ".";
        field_owner_name += field.name;
        auto field_pointer = "%" + field_owner_name + ".addr" +
            std::to_string(session.state.next_temporary_index++);
        output << "  " << field_pointer << " = getelementptr " << record->second.llvm_type_name;
        output << ", ptr " << storage_name << ", i32 0, i32 " << field.index << "\n";

        if (field_is_dynamic_array) {
            if (!seed_dynamic_array_cleanup_plan_for_storage(
                    field_owner_name,
                    field.source_type_name,
                    field_pointer,
                    source_line,
                    context,
                    session,
                    diagnostics
                )) {
                return false;
            }
            continue;
        }

        if (!seed_record_type_dynamic_array_local_cleanup_plans(
                field_owner_name,
                field.source_type_name,
                field_pointer,
                source_line,
                context,
                session,
                diagnostics,
                output,
                depth + 1
            )) {
            return false;
        }
    }
    return true;
}

auto seed_record_field_dynamic_array_local_cleanup_plans(
    std::string_view owner_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto const source_type = session.state.source_type_names.find(std::string(owner_name));
    if (source_type == session.state.source_type_names.end()) {
        return true;
    }
    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        return true;
    }

    return seed_record_type_dynamic_array_local_cleanup_plans(
        owner_name,
        source_type->second,
        *storage,
        source_line,
        context,
        session,
        diagnostics,
        output
    );
}

auto seed_local_cleanup_plans(
    std::string_view owner_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    return seed_dynamic_array_local_cleanup_plan(
        owner_name,
        source_line,
        context,
        session,
        diagnostics
    ) && seed_record_field_dynamic_array_local_cleanup_plans(
        owner_name,
        source_line,
        context,
        session,
        diagnostics,
        output
    );
}

auto binary_instruction_for_assignment_operator(
    std::string const& assignment_operator,
    IntegerSignedness signedness
) -> std::optional<std::string_view> {
    if (assignment_operator == "+=") {
        return std::string_view {"add"};
    }
    if (assignment_operator == "-=") {
        return std::string_view {"sub"};
    }
    if (assignment_operator == "*=") {
        return std::string_view {"mul"};
    }
    if (assignment_operator == "/=") {
        if (signedness == IntegerSignedness::not_integer) {
            return std::nullopt;
        }
        return signedness == IntegerSignedness::signed_integer
            ? std::string_view {"sdiv"}
            : std::string_view {"udiv"};
    }
    if (assignment_operator == "%=") {
        if (signedness == IntegerSignedness::not_integer) {
            return std::nullopt;
        }
        return signedness == IntegerSignedness::signed_integer
            ? std::string_view {"srem"}
            : std::string_view {"urem"};
    }
    return std::nullopt;
}

auto lower_thread_let_statement(
    syntax::StatementSyntax const& statement,
    ConcurrencyPlanKind expected_kind,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto const expression_name = expected_kind == ConcurrencyPlanKind::thread
        ? std::string_view {"thread"}
        : std::string_view {"task"};
    if (session.semantics == nullptr) {
        diagnostics.error(
            statement.line,
            "lowering " + std::string(expression_name) + " expressions requires semantic capture analysis"
        );
        return false;
    }

    auto plan = plan_concurrency_expression(
        statement.expression,
        session.enclosing_symbol_name,
        session.state.next_concurrency_ordinal++,
        context,
        session.state,
        *session.semantics
    );
    if (!plan.has_value() || plan->kind != expected_kind) {
        diagnostics.error(
            statement.line,
            "lowering does not yet support this " + std::string(expression_name) + " expression"
        );
        return false;
    }
    apply_drop_cleanup_authorization_options(plan->cleanup.drop_cleanup, context.options);

    auto thunk_definition = emit_concurrency_entry_thunk(
        *plan,
        statement.expression,
        context,
        session,
        diagnostics
    );
    if (!thunk_definition.has_value()) {
        return false;
    }
    auto cleanup_definition = emit_concurrency_cleanup_thunk(*plan);

    auto const binding_prefix = statement.name + "." + std::string(expression_name);
    auto environment_storage = next_llvm_local_value_name(
        binding_prefix + ".env",
        session.state.local_name_counts
    );
    output << "  " << environment_storage << " = alloca " << plan->environment_layout.llvm_type << "\n";
    auto capture_store_result = emit_concurrency_capture_environment_stores(
        *plan,
        environment_storage,
        session.state,
        output
    );
    if (capture_store_result == ConcurrencyCaptureStoreEmissionResult::unsupported_capture_type) {
        diagnostics.error(
            statement.line,
            "lowering does not yet support this " + std::string(expression_name) + " capture type"
        );
        return false;
    }
    if (capture_store_result == ConcurrencyCaptureStoreEmissionResult::missing_capture_source) {
        diagnostics.error(
            statement.line,
            "lowering does not yet support this " + std::string(expression_name) + " capture source"
        );
        return false;
    }

    auto result_storage = next_llvm_local_value_name(
        binding_prefix + ".result",
        session.state.local_name_counts
    );
    output << "  " << result_storage << " = alloca " << plan->result_storage.llvm_type << "\n";

    auto handle_name = next_llvm_local_value_name(statement.name, session.state.local_name_counts);
    emit_concurrency_spawn(*plan, handle_name, environment_storage, result_storage, output);

    auto const spawn_block_index = next_llvm_block_index(session.state.next_block_index);
    auto const spawn_failed_block = llvm_block_name(binding_prefix + ".spawn_failed", spawn_block_index);
    auto const spawn_ok_block = llvm_block_name(binding_prefix + ".spawn_ok", spawn_block_index);
    emit_concurrency_spawn_failure_check(
        handle_name,
        spawn_failed_block,
        spawn_ok_block,
        session.state,
        output
    );
    session.state.current_block = spawn_ok_block;

    register_concurrency_binding(
        expected_kind,
        statement.name,
        std::move(handle_name),
        std::move(result_storage),
        plan->result_type,
        session.state
    );
    queue_concurrency_function_definitions(
        *plan,
        std::move(*thunk_definition),
        std::move(cleanup_definition),
        statement.line,
        session.state
    );
    return true;
}

struct LoweredAssignmentTarget {
    LoweredType type;
    std::string pointer;
    std::optional<std::string> source_type_name;
    std::optional<std::string> owner_name;
    bool consumes_owned_value_on_store = false;
};

auto lower_exclusive_view_index_assignment_target(
    syntax::ExpressionSyntax const& target,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredAssignmentTarget> {
    if (target.kind != syntax::ExpressionKind::index_access ||
        target.left == nullptr ||
        target.left->kind != syntax::ExpressionKind::name ||
        target.arguments.size() != 1) {
        return std::nullopt;
    }

    auto const& owner_name = target.left->text;
    auto source_type = session.state.source_type_names.find(owner_name);
    if (source_type == session.state.source_type_names.end()) {
        return std::nullopt;
    }
    auto sequence = dynamic_sequence_source_type(source_type->second);
    if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::exclusive_view) {
        return std::nullopt;
    }

    auto element_type = lowered_type_for_source_type_name(
        sequence->element_source_type_name,
        context.lowering
    );
    if (!element_type.has_value()) {
        diagnostics.error(target.line, "lowering exclusive View assignment element type is unsupported");
        return std::nullopt;
    }

    auto lowered_base = lower_expression(
        *target.left,
        std::string {view_descriptor_llvm_type()},
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!lowered_base.has_value()) {
        diagnostics.error(
            target.line,
            append_expression_lowering_failure(
                "lowering exclusive View assignment target failed",
                session.failures.expression
            )
        );
        return std::nullopt;
    }

    auto lowered_index = lower_expression(
        target.arguments.front(),
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        diagnostics.error(
            target.line,
            append_expression_lowering_failure(
                "lowering exclusive View assignment index failed",
                session.failures.expression
            )
        );
        return std::nullopt;
    }

    auto prefix = "%" + owner_name + ".view_assign" +
        std::to_string(session.state.next_temporary_index++);
    output << emit_view_descriptor_field_projection(prefix + ".data", lowered_base->value, 0);
    output << emit_view_descriptor_field_projection(prefix + ".length", lowered_base->value, 1);
    output << emit_dynamic_array_bounds_check(
        prefix + ".in_bounds",
        lowered_index->value,
        prefix + ".length",
        DynamicArrayBoundsCheckKind::index_within_length
    );
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto value_block = llvm_block_name("view.assign.in_bounds", block_index);
    auto failure_block = llvm_block_name("view.assign.out_of_bounds", block_index);
    emit_llvm_conditional_branch(output, prefix + ".in_bounds", value_block, failure_block);
    emit_llvm_block_label(output, failure_block);
    output << "  call void @__orison_dynamic_array_bounds_failed()\n";
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, value_block);
    session.state.current_block = value_block;
    output << "  " << prefix << ".element.addr = getelementptr " << element_type->type;
    output << ", ptr " << prefix << ".data, i64 " << lowered_index->value << "\n";

    return LoweredAssignmentTarget {
        .type = std::move(*element_type),
        .pointer = prefix + ".element.addr",
        .source_type_name = sequence->element_source_type_name,
    };
}

auto lower_dynamic_array_index_assignment_target(
    syntax::ExpressionSyntax const& target,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredAssignmentTarget> {
    if (target.kind != syntax::ExpressionKind::index_access ||
        target.left == nullptr ||
        target.left->kind != syntax::ExpressionKind::name ||
        target.arguments.size() != 1) {
        return std::nullopt;
    }

    auto const& owner_name = target.left->text;
    if (is_moved_owned_dynamic_array_binding(owner_name, session.state)) {
        diagnostics.error(target.line, "use after move: " + owner_name);
        return std::nullopt;
    }
    auto const owner_is_mutable_local = session.state.mutable_bindings.contains(owner_name);
    auto const owner_is_exclusive_receiver =
        owner_name == "this" && session.state.exclusive_receiver_bindings.contains(owner_name);
    if (!owner_is_mutable_local && !owner_is_exclusive_receiver) {
        return std::nullopt;
    }

    auto source_type = session.state.source_type_names.find(owner_name);
    if (source_type == session.state.source_type_names.end()) {
        return std::nullopt;
    }
    auto sequence = dynamic_sequence_source_type(source_type->second);
    if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array ||
        !sequence->owns_storage) {
        return std::nullopt;
    }
    auto element_drop_symbol_name = std::optional<std::string> {};
    auto const element_requires_ownership_transfer =
        is_owned_transfer_source_type(sequence->element_source_type_name, context.lowering);
    if (element_requires_ownership_transfer) {
        element_drop_symbol_name = authorized_dynamic_array_element_drop_symbol_name(
            owner_name,
            sequence->element_source_type_name,
            context
        );
        if (!element_drop_symbol_name.has_value()) {
            diagnostics.error(
                target.line,
                "lowering DynamicArray assignment to owned element requires authorized replacement drop"
            );
            return std::nullopt;
        }
    }

    auto element_type = lowered_type_for_source_type_name(
        sequence->element_source_type_name,
        context.lowering
    );
    if (!element_type.has_value()) {
        diagnostics.error(target.line, "lowering DynamicArray assignment element type is unsupported");
        return std::nullopt;
    }

    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        return std::nullopt;
    }
    auto lowered_index = lower_expression(
        target.arguments.front(),
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        diagnostics.error(
            target.line,
            append_expression_lowering_failure(
                "lowering DynamicArray assignment index failed",
                session.failures.expression
            )
        );
        return std::nullopt;
    }

    auto prefix = "%" + owner_name + ".dynamic_array_assign" +
        std::to_string(session.state.next_temporary_index++);
    output << emit_dynamic_array_descriptor_load(prefix + ".descriptor", *storage);
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".data",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".length",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_bounds_check(
        prefix + ".in_bounds",
        lowered_index->value,
        prefix + ".length",
        DynamicArrayBoundsCheckKind::index_within_length
    );
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto value_block = llvm_block_name("dynamic_array.assign.in_bounds", block_index);
    auto failure_block = llvm_block_name("dynamic_array.assign.out_of_bounds", block_index);
    emit_llvm_conditional_branch(output, prefix + ".in_bounds", value_block, failure_block);
    emit_llvm_block_label(output, failure_block);
    output << "  call void @__orison_dynamic_array_bounds_failed()\n";
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, value_block);
    session.state.current_block = value_block;
    output << "  " << prefix << ".element.addr = getelementptr " << element_type->type;
    output << ", ptr " << prefix << ".data, i64 " << lowered_index->value << "\n";
    if (element_drop_symbol_name.has_value()) {
        output << "  call void @" << *element_drop_symbol_name << "(ptr ";
        output << prefix << ".element.addr)\n";
    }

    return LoweredAssignmentTarget {
        .type = std::move(*element_type),
        .pointer = prefix + ".element.addr",
        .source_type_name = sequence->element_source_type_name,
        .consumes_owned_value_on_store = element_requires_ownership_transfer,
    };
}

auto lower_assignment_target(
    syntax::ExpressionSyntax const& target,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredAssignmentTarget> {
    if (target.kind == syntax::ExpressionKind::name) {
        auto binding = session.state.mutable_bindings.find(target.text);
        if (binding == session.state.mutable_bindings.end()) {
            diagnostics.error(target.line, "lowering assignment target is not a mutable local");
            return std::nullopt;
        }
        return LoweredAssignmentTarget {
            .type = binding->second.type,
            .pointer = binding->second.storage,
            .source_type_name = [&]() -> std::optional<std::string> {
                auto source_type = session.state.source_type_names.find(target.text);
                if (source_type == session.state.source_type_names.end()) {
                    return std::nullopt;
                }
                return source_type->second;
            }(),
            .owner_name = target.text,
        };
    }

    if (auto view_target = lower_exclusive_view_index_assignment_target(
            target,
            context,
            session,
            diagnostics,
            output
        )) {
        return view_target;
    }

    if (auto dynamic_array_target = lower_dynamic_array_index_assignment_target(
            target,
            context,
            session,
            diagnostics,
            output
        )) {
        return dynamic_array_target;
    }
    if (target.kind == syntax::ExpressionKind::index_access &&
        target.left != nullptr &&
        target.left->kind == syntax::ExpressionKind::name &&
        is_bound_dynamic_array_parameter(target.left->text, session.state)) {
        diagnostics.error(
            target.line,
            "lowering DynamicArray parameter indexed assignment is unsupported; use exclusive.View<T> for mutable "
            "parameter element writes"
        );
        return std::nullopt;
    }

    auto path = collect_aggregate_path(target);
    if (path.steps.empty() || path.base_expression == nullptr ||
        path.base_expression->kind != syntax::ExpressionKind::name) {
        diagnostics.error(
            target.line,
            "lowering only supports assignment to mutable local names and aggregate paths"
        );
        return std::nullopt;
    }

    auto const& base_expression = *path.base_expression;
    auto binding = session.state.mutable_bindings.find(base_expression.text);
    auto source_type = session.state.source_type_names.find(base_expression.text);
    if (source_type == session.state.source_type_names.end()) {
        diagnostics.error(
            target.line,
            "lowering aggregate assignment target type is unknown"
        );
        return std::nullopt;
    }

    auto current_source_type_name = source_type->second;
    auto current_pointer = std::string {};
    auto cleanup_owner_name = base_expression.text;
    if (auto pointee_source_type = pointer_pointee_source_type_name(current_source_type_name)) {
        auto lowered_base = lower_expression(
            base_expression,
            "ptr",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!lowered_base.has_value()) {
            diagnostics.error(
                target.line,
                append_expression_lowering_failure(
                    "lowering aggregate assignment target failed",
                    session.failures.expression
                )
            );
            return std::nullopt;
        }
        current_pointer = std::move(lowered_base->value);
        current_source_type_name = std::move(*pointee_source_type);
    } else {
        if (binding == session.state.mutable_bindings.end()) {
            diagnostics.error(target.line, "lowering assignment target is not a mutable local");
            return std::nullopt;
        }
        current_pointer = binding->second.storage;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(current_pointer),
        std::move(current_source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        diagnostics.error(target.line, "lowering aggregate assignment target type is unsupported");
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto result = advance_aggregate_path_member_with_temporary(
                *cursor,
                step.field_name,
                context.lowering,
                session.state.next_temporary_index,
                output
            );
            if (result.error != AggregatePathError::none) {
                diagnostics.error(
                    target.line,
                    aggregate_assignment_target_failure("member target", result.error)
                );
                return std::nullopt;
            }
            cleanup_owner_name += ".";
            cleanup_owner_name += step.field_name;
            continue;
        }

        if (step.index_expression == nullptr) {
            diagnostics.error(target.line, "lowering aggregate assignment index target is unsupported");
            return std::nullopt;
        }

        auto lowered_index = lower_expression(
            *step.index_expression,
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output
        );
        if (!lowered_index.has_value()) {
            diagnostics.error(
                target.line,
                append_expression_lowering_failure(
                    "lowering aggregate assignment index failed",
                    session.failures.expression
                )
            );
            return std::nullopt;
        }

        auto sequence = dynamic_sequence_source_type(cursor->source_type_name);
        if (sequence.has_value() && sequence->kind == DynamicSequenceKind::dynamic_array &&
            sequence->owns_storage) {
            auto element_type = lowered_type_for_source_type_name(
                sequence->element_source_type_name,
                context.lowering
            );
            if (!element_type.has_value()) {
                diagnostics.error(target.line, "lowering aggregate DynamicArray index target element type is unsupported");
                return std::nullopt;
            }

            auto prefix = "%" + cleanup_owner_name + ".dynamic_array_index" +
                std::to_string(session.state.next_temporary_index++);
            output << emit_dynamic_array_descriptor_load(prefix + ".descriptor", cursor->pointer);
            output << emit_dynamic_array_descriptor_field_projection(
                prefix + ".data",
                prefix + ".descriptor",
                DynamicArrayDescriptorField::data
            );
            output << emit_dynamic_array_descriptor_field_projection(
                prefix + ".length",
                prefix + ".descriptor",
                DynamicArrayDescriptorField::length
            );
            output << emit_dynamic_array_bounds_check(
                prefix + ".in_bounds",
                lowered_index->value,
                prefix + ".length",
                DynamicArrayBoundsCheckKind::index_within_length
            );
            auto block_index = next_llvm_block_index(session.state.next_block_index);
            auto value_block = llvm_block_name("dynamic_array.aggregate_index.in_bounds", block_index);
            auto failure_block = llvm_block_name("dynamic_array.aggregate_index.out_of_bounds", block_index);
            emit_llvm_conditional_branch(output, prefix + ".in_bounds", value_block, failure_block);
            emit_llvm_block_label(output, failure_block);
            output << "  call void @__orison_dynamic_array_bounds_failed()\n";
            emit_llvm_unreachable(output);
            emit_llvm_block_label(output, value_block);
            session.state.current_block = value_block;
            output << "  " << prefix << ".element.addr = getelementptr " << element_type->type;
            output << ", ptr " << prefix << ".data, i64 " << lowered_index->value << "\n";

            auto next_cursor = initialize_aggregate_path_cursor(
                prefix + ".element.addr",
                sequence->element_source_type_name,
                context.lowering
            );
            if (!next_cursor.has_value()) {
                diagnostics.error(target.line, "lowering aggregate DynamicArray index target type is unsupported");
                return std::nullopt;
            }
            cursor = std::move(*next_cursor);
            cleanup_owner_name += ".element";
            continue;
        }

        auto result = advance_aggregate_path_index_with_temporary(
            *cursor,
            lowered_index->value,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error == AggregatePathError::unsupported_element_source_type) {
            diagnostics.error(
                target.line,
                aggregate_assignment_target_failure("index target", result.error)
            );
            return std::nullopt;
        }
        if (result.error != AggregatePathError::none) {
            diagnostics.error(
                target.line,
                aggregate_assignment_target_failure("index target", result.error)
            );
            return std::nullopt;
        }
        cleanup_owner_name += ".element";
        if (step.index_expression->kind == syntax::ExpressionKind::integer_literal) {
            cleanup_owner_name += step.index_expression->text;
        }
    }

    auto lowered_type = lowered_type_for_source_type_name(cursor->source_type_name, context.lowering);
    if (!lowered_type.has_value()) {
        diagnostics.error(target.line, "lowering aggregate assignment target type is unsupported");
        return std::nullopt;
    }

    return LoweredAssignmentTarget {
        .type = std::move(*lowered_type),
        .pointer = std::move(cursor->pointer),
        .source_type_name = cursor->source_type_name,
        .owner_name = std::move(cleanup_owner_name),
    };
}

auto emit_dynamic_array_assignment_storage_cleanup(
    std::string_view owner_name,
    std::string_view source_type_name,
    std::string_view descriptor_storage_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

auto emit_dynamic_array_descriptor_assignment_storage_cleanup(
    std::string_view owner_name,
    std::string_view source_type_name,
    std::string_view descriptor_storage_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        owner_name,
        source_type_name,
        context.lowering
    );
    if (!cleanup_plan.has_value()) {
        diagnostics.error(source_line, "source dynamic array cleanup could not be planned");
        return false;
    }
    cleanup_plan->descriptor_storage_name = std::string {descriptor_storage_name};
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    cleanup_plan->source_line = source_line;

    auto element_drop_symbol_name = std::optional<std::string> {};
    if (!is_scalar_or_nonowning_source_type(cleanup_plan->element_source_type_name)) {
        element_drop_symbol_name = authorized_dynamic_array_element_drop_symbol_name(
            owner_name,
            cleanup_plan->element_source_type_name,
            context
        );
        if (!element_drop_symbol_name.has_value()) {
            diagnostics.error(
                source_line,
                "lowering DynamicArray assignment target cleanup requires authorized element drop"
            );
            return false;
        }
    }

    auto prefix = "%" + std::string {owner_name} + ".dynamic_array_reassign_cleanup" +
        std::to_string(session.state.next_temporary_index++);
    auto const label_prefix = prefix.substr(1);
    output << "  br label %" << label_prefix << ".cleanup.entry\n";
    output << label_prefix << ".cleanup.entry:\n";
    output << emit_dynamic_array_descriptor_load(prefix + ".descriptor", cleanup_plan->descriptor_storage_name);
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.data",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.length",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.capacity",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::capacity
    );
    if (element_drop_symbol_name.has_value()) {
        output << "  br label %" << label_prefix << ".drop.walk\n";
        output << label_prefix << ".drop.walk:\n";
        output << "  " << prefix << ".drop.index = phi i64 [ 0, %" << label_prefix
               << ".cleanup.entry ], [ " << prefix << ".drop.next, %" << label_prefix
               << ".drop.body ]\n";
        output << "  " << prefix << ".drop.more = icmp ult i64 " << prefix
               << ".drop.index, " << prefix << ".cleanup.length\n";
        output << "  br i1 " << prefix << ".drop.more, label %" << label_prefix
               << ".drop.body, label %" << label_prefix << ".drop.done\n";
        output << label_prefix << ".drop.body:\n";
        output << emit_dynamic_array_element_address(
            *cleanup_plan,
            prefix + ".drop.element.addr",
            prefix + ".cleanup.data",
            prefix + ".drop.index"
        );
        output << "  call void @" << *element_drop_symbol_name << "(ptr "
               << prefix << ".drop.element.addr)\n";
        output << "  " << prefix << ".drop.next = add i64 " << prefix << ".drop.index, 1\n";
        output << "  br label %" << label_prefix << ".drop.walk\n";
        output << label_prefix << ".drop.done:\n";
    }
    output << "  call void @__orison_dynamic_array_deallocate(ptr ";
    output << prefix << ".cleanup.data, i64 " << cleanup_plan->element_size_bytes;
    output << ", i64 " << prefix << ".cleanup.capacity)\n";
    session.state.current_block = element_drop_symbol_name.has_value()
        ? label_prefix + ".drop.done"
        : label_prefix + ".cleanup.entry";
    return true;
}

auto emit_dynamic_array_assignment_storage_cleanup(
    std::string_view owner_name,
    std::string_view source_type_name,
    std::string_view descriptor_storage_name,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (is_dynamic_array_source_type(source_type_name)) {
        return emit_dynamic_array_descriptor_assignment_storage_cleanup(
            owner_name,
            source_type_name,
            descriptor_storage_name,
            source_line,
            context,
            session,
            diagnostics,
            output
        );
    }

    if (auto array = fixed_array_source_type(source_type_name)) {
        if (!source_type_has_dynamic_array_cleanup_descendant(array->element_source_type_name, context)) {
            return true;
        }

        auto array_type = lowered_type_for_source_type_name(source_type_name, context.lowering);
        if (!array_type.has_value()) {
            diagnostics.error(source_line, "source fixed-array dynamic array cleanup could not be planned");
            return false;
        }

        for (auto index = std::size_t {0}; index < array->length; ++index) {
            auto element_owner_name = std::string {owner_name};
            element_owner_name += ".element";
            element_owner_name += std::to_string(index);
            auto element_pointer = "%" + element_owner_name + ".reassign.addr" +
                std::to_string(session.state.next_temporary_index++);
            output << "  " << element_pointer << " = getelementptr " << array_type->type;
            output << ", ptr " << descriptor_storage_name << ", i64 0, i64 " << index << "\n";
            if (!emit_dynamic_array_assignment_storage_cleanup(
                    element_owner_name,
                    array->element_source_type_name,
                    element_pointer,
                    source_line,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                return false;
            }
        }
    }

    auto const record = context.lowering.records.find(std::string(source_type_name));
    if (record != context.lowering.records.end()) {
        for (auto const& field : record->second.fields) {
            if (!source_type_has_dynamic_array_cleanup_descendant(field.source_type_name, context)) {
                continue;
            }

            auto field_owner_name = std::string {owner_name};
            field_owner_name += ".";
            field_owner_name += field.name;
            auto field_pointer = "%" + field_owner_name + ".reassign.addr" +
                std::to_string(session.state.next_temporary_index++);
            output << "  " << field_pointer << " = getelementptr " << record->second.llvm_type_name;
            output << ", ptr " << descriptor_storage_name << ", i32 0, i32 " << field.index << "\n";
            if (!emit_dynamic_array_assignment_storage_cleanup(
                    field_owner_name,
                    field.source_type_name,
                    field_pointer,
                    source_line,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                return false;
            }
        }
    }

    return true;
}

auto emit_dynamic_array_assignment_target_cleanup(
    LoweredAssignmentTarget const& target,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (!target.owner_name.has_value() || !target.source_type_name.has_value()) {
        return true;
    }

    return emit_dynamic_array_assignment_storage_cleanup(
        *target.owner_name,
        *target.source_type_name,
        target.pointer,
        source_line,
        context,
        session,
        diagnostics,
        output
    );
}

auto deferred_cleanup_block_for(
    syntax::StatementSyntax const& statement
) -> DeferredCleanupBlock {
    auto block = DeferredCleanupBlock {
        .line = statement.line,
    };
    block.statements.reserve(statement.nested_statements.size());
    for (auto const& nested_statement : statement.nested_statements) {
        block.statements.push_back(&nested_statement);
    }
    return block;
}

auto lower_prefix_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind == syntax::StatementKind::let_binding) {
        return lower_let_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::var_binding) {
        return lower_var_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::assignment_statement) {
        return lower_assignment_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::defer_statement) {
        return record_deferred_cleanup(statement, session, diagnostics);
    }
    if (statement.kind == syntax::StatementKind::for_statement) {
        auto lower_body = [&]() {
            auto statement_pointers = statement_pointers_for(statement.nested_statements);
            return lower_prefix_statement_block(
                statement_pointers,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            ) ? StatementFlow::falls_through : StatementFlow::failed;
        };
        if (statement.expression.kind == syntax::ExpressionKind::array_literal) {
            auto flow = lower_array_literal_for_statement(
                statement,
                context,
                session,
                diagnostics,
                output,
                infer_expression_type,
                lower_body
            );
            return flow != StatementFlow::failed;
        }

        auto source_type_name =
            source_type_name_for_expression(statement.expression, context.lowering, session.state);
        auto dynamic_sequence = source_type_name.has_value()
            ? dynamic_sequence_source_type(*source_type_name)
            : std::nullopt;
        if (dynamic_sequence.has_value() &&
            (dynamic_sequence_for_lowering_enabled(*dynamic_sequence, context.options) ||
             dynamic_sequence->kind == DynamicSequenceKind::dynamic_array)) {
            auto flow = lower_sequence_for_statement(
                statement,
                context,
                session,
                diagnostics,
                output,
                lower_body
            );
            return flow != StatementFlow::failed;
        }

        auto flow = lower_fixed_array_for_statement(
            statement,
            context,
            session,
            diagnostics,
            output,
            lower_body
        );
        return flow != StatementFlow::failed;
    }
    return false;
}

auto emit_value_block_deferred_cleanup(
    DeferredCleanupScope const& defer_scope,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> bool {
    return emit_deferred_cleanup_to_depth(
        defer_scope.cleanup_depth(),
        context,
        session,
        diagnostics,
        output,
        lower_cleanup_block
    );
}

auto lower_value_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block,
    std::optional<std::string_view> expected_source_type_name = std::nullopt
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }
    DeferredCleanupScope defer_scope(session.state);

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index];
        auto tail_scope = SiblingStatementTailScope {
            session.state,
            statements,
            index,
        };
        if (statement == nullptr ||
            !lower_prefix_statement(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const* final_statement = statements.back();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement ||
        final_statement->kind == syntax::StatementKind::switch_statement) {
        auto lowered = lower_final_control_flow(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output,
            expected_source_type_name
        );
        if (!lowered.has_value()) {
            return std::nullopt;
        }
        if (!emit_value_block_deferred_cleanup(
                defer_scope,
                context,
                session,
                diagnostics,
                output,
                lower_cleanup_block
            )) {
            return std::nullopt;
        }
        return lowered;
    }

    auto const* expression = value_expression_for(*final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    auto lowered = lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output,
        expected_source_type_name
    );
    if (!lowered.has_value()) {
        return std::nullopt;
    }
    if (!emit_value_block_deferred_cleanup(
            defer_scope,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return std::nullopt;
    }
    return lowered;
}

auto lower_void_call_statement(
    syntax::StatementSyntax const& statement,
    std::string const& function_name,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (function.parameter_types.size() != statement.expression.arguments.size()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            function_name + " expects " +
                std::to_string(function.parameter_types.size()) + " arguments, got " +
                std::to_string(statement.expression.arguments.size())
        );
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering call statement failed",
                session.failures.expression
            )
        );
        return false;
    }

    auto arguments = lower_call_arguments(
        statement.expression,
        function,
        context,
        session,
        output
    );
    if (!arguments.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_argument_failure,
            function_name
        );
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering call statement failed",
                session.failures.expression
            )
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    return true;
}

struct ResolvedMemberCall {
    MemberCallReceiverInference receiver;
    LoweredMethodLookup method;
};

auto resolve_member_call(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> ResolvedMemberCall {
    auto receiver = infer_member_call_receiver(expression, context.lowering, state);
    if (receiver.result != MemberCallReceiverInferenceResult::found) {
        return {.receiver = std::move(receiver)};
    }

    auto method = find_lowered_method_signature(
        context.lowering,
        receiver.receiver_type_name,
        receiver.method_name
    );
    return ResolvedMemberCall {
        .receiver = std::move(receiver),
        .method = std::move(method),
    };
}

auto member_call_target_name(ResolvedMemberCall const& resolved) -> std::string {
    return resolved.receiver.receiver_type_name + "." + resolved.receiver.method_name;
}

auto null_safe_member_call_void_target_name(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    auto resolved = infer_member_call_receiver(expression, context.lowering, state);
    if (resolved.result != MemberCallReceiverInferenceResult::found) {
        return std::nullopt;
    }

    auto payload_type = maybe_payload_source_type_name(resolved.receiver_type_name);
    if (!payload_type.has_value()) {
        return std::nullopt;
    }

    auto method = find_lowered_method_signature(
        context.lowering,
        *payload_type,
        resolved.method_name
    );
    if (method.result != LoweredMethodLookupResult::found ||
        method.method == nullptr ||
        method.method->signature.return_type != "void") {
        return std::nullopt;
    }

    return *payload_type + "." + resolved.method_name;
}

auto diagnose_member_call_statement(
    syntax::StatementSyntax const& statement,
    ResolvedMemberCall const& resolved,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweredFunctionSignature const* {
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
        diagnostics.error(statement.line, "lowering member call statement has unsupported receiver shape");
        return nullptr;
    }
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
        diagnostics.error(statement.line, "lowering member call receiver type is unknown");
        return nullptr;
    }

    auto const target_name = member_call_target_name(resolved);
    if (resolved.method.result == LoweredMethodLookupResult::not_found) {
        diagnostics.error(
            statement.line,
            "lowering member call target is unknown: " + target_name
        );
        return nullptr;
    }
    if (resolved.method.result == LoweredMethodLookupResult::ambiguous) {
        diagnostics.error(
            statement.line,
            "lowering member call target is ambiguous: " + target_name
        );
        return nullptr;
    }

    if (resolved.method.method == nullptr ||
        !has_supported_function_signature_types(resolved.method.method->signature)) {
        diagnostics.error(
            statement.line,
            "lowering member call target is not lowerable: " + target_name
        );
        return nullptr;
    }

    return &resolved.method.method->signature;
}

auto lower_void_member_call_statement(
    syntax::StatementSyntax const& statement,
    syntax::ExpressionSyntax const& receiver_expression,
    std::string_view receiver_type_name,
    std::string const& target_name,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto const expected_argument_count = function.parameter_types.empty()
        ? std::size_t {0}
        : function.parameter_types.size() - 1;
    if (expected_argument_count != statement.expression.arguments.size()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            target_name + " expects " + std::to_string(expected_argument_count) +
                " arguments, got " + std::to_string(statement.expression.arguments.size())
        );
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering member call statement failed",
                session.failures.expression
            )
        );
        return false;
    }

    auto direct_receiver = std::optional<DirectDynamicArrayReceiver> {};
    if (receiver_expression.kind != syntax::ExpressionKind::name &&
        dynamic_array_element_source_type_name(receiver_type_name).has_value()) {
        auto direct_receiver_lowering = lower_direct_dynamic_array_receiver(
            receiver_expression,
            function,
            receiver_type_name,
            context,
            session,
            session.failures,
            output,
            false
        );
        direct_receiver = std::move(direct_receiver_lowering.receiver);
        if (!direct_receiver.has_value()) {
            auto message = direct_receiver_lowering.diagnostic.empty()
                ? append_expression_lowering_failure(
                      "lowering DynamicArray receiver expression failed",
                      session.failures.expression
                  )
                : "lowering " + direct_receiver_lowering.diagnostic;
            diagnostics.error(statement.line, std::move(message));
            return false;
        }
    }

    auto arguments = direct_receiver.has_value()
        ? lower_member_call_arguments(
              direct_receiver->argument,
              std::span<syntax::ExpressionSyntax const>(
                  statement.expression.arguments.data(),
                  statement.expression.arguments.size()
              ),
              function,
              context,
              session,
              output
          )
        : lower_member_call_arguments(
              receiver_expression,
              std::span<syntax::ExpressionSyntax const>(
                  statement.expression.arguments.data(),
                  statement.expression.arguments.size()
              ),
              function,
              context,
              session,
              output
          );
    if (!arguments.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_argument_failure,
            target_name
        );
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering member call statement failed",
                session.failures.expression
            )
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    if (direct_receiver.has_value() &&
        !emit_local_dynamic_array_cleanups_for_names(
            context,
            session,
            output,
            {direct_receiver->cleanup_owner_name}
        )) {
        diagnostics.error(
            statement.line,
            "lowering DynamicArray receiver cleanup could not be emitted: " + target_name
        );
        return false;
    }
    return true;
}

auto lower_void_null_safe_member_call_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto resolved = infer_member_call_receiver(statement.expression, context.lowering, session.state);
    if (resolved.result != MemberCallReceiverInferenceResult::found) {
        diagnostics.error(statement.line, "lowering null-safe member call receiver type is unknown");
        return false;
    }

    auto payload_type = maybe_payload_source_type_name(resolved.receiver_type_name);
    if (!payload_type.has_value()) {
        diagnostics.error(statement.line, "lowering null-safe member call receiver is not Maybe");
        return false;
    }

    auto method_lookup = find_lowered_method_signature(
        context.lowering,
        *payload_type,
        resolved.method_name
    );
    auto const target_name = *payload_type + "." + resolved.method_name;
    if (method_lookup.result == LoweredMethodLookupResult::not_found) {
        diagnostics.error(statement.line, "lowering member call target is unknown: " + target_name);
        return false;
    }
    if (method_lookup.result == LoweredMethodLookupResult::ambiguous) {
        diagnostics.error(statement.line, "lowering member call target is ambiguous: " + target_name);
        return false;
    }
    if (method_lookup.method == nullptr ||
        !has_supported_function_signature_types(method_lookup.method->signature) ||
        method_lookup.method->signature.return_type != "void") {
        diagnostics.error(statement.line, "lowering member call target is not lowerable: " + target_name);
        return false;
    }

    auto const& function = method_lookup.method->signature;
    auto const expected_argument_count = function.parameter_types.empty()
        ? std::size_t {0}
        : function.parameter_types.size() - 1;
    if (expected_argument_count != statement.expression.arguments.size()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            target_name + " expects " + std::to_string(expected_argument_count) +
                " arguments, got " + std::to_string(statement.expression.arguments.size())
        );
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering member call statement failed",
                session.failures.expression
            )
        );
        return false;
    }

    auto receiver_abi = maybe_value_abi_for_source_type(resolved.receiver_type_name, context.lowering);
    if (!receiver_abi.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering does not yet support null-safe member call receiver ABI: " + target_name
        );
        return false;
    }

    if (statement.expression.left == nullptr || statement.expression.left->left == nullptr) {
        diagnostics.error(statement.line, "lowering null-safe member call has unsupported receiver shape");
        return false;
    }

    auto lowered_receiver = lower_expression(
        *statement.expression.left->left,
        receiver_abi->llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output,
        resolved.receiver_type_name
    );
    if (!lowered_receiver.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering member call statement failed",
                session.failures.expression
            )
        );
        return false;
    }

    auto const merge_block = llvm_block_name(
        "nullsafe.call.merge",
        next_llvm_block_index(session.state.next_block_index)
    );
    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const some_block = llvm_block_name("nullsafe.call.some", block_index);
    auto const empty_block = llvm_block_name("nullsafe.call.empty", block_index);

    auto tag_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag_name << " = extractvalue " << receiver_abi->llvm_type << " "
           << lowered_receiver->value << ", 0\n";
    output << "  br i1 " << tag_name << ", label %" << some_block
           << ", label %" << empty_block << "\n";

    session.state.current_block = empty_block;
    output << empty_block << ":\n";
    output << "  br label %" << merge_block << "\n";

    session.state.current_block = some_block;
    output << some_block << ":\n";
    auto payload_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << payload_name << " = extractvalue " << receiver_abi->llvm_type << " "
           << lowered_receiver->value << ", 1\n";
    auto receiver_payload = LoweredExpression {
        .type = receiver_abi->payload_llvm_type,
        .value = std::move(payload_name),
        .signedness = IntegerSignedness::not_integer,
    };

    auto arguments = lower_member_call_arguments(
        std::move(receiver_payload),
        std::span<syntax::ExpressionSyntax const>(
            statement.expression.arguments.data(),
            statement.expression.arguments.size()
        ),
        function,
        context,
        session,
        output
    );
    if (!arguments.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_argument_failure,
            target_name
        );
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering member call statement failed",
                session.failures.expression
            )
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    output << "  br label %" << merge_block << "\n";
    output << merge_block << ":\n";
    session.state.current_block = merge_block;
    return true;
}

auto lower_dynamic_array_push_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto const& expression = statement.expression;
    if (!context.options.enable_dynamic_array_append_lowering ||
        expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::member_access ||
        expression.left->text != "push" ||
        expression.left->left == nullptr ||
        expression.left->left->kind != syntax::ExpressionKind::name ||
        expression.arguments.size() != 1) {
        return false;
    }

    auto const& owner_name = expression.left->left->text;
    if (is_moved_owned_dynamic_array_binding(owner_name, session.state)) {
        diagnostics.error(statement.line, "use after move: " + owner_name);
        return true;
    }
    auto const owner_is_mutable_local = session.state.mutable_bindings.contains(owner_name);
    auto const owner_is_exclusive_receiver =
        owner_name == "this" && session.state.exclusive_receiver_bindings.contains(owner_name);
    if (!owner_is_mutable_local && !owner_is_exclusive_receiver) {
        if (is_bound_dynamic_array_parameter(owner_name, session.state)) {
            diagnostics.error(
                statement.line,
                "lowering DynamicArray parameter push is unsupported; pass an owned local DynamicArray<T> or use "
                "exclusive.View<T> for mutable parameter element writes"
            );
            return true;
        }
        return false;
    }
    auto source_type = session.state.source_type_names.find(owner_name);
    if (source_type == session.state.source_type_names.end()) {
        return false;
    }
    auto element_source_type = dynamic_array_element_source_type_name(source_type->second);
    if (!element_source_type.has_value()) {
        return false;
    }
    auto const element_requires_ownership_transfer =
        is_owned_transfer_source_type(*element_source_type, context.lowering);
    if (element_requires_ownership_transfer &&
        !has_authorized_dynamic_array_element_drop_type(*element_source_type, context)) {
        diagnostics.error(
            statement.line,
            "lowering DynamicArray push to owned element requires authorized element drop: owner " +
                owner_name + " element " + *element_source_type
        );
        return true;
    }
    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        return false;
    }
    auto plan = plan_dynamic_array_construction(source_type->second, 0, context.lowering);
    if (!plan.has_value()) {
        diagnostics.error(statement.line, "source dynamic array append could not be planned");
        return true;
    }
    plan->owner_name = owner_name;

    auto value_type = lowered_type_for_source_type_name(*element_source_type, context.lowering);
    if (!value_type.has_value()) {
        diagnostics.error(statement.line, "source dynamic array append element type is not lowerable");
        return true;
    }
    auto value = lower_expression(
        expression.arguments.front(),
        value_type->type,
        value_type->signedness,
        context,
        session,
        output,
        std::optional<std::string_view> {*element_source_type}
    );
    if (!value.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering dynamic array push argument failed",
                session.failures.expression
            )
        );
        return true;
    }

    auto prefix = "%" + owner_name + ".dynamic_array_append" +
        std::to_string(session.state.next_temporary_index++);
    output << emit_dynamic_array_descriptor_load(
        prefix + ".descriptor",
        *storage
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".data",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".length",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".capacity",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::capacity
    );
    output << emit_dynamic_array_bounds_check(
        prefix + ".has_capacity",
        prefix + ".length",
        prefix + ".capacity",
        DynamicArrayBoundsCheckKind::append_has_capacity
    );
    auto const append_entry_block = session.state.current_block;
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto append_block = llvm_block_name("dynamic_array.append.ready", block_index);
    auto grow_block = llvm_block_name("dynamic_array.append.grow", block_index);
    emit_llvm_conditional_branch(
        output,
        prefix + ".has_capacity",
        append_block,
        grow_block
    );
    emit_llvm_block_label(output, grow_block);
    output << "  " << prefix << ".capacity.is_zero = icmp eq i64 " << prefix << ".capacity, 0\n";
    output << "  " << prefix << ".doubled.capacity = mul i64 " << prefix << ".capacity, 2\n";
    output << "  " << prefix << ".next.capacity = select i1 " << prefix << ".capacity.is_zero";
    output << ", i64 1, i64 " << prefix << ".doubled.capacity\n";
    output << emit_dynamic_array_grow_call(
        *plan,
        prefix + ".grown",
        prefix + ".descriptor",
        prefix + ".next.capacity"
    );
    output << "  br label %" << append_block << "\n";
    emit_llvm_block_label(output, append_block);
    session.state.current_block = append_block;
    output << "  " << prefix << ".active = phi " << dynamic_array_descriptor_llvm_type();
    output << " [ " << prefix << ".descriptor, %" << append_entry_block << " ],";
    output << " [ " << prefix << ".grown, %" << grow_block << " ]\n";
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".active.data",
        prefix + ".active",
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".active.length",
        prefix + ".active",
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_element_address(
        *plan,
        prefix + ".element.addr",
        prefix + ".active.data",
        prefix + ".active.length"
    );
    output << emit_dynamic_array_element_store(
        *plan,
        value->value,
        prefix + ".element.addr"
    );
    output << emit_dynamic_array_descriptor_length_update(
        prefix + ".updated",
        prefix + ".next.length",
        prefix + ".active",
        prefix + ".active.length"
    );
    output << emit_dynamic_array_descriptor_write_back(
        prefix + ".updated",
        *storage
    );
    if (auto consumed_name = consumed_owned_push_argument_name(
            expression.arguments.front(),
            *element_source_type,
            context,
            session
        )) {
        mark_owned_binding_consumed(session.state.ownership_transfers, std::move(*consumed_name));
    }
    return true;
}

}  // namespace

auto value_expression_for(
    syntax::StatementSyntax const& statement
) -> syntax::ExpressionSyntax const* {
    if (statement.kind == syntax::StatementKind::return_statement ||
        statement.kind == syntax::StatementKind::expression_statement) {
        return &statement.expression;
    }
    return nullptr;
}

auto lower_let_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (lower_dynamic_array_default_construction(
            statement,
            context,
            session,
            diagnostics,
            output,
            false
        )) {
        return !diagnostics.has_errors();
    }

    if (is_thread_expression(statement.expression)) {
        return lower_thread_let_statement(
            statement,
            ConcurrencyPlanKind::thread,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (is_task_expression(statement.expression)) {
        return lower_thread_let_statement(
            statement,
            ConcurrencyPlanKind::task,
            context,
            session,
            diagnostics,
            output
        );
    }

    auto type = LoweredType {
        .type = std::string(expected_llvm_type),
        .signedness = expected_signedness,
    };
    auto annotated_source_type_name = std::optional<std::string> {};
    if (!statement.annotated_type.name.empty()) {
        annotated_source_type_name = render_source_type_name(statement.annotated_type);
        auto annotated_type = lowered_type_for_source_type_name(*annotated_source_type_name, context.lowering);
        if (!annotated_type.has_value() || annotated_type->type == "void") {
            if (auto choice_diagnostic = unsupported_choice_abi_diagnostic(
                    statement.annotated_type,
                    context.lowering,
                    "let binding type"
                )) {
                diagnostics.error(statement.line, *choice_diagnostic);
                return false;
            }
            diagnostics.error(
                statement.line,
                "lowering does not yet support let type: " + *annotated_source_type_name
            );
            return false;
        }
        type = std::move(*annotated_type);
    } else if (auto inferred = infer_expression_type(statement.expression, context, session.state)) {
        type = std::move(*inferred);
        annotated_source_type_name =
            source_type_name_for_expression(statement.expression, context.lowering, session.state);
    }

    if (reject_owned_aggregate_projection_value_read(statement.expression, context, session)) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this let initializer",
                session.failures.expression
            )
        );
        return false;
    }

    auto lowered = lower_expression(
        statement.expression,
        type.type,
        type.signedness,
        context,
        session,
        output,
        annotated_source_type_name
    );
    if (!lowered.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this let initializer",
                session.failures.expression
            )
        );
        return false;
    }
    release_moved_owned_cleanup_expression(statement.expression, context.lowering, session.state);
    auto const direct_binding = lowered->type == "ptr" || is_aggregate_llvm_type(lowered->type);
    auto local_name = direct_binding
        ? lowered->value
        : next_llvm_local_value_name(statement.name, session.state.local_name_counts);
    if (!direct_binding) {
        output << "  " << local_name << " = add " << lowered->type << " 0, " << lowered->value << "\n";
    }
    session.state.immutable_bindings[statement.name] = LoweredExpression {
        .type = lowered->type,
        .value = std::move(local_name),
        .signedness = lowered->signedness,
    };
    bind_addressable_aggregate_value(
        statement.name,
        session.state.immutable_bindings.at(statement.name),
        session,
        output
    );
    if (!statement.annotated_type.name.empty()) {
        session.state.source_type_names[statement.name] =
            render_source_type_name(statement.annotated_type);
    } else if (auto inferred_source_type =
                   source_type_name_for_initializer(
                       statement.expression,
                       context.lowering,
                       session.state,
                       lowered->type
                   )) {
        session.state.source_type_names[statement.name] = std::move(*inferred_source_type);
    }
    return seed_local_cleanup_plans(
        statement.name,
        statement.line,
        context,
        session,
        diagnostics,
        output
    );
}

auto lower_var_statement(
    syntax::StatementSyntax const& statement,
    std::string_view fallback_llvm_type,
    IntegerSignedness fallback_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (lower_dynamic_array_default_construction(
            statement,
            context,
            session,
            diagnostics,
            output,
            true
        )) {
        return !diagnostics.has_errors();
    }

    auto type = LoweredType {
        .type = std::string(fallback_llvm_type),
        .signedness = fallback_signedness,
    };
    auto annotated_source_type_name = std::optional<std::string> {};
    if (!statement.annotated_type.name.empty()) {
        annotated_source_type_name = render_source_type_name(statement.annotated_type);
        auto annotated_type = lowered_type_for_source_type_name(*annotated_source_type_name, context.lowering);
        if (!annotated_type.has_value() || annotated_type->type == "void") {
            if (auto choice_diagnostic = unsupported_choice_abi_diagnostic(
                    statement.annotated_type,
                    context.lowering,
                    "var binding type"
                )) {
                diagnostics.error(statement.line, *choice_diagnostic);
                return false;
            }
            diagnostics.error(
                statement.line,
                "lowering does not yet support var type: " + *annotated_source_type_name
            );
            return false;
        }
        type = std::move(*annotated_type);
    } else if (auto inferred = infer_expression_type(statement.expression, context, session.state)) {
        type = std::move(*inferred);
        annotated_source_type_name =
            source_type_name_for_expression(statement.expression, context.lowering, session.state);
    }

    if (reject_owned_aggregate_projection_value_read(statement.expression, context, session)) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this var initializer",
                session.failures.expression
            )
        );
        return false;
    }

    auto lowered = lower_expression(
        statement.expression,
        type.type,
        type.signedness,
        context,
        session,
        output,
        annotated_source_type_name
    );
    if (!lowered.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this var initializer",
                session.failures.expression
            )
        );
        return false;
    }
    release_moved_owned_cleanup_expression(statement.expression, context.lowering, session.state);

    auto storage = next_llvm_local_value_name(
        statement.name + ".addr",
        session.state.local_name_counts
    );
    output << "  " << storage << " = alloca " << type.type << "\n";
    output << "  store " << type.type << " " << lowered->value << ", ptr " << storage << "\n";
    session.state.mutable_bindings[statement.name] = MutableBinding {
        .type = std::move(type),
        .storage = std::move(storage),
    };
    if (!statement.annotated_type.name.empty()) {
        session.state.source_type_names[statement.name] =
            render_source_type_name(statement.annotated_type);
    } else if (auto inferred_source_type =
                   source_type_name_for_initializer(
                       statement.expression,
                       context.lowering,
                       session.state,
                       lowered->type
                   )) {
        session.state.source_type_names[statement.name] = std::move(*inferred_source_type);
    }
    return seed_local_cleanup_plans(
        statement.name,
        statement.line,
        context,
        session,
        diagnostics,
        output
    );
}

auto lower_assignment_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto target = lower_assignment_target(
        statement.assignment_target,
        context,
        session,
        diagnostics,
        output
    );
    if (!target.has_value()) {
        return false;
    }

    auto lowered = std::optional<LoweredExpression> {};
    if (statement.assignment_operator != "=") {
        auto instruction = binary_instruction_for_assignment_operator(
            statement.assignment_operator,
            target->type.signedness
        );
        if (!instruction.has_value() || !is_integer_llvm_type(target->type.type) ||
            target->type.signedness == IntegerSignedness::not_integer) {
            diagnostics.error(
                statement.line,
                "lowering compound assignment operator '" + statement.assignment_operator +
                    "' requires an integer assignment target"
            );
            return false;
        }

        auto current_value = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << current_value << " = load " << target->type.type << ", ptr "
               << target->pointer << "\n";
        auto right = lower_expression(
            statement.expression,
            target->type.type,
            target->type.signedness,
            context,
            session,
            output,
            target->source_type_name
        );
        if (!right.has_value()) {
            diagnostics.error(
                statement.line,
                append_expression_lowering_failure(
                    "lowering does not yet support this assignment value",
                    session.failures.expression
                )
            );
            return false;
        }

        auto computed_value = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << computed_value << " = " << *instruction << " " << target->type.type
               << " " << current_value << ", " << right->value << "\n";
        lowered = LoweredExpression {
            .type = target->type.type,
            .value = std::move(computed_value),
            .signedness = target->type.signedness,
        };
    } else {
        lowered = lower_expression(
            statement.expression,
            target->type.type,
            target->type.signedness,
            context,
            session,
            output,
            target->source_type_name
        );
    }
    if (!lowered.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this assignment value",
                session.failures.expression
            )
        );
        return false;
    }

    if (!emit_dynamic_array_assignment_target_cleanup(
            *target,
            statement.line,
            context,
            session,
            diagnostics,
            output
        )) {
        return false;
    }
    output << "  store " << target->type.type << " " << lowered->value
           << ", ptr " << target->pointer << "\n";
    if (target->consumes_owned_value_on_store && target->source_type_name.has_value()) {
        if (auto consumed_name = consumed_owned_push_argument_name(
                statement.expression,
                *target->source_type_name,
                context,
                session
            )) {
            mark_owned_binding_consumed(session.state.ownership_transfers, std::move(*consumed_name));
        }
    }
    return true;
}

auto lower_call_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind != syntax::StatementKind::expression_statement ||
        statement.expression.kind != syntax::ExpressionKind::call) {
        diagnostics.error(statement.line, "lowering call statement requires a call expression");
        return false;
    }
    if (statement.expression.left != nullptr &&
        statement.expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
        if (auto void_target = null_safe_member_call_void_target_name(
                statement.expression,
                context,
                session.state
            )) {
            static_cast<void>(*void_target);
            return lower_void_null_safe_member_call_statement(
                statement,
                context,
                session,
                diagnostics,
                output
            );
        }

        auto source_type = source_type_name_for_expression(
            statement.expression,
            context.lowering,
            session.state
        );
        if (!source_type.has_value()) {
            diagnostics.error(
                statement.line,
                "lowering does not yet support this null-safe call statement result type"
            );
            return false;
        }
        auto type = lowered_type_for_source_type_name(*source_type, context.lowering);
        if (!type.has_value() || type->type.empty()) {
            diagnostics.error(
                statement.line,
                "lowering does not yet support this null-safe call statement result type"
            );
            return false;
        }
        auto lowered = lower_expression(
            statement.expression,
            type->type,
            type->signedness,
            context,
            session,
            output,
            std::optional<std::string_view> {*source_type}
        );
        if (!lowered.has_value()) {
            diagnostics.error(
                statement.line,
                append_expression_lowering_failure(
                    "lowering does not yet support this null-safe call statement",
                    session.failures.expression
                )
            );
            return false;
        }
        return true;
    }
    if (statement.expression.left != nullptr &&
        statement.expression.left->kind == syntax::ExpressionKind::member_access) {
        if (lower_dynamic_array_push_statement(statement, context, session, diagnostics, output)) {
            return !diagnostics.has_errors();
        }
        auto resolved = resolve_member_call(statement.expression, context, session.state);
        auto function = diagnose_member_call_statement(statement, resolved, diagnostics);
        if (function == nullptr) {
            return false;
        }

        auto const target_name = member_call_target_name(resolved);
        if (function->return_type == "void") {
            return lower_void_member_call_statement(
                statement,
                *statement.expression.left->left,
                resolved.receiver.receiver_type_name,
                target_name,
                *function,
                context,
                session,
                diagnostics,
                output
            );
        }

        auto lowered = lower_expression(
            statement.expression,
            function->return_type,
            function->return_signedness,
            context,
            session,
            output
        );
        if (!lowered.has_value()) {
            diagnostics.error(
                statement.line,
                append_expression_lowering_failure(
                    "lowering does not yet support this call statement",
                    session.failures.expression
                )
            );
            return false;
        }
        return true;
    }

    auto type = infer_expression_type(statement.expression, context, session.state);
    if (!type.has_value() || type->type.empty()) {
        diagnostics.error(statement.line, "lowering does not yet support this call statement result type");
        return false;
    }
    if (type->type == "void") {
        if (statement.expression.left != nullptr &&
            statement.expression.left->kind == syntax::ExpressionKind::name &&
            (statement.expression.left->text == "raw_write" ||
             statement.expression.left->text == "volatile_write")) {
            auto lowered = lower_expression(
                statement.expression,
                "void",
                IntegerSignedness::not_integer,
                context,
                session,
                output
            );
            if (!lowered.has_value()) {
                diagnostics.error(
                    statement.line,
                    append_expression_lowering_failure(
                        "lowering does not yet support this call statement",
                        session.failures.expression
                    )
                );
                return false;
            }
            return true;
        }
        if (statement.expression.left == nullptr ||
            statement.expression.left->kind != syntax::ExpressionKind::name) {
            diagnostics.error(statement.line, "lowering call statement requires a direct function name");
            return false;
        }
        auto function = context.lowering.functions.find(statement.expression.left->text);
        if (function == context.lowering.functions.end()) {
            diagnostics.error(statement.line, "lowering call statement target is unknown");
            return false;
        }
        return lower_void_call_statement(
            statement,
            statement.expression.left->text,
            function->second,
            context,
            session,
            diagnostics,
            output
        );
    }
    auto lowered = lower_expression(
        statement.expression,
        type->type,
        type->signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this call statement",
                session.failures.expression
            )
        );
        return false;
    }
    return true;
}

auto record_deferred_cleanup(
    syntax::StatementSyntax const& statement,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    if (session.state.defer_cleanup_scopes.empty()) {
        diagnostics.error(statement.line, "lowering defer statements requires an active cleanup scope");
        return false;
    }

    session.state.defer_cleanup_scopes.back().blocks.push_back(deferred_cleanup_block_for(statement));
    return true;
}

auto lower_loop_control_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> bool {
    if (statement.kind != syntax::StatementKind::break_statement &&
        statement.kind != syntax::StatementKind::continue_statement) {
        diagnostics.error(statement.line, "loop-control lowering requires break or continue");
        return false;
    }
    if (session.state.loop_targets.empty()) {
        diagnostics.error(statement.line, "loop-control lowering requires an active loop");
        return false;
    }

    auto const& targets = session.state.loop_targets.back();
    auto const& target = statement.kind == syntax::StatementKind::break_statement
        ? targets.break_target
        : targets.continue_target;
    if (!emit_deferred_cleanup_to_depth(
            targets.defer_cleanup_depth,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return false;
    }
    emit_llvm_branch(output, target);
    return true;
}

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto statement_pointers = statement_pointers_for(statements);
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow,
        lower_cleanup_block,
        expected_source_type_name
    );
}

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }
    DeferredCleanupScope defer_scope(session.state);

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr ||
            !lower_prefix_statement(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const* final_statement = statements.back();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement ||
        final_statement->kind == syntax::StatementKind::switch_statement) {
        auto lowered = lower_final_control_flow(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output,
            expected_source_type_name
        );
        if (!lowered.has_value()) {
            return std::nullopt;
        }
        if (!emit_value_block_deferred_cleanup(
                defer_scope,
                context,
                session,
                diagnostics,
                output,
                lower_cleanup_block
            )) {
            return std::nullopt;
        }
        return lowered;
    }

    auto const* expression = value_expression_for(*final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    auto lowered = lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output,
        expected_source_type_name
    );
    if (!lowered.has_value()) {
        return std::nullopt;
    }
    if (!emit_value_block_deferred_cleanup(
            defer_scope,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return std::nullopt;
    }
    return lowered;
}

auto lower_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto statement_pointers = statement_pointers_for(statements);
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow,
        lower_cleanup_block,
        expected_source_type_name
    );
}

}  // namespace orison::lowering
