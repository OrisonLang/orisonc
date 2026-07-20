#include "orison/lowering/llvm_ir_emitter.hpp"

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/llvm_ir_verifier.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/module_prelude.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/syntax_traversal.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

auto can_emit_record_layout(syntax::RecordSyntax const& record, LoweredRecordLayout const& layout) -> bool {
    if (!record.generic_parameters.empty() || layout.fields.size() != record.fields.size()) {
        return false;
    }
    for (auto const& field : layout.fields) {
        if (field.llvm_type.empty() || field.llvm_type == "void") {
            return false;
        }
    }
    return true;
}

auto emit_record_layouts(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::string {
    auto output = std::ostringstream {};
    auto emitted_record_names = std::unordered_set<std::string> {};
    for (auto const& record : module.records) {
        auto layout = context.records.find(record.name);
        if (layout == context.records.end() || !can_emit_record_layout(record, layout->second)) {
            continue;
        }

        output << layout->second.llvm_type_name << " = type { ";
        for (auto index = std::size_t {0}; index < layout->second.fields.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << layout->second.fields[index].llvm_type;
        }
        output << " }\n";
        emitted_record_names.insert(record.name);
    }

    auto instantiated_record_names = std::vector<std::string> {};
    for (auto const& [record_name, layout] : context.records) {
        if (emitted_record_names.contains(record_name)) {
            continue;
        }
        auto can_emit_layout = true;
        for (auto const& field : layout.fields) {
            if (field.llvm_type.empty() || field.llvm_type == "void") {
                can_emit_layout = false;
                break;
            }
        }
        if (can_emit_layout) {
            instantiated_record_names.push_back(record_name);
        }
    }
    std::ranges::sort(instantiated_record_names);
    for (auto const& record_name : instantiated_record_names) {
        auto const& layout = context.records.at(record_name);
        output << layout.llvm_type_name << " = type { ";
        for (auto index = std::size_t {0}; index < layout.fields.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << layout.fields[index].llvm_type;
        }
        output << " }\n";
    }
    return output.str();
}

auto is_uninstantiated_generic_function(syntax::FunctionSyntax const& function) -> bool {
    return !function.generic_parameters.empty();
}

auto has_authorized_dynamic_array_owned_element_cleanup(
    std::string const& owner_name,
    std::string const& element_source_type_name,
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> bool {
    auto const expected_owner_name = owner_name + ".element";
    auto const expected_symbol_name = "__orison_drop." + element_source_type_name;
    return std::ranges::any_of(
        authorizations,
        [&](semantics::DropLoweringAuthorization const& authorization) {
            return authorization.authorized &&
                authorization.site.owner_name == expected_owner_name &&
                authorization.site.source_type_name == element_source_type_name &&
                authorization.site.abi_symbol_name == expected_symbol_name;
        }
    );
}

auto can_lower_dynamic_array_parameter_descriptor(
    syntax::ParameterSyntax const& parameter,
    DynamicSequenceSourceType const& sequence,
    LlvmIrEmissionOptions const& options
) -> bool {
    return options.test_only_enable_dynamic_array_parameter_descriptors ||
        is_scalar_or_nonowning_source_type(sequence.element_source_type_name) ||
        has_authorized_dynamic_array_owned_element_cleanup(
            parameter.name,
            sequence.element_source_type_name,
            options.semantic_drop_lowering_authorizations
        );
}

void enable_dynamic_array_parameter_descriptors(
    syntax::ModuleSyntax const& module,
    LoweringContext& context,
    LlvmIrEmissionOptions const& options
) {
    for (auto const& function : module.functions) {
        auto signature = context.functions.find(function.name);
        if (signature == context.functions.end()) {
            continue;
        }
        for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
            if (index >= signature->second.parameter_types.size()) {
                continue;
            }
            auto source_type_name = render_source_type_name(function.parameters[index].type);
            auto sequence = dynamic_sequence_source_type(source_type_name);
            if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array) {
                continue;
            }
            if (!can_lower_dynamic_array_parameter_descriptor(function.parameters[index], *sequence, options)) {
                continue;
            }
            signature->second.parameter_types[index] = std::string {dynamic_array_descriptor_llvm_type()};
            signature->second.parameter_signedness[index] = IntegerSignedness::not_integer;
        }
    }
}

auto has_bound_dynamic_array_parameter_descriptor(
    semantics::DynamicArrayDescriptorOrigin const& origin,
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> bool {
    for (auto const& function : module.functions) {
        auto signature = context.functions.find(function.name);
        if (signature == context.functions.end()) {
            continue;
        }
        for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
            if (index >= signature->second.parameter_types.size()) {
                continue;
            }
            if (function.parameters[index].name != origin.owner_name) {
                continue;
            }
            if (render_source_type_name(function.parameters[index].type) != origin.source_type_name) {
                continue;
            }
            if (signature->second.parameter_types[index] == dynamic_array_descriptor_llvm_type()) {
                return true;
            }
        }
    }
    return false;
}

auto has_dynamic_array_parameter_descriptor_origin(
    semantics::DynamicArrayDescriptorOrigin const& origin,
    syntax::ModuleSyntax const& module
) -> bool {
    for (auto const& function : module.functions) {
        for (auto const& parameter : function.parameters) {
            if (parameter.name != origin.owner_name) {
                continue;
            }
            if (render_source_type_name(parameter.type) == origin.source_type_name) {
                return true;
            }
        }
    }
    return false;
}

void collect_concurrency_runtime_operations(
    syntax::ExpressionSyntax const& expression,
    std::vector<ConcurrencyRuntimeOperation>& operations
) {
    if (expression.kind == syntax::ExpressionKind::thread) {
        operations.push_back(ConcurrencyRuntimeOperation::spawn_thread);
        operations.push_back(ConcurrencyRuntimeOperation::spawn_failed);
        operations.push_back(ConcurrencyRuntimeOperation::destroy_handle);
    }
    if (expression.kind == syntax::ExpressionKind::task) {
        operations.push_back(ConcurrencyRuntimeOperation::spawn_task);
        operations.push_back(ConcurrencyRuntimeOperation::spawn_failed);
        operations.push_back(ConcurrencyRuntimeOperation::destroy_handle);
    }
    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "await") {
        operations.push_back(ConcurrencyRuntimeOperation::await_task);
        operations.push_back(ConcurrencyRuntimeOperation::destroy_handle);
    }
    if (expression.kind == syntax::ExpressionKind::call &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::member_access &&
        expression.left->text == "join") {
        operations.push_back(ConcurrencyRuntimeOperation::join_thread);
        operations.push_back(ConcurrencyRuntimeOperation::destroy_handle);
    }
}

auto collect_concurrency_runtime_operations(syntax::ModuleSyntax const& module)
    -> std::vector<ConcurrencyRuntimeOperation> {
    auto operations = std::vector<ConcurrencyRuntimeOperation> {};
    walk_module_expressions(module, [&operations](syntax::ExpressionSyntax const& expression) {
        collect_concurrency_runtime_operations(expression, operations);
    });
    return operations;
}

auto is_dynamic_array_default_constructor(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::call &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name &&
        expression.left->text == "DynamicArray" &&
        expression.arguments.empty();
}

auto is_dynamic_array_source_type(syntax::TypeSyntax const& type) -> bool {
    auto sequence = dynamic_sequence_source_type(render_source_type_name(type));
    return sequence.has_value() && sequence->kind == DynamicSequenceKind::dynamic_array;
}

auto is_view_source_type(syntax::TypeSyntax const& type) -> bool {
    auto sequence = dynamic_sequence_source_type(render_source_type_name(type));
    return sequence.has_value() &&
        (sequence->kind == DynamicSequenceKind::view ||
         sequence->kind == DynamicSequenceKind::shared_view ||
         sequence->kind == DynamicSequenceKind::exclusive_view);
}

void collect_dynamic_array_owner_names(
    syntax::StatementSyntax const& statement,
    std::unordered_set<std::string>& owner_names
) {
    if ((statement.kind == syntax::StatementKind::let_binding ||
         statement.kind == syntax::StatementKind::var_binding) &&
        !statement.name.empty() &&
        !statement.annotated_type.name.empty() &&
        is_dynamic_array_source_type(statement.annotated_type)) {
        owner_names.insert(statement.name);
    }
    for (auto const& nested_statement : statement.nested_statements) {
        collect_dynamic_array_owner_names(nested_statement, owner_names);
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        collect_dynamic_array_owner_names(alternate_statement, owner_names);
    }
    for (auto const& switch_case : statement.switch_cases) {
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                collect_dynamic_array_owner_names(*case_statement, owner_names);
            }
        }
    }
}

void collect_view_owner_names(
    syntax::StatementSyntax const& statement,
    std::unordered_set<std::string>& owner_names
) {
    if ((statement.kind == syntax::StatementKind::let_binding ||
         statement.kind == syntax::StatementKind::var_binding) &&
        !statement.name.empty() &&
        !statement.annotated_type.name.empty() &&
        is_view_source_type(statement.annotated_type)) {
        owner_names.insert(statement.name);
    }
    for (auto const& nested_statement : statement.nested_statements) {
        collect_view_owner_names(nested_statement, owner_names);
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        collect_view_owner_names(alternate_statement, owner_names);
    }
    for (auto const& switch_case : statement.switch_cases) {
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                collect_view_owner_names(*case_statement, owner_names);
            }
        }
    }
}

auto has_dynamic_array_index_read(
    syntax::FunctionSyntax const& function
) -> bool {
    auto owner_names = std::unordered_set<std::string> {};
    for (auto const& parameter : function.parameters) {
        if (!parameter.name.empty() && is_dynamic_array_source_type(parameter.type)) {
            owner_names.insert(parameter.name);
        }
    }
    for (auto const& statement : function.body_statements) {
        collect_dynamic_array_owner_names(statement, owner_names);
    }
    if (owner_names.empty()) {
        return false;
    }

    auto found = false;
    walk_function_expressions(function, [&owner_names, &found](syntax::ExpressionSyntax const& expression) {
        if (found ||
            expression.kind != syntax::ExpressionKind::index_access ||
            expression.left == nullptr ||
            expression.left->kind != syntax::ExpressionKind::name) {
            return;
        }
        found = owner_names.contains(expression.left->text);
    });
    return found;
}

auto has_dynamic_array_append_call(
    syntax::FunctionSyntax const& function
) -> bool {
    auto owner_names = std::unordered_set<std::string> {};
    for (auto const& parameter : function.parameters) {
        if (!parameter.name.empty() && is_dynamic_array_source_type(parameter.type)) {
            owner_names.insert(parameter.name);
        }
    }
    for (auto const& statement : function.body_statements) {
        collect_dynamic_array_owner_names(statement, owner_names);
    }
    if (owner_names.empty()) {
        return false;
    }

    auto found = false;
    walk_function_expressions(function, [&owner_names, &found](syntax::ExpressionSyntax const& expression) {
        if (found ||
            expression.kind != syntax::ExpressionKind::call ||
            expression.left == nullptr ||
            expression.left->kind != syntax::ExpressionKind::member_access ||
            expression.left->text != "push" ||
            expression.left->left == nullptr ||
            expression.left->left->kind != syntax::ExpressionKind::name) {
            return;
        }
        found = owner_names.contains(expression.left->left->text);
    });
    return found;
}

auto has_view_index_read(
    syntax::FunctionSyntax const& function
) -> bool {
    auto owner_names = std::unordered_set<std::string> {};
    for (auto const& parameter : function.parameters) {
        if (!parameter.name.empty() && is_view_source_type(parameter.type)) {
            owner_names.insert(parameter.name);
        }
    }
    for (auto const& statement : function.body_statements) {
        collect_view_owner_names(statement, owner_names);
    }
    if (owner_names.empty()) {
        return false;
    }

    auto found = false;
    walk_function_expressions(function, [&owner_names, &found](syntax::ExpressionSyntax const& expression) {
        if (found ||
            expression.kind != syntax::ExpressionKind::index_access ||
            expression.left == nullptr ||
            expression.left->kind != syntax::ExpressionKind::name) {
            return;
        }
        found = owner_names.contains(expression.left->text);
    });
    return found;
}

auto has_dynamic_array_index_read(
    syntax::ModuleSyntax const& module
) -> bool {
    for (auto const& function : module.functions) {
        if (has_dynamic_array_index_read(function)) {
            return true;
        }
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            if (has_dynamic_array_index_read(method)) {
                return true;
            }
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            if (has_dynamic_array_index_read(method)) {
                return true;
            }
        }
    }
    return false;
}

auto has_dynamic_array_append_call(
    syntax::ModuleSyntax const& module
) -> bool {
    for (auto const& function : module.functions) {
        if (has_dynamic_array_append_call(function)) {
            return true;
        }
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            if (has_dynamic_array_append_call(method)) {
                return true;
            }
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            if (has_dynamic_array_append_call(method)) {
                return true;
            }
        }
    }
    return false;
}

auto has_lowerable_dynamic_array_parameter(
    syntax::FunctionSyntax const& function,
    LlvmIrEmissionOptions const& options
) -> bool {
    for (auto const& parameter : function.parameters) {
        auto source_type_name = render_source_type_name(parameter.type);
        auto sequence = dynamic_sequence_source_type(source_type_name);
        if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array) {
            continue;
        }
        if (can_lower_dynamic_array_parameter_descriptor(parameter, *sequence, options)) {
            return true;
        }
    }
    return false;
}

auto has_lowerable_dynamic_array_parameter(
    syntax::ModuleSyntax const& module,
    LlvmIrEmissionOptions const& options
) -> bool {
    for (auto const& function : module.functions) {
        if (has_lowerable_dynamic_array_parameter(function, options)) {
            return true;
        }
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            if (has_lowerable_dynamic_array_parameter(method, options)) {
                return true;
            }
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            if (has_lowerable_dynamic_array_parameter(method, options)) {
                return true;
            }
        }
    }
    return false;
}

auto has_view_index_read(
    syntax::ModuleSyntax const& module
) -> bool {
    for (auto const& function : module.functions) {
        if (has_view_index_read(function)) {
            return true;
        }
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            if (has_view_index_read(method)) {
                return true;
            }
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            if (has_view_index_read(method)) {
                return true;
            }
        }
    }
    return false;
}

void push_dynamic_array_runtime_operation_once(
    std::vector<DynamicArrayRuntimeOperation>& operations,
    DynamicArrayRuntimeOperation operation
) {
    if (std::ranges::find(operations, operation) == operations.end()) {
        operations.push_back(operation);
    }
}

void collect_source_dynamic_array_construction_plans(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    diagnostics::DiagnosticBag& diagnostics,
    std::vector<DynamicArrayConstructionPlan>& plans
);

void collect_source_dynamic_array_construction_plans(
    syntax::StatementSyntax const& statement,
    LoweringContext const& context,
    diagnostics::DiagnosticBag& diagnostics,
    std::vector<DynamicArrayConstructionPlan>& plans
) {
    if ((statement.kind == syntax::StatementKind::let_binding ||
         statement.kind == syntax::StatementKind::var_binding) &&
        !statement.annotated_type.name.empty() &&
        is_dynamic_array_source_type(statement.annotated_type) &&
        is_dynamic_array_default_constructor(statement.expression)) {
        auto plan = plan_dynamic_array_construction(
            render_source_type_name(statement.annotated_type),
            0,
            context
        );
        if (!plan.has_value()) {
            diagnostics.error(statement.line, "source dynamic array construction could not be planned");
        } else {
            plan->owner_name = statement.name;
            plans.push_back(std::move(*plan));
        }
    }

    collect_source_dynamic_array_construction_plans(statement.expression, context, diagnostics, plans);
    collect_source_dynamic_array_construction_plans(statement.assignment_target, context, diagnostics, plans);
    for (auto const& nested_statement : statement.nested_statements) {
        collect_source_dynamic_array_construction_plans(nested_statement, context, diagnostics, plans);
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        collect_source_dynamic_array_construction_plans(alternate_statement, context, diagnostics, plans);
    }
    for (auto const& switch_case : statement.switch_cases) {
        collect_source_dynamic_array_construction_plans(switch_case.pattern, context, diagnostics, plans);
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                collect_source_dynamic_array_construction_plans(*case_statement, context, diagnostics, plans);
            }
        }
    }
}

void collect_source_dynamic_array_construction_plans(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    diagnostics::DiagnosticBag& diagnostics,
    std::vector<DynamicArrayConstructionPlan>& plans
) {
    for (auto const& argument : expression.arguments) {
        collect_source_dynamic_array_construction_plans(argument, context, diagnostics, plans);
    }
    for (auto const& nested_statement : expression.nested_statements) {
        if (nested_statement != nullptr) {
            collect_source_dynamic_array_construction_plans(*nested_statement, context, diagnostics, plans);
        }
    }
    if (expression.left != nullptr) {
        collect_source_dynamic_array_construction_plans(*expression.left, context, diagnostics, plans);
    }
    if (expression.right != nullptr) {
        collect_source_dynamic_array_construction_plans(*expression.right, context, diagnostics, plans);
    }
    if (expression.alternate != nullptr) {
        collect_source_dynamic_array_construction_plans(*expression.alternate, context, diagnostics, plans);
    }
}

void collect_source_dynamic_array_construction_plans(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context,
    diagnostics::DiagnosticBag& diagnostics,
    std::vector<DynamicArrayConstructionPlan>& plans
) {
    auto collect_from_function = [&](syntax::FunctionSyntax const& function) {
        for (auto const& statement : function.body_statements) {
            collect_source_dynamic_array_construction_plans(statement, context, diagnostics, plans);
        }
    };
    for (auto const& function : module.functions) {
        collect_from_function(function);
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            collect_from_function(method);
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            collect_from_function(method);
        }
    }
}

auto collect_dynamic_array_runtime_operations(
    LlvmIrEmissionOptions const& options,
    syntax::ModuleSyntax const& module,
    LoweringContext const& context,
    diagnostics::DiagnosticBag& diagnostics,
    std::vector<DynamicArrayConstructionPlan>& plans
) -> std::vector<DynamicArrayRuntimeOperation> {
    auto operations = std::vector<DynamicArrayRuntimeOperation> {};
    if (options.enable_dynamic_array_construction_lowering) {
        auto source_plan_offset = plans.size();
        collect_source_dynamic_array_construction_plans(module, context, diagnostics, plans);
        for (auto index = source_plan_offset; index < plans.size(); ++index) {
            operations.push_back(plans[index].operation);
        }
        if (options.enable_dynamic_array_index_lowering &&
            source_plan_offset < plans.size() &&
            has_dynamic_array_index_read(module)) {
            push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::bounds_failed);
        }
        if (options.enable_dynamic_array_append_lowering &&
            source_plan_offset < plans.size() &&
            has_dynamic_array_append_call(module)) {
            push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::grow);
        }
        if (dynamic_array_cleanup_emission_enabled(options) && source_plan_offset < plans.size()) {
            push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::deallocate);
        }
    }
    if (options.enable_dynamic_array_index_lowering && has_dynamic_array_index_read(module)) {
        push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::bounds_failed);
    }
    if (dynamic_array_parameter_descriptors_enabled(options) &&
        dynamic_array_cleanup_emission_enabled(options) &&
        has_lowerable_dynamic_array_parameter(module, options)) {
        push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::deallocate);
    }
    if (has_view_index_read(module)) {
        push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::bounds_failed);
    }
    for (auto const& request : options.test_only_dynamic_array_construction_requests) {
        auto plan = plan_dynamic_array_construction(
            request.source_type_name,
            request.initial_capacity,
            context
        );
        if (!plan.has_value()) {
            diagnostics.error(1, "test-only dynamic array construction request could not be planned");
            continue;
        }
        plan->owner_name = request.owner_name;
        push_dynamic_array_runtime_operation_once(operations, plan->operation);
        if (options.test_only_render_dynamic_array_grow_calls ||
            options.test_only_render_dynamic_array_grow_sequences ||
            options.test_only_render_dynamic_array_append_with_grow_sequences) {
            push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::grow);
        }
        if (options.test_only_render_dynamic_array_deallocation_calls ||
            options.test_only_render_dynamic_array_cleanup_sequences) {
            push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::deallocate);
        }
        plans.push_back(std::move(*plan));
    }
    return operations;
}

auto dynamic_array_cleanup_symbol_name(std::size_t ordinal) -> std::string {
    auto output = std::ostringstream {};
    output << "__orison_dynamic_array_cleanup." << ordinal;
    return output.str();
}

auto dynamic_array_element_drop_action(
    DynamicArrayConstructionPlan const& plan,
    std::size_t ordinal,
    std::string_view owner_name
) -> PlannedDropAction {
    auto capture_name = !owner_name.empty()
        ? std::string {owner_name} + ".element"
        : "dynamic_array" + std::to_string(ordinal) + ".element";
    return PlannedDropAction {
        .capture_name = std::move(capture_name),
        .source_type_name = plan.element_source_type_name,
        .symbol_name = semantics::drop_abi_symbol_name(plan.element_source_type_name),
        .field_index = ordinal,
    };
}

auto dynamic_array_element_drop_cleanup(
    DynamicArrayConstructionPlan const& plan,
    std::size_t ordinal,
    std::string_view owner_name
) -> std::optional<ConcurrencyDropCleanupPlan> {
    if (is_scalar_or_nonowning_source_type(plan.element_source_type_name)) {
        return std::nullopt;
    }

    auto cleanup = ConcurrencyDropCleanupPlan {
        .cleanup_symbol_name = dynamic_array_cleanup_symbol_name(ordinal),
        .requires_semantic_authorization = true,
    };
    cleanup.actions.push_back(dynamic_array_element_drop_action(plan, ordinal, owner_name));
    return cleanup;
}

auto collect_dynamic_array_element_drop_cleanups(
    std::vector<DynamicArrayConstructionPlan> const& plans,
    std::vector<TestOnlyDynamicArrayConstructionRequest> const& requests
) -> std::vector<ConcurrencyDropCleanupPlan> {
    auto cleanups = std::vector<ConcurrencyDropCleanupPlan> {};
    for (auto index = std::size_t {0}; index < plans.size(); ++index) {
        auto owner_name = index < requests.size() ? requests[index].owner_name : std::string_view {};
        auto cleanup = dynamic_array_element_drop_cleanup(plans[index], index, owner_name);
        if (cleanup.has_value()) {
            cleanups.push_back(std::move(*cleanup));
        }
    }
    return cleanups;
}

auto collect_dynamic_array_descriptor_cleanup_plans(
    syntax::ModuleSyntax const& module,
    semantics::SemanticAnalysisResult const& semantic_result,
    LoweringContext const& context,
    LlvmIrEmissionOptions const& options,
    diagnostics::DiagnosticBag& diagnostics
) -> std::vector<DynamicArrayDescriptorCleanupPlan> {
    auto plans = std::vector<DynamicArrayDescriptorCleanupPlan> {};
    plans.reserve(semantic_result.dynamic_array_descriptor_origins.size());
    for (auto const& origin : semantic_result.dynamic_array_descriptor_origins) {
        auto plan = plan_dynamic_array_descriptor_cleanup(
            origin.owner_name,
            origin.source_type_name,
            context
        );
        if (!plan.has_value()) {
            diagnostics.error(origin.line, "dynamic array descriptor cleanup could not be planned");
            continue;
        }
        if (has_bound_dynamic_array_parameter_descriptor(origin, module, context)) {
            plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor;
        } else if (dynamic_array_parameter_descriptor_audit_bindings_enabled(options) &&
            has_dynamic_array_parameter_descriptor_origin(origin, module)) {
            plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::audit_parameter_descriptor;
        }
        plan->source_line = origin.line;
        plans.push_back(std::move(*plan));
    }
    return plans;
}

void add_dynamic_array_planned_drop_declarations(
    LlvmIrEmissionOptions const& options,
    std::vector<PlannedDropDeclaration>& declarations,
    std::vector<PlannedDropAction> const& actions
) {
    if (options.test_only_declared_drop_source_type_allowlist.empty()) {
        for (auto const& action : actions) {
            add_planned_drop_declaration(declarations, planned_drop_declaration_for_action(action));
        }
        return;
    }

    for (auto declaration : declared_drop_declarations_for_allowed_source_types(
             actions,
             options.test_only_declared_drop_source_type_allowlist
         )) {
        add_planned_drop_declaration(declarations, std::move(declaration));
    }
}

}  // namespace

auto LlvmIrEmissionResult::has_errors() const -> bool {
    return diagnostics.has_errors();
}

auto LlvmIrEmissionResult::render(std::string_view path) const -> std::string {
    return diagnostics.render(path);
}

auto LlvmIrEmissionResult::planned_drop_report() const -> std::vector<std::string> {
    return format_planned_drop_report(planned_drop_declarations);
}

auto LlvmIrEmissionResult::dynamic_array_construction_plan_report() const -> std::vector<std::string> {
    return format_dynamic_array_construction_plan_report(dynamic_array_construction_plans);
}

auto LlvmIrEmissionResult::dynamic_array_descriptor_cleanup_plan_report() const -> std::vector<std::string> {
    return format_dynamic_array_descriptor_cleanup_plan_report(dynamic_array_descriptor_cleanup_plans);
}

auto LlvmIrEmissionResult::dynamic_array_cleanup_obligation_report() const -> std::vector<std::string> {
    return format_dynamic_array_cleanup_obligation_report(dynamic_array_cleanup_obligations);
}

auto LlvmIrEmissionResult::dynamic_array_cleanup_sequence_plan_report() const -> std::vector<std::string> {
    return format_dynamic_array_cleanup_sequence_plan_report(dynamic_array_cleanup_sequence_plans);
}

auto LlvmIrEmissionResult::dynamic_array_cleanup_sequence_verification_report() const -> std::vector<std::string> {
    return format_dynamic_array_cleanup_sequence_verification_report(dynamic_array_cleanup_sequence_verifications);
}

auto LlvmIrEmissionResult::dynamic_array_cleanup_emission_gate_report() const -> std::vector<std::string> {
    return format_dynamic_array_cleanup_emission_gate_report(dynamic_array_cleanup_sequence_verifications);
}

auto LlvmIrEmissionResult::dynamic_array_cleanup_emission_capability_report() const -> std::vector<std::string> {
    if (!dynamic_array_cleanup_emission_capability.has_value()) {
        return {};
    }
    return {format_dynamic_array_cleanup_emission_capability(*dynamic_array_cleanup_emission_capability)};
}

auto LlvmIrEmissionResult::dynamic_array_runtime_request_report() const -> std::vector<std::string> {
    return format_dynamic_array_runtime_request_report(dynamic_array_runtime_operations);
}

auto LlvmIrEmissionResult::emitted_drop_declaration_report() const -> std::vector<std::string> {
    return format_emitted_drop_declaration_report(planned_drop_declarations);
}

auto LlvmIrEmissionResult::planned_drop_action_report() const -> std::vector<std::string> {
    return format_planned_drop_action_report(planned_drop_actions);
}

auto LlvmIrEmissionResult::drop_cleanup_authorization_report() const -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    for (auto const& cleanup : drop_cleanups) {
        auto authorization = plan_drop_cleanup_authorization(
            cleanup,
            planned_drop_declarations,
            semantic_drop_lowering_authorizations
        );
        if (
            authorization.authorized ||
            (authorization.semantic_lowering_blockers.empty() && authorization.missing_declarations.empty())
        ) {
            continue;
        }
        auto cleanup_lines = format_drop_cleanup_authorization_report(cleanup, authorization);
        lines.insert(lines.end(), cleanup_lines.begin(), cleanup_lines.end());
    }
    return lines;
}

auto LlvmIrEmissionResult::drop_readiness_snapshot() const -> DropReadinessSnapshot {
    return plan_drop_readiness_snapshot(
        semantic_drop_lowering_authorizations,
        planned_drop_declarations,
        drop_cleanups
    );
}

auto LlvmIrEmissionResult::drop_readiness_snapshot_report() const -> std::vector<std::string> {
    return format_drop_readiness_snapshot_report(drop_readiness_snapshot());
}

auto LlvmIrEmissionResult::drop_readiness_summary() const -> DropReadinessSummary {
    return summarize_drop_readiness(drop_readiness_snapshot());
}

auto LlvmIrEmissionResult::drop_readiness_summary_report() const -> std::vector<std::string> {
    return {format_drop_readiness_summary(drop_readiness_summary())};
}

auto LlvmIrEmissionResult::drop_readiness_relation_report() const -> std::vector<std::string> {
    return format_drop_readiness_relation_report(drop_readiness_snapshot());
}

auto emit_module(
    syntax::ModuleSyntax const& module,
    semantics::SemanticAnalysisResult const& semantic_result,
    LlvmIrEmissionOptions const& options,
    bool metadata_only
) -> LlvmIrEmissionResult {
    auto result = LlvmIrEmissionResult {};
    result.semantic_drop_lowering_authorizations = options.semantic_drop_lowering_authorizations;
    if (semantic_result.has_errors()) {
        result.diagnostics.error(1, "cannot lower module with semantic errors");
        return result;
    }

    auto output = std::ostringstream {};
    output << "; Orison LLVM IR scaffold\n";
    if (!module.package_name.empty()) {
        output << "; package " << module.package_name << "\n";
    }
    output << "\n";

    auto context = build_lowering_context(module, result.diagnostics);
    if (result.has_errors()) {
        return result;
    }
    if (dynamic_array_parameter_descriptors_enabled(options)) {
        enable_dynamic_array_parameter_descriptors(module, context, options);
    }
    auto string_constants = collect_string_constants(module);
    auto emission_context = LoweringEmissionContext {
        .lowering = context,
        .string_constants = string_constants,
        .options = options,
    };
    result.drop_cleanups = plan_concurrency_drop_cleanups(
        module,
        emission_context,
        semantic_result
    );
    for (auto const& cleanup : result.drop_cleanups) {
        result.planned_drop_actions.insert(
            result.planned_drop_actions.end(),
            cleanup.actions.begin(),
            cleanup.actions.end()
        );
    }
    if (options.test_only_declared_drop_source_type_allowlist.empty()) {
        for (auto const& action : result.planned_drop_actions) {
            add_planned_drop_declaration(
                result.planned_drop_declarations,
                planned_drop_declaration_for_action(action)
            );
        }
        for (auto declaration : declared_drop_declarations_for_authorized_semantic_drops(
                 result.semantic_drop_lowering_authorizations
             )) {
            add_planned_drop_declaration(result.planned_drop_declarations, std::move(declaration));
        }
    } else {
        result.planned_drop_declarations = declared_drop_declarations_for_allowed_source_types(
            result.planned_drop_actions,
            options.test_only_declared_drop_source_type_allowlist
        );
    }
    output << emit_record_layouts(module, context);
    result.dynamic_array_runtime_operations = collect_dynamic_array_runtime_operations(
        options,
        module,
        context,
        result.diagnostics,
        result.dynamic_array_construction_plans
    );
    if (result.has_errors()) {
        return result;
    }
    if (dynamic_array_descriptor_cleanup_planning_enabled(options)) {
        result.dynamic_array_descriptor_cleanup_plans = collect_dynamic_array_descriptor_cleanup_plans(
            module,
            semantic_result,
            context,
            options,
            result.diagnostics
        );
        if (result.has_errors()) {
            return result;
        }
        result.dynamic_array_cleanup_obligations = plan_dynamic_array_descriptor_cleanup_obligations(
            result.dynamic_array_descriptor_cleanup_plans,
            result.dynamic_array_construction_plans.size()
        );
        result.dynamic_array_cleanup_sequence_plans = plan_dynamic_array_cleanup_sequences(
            result.dynamic_array_cleanup_obligations
        );
        result.dynamic_array_cleanup_sequence_verifications = verify_dynamic_array_cleanup_sequence_plans(
            result.dynamic_array_cleanup_sequence_plans
        );
        if (dynamic_array_cleanup_emission_enabled(options)) {
            result.dynamic_array_cleanup_emission_capability = prove_dynamic_array_cleanup_emission_capability(
                dynamic_array_parameter_descriptors_enabled(options) &&
                    dynamic_array_cleanup_emission_enabled(options),
                result.dynamic_array_descriptor_cleanup_plans,
                result.dynamic_array_cleanup_sequence_verifications,
                result.dynamic_array_cleanup_obligations,
                result.semantic_drop_lowering_authorizations
            );
            for (auto const& plan : result.dynamic_array_descriptor_cleanup_plans) {
                if (plan.descriptor_storage_status ==
                        DynamicArrayDescriptorStorageStatus::audit_parameter_descriptor ||
                    plan.descriptor_storage_status ==
                    DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor) {
                    push_dynamic_array_runtime_operation_once(
                        result.dynamic_array_runtime_operations,
                        DynamicArrayRuntimeOperation::deallocate
                    );
                    break;
                }
            }
        }
    }
    if (options.test_only_render_dynamic_array_element_drop_walks ||
        dynamic_array_cleanup_emission_enabled(options)) {
        auto dynamic_array_drop_cleanups =
            collect_dynamic_array_element_drop_cleanups(
                result.dynamic_array_construction_plans,
                options.test_only_dynamic_array_construction_requests
            );
        auto dynamic_array_descriptor_drop_cleanups = std::vector<ConcurrencyDropCleanupPlan> {};
        dynamic_array_descriptor_drop_cleanups.reserve(result.dynamic_array_cleanup_obligations.size());
        for (auto const& obligation : result.dynamic_array_cleanup_obligations) {
            dynamic_array_descriptor_drop_cleanups.push_back(
                drop_cleanup_for_dynamic_array_cleanup_obligation(obligation)
            );
        }
        dynamic_array_drop_cleanups.insert(
            dynamic_array_drop_cleanups.end(),
            std::make_move_iterator(dynamic_array_descriptor_drop_cleanups.begin()),
            std::make_move_iterator(dynamic_array_descriptor_drop_cleanups.end())
        );
        for (auto& cleanup : dynamic_array_drop_cleanups) {
            result.planned_drop_actions.insert(
                result.planned_drop_actions.end(),
                cleanup.actions.begin(),
                cleanup.actions.end()
            );
            add_dynamic_array_planned_drop_declarations(
                options,
                result.planned_drop_declarations,
                cleanup.actions
            );
            result.drop_cleanups.push_back(std::move(cleanup));
        }
    }
    if (dynamic_array_allocation_calls_enabled(options)) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto call_ir = emit_dynamic_array_allocation_call(
                result.dynamic_array_construction_plans[index],
                "%dynamic_array_alloc" + std::to_string(index)
            );
            if (options.enable_dynamic_array_construction_lowering) {
                result.dynamic_array_allocation_call_ir.push_back(call_ir);
            }
            if (options.test_only_render_dynamic_array_allocation_calls) {
                result.test_only_dynamic_array_allocation_call_ir.push_back(std::move(call_ir));
            }
        }
    }
    if (options.test_only_render_dynamic_array_grow_calls) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_grow_call_ir.push_back(
                emit_dynamic_array_grow_call(
                    result.dynamic_array_construction_plans[index],
                    prefix + ".grown",
                    "%dynamic_array_alloc" + std::to_string(index),
                    prefix + ".grow.next.capacity"
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_deallocation_calls) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_deallocation_call_ir.push_back(
                emit_dynamic_array_deallocation_call(
                    result.dynamic_array_construction_plans[index],
                    prefix + ".data",
                    prefix + ".capacity"
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_descriptor_bindings) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            result.test_only_dynamic_array_descriptor_binding_ir.push_back(
                emit_dynamic_array_descriptor_binding(
                    result.dynamic_array_construction_plans[index],
                    "%dynamic_array" + std::to_string(index) + ".addr",
                    "%dynamic_array_alloc" + std::to_string(index)
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_descriptor_projections) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto descriptor_name = "%dynamic_array_alloc" + std::to_string(index);
            result.test_only_dynamic_array_descriptor_projection_ir.push_back(
                emit_dynamic_array_descriptor_field_projection(
                    "%dynamic_array" + std::to_string(index) + ".data",
                    descriptor_name,
                    DynamicArrayDescriptorField::data
                )
            );
            result.test_only_dynamic_array_descriptor_projection_ir.push_back(
                emit_dynamic_array_descriptor_field_projection(
                    "%dynamic_array" + std::to_string(index) + ".length",
                    descriptor_name,
                    DynamicArrayDescriptorField::length
                )
            );
            result.test_only_dynamic_array_descriptor_projection_ir.push_back(
                emit_dynamic_array_descriptor_field_projection(
                    "%dynamic_array" + std::to_string(index) + ".capacity",
                    descriptor_name,
                    DynamicArrayDescriptorField::capacity
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_bounds_checks) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_bounds_check_ir.push_back(
                emit_dynamic_array_bounds_check(
                    prefix + ".index.in_bounds",
                    prefix + ".index",
                    prefix + ".length",
                    DynamicArrayBoundsCheckKind::index_within_length
                )
            );
            result.test_only_dynamic_array_bounds_check_ir.push_back(
                emit_dynamic_array_bounds_check(
                    prefix + ".append.has_capacity",
                    prefix + ".length",
                    prefix + ".capacity",
                    DynamicArrayBoundsCheckKind::append_has_capacity
                )
            );
            result.test_only_dynamic_array_bounds_check_ir.push_back(
                emit_dynamic_array_bounds_check(
                    prefix + ".length.within_capacity",
                    prefix + ".length",
                    prefix + ".capacity",
                    DynamicArrayBoundsCheckKind::length_within_capacity
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_element_addresses) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_element_address_ir.push_back(
                emit_dynamic_array_element_address(
                    result.dynamic_array_construction_plans[index],
                    prefix + ".element.addr",
                    prefix + ".data",
                    prefix + ".index"
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_element_loads) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_element_load_ir.push_back(
                emit_dynamic_array_element_load(
                    result.dynamic_array_construction_plans[index],
                    prefix + ".element",
                    prefix + ".element.addr"
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_element_stores) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_element_store_ir.push_back(
                emit_dynamic_array_element_store(
                    result.dynamic_array_construction_plans[index],
                    prefix + ".value",
                    prefix + ".element.addr"
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_descriptor_length_updates) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_descriptor_length_update_ir.push_back(
                emit_dynamic_array_descriptor_length_update(
                    prefix + ".updated",
                    prefix + ".next.length",
                    "%dynamic_array_alloc" + std::to_string(index),
                    prefix + ".length"
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_descriptor_write_backs) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_descriptor_write_back_ir.push_back(
                emit_dynamic_array_descriptor_write_back(
                    prefix + ".updated",
                    prefix + ".addr"
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_append_sequences) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_append_sequence_ir.push_back(
                emit_dynamic_array_append_sequence(
                    result.dynamic_array_construction_plans[index],
                    "%dynamic_array_alloc" + std::to_string(index),
                    prefix + ".addr",
                    prefix + ".data",
                    prefix + ".length",
                    prefix + ".capacity",
                    prefix + ".value",
                    prefix
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_grow_sequences) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_grow_sequence_ir.push_back(
                emit_dynamic_array_grow_sequence(
                    result.dynamic_array_construction_plans[index],
                    "%dynamic_array_alloc" + std::to_string(index),
                    prefix + ".addr",
                    prefix + ".capacity",
                    prefix
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_append_with_grow_sequences) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_append_with_grow_sequence_ir.push_back(
                emit_dynamic_array_append_with_grow_sequence(
                    result.dynamic_array_construction_plans[index],
                    "%dynamic_array_alloc" + std::to_string(index),
                    prefix + ".addr",
                    prefix + ".data",
                    prefix + ".length",
                    prefix + ".capacity",
                    prefix + ".value",
                    prefix
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_cleanup_sequences) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_cleanup_sequence_ir.push_back(
                emit_dynamic_array_cleanup_sequence(
                    result.dynamic_array_construction_plans[index],
                    "%dynamic_array_alloc" + std::to_string(index),
                    prefix
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_descriptor_load_cleanup_sequences) {
        auto offset = result.dynamic_array_construction_plans.size();
        for (auto index = std::size_t {0}; index < result.dynamic_array_descriptor_cleanup_plans.size(); ++index) {
            auto ordinal = offset + index;
            auto prefix = "%dynamic_array" + std::to_string(ordinal);
            result.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.push_back(
                emit_dynamic_array_descriptor_load_cleanup_sequence(
                    result.dynamic_array_descriptor_cleanup_plans[index],
                    prefix + ".descriptor",
                    prefix
                )
            );
        }
    }
    if (options.test_only_render_dynamic_array_element_drop_walks) {
        for (auto index = std::size_t {0}; index < result.dynamic_array_construction_plans.size(); ++index) {
            auto prefix = "%dynamic_array" + std::to_string(index);
            result.test_only_dynamic_array_element_drop_walk_ir.push_back(
                emit_dynamic_array_element_drop_walk(
                    result.dynamic_array_construction_plans[index],
                    prefix + ".cleanup.data",
                    prefix + ".cleanup.length",
                    prefix
                )
            );
        }
        auto offset = result.dynamic_array_construction_plans.size();
        for (auto index = std::size_t {0}; index < result.dynamic_array_descriptor_cleanup_plans.size(); ++index) {
            auto ordinal = offset + index;
            auto prefix = "%dynamic_array" + std::to_string(ordinal);
            result.test_only_dynamic_array_element_drop_walk_ir.push_back(
                emit_dynamic_array_element_drop_walk(
                    result.dynamic_array_descriptor_cleanup_plans[index],
                    prefix + ".cleanup.data",
                    prefix + ".cleanup.length",
                    prefix
                )
            );
        }
    }
    if (metadata_only) {
        result.ir_text = output.str();
        return result;
    }
    output << emit_module_prelude(
        string_constants,
        context.foreign_declarations,
        collect_concurrency_runtime_operations(module),
        result.planned_drop_declarations,
        result.dynamic_array_runtime_operations
    );
    for (auto const& function : module.functions) {
        if (is_uninstantiated_generic_function(function)) {
            continue;
        }
        auto signature = context.functions.find(function.name);
        if (signature == context.functions.end()) {
            result.diagnostics.error(function.line, "lowering context is missing function signature");
            return result;
        }
        output << emit_function(
            function,
            signature->second,
            context,
            string_constants,
            semantic_result,
            result.diagnostics,
            options
        );
        output << "\n";
    }

    auto method_index = std::size_t {0};
    auto emit_method = [&](syntax::FunctionSyntax const& method) -> bool {
        if (method_index >= context.methods.size()) {
            result.diagnostics.error(method.line, "lowering context is missing method signature");
            return false;
        }
        auto const& lowered_method = context.methods[method_index++];
        if (is_uninstantiated_generic_function(method)) {
            return true;
        }
        output << emit_function(
            method,
            lowered_method.signature,
            context,
            string_constants,
            semantic_result,
            result.diagnostics,
            options
        );
        output << "\n";
        return !result.has_errors();
    };

    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            if (!emit_method(method)) {
                return result;
            }
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            if (!emit_method(method)) {
                return result;
            }
        }
    }

    if (!result.has_errors()) {
        auto ir_text = output.str();
        auto verifier = LlvmIrVerifier {};
        result.diagnostics = verifier.verify(ir_text);
        if (!result.has_errors()) {
            result.ir_text = std::move(ir_text);
        }
    }
    return result;
}

auto LlvmIrEmitter::emit(
    syntax::ModuleSyntax const& module,
    semantics::SemanticAnalysisResult const& semantic_result,
    LlvmIrEmissionOptions const& options
) const -> LlvmIrEmissionResult {
    return emit_module(module, semantic_result, options, false);
}

auto LlvmIrEmitter::emit_metadata(
    syntax::ModuleSyntax const& module,
    semantics::SemanticAnalysisResult const& semantic_result,
    LlvmIrEmissionOptions const& options
) const -> LlvmIrEmissionResult {
    return emit_module(module, semantic_result, options, true);
}

}  // namespace orison::lowering
