#include "orison/lowering/llvm_ir_emitter.hpp"

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/llvm_ir_verifier.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/module_prelude.hpp"
#include "orison/lowering/module_symbol_registry.hpp"
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

auto collect_emitted_record_type_symbols(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<GeneratedModuleSymbol> {
    auto symbols = std::vector<GeneratedModuleSymbol> {};
    auto emitted_record_names = std::unordered_set<std::string> {};
    for (auto const& record : module.records) {
        auto layout = context.records.find(record.name);
        if (layout == context.records.end() || !can_emit_record_layout(record, layout->second)) {
            continue;
        }
        symbols.push_back(GeneratedModuleSymbol {
            .symbol_name = layout->second.llvm_type_name,
            .category = "lowered record type",
            .line = record.line,
        });
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
        symbols.push_back(GeneratedModuleSymbol {
            .symbol_name = layout.llvm_type_name,
            .category = "lowered instantiated record type",
            .line = 1,
        });
    }
    return symbols;
}

auto is_uninstantiated_generic_function(syntax::FunctionSyntax const& function) -> bool {
    return !function.generic_parameters.empty();
}

auto is_generic_receiver_pattern(
    syntax::TypeSyntax const& receiver_type,
    syntax::ModuleSyntax const& module
) -> bool {
    if (receiver_type.name == "DynamicArray" && receiver_type.generic_arguments.size() == 1) {
        auto const& argument = receiver_type.generic_arguments.front();
        return argument.generic_arguments.empty() && !argument.name.empty();
    }

    auto record = std::ranges::find_if(
        module.records,
        [&](syntax::RecordSyntax const& candidate) {
            return candidate.name == receiver_type.name;
        }
    );
    if (record == module.records.end() ||
        record->generic_parameters.size() != receiver_type.generic_arguments.size()) {
        return false;
    }
    for (auto index = std::size_t {0}; index < receiver_type.generic_arguments.size(); ++index) {
        auto const& argument = receiver_type.generic_arguments[index];
        if (argument.generic_arguments.empty() && argument.name == record->generic_parameters[index]) {
            return true;
        }
    }
    return false;
}

auto has_authorized_source_drop_definition(
    semantics::DropImplementation const& implementation,
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> bool {
    if (implementation.origin != semantics::DropImplementationOrigin::source_derived ||
        !implementation.proven ||
        !implementation.body.finite) {
        return false;
    }
    return std::ranges::any_of(
        authorizations,
        [&](semantics::DropLoweringAuthorization const& authorization) {
            return authorization.authorized &&
                authorization.site.source_type_name == implementation.source_type_name &&
                authorization.site.abi_symbol_name == implementation.abi_symbol_name;
        }
    );
}

auto emit_source_drop_definitions(
    syntax::ModuleSyntax const& module,
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> std::string {
    auto candidates = semantics::collect_source_derived_drop_implementation_candidates(module);
    auto implementations = semantics::collect_source_derived_drop_implementations(candidates);
    auto emitted_symbols = std::vector<std::string> {};
    auto output = std::ostringstream {};
    for (auto const& implementation : implementations) {
        if (!has_authorized_source_drop_definition(implementation, authorizations)) {
            continue;
        }
        if (std::ranges::find(emitted_symbols, implementation.abi_symbol_name) != emitted_symbols.end()) {
            continue;
        }
        output << "define void @" << implementation.abi_symbol_name << "(ptr %value) {\n";
        output << "entry:\n";
        output << "  ret void\n";
        output << "}\n\n";
        emitted_symbols.push_back(implementation.abi_symbol_name);
    }
    return output.str();
}

auto collect_source_drop_definition_symbols(
    syntax::ModuleSyntax const& module,
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> std::vector<std::string> {
    auto candidates = semantics::collect_source_derived_drop_implementation_candidates(module);
    auto implementations = semantics::collect_source_derived_drop_implementations(candidates);
    auto symbols = std::vector<std::string> {};
    for (auto const& implementation : implementations) {
        if (!has_authorized_source_drop_definition(implementation, authorizations)) {
            continue;
        }
        if (std::ranges::find(symbols, implementation.abi_symbol_name) != symbols.end()) {
            continue;
        }
        symbols.push_back(implementation.abi_symbol_name);
    }
    return symbols;
}

auto register_unique_module_symbol(
    ModuleSymbolRegistry& registry,
    std::unordered_set<std::string>& emitted_symbols,
    std::string const& symbol_name,
    std::string category,
    std::size_t line,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    if (symbol_name.empty()) {
        return true;
    }
    if (emitted_symbols.contains(symbol_name)) {
        return true;
    }
    emitted_symbols.insert(symbol_name);
    return registry.register_symbol(symbol_name, std::move(category), line, diagnostics);
}

auto validate_prelude_module_symbols(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context,
    std::vector<ConcurrencyRuntimeOperation> const& concurrency_runtime_operations,
    std::vector<PlannedDropDeclaration> const& planned_drop_declarations,
    std::vector<DynamicArrayRuntimeOperation> const& dynamic_array_runtime_operations,
    std::vector<std::string> const& source_defined_drop_symbols,
    ModuleSymbolRegistry& registry,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    auto emitted_symbols = std::unordered_set<std::string> {};
    for (auto const& function : module.functions) {
        if (is_uninstantiated_generic_function(function)) {
            continue;
        }
        auto signature = context.functions.find(function.name);
        if (signature == context.functions.end()) {
            continue;
        }
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            signature->second.symbol_name,
            "source function symbol",
            function.line,
            diagnostics
        );
    }
    for (auto const& function_ptr : context.generic_function_specializations) {
        auto const& function = *function_ptr;
        auto signature = context.functions.find(function.name);
        if (signature == context.functions.end()) {
            continue;
        }
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            signature->second.symbol_name,
            "generated generic function specialization",
            function.line,
            diagnostics
        );
    }
    for (auto const& method : context.methods) {
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            method.signature.symbol_name,
            "generated method symbol",
            1,
            diagnostics
        );
    }
    for (auto const& specialization : context.generic_method_specializations) {
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            specialization.symbol_name,
            "generated method symbol",
            specialization.source_method == nullptr ? 1 : specialization.source_method->line,
            diagnostics
        );
    }
    for (auto const& declaration : context.foreign_declarations) {
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            declaration.symbol_name,
            "foreign declaration",
            1,
            diagnostics
        );
    }
    for (auto operation : concurrency_runtime_operations) {
        auto runtime_call = concurrency_runtime_call(operation);
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            std::string {runtime_call.symbol_name},
            "runtime prelude declaration",
            1,
            diagnostics
        );
    }
    for (auto operation : dynamic_array_runtime_operations) {
        auto runtime_call = dynamic_array_runtime_call(operation);
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            std::string {runtime_call.symbol_name},
            "dynamic array runtime prelude declaration",
            1,
            diagnostics
        );
    }
    for (auto const& symbol_name : source_defined_drop_symbols) {
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            symbol_name,
            "source drop definition",
            1,
            diagnostics
        );
    }
    for (auto const& declaration : planned_drop_declarations) {
        if (!declaration.emit_declaration ||
            emitted_symbols.contains(declaration.symbol_name)) {
            continue;
        }
        register_unique_module_symbol(
            registry,
            emitted_symbols,
            declaration.symbol_name,
            "planned drop declaration",
            declaration.discovery_line,
            diagnostics
        );
    }
    return !diagnostics.has_errors();
}

auto validate_generated_definition_symbols(
    std::vector<GeneratedModuleSymbol> const& generated_symbols,
    ModuleSymbolRegistry& registry,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    for (auto const& symbol : generated_symbols) {
        registry.register_symbol(
            symbol.symbol_name,
            symbol.category,
            symbol.line,
            diagnostics
        );
    }
    return !diagnostics.has_errors();
}

auto validate_generated_type_symbols(
    std::vector<GeneratedModuleSymbol> const& generated_symbols,
    ModuleSymbolRegistry& registry,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    for (auto const& symbol : generated_symbols) {
        registry.register_type_symbol(
            symbol.symbol_name,
            symbol.category,
            symbol.line,
            diagnostics
        );
    }
    return !diagnostics.has_errors();
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

auto is_dynamic_array_local_constructor_origin(
    syntax::StatementSyntax const& statement,
    semantics::DynamicArrayDescriptorOrigin const& origin
) -> bool {
    if ((statement.kind == syntax::StatementKind::let_binding ||
         statement.kind == syntax::StatementKind::var_binding) &&
        statement.name == origin.owner_name &&
        statement.line == origin.line &&
        !statement.annotated_type.name.empty() &&
        render_source_type_name(statement.annotated_type) == origin.source_type_name &&
        is_dynamic_array_default_constructor(statement.expression)) {
        return true;
    }
    return std::ranges::any_of(statement.nested_statements, [&](auto const& nested_statement) {
        return is_dynamic_array_local_constructor_origin(nested_statement, origin);
    });
}

auto has_dynamic_array_local_constructor_origin(
    semantics::DynamicArrayDescriptorOrigin const& origin,
    syntax::ModuleSyntax const& module
) -> bool {
    for (auto const& function : module.functions) {
        for (auto const& statement : function.body_statements) {
            if (is_dynamic_array_local_constructor_origin(statement, origin)) {
                return true;
            }
        }
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            for (auto const& statement : method.body_statements) {
                if (is_dynamic_array_local_constructor_origin(statement, origin)) {
                    return true;
                }
            }
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            for (auto const& statement : method.body_statements) {
                if (is_dynamic_array_local_constructor_origin(statement, origin)) {
                    return true;
                }
            }
        }
    }
    return false;
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
        if (parameter.name == "this" &&
            (parameter.type.name == "This" ||
             parameter.type.name == "shared.This" ||
             parameter.type.name == "exclusive.This")) {
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
        if (parameter.name == "this" &&
            (parameter.type.name == "This" ||
             parameter.type.name == "shared.This" ||
             parameter.type.name == "exclusive.This")) {
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

void bind_dynamic_array_local_for_computed_for_collection(
    syntax::StatementSyntax const& statement,
    LoweringContext const& context,
    FunctionLoweringState& state
) {
    if ((statement.kind != syntax::StatementKind::let_binding &&
         statement.kind != syntax::StatementKind::var_binding) ||
        statement.name.empty() ||
        statement.annotated_type.name.empty() ||
        !is_dynamic_array_source_type(statement.annotated_type) ||
        !is_dynamic_array_default_constructor(statement.expression)) {
        return;
    }

    auto source_type_name = render_source_type_name(statement.annotated_type);
    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        statement.name,
        source_type_name,
        context
    );
    if (!cleanup_plan.has_value()) {
        return;
    }

    auto descriptor_type = std::string {dynamic_array_descriptor_llvm_type()};
    auto descriptor_storage_name = "%" + statement.name + ".addr";
    state.source_type_names[statement.name] = std::move(source_type_name);
    state.addressable_bindings[statement.name] = AddressableBinding {
        .type = LoweredType {
            .type = descriptor_type,
            .signedness = IntegerSignedness::not_integer,
        },
        .storage = descriptor_storage_name,
    };
    cleanup_plan->descriptor_storage_name = std::move(descriptor_storage_name);
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    cleanup_plan->source_line = statement.line;
    state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));
}

template <typename CollectForStatement>
void collect_computed_dynamic_array_for_statements(
    syntax::StatementSyntax const& statement,
    std::string_view enclosing_function_name,
    LoweringContext const& context,
    FunctionLoweringState& state,
    CollectForStatement&& collect_for_statement
) {
    bind_dynamic_array_local_for_computed_for_collection(statement, context, state);
    if (statement.kind == syntax::StatementKind::for_statement) {
        collect_for_statement(statement, enclosing_function_name, context, state);
    }

    for (auto const& nested_statement : statement.nested_statements) {
        collect_computed_dynamic_array_for_statements(
            nested_statement,
            enclosing_function_name,
            context,
            state,
            collect_for_statement
        );
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        collect_computed_dynamic_array_for_statements(
            alternate_statement,
            enclosing_function_name,
            context,
            state,
            collect_for_statement
        );
    }
    for (auto const& switch_case : statement.switch_cases) {
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                collect_computed_dynamic_array_for_statements(
                    *case_statement,
                    enclosing_function_name,
                    context,
                    state,
                    collect_for_statement
                );
            }
        }
    }
}

template <typename CollectForStatement>
void collect_computed_dynamic_array_for_function(
    syntax::FunctionSyntax const& function,
    LoweringContext const& context,
    CollectForStatement&& collect_for_statement
) {
    auto state = FunctionLoweringState {};
    for (auto const& parameter : function.parameters) {
        if (!parameter.name.empty() && !parameter.type.name.empty()) {
            state.source_type_names[parameter.name] = render_source_type_name(parameter.type);
        }
    }
    for (auto const& statement : function.body_statements) {
        collect_computed_dynamic_array_for_statements(
            statement,
            function.name,
            context,
            state,
            collect_for_statement
        );
    }
}

template <typename CollectForStatement>
void collect_computed_dynamic_array_for_module(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context,
    CollectForStatement&& collect_for_statement
) {
    for (auto const& function : module.functions) {
        collect_computed_dynamic_array_for_function(function, context, collect_for_statement);
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            collect_computed_dynamic_array_for_function(method, context, collect_for_statement);
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            collect_computed_dynamic_array_for_function(method, context, collect_for_statement);
        }
    }
}

void append_if_present(std::ostringstream& output, std::string_view label, std::string const& value) {
    if (!value.empty()) {
        output << ' ' << label << ' ' << value;
    }
}

void append_computed_dynamic_array_for_metadata_prefix(
    std::ostringstream& output,
    std::string_view report_name,
    std::string const& enclosing_function_name,
    std::size_t source_line,
    std::string const& source_type_name,
    std::string const& element_source_type_name,
    std::string const& cleanup_owner_name
) {
    output << "computed DynamicArray for " << report_name;
    append_if_present(output, "function", enclosing_function_name);
    if (source_line != 0) {
        output << " line " << source_line;
    }
    append_if_present(output, "source", source_type_name);
    append_if_present(output, "element", element_source_type_name);
    append_if_present(output, "owner", cleanup_owner_name);
}

template <typename Metadata, typename FormatMetadata>
auto format_computed_dynamic_array_for_metadata_report(
    std::vector<Metadata> const& metadata,
    FormatMetadata&& format_metadata
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    lines.reserve(metadata.size());
    for (auto const& item : metadata) {
        lines.push_back(format_metadata(item));
    }
    return lines;
}

template <typename Metadata>
void append_rendered_ir(std::vector<std::string>& output, std::vector<Metadata> const& metadata) {
    for (auto const& item : metadata) {
        output.insert(output.end(), item.rendered_ir.begin(), item.rendered_ir.end());
    }
}

auto render_computed_dynamic_array_for_production_sequence_module_comments(
    std::vector<ComputedDynamicArrayForProductionSequenceMetadata> const& sequences
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    for (auto const& sequence : sequences) {
        auto report = std::ostringstream {};
        append_computed_dynamic_array_for_metadata_prefix(
            report,
            "production sequence",
            sequence.enclosing_function_name,
            sequence.source_line,
            sequence.source_type_name,
            sequence.element_source_type_name,
            sequence.cleanup_owner_name
        );
        report << " snippets " << sequence.rendered_ir.size();
        report << " (metadata only)";
        lines.push_back("; " + report.str() + "\n");
        for (auto const& snippet : sequence.rendered_ir) {
            lines.push_back("; " + snippet);
        }
    }
    return lines;
}

auto collect_computed_dynamic_array_for_descriptor_renders(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForDescriptorRenderMetadata> {
    auto renders = std::vector<ComputedDynamicArrayForDescriptorRenderMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&renders](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_descriptor_render(
                statement.expression,
                lowering_context,
                state
            );
            if (plan.kind ==
                    ComputedDynamicArrayIterableDescriptorRenderPlanKind::descriptor_render_planned &&
                plan.descriptor_load_planned &&
                plan.data_projection_planned &&
                plan.length_projection_planned &&
                plan.capacity_projection_planned) {
                renders.push_back(ComputedDynamicArrayForDescriptorRenderMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .descriptor_storage_name = plan.descriptor_storage_name,
                    .descriptor_value_name = plan.descriptor_value_name,
                    .data_pointer_name = plan.data_pointer_name,
                    .length_name = plan.length_name,
                    .capacity_name = plan.capacity_name,
                    .rendered_ir = plan.rendered_ir,
                });
            }
        }
    );
    return renders;
}

auto collect_computed_dynamic_array_for_loop_control_renders(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForLoopControlRenderMetadata> {
    auto renders = std::vector<ComputedDynamicArrayForLoopControlRenderMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&renders](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_loop_control_render(
                statement.expression,
                lowering_context,
                state
            );
            if (plan.kind ==
                    ComputedDynamicArrayIterableLoopControlRenderPlanKind::loop_control_render_planned &&
                plan.entry_branch_planned &&
                plan.index_phi_planned &&
                plan.bounds_check_planned &&
                plan.conditional_branch_planned) {
                renders.push_back(ComputedDynamicArrayForLoopControlRenderMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .condition_block_name = plan.condition_block_name,
                    .body_block_name = plan.body_block_name,
                    .continue_block_name = plan.continue_block_name,
                    .exit_block_name = plan.exit_block_name,
                    .index_name = plan.index_name,
                    .next_index_name = plan.next_index_name,
                    .bounds_check_name = plan.bounds_check_name,
                    .rendered_ir = plan.rendered_ir,
                });
            }
        }
    );
    return renders;
}

auto collect_computed_dynamic_array_for_element_address_renders(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForElementAddressRenderMetadata> {
    auto renders = std::vector<ComputedDynamicArrayForElementAddressRenderMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&renders](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_element_address_render(
                statement.expression,
                lowering_context,
                state
            );
            if (plan.kind ==
                    ComputedDynamicArrayIterableElementAddressRenderPlanKind::element_address_render_planned &&
                plan.data_pointer_available &&
                plan.index_available &&
                plan.element_address_planned) {
                renders.push_back(ComputedDynamicArrayForElementAddressRenderMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .element_llvm_type_name = plan.element_llvm_type_name,
                    .data_pointer_name = plan.data_pointer_name,
                    .index_name = plan.index_name,
                    .element_address_name = plan.element_address_name,
                    .rendered_ir = plan.rendered_ir,
                });
            }
        }
    );
    return renders;
}

auto collect_computed_dynamic_array_for_element_load_renders(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForElementLoadRenderMetadata> {
    auto renders = std::vector<ComputedDynamicArrayForElementLoadRenderMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&renders](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_element_load_render(
                statement.expression,
                lowering_context,
                state
            );
            if (plan.kind ==
                    ComputedDynamicArrayIterableElementLoadRenderPlanKind::element_load_render_planned &&
                plan.element_address_available &&
                plan.item_value_planned) {
                renders.push_back(ComputedDynamicArrayForElementLoadRenderMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .element_llvm_type_name = plan.element_llvm_type_name,
                    .element_address_name = plan.element_address_name,
                    .item_value_name = plan.item_value_name,
                    .rendered_ir = plan.rendered_ir,
                });
            }
        }
    );
    return renders;
}

auto collect_computed_dynamic_array_for_loop_continue_renders(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForLoopContinueRenderMetadata> {
    auto renders = std::vector<ComputedDynamicArrayForLoopContinueRenderMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&renders](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_loop_continue_render(
                statement.expression,
                lowering_context,
                state
            );
            if (plan.kind ==
                    ComputedDynamicArrayIterableLoopContinueRenderPlanKind::loop_continue_render_planned &&
                plan.continue_block_planned &&
                plan.next_index_planned &&
                plan.backedge_branch_planned) {
                renders.push_back(ComputedDynamicArrayForLoopContinueRenderMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .continue_block_name = plan.continue_block_name,
                    .condition_block_name = plan.condition_block_name,
                    .index_name = plan.index_name,
                    .next_index_name = plan.next_index_name,
                    .rendered_ir = plan.rendered_ir,
                });
            }
        }
    );
    return renders;
}

auto collect_computed_dynamic_array_for_loop_render_sequences(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForLoopRenderSequenceMetadata> {
    auto sequences = std::vector<ComputedDynamicArrayForLoopRenderSequenceMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&sequences](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_loop_render_sequence(
                statement.expression,
                lowering_context,
                state
            );
            if (plan.kind ==
                    ComputedDynamicArrayIterableLoopRenderSequencePlanKind::loop_render_sequence_planned &&
                plan.descriptor_render_planned &&
                plan.loop_control_render_planned &&
                plan.body_block_planned &&
                plan.element_address_render_planned &&
                plan.element_load_render_planned &&
                plan.loop_continue_render_planned) {
                sequences.push_back(ComputedDynamicArrayForLoopRenderSequenceMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .body_block_name = plan.body_block_name,
                    .rendered_ir = plan.rendered_ir,
                });
            }
        }
    );
    return sequences;
}

auto collect_computed_dynamic_array_for_loop_exit_cleanups(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForLoopExitCleanupMetadata> {
    auto cleanups = std::vector<ComputedDynamicArrayForLoopExitCleanupMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&cleanups](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_loop_exit_cleanup(
                statement.expression,
                lowering_context,
                state
            );
            if (plan.kind ==
                    ComputedDynamicArrayIterableLoopExitCleanupPlanKind::loop_exit_cleanup_planned &&
                plan.exit_block_planned &&
                plan.cleanup_resumption_planned &&
                !plan.cleanup_resumption_operation_name.empty()) {
                cleanups.push_back(ComputedDynamicArrayForLoopExitCleanupMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .exit_block_name = plan.exit_block_name,
                    .loop_entry_cleanup_owner_name = plan.loop_entry_cleanup_owner_name,
                    .loop_exit_cleanup_owner_name = plan.loop_exit_cleanup_owner_name,
                    .cleanup_resumption_operation_name = plan.cleanup_resumption_operation_name,
                    .rendered_ir = plan.rendered_ir,
                });
            }
        }
    );
    return cleanups;
}

auto collect_computed_dynamic_array_for_cleanup_transitions(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForCleanupTransitionMetadata> {
    auto transitions = std::vector<ComputedDynamicArrayForCleanupTransitionMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&transitions](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto plan = plan_computed_dynamic_array_iterable_loop_exit_cleanup(
                statement.expression,
                lowering_context,
                state
            );
            auto const& cleanup_sequence_plan = plan.loop_render_sequence_plan.loop_continue_render_plan
                .element_load_render_plan.element_address_render_plan.loop_control_render_plan
                .descriptor_render_plan.cleanup_sequence_plan;
            if (plan.kind ==
                    ComputedDynamicArrayIterableLoopExitCleanupPlanKind::loop_exit_cleanup_planned &&
                plan.cleanup_resumption_planned &&
                !cleanup_sequence_plan.cleanup_owner_name.empty() &&
                !cleanup_sequence_plan.loop_entry_cleanup_owner_name.empty() &&
                !cleanup_sequence_plan.loop_entry_cleanup_operation_name.empty() &&
                !plan.cleanup_resumption_operation_name.empty()) {
                transitions.push_back(ComputedDynamicArrayForCleanupTransitionMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = plan.cleanup_owner_name,
                    .source_type_name = plan.source_type_name,
                    .element_source_type_name = plan.element_source_type_name,
                    .acquisition_source_owner_name = cleanup_sequence_plan.cleanup_owner_name,
                    .acquisition_target_owner_name = cleanup_sequence_plan.loop_entry_cleanup_owner_name,
                    .acquisition_operation_name = cleanup_sequence_plan.loop_entry_cleanup_operation_name,
                    .resumption_source_owner_name = plan.loop_entry_cleanup_owner_name,
                    .resumption_target_owner_name = plan.loop_exit_cleanup_owner_name,
                    .resumption_operation_name = plan.cleanup_resumption_operation_name,
                });
            }
        }
    );
    return transitions;
}

auto collect_computed_dynamic_array_for_production_emission_gates(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context,
    LlvmIrEmissionOptions const& options
) -> std::vector<ComputedDynamicArrayForProductionEmissionGateMetadata> {
    auto gates = std::vector<ComputedDynamicArrayForProductionEmissionGateMetadata> {};
    auto const insertion_capability =
        computed_dynamic_array_cleanup_call_insertion_capability(options);
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&gates, insertion_capability](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto gate = plan_computed_dynamic_array_iterable_production_emission_gate(
                statement.expression,
                lowering_context,
                state
            );
            if (computed_dynamic_array_iterable_production_emission_gate_ready(gate)) {
                gates.push_back(ComputedDynamicArrayForProductionEmissionGateMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = gate.cleanup_owner_name,
                    .source_type_name = gate.source_type_name,
                    .element_source_type_name = gate.element_source_type_name,
                    .rendered_ir = gate.rendered_ir,
                    .ownership_ready = gate.ownership_ready,
                    .loop_render_ready = gate.loop_render_ready,
                    .loop_cleanup_ownership_ready = gate.loop_cleanup_ownership_ready,
                    .function_cleanup_resumption_ready = gate.function_cleanup_resumption_ready,
                    .exit_cleanup_ready = gate.exit_cleanup_ready,
                    .production_sequence_render_planned = gate.production_sequence_render_planned,
                    .production_emission_enabled = insertion_capability.enabled,
                });
            }
        }
    );
    return gates;
}

auto collect_computed_dynamic_array_for_production_sequences(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context
) -> std::vector<ComputedDynamicArrayForProductionSequenceMetadata> {
    auto sequences = std::vector<ComputedDynamicArrayForProductionSequenceMetadata> {};
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&sequences](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto gate = plan_computed_dynamic_array_iterable_production_emission_gate(
                statement.expression,
                lowering_context,
                state
            );
            if (computed_dynamic_array_iterable_production_emission_gate_ready(gate)) {
                sequences.push_back(ComputedDynamicArrayForProductionSequenceMetadata {
                    .enclosing_function_name = std::string {enclosing_function_name},
                    .source_line = statement.line,
                    .cleanup_owner_name = gate.cleanup_owner_name,
                    .source_type_name = gate.source_type_name,
                    .element_source_type_name = gate.element_source_type_name,
                    .rendered_ir = gate.rendered_ir,
                });
            }
        }
    );
    return sequences;
}

auto collect_computed_dynamic_array_for_consumed_cleanup_descriptors(
    syntax::ModuleSyntax const& module,
    LoweringContext const& context,
    LlvmIrEmissionOptions const& options
) -> std::vector<ComputedDynamicArrayForConsumedCleanupDescriptorMetadata> {
    auto descriptors = std::vector<ComputedDynamicArrayForConsumedCleanupDescriptorMetadata> {};
    auto const insertion_capability =
        computed_dynamic_array_cleanup_call_insertion_capability(options);
    auto const production_computed_cleanup_emission_enabled =
        options.enable_computed_dynamic_array_consumed_cleanup_descriptor_collection &&
        insertion_capability.enabled;
    if (!production_computed_cleanup_emission_enabled) {
        return descriptors;
    }
    collect_computed_dynamic_array_for_module(
        module,
        context,
        [&descriptors](
            syntax::StatementSyntax const& statement,
            std::string_view enclosing_function_name,
            LoweringContext const& lowering_context,
            FunctionLoweringState& state
        ) {
            auto gate = plan_computed_dynamic_array_iterable_production_emission_gate(
                statement.expression,
                lowering_context,
                state
            );
            if (!computed_dynamic_array_iterable_production_emission_gate_ready(gate)) {
                return;
            }
            auto const& descriptor_plan = gate.loop_exit_cleanup_plan.loop_render_sequence_plan
                .loop_continue_render_plan.element_load_render_plan.element_address_render_plan
                .loop_control_render_plan.descriptor_render_plan;
            auto finalization_plan = plan_consumed_descriptor_finalization(
                gate.cleanup_owner_name,
                descriptor_plan.descriptor_storage_name,
                gate.loop_exit_cleanup_plan.cleanup_resumption_operation_name
            );
            auto const finalization_readiness = plan_consumed_descriptor_finalization_readiness(finalization_plan);
            if (!finalization_readiness.ready) {
                return;
            }
            descriptors.push_back(ComputedDynamicArrayForConsumedCleanupDescriptorMetadata {
                .enclosing_function_name = std::string {enclosing_function_name},
                .source_line = statement.line,
                .source_type_name = gate.source_type_name,
                .element_source_type_name = gate.element_source_type_name,
                .finalization_plan = std::move(finalization_plan),
            });
        }
    );
    return descriptors;
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
    if (
        options.enable_computed_dynamic_array_consumed_cleanup_descriptor_collection &&
        computed_dynamic_array_cleanup_call_insertion_capability(options).enabled &&
        !collect_computed_dynamic_array_for_production_sequences(module, context).empty()
    ) {
        push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::deallocate);
    }
    if (has_view_index_read(module)) {
        push_dynamic_array_runtime_operation_once(operations, DynamicArrayRuntimeOperation::bounds_failed);
    }
    for (auto const& request : options.fixture_dynamic_array_construction_requests) {
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
    std::vector<FixtureDynamicArrayConstructionRequest> const& requests
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

void append_function_emission_reports(
    LlvmIrEmissionResult& result,
    FunctionEmissionResult const& function_emission
) {
    result.generated_module_symbols.insert(
        result.generated_module_symbols.end(),
        function_emission.generated_module_symbols.begin(),
        function_emission.generated_module_symbols.end()
    );
    result.emitted_dynamic_array_cleanup_obligations.reserve(
        result.emitted_dynamic_array_cleanup_obligations.size() +
        function_emission.emitted_dynamic_array_cleanup_obligations.size()
    );
    for (auto const& obligation : function_emission.emitted_dynamic_array_cleanup_obligations) {
        result.emitted_dynamic_array_cleanup_obligations.push_back(
            DynamicArrayCleanupObligationRecord {
                .function_symbol_name = function_emission.function_symbol_name,
                .obligation = obligation,
            }
        );
    }
    result.emitted_dynamic_array_cleanup_sequence_plans.reserve(
        result.emitted_dynamic_array_cleanup_sequence_plans.size() +
        function_emission.emitted_dynamic_array_cleanup_sequence_plans.size()
    );
    for (auto const& plan : function_emission.emitted_dynamic_array_cleanup_sequence_plans) {
        result.emitted_dynamic_array_cleanup_sequence_plans.push_back(
            DynamicArrayCleanupSequencePlanRecord {
                .function_symbol_name = function_emission.function_symbol_name,
                .plan = plan,
            }
        );
    }
    result.emitted_dynamic_array_cleanup_sequence_verifications.reserve(
        result.emitted_dynamic_array_cleanup_sequence_verifications.size() +
        function_emission.emitted_dynamic_array_cleanup_sequence_verifications.size()
    );
    for (auto const& verification : function_emission.emitted_dynamic_array_cleanup_sequence_verifications) {
        result.emitted_dynamic_array_cleanup_sequence_verifications.push_back(
            DynamicArrayCleanupSequenceVerificationRecord {
                .function_symbol_name = function_emission.function_symbol_name,
                .verification = verification,
            }
        );
    }
    result.emitted_dynamic_array_cleanup_emission_capabilities.reserve(
        result.emitted_dynamic_array_cleanup_emission_capabilities.size() +
        function_emission.emitted_dynamic_array_cleanup_emission_capabilities.size()
    );
    for (auto const& capability : function_emission.emitted_dynamic_array_cleanup_emission_capabilities) {
        result.emitted_dynamic_array_cleanup_emission_capabilities.push_back(
            DynamicArrayCleanupEmissionCapabilityRecord {
                .function_symbol_name = function_emission.function_symbol_name,
                .capability = capability,
            }
        );
    }
    result.aggregate_projection_access_plans.reserve(
        result.aggregate_projection_access_plans.size() +
        function_emission.aggregate_projection_access_plans.size()
    );
    for (auto const& plan : function_emission.aggregate_projection_access_plans) {
        result.aggregate_projection_access_plans.push_back(AggregateProjectionAccessPlanRecord {
            .function_symbol_name = function_emission.function_symbol_name,
            .plan = plan,
        });
    }
    result.computed_dynamic_array_inserted_cleanup_handoffs.insert(
        result.computed_dynamic_array_inserted_cleanup_handoffs.end(),
        function_emission.computed_dynamic_array_inserted_cleanup_handoffs.begin(),
        function_emission.computed_dynamic_array_inserted_cleanup_handoffs.end()
    );
    result.computed_dynamic_array_cleanup_call_operands.insert(
        result.computed_dynamic_array_cleanup_call_operands.end(),
        function_emission.computed_dynamic_array_cleanup_call_operands.begin(),
        function_emission.computed_dynamic_array_cleanup_call_operands.end()
    );
    result.consumed_descriptor_finalization_plans.insert(
        result.consumed_descriptor_finalization_plans.end(),
        function_emission.consumed_descriptor_finalization_plans.begin(),
        function_emission.consumed_descriptor_finalization_plans.end()
    );
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
        } else if (has_dynamic_array_local_constructor_origin(origin, module)) {
            plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
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

auto format_computed_dynamic_array_for_production_sequence_metadata(
    ComputedDynamicArrayForProductionSequenceMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "production sequence",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_production_sequence_metadata_report(
    std::vector<ComputedDynamicArrayForProductionSequenceMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& sequence) {
            return format_computed_dynamic_array_for_production_sequence_metadata(sequence);
        }
    );
}

auto format_computed_dynamic_array_for_descriptor_render_metadata(
    ComputedDynamicArrayForDescriptorRenderMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "descriptor render",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    append_if_present(output, "descriptor", metadata.descriptor_storage_name);
    append_if_present(output, "value", metadata.descriptor_value_name);
    append_if_present(output, "data", metadata.data_pointer_name);
    append_if_present(output, "length", metadata.length_name);
    append_if_present(output, "capacity", metadata.capacity_name);
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_descriptor_render_metadata_report(
    std::vector<ComputedDynamicArrayForDescriptorRenderMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& render) {
            return format_computed_dynamic_array_for_descriptor_render_metadata(render);
        }
    );
}

auto format_computed_dynamic_array_for_loop_control_render_metadata(
    ComputedDynamicArrayForLoopControlRenderMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "loop control render",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    append_if_present(output, "condition", metadata.condition_block_name);
    append_if_present(output, "body", metadata.body_block_name);
    append_if_present(output, "continue", metadata.continue_block_name);
    append_if_present(output, "exit", metadata.exit_block_name);
    append_if_present(output, "index", metadata.index_name);
    append_if_present(output, "next", metadata.next_index_name);
    append_if_present(output, "bounds", metadata.bounds_check_name);
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_loop_control_render_metadata_report(
    std::vector<ComputedDynamicArrayForLoopControlRenderMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& render) {
            return format_computed_dynamic_array_for_loop_control_render_metadata(render);
        }
    );
}

auto format_computed_dynamic_array_for_element_address_render_metadata(
    ComputedDynamicArrayForElementAddressRenderMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for element address render";
    append_if_present(output, "function", metadata.enclosing_function_name);
    if (metadata.source_line != 0) {
        output << " line " << metadata.source_line;
    }
    append_if_present(output, "source", metadata.source_type_name);
    append_if_present(output, "element", metadata.element_source_type_name);
    append_if_present(output, "lowers-to", metadata.element_llvm_type_name);
    append_if_present(output, "owner", metadata.cleanup_owner_name);
    append_if_present(output, "data", metadata.data_pointer_name);
    append_if_present(output, "index", metadata.index_name);
    append_if_present(output, "address", metadata.element_address_name);
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_element_address_render_metadata_report(
    std::vector<ComputedDynamicArrayForElementAddressRenderMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& render) {
            return format_computed_dynamic_array_for_element_address_render_metadata(render);
        }
    );
}

auto format_computed_dynamic_array_for_element_load_render_metadata(
    ComputedDynamicArrayForElementLoadRenderMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for element load render";
    append_if_present(output, "function", metadata.enclosing_function_name);
    if (metadata.source_line != 0) {
        output << " line " << metadata.source_line;
    }
    append_if_present(output, "source", metadata.source_type_name);
    append_if_present(output, "element", metadata.element_source_type_name);
    append_if_present(output, "lowers-to", metadata.element_llvm_type_name);
    append_if_present(output, "owner", metadata.cleanup_owner_name);
    append_if_present(output, "address", metadata.element_address_name);
    append_if_present(output, "item", metadata.item_value_name);
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_element_load_render_metadata_report(
    std::vector<ComputedDynamicArrayForElementLoadRenderMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& render) {
            return format_computed_dynamic_array_for_element_load_render_metadata(render);
        }
    );
}

auto format_computed_dynamic_array_for_loop_continue_render_metadata(
    ComputedDynamicArrayForLoopContinueRenderMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "loop continue render",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    append_if_present(output, "continue", metadata.continue_block_name);
    append_if_present(output, "condition", metadata.condition_block_name);
    append_if_present(output, "index", metadata.index_name);
    append_if_present(output, "next", metadata.next_index_name);
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_loop_continue_render_metadata_report(
    std::vector<ComputedDynamicArrayForLoopContinueRenderMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& render) {
            return format_computed_dynamic_array_for_loop_continue_render_metadata(render);
        }
    );
}

auto format_computed_dynamic_array_for_loop_render_sequence_metadata(
    ComputedDynamicArrayForLoopRenderSequenceMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "loop render sequence",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    append_if_present(output, "body", metadata.body_block_name);
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_loop_render_sequence_metadata_report(
    std::vector<ComputedDynamicArrayForLoopRenderSequenceMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& sequence) {
            return format_computed_dynamic_array_for_loop_render_sequence_metadata(sequence);
        }
    );
}

auto format_computed_dynamic_array_for_loop_exit_cleanup_metadata(
    ComputedDynamicArrayForLoopExitCleanupMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "loop exit cleanup",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    append_if_present(output, "exit", metadata.exit_block_name);
    append_if_present(output, "from", metadata.loop_entry_cleanup_owner_name);
    append_if_present(output, "to", metadata.loop_exit_cleanup_owner_name);
    append_if_present(output, "operation", metadata.cleanup_resumption_operation_name);
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_loop_exit_cleanup_metadata_report(
    std::vector<ComputedDynamicArrayForLoopExitCleanupMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& cleanup) {
            return format_computed_dynamic_array_for_loop_exit_cleanup_metadata(cleanup);
        }
    );
}

auto format_computed_dynamic_array_for_cleanup_transition_metadata(
    ComputedDynamicArrayForCleanupTransitionMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "cleanup transition",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    append_if_present(output, "acquire-from", metadata.acquisition_source_owner_name);
    append_if_present(output, "acquire-to", metadata.acquisition_target_owner_name);
    append_if_present(output, "acquire-operation", metadata.acquisition_operation_name);
    append_if_present(output, "resume-from", metadata.resumption_source_owner_name);
    append_if_present(output, "resume-to", metadata.resumption_target_owner_name);
    append_if_present(output, "resume-operation", metadata.resumption_operation_name);
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_cleanup_transition_metadata_report(
    std::vector<ComputedDynamicArrayForCleanupTransitionMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& transition) {
            return format_computed_dynamic_array_for_cleanup_transition_metadata(transition);
        }
    );
}

auto format_computed_dynamic_array_for_consumed_cleanup_descriptor_metadata(
    ComputedDynamicArrayForConsumedCleanupDescriptorMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "consumed cleanup descriptor model",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.finalization_plan.cleanup_owner_name
    );
    append_if_present(output, "descriptor", metadata.finalization_plan.descriptor_storage_name);
    append_if_present(output, "cleanup-operation", metadata.finalization_plan.cleanup_operation_name);
    output << " [generic finalization proof referenced]";
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_consumed_cleanup_descriptor_metadata_report(
    std::vector<ComputedDynamicArrayForConsumedCleanupDescriptorMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& descriptor) {
            return format_computed_dynamic_array_for_consumed_cleanup_descriptor_metadata(descriptor);
        }
    );
}

auto format_computed_dynamic_array_for_production_emission_gate_metadata(
    ComputedDynamicArrayForProductionEmissionGateMetadata const& metadata
) -> std::string {
    auto output = std::ostringstream {};
    append_computed_dynamic_array_for_metadata_prefix(
        output,
        "production emission gate",
        metadata.enclosing_function_name,
        metadata.source_line,
        metadata.source_type_name,
        metadata.element_source_type_name,
        metadata.cleanup_owner_name
    );
    output << (metadata.ownership_ready ? " [ownership ready]" : " [ownership missing]");
    output << (metadata.loop_render_ready ? " [loop render ready]" : " [loop render missing]");
    output << (metadata.loop_cleanup_ownership_ready ? " [loop cleanup ownership ready]" :
        " [loop cleanup ownership missing]");
    output << (metadata.function_cleanup_resumption_ready ? " [function cleanup resumption ready]" :
        " [function cleanup resumption missing]");
    output << (metadata.exit_cleanup_ready ? " [exit cleanup ready]" : " [exit cleanup missing]");
    output << (metadata.production_sequence_render_planned ? " [production sequence planned]" :
        " [production sequence missing]");
    output << (metadata.production_emission_enabled ? " [production emission enabled]" :
        " [production emission disabled]");
    output << " snippets " << metadata.rendered_ir.size();
    output << " (metadata only)";
    return output.str();
}

auto format_computed_dynamic_array_for_production_emission_gate_metadata_report(
    std::vector<ComputedDynamicArrayForProductionEmissionGateMetadata> const& metadata
) -> std::vector<std::string> {
    return format_computed_dynamic_array_for_metadata_report(
        metadata,
        [](auto const& gate) {
            return format_computed_dynamic_array_for_production_emission_gate_metadata(gate);
        }
    );
}

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

auto LlvmIrEmissionResult::computed_dynamic_array_for_production_sequence_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_production_sequence_metadata_report(
        computed_dynamic_array_for_production_sequences
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_descriptor_render_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_descriptor_render_metadata_report(
        computed_dynamic_array_for_descriptor_renders
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_loop_control_render_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_loop_control_render_metadata_report(
        computed_dynamic_array_for_loop_control_renders
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_element_address_render_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_element_address_render_metadata_report(
        computed_dynamic_array_for_element_address_renders
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_element_load_render_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_element_load_render_metadata_report(
        computed_dynamic_array_for_element_load_renders
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_loop_continue_render_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_loop_continue_render_metadata_report(
        computed_dynamic_array_for_loop_continue_renders
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_loop_render_sequence_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_loop_render_sequence_metadata_report(
        computed_dynamic_array_for_loop_render_sequences
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_loop_exit_cleanup_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_loop_exit_cleanup_metadata_report(
        computed_dynamic_array_for_loop_exit_cleanups
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_cleanup_transition_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_cleanup_transition_metadata_report(
        computed_dynamic_array_for_cleanup_transitions
    );
}

auto LlvmIrEmissionResult::consumed_descriptor_finalization_plan_report() const
    -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    lines.reserve(
        computed_dynamic_array_for_consumed_cleanup_descriptors.size() +
        consumed_descriptor_finalization_plans.size()
    );
    for (auto const& descriptor : computed_dynamic_array_for_consumed_cleanup_descriptors) {
        lines.push_back(format_consumed_descriptor_finalization_plan(descriptor.finalization_plan));
    }
    for (auto const& plan : consumed_descriptor_finalization_plans) {
        lines.push_back(format_consumed_descriptor_finalization_plan(plan));
    }
    return lines;
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_consumed_cleanup_descriptor_model_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_consumed_cleanup_descriptor_metadata_report(
        computed_dynamic_array_for_consumed_cleanup_descriptors
    );
}

auto LlvmIrEmissionResult::computed_dynamic_array_for_production_emission_gate_report() const
    -> std::vector<std::string> {
    return format_computed_dynamic_array_for_production_emission_gate_metadata_report(
        computed_dynamic_array_for_production_emission_gates
    );
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
    result.generated_module_type_symbols = collect_emitted_record_type_symbols(module, context);
    auto module_symbol_registry = ModuleSymbolRegistry {};
    if (!validate_generated_type_symbols(
            result.generated_module_type_symbols,
            module_symbol_registry,
            result.diagnostics
        )) {
        return result;
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
    if (options.collect_computed_dynamic_array_for_descriptor_renders) {
        result.computed_dynamic_array_for_descriptor_renders =
            collect_computed_dynamic_array_for_descriptor_renders(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_descriptor_render_ir,
            result.computed_dynamic_array_for_descriptor_renders
        );
    }
    if (options.collect_computed_dynamic_array_for_loop_control_renders) {
        result.computed_dynamic_array_for_loop_control_renders =
            collect_computed_dynamic_array_for_loop_control_renders(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_loop_control_render_ir,
            result.computed_dynamic_array_for_loop_control_renders
        );
    }
    if (options.collect_computed_dynamic_array_for_element_address_renders) {
        result.computed_dynamic_array_for_element_address_renders =
            collect_computed_dynamic_array_for_element_address_renders(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_element_address_render_ir,
            result.computed_dynamic_array_for_element_address_renders
        );
    }
    if (options.collect_computed_dynamic_array_for_element_load_renders) {
        result.computed_dynamic_array_for_element_load_renders =
            collect_computed_dynamic_array_for_element_load_renders(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_element_load_render_ir,
            result.computed_dynamic_array_for_element_load_renders
        );
    }
    if (options.collect_computed_dynamic_array_for_loop_continue_renders) {
        result.computed_dynamic_array_for_loop_continue_renders =
            collect_computed_dynamic_array_for_loop_continue_renders(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_loop_continue_render_ir,
            result.computed_dynamic_array_for_loop_continue_renders
        );
    }
    if (options.collect_computed_dynamic_array_for_loop_render_sequences) {
        result.computed_dynamic_array_for_loop_render_sequences =
            collect_computed_dynamic_array_for_loop_render_sequences(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_loop_render_sequence_ir,
            result.computed_dynamic_array_for_loop_render_sequences
        );
    }
    if (options.collect_computed_dynamic_array_for_loop_exit_cleanups) {
        result.computed_dynamic_array_for_loop_exit_cleanups =
            collect_computed_dynamic_array_for_loop_exit_cleanups(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_loop_exit_cleanup_ir,
            result.computed_dynamic_array_for_loop_exit_cleanups
        );
    }
    if (options.collect_computed_dynamic_array_for_cleanup_transitions) {
        result.computed_dynamic_array_for_cleanup_transitions =
            collect_computed_dynamic_array_for_cleanup_transitions(module, context);
    }
    result.computed_dynamic_array_for_consumed_cleanup_descriptors =
        collect_computed_dynamic_array_for_consumed_cleanup_descriptors(module, context, options);
    if (options.collect_computed_dynamic_array_for_production_emission_gates) {
        result.computed_dynamic_array_for_production_emission_gates =
            collect_computed_dynamic_array_for_production_emission_gates(module, context, options);
        append_rendered_ir(
            result.computed_dynamic_array_for_production_emission_gate_ir,
            result.computed_dynamic_array_for_production_emission_gates
        );
    }
    if (options.collect_computed_dynamic_array_for_production_sequences ||
        options.emit_computed_dynamic_array_for_production_sequence_comments) {
        result.computed_dynamic_array_for_production_sequences =
            collect_computed_dynamic_array_for_production_sequences(module, context);
        append_rendered_ir(
            result.computed_dynamic_array_for_production_sequence_ir,
            result.computed_dynamic_array_for_production_sequences
        );
        if (options.emit_computed_dynamic_array_for_production_sequence_comments) {
            result.computed_dynamic_array_for_production_sequence_module_ir =
                render_computed_dynamic_array_for_production_sequence_module_comments(
                    result.computed_dynamic_array_for_production_sequences
                );
        }
    }
    if (options.test_only_render_dynamic_array_element_drop_walks ||
        dynamic_array_cleanup_emission_enabled(options)) {
        auto dynamic_array_drop_cleanups =
            collect_dynamic_array_element_drop_cleanups(
                result.dynamic_array_construction_plans,
                options.fixture_dynamic_array_construction_requests
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
            result.generated_module_symbols.push_back(GeneratedModuleSymbol {
                .symbol_name = cleanup.cleanup_symbol_name,
                .category = "generated dynamic array cleanup helper",
                .line = cleanup.actions.empty() ? 1 : cleanup.actions.front().discovery_line,
            });
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
        for (auto const& line : result.computed_dynamic_array_for_production_sequence_module_ir) {
            output << line;
        }
        result.ir_text = output.str();
        return result;
    }
    auto source_defined_drop_symbols =
        collect_source_drop_definition_symbols(module, result.semantic_drop_lowering_authorizations);
    auto concurrency_runtime_operations = collect_concurrency_runtime_operations(module);
    if (!validate_prelude_module_symbols(
            module,
            context,
            concurrency_runtime_operations,
            result.planned_drop_declarations,
            result.dynamic_array_runtime_operations,
            source_defined_drop_symbols,
            module_symbol_registry,
            result.diagnostics
        )) {
        return result;
    }
    output << emit_module_prelude(
        string_constants,
        context.foreign_declarations,
        concurrency_runtime_operations,
        result.planned_drop_declarations,
        result.dynamic_array_runtime_operations,
        source_defined_drop_symbols
    );
    output << emit_source_drop_definitions(module, result.semantic_drop_lowering_authorizations);
    for (auto const& function : module.functions) {
        if (is_uninstantiated_generic_function(function)) {
            continue;
        }
        auto signature = context.functions.find(function.name);
        if (signature == context.functions.end()) {
            result.diagnostics.error(function.line, "lowering context is missing function signature");
            return result;
        }
        auto function_emission = emit_function_with_metadata(
            function,
            signature->second,
            context,
            string_constants,
            semantic_result,
            result.diagnostics,
            options
        );
        output << function_emission.ir_text;
        append_function_emission_reports(result, function_emission);
        output << "\n";
    }
    for (auto const& function_ptr : context.generic_function_specializations) {
        auto const& function = *function_ptr;
        auto signature = context.functions.find(function.name);
        if (signature == context.functions.end()) {
            result.diagnostics.error(function.line, "lowering context is missing generic function specialization");
            return result;
        }
        auto function_emission = emit_function_with_metadata(
            function,
            signature->second,
            context,
            string_constants,
            semantic_result,
            result.diagnostics,
            options
        );
        output << function_emission.ir_text;
        append_function_emission_reports(result, function_emission);
        output << "\n";
    }

    auto method_index = std::size_t {0};
    auto emit_method = [&](syntax::TypeSyntax const& receiver_type, syntax::FunctionSyntax const& method) -> bool {
        if (method_index >= context.methods.size()) {
            result.diagnostics.error(method.line, "lowering context is missing method signature");
            return false;
        }
        auto const& lowered_method = context.methods[method_index++];
        if (is_uninstantiated_generic_function(method) || is_generic_receiver_pattern(receiver_type, module)) {
            return true;
        }
        auto function_emission = emit_function_with_metadata(
            method,
            lowered_method.signature,
            context,
            string_constants,
            semantic_result,
            result.diagnostics,
            options
        );
        output << function_emission.ir_text;
        append_function_emission_reports(result, function_emission);
        output << "\n";
        return !result.has_errors();
    };

    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            if (!emit_method(implementation.receiver_type, method)) {
                return result;
            }
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            if (!emit_method(extension.receiver_type, method)) {
                return result;
            }
        }
    }
    for (auto const& specialization : context.generic_method_specializations) {
        if (specialization.method == nullptr) {
            continue;
        }
        auto method = std::ranges::find_if(
            context.methods,
            [&](LoweredMethodSignature const& candidate) {
                return candidate.signature.symbol_name == specialization.symbol_name;
            }
        );
        if (method == context.methods.end()) {
            result.diagnostics.error(
                specialization.method->line,
                "lowering context is missing generic method specialization"
            );
            return result;
        }
        auto function_emission = emit_function_with_metadata(
            *specialization.method,
            method->signature,
            context,
            string_constants,
            semantic_result,
            result.diagnostics,
            options
        );
        output << function_emission.ir_text;
        append_function_emission_reports(result, function_emission);
        output << "\n";
        if (result.has_errors()) {
            return result;
        }
    }

    for (auto const& line : result.computed_dynamic_array_for_production_sequence_module_ir) {
        output << line;
    }

    if (!validate_generated_definition_symbols(
            result.generated_module_symbols,
            module_symbol_registry,
            result.diagnostics
        )) {
        return result;
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
