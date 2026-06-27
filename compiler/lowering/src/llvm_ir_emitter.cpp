#include "orison/lowering/llvm_ir_emitter.hpp"

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/llvm_ir_verifier.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/module_prelude.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/syntax_traversal.hpp"

#include <cstddef>
#include <sstream>
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
    }
    return output.str();
}

auto is_uninstantiated_generic_function(syntax::FunctionSyntax const& function) -> bool {
    return !function.generic_parameters.empty();
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

}  // namespace

auto LlvmIrEmissionResult::has_errors() const -> bool {
    return diagnostics.has_errors();
}

auto LlvmIrEmissionResult::render(std::string_view path) const -> std::string {
    return diagnostics.render(path);
}

auto LlvmIrEmitter::emit(
    syntax::ModuleSyntax const& module,
    semantics::SemanticAnalysisResult const& semantic_result
) const -> LlvmIrEmissionResult {
    auto result = LlvmIrEmissionResult {};
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
    auto string_constants = collect_string_constants(module);
    auto emission_context = LoweringEmissionContext {
        .lowering = context,
        .string_constants = string_constants,
    };
    result.planned_drop_declarations = plan_concurrency_drop_declarations(
        module,
        emission_context,
        semantic_result
    );
    output << emit_record_layouts(module, context);
    output << emit_module_prelude(
        string_constants,
        context.foreign_declarations,
        collect_concurrency_runtime_operations(module),
        result.planned_drop_declarations
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
            result.diagnostics
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
            result.diagnostics
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

}  // namespace orison::lowering
