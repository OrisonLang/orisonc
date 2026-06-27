#include "orison/lowering/module_prelude.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

auto has_emitted_symbol(
    std::vector<std::string_view> const& emitted_symbols,
    std::string_view symbol_name
) -> bool {
    for (auto emitted_symbol : emitted_symbols) {
        if (emitted_symbol == symbol_name) {
            return true;
        }
    }
    return false;
}

void emit_declaration(
    std::ostringstream& output,
    std::string_view return_type,
    std::string_view symbol_name,
    std::vector<std::string_view> const& parameter_types
) {
    output << "declare " << return_type << " @" << symbol_name << "(";
    for (auto index = std::size_t {0}; index < parameter_types.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << parameter_types[index];
    }
    output << ")\n";
}

}  // namespace

auto format_planned_drop_declaration(DropPreludeDeclaration const& declaration) -> std::string {
    auto output = std::ostringstream {};
    output << "planned drop " << declaration.symbol_name;
    if (!declaration.source_type_name.empty()) {
        output << " for " << declaration.source_type_name;
    }
    if (declaration.discovery_line > 0) {
        output << " discovered at line " << declaration.discovery_line;
    }
    if (!declaration.emit_declaration) {
        output << " (metadata only)";
    }
    return output.str();
}

auto add_planned_drop_declaration(
    std::vector<DropPreludeDeclaration>& declarations,
    DropPreludeDeclaration declaration
) -> bool {
    for (auto const& existing_declaration : declarations) {
        if (existing_declaration.symbol_name == declaration.symbol_name) {
            return false;
        }
    }
    declarations.push_back(std::move(declaration));
    return true;
}

auto emit_module_prelude(
    StringConstantTable const& string_constants,
    std::vector<LoweredFunctionSignature> const& foreign_declarations,
    std::vector<ConcurrencyRuntimeOperation> const& concurrency_runtime_operations,
    std::vector<DropPreludeDeclaration> const& drop_declarations
) -> std::string {
    auto output = std::ostringstream {};
    for (auto const& constant : string_constants.constants) {
        output << "@" << constant.name << " = private unnamed_addr constant [";
        output << constant.bytes.size() << " x i8] c\"" << constant.llvm_bytes << "\"\n";
    }
    if (!string_constants.constants.empty()) {
        output << "\n";
    }

    for (auto const& declaration : foreign_declarations) {
        output << "declare " << declaration.return_type << " @" << declaration.symbol_name << "(";
        auto emitted_parameter_count = declaration.adapter == CAbiAdapterKind::variadic
            ? declaration.fixed_abi_parameter_count
            : declaration.parameter_types.size();
        for (auto index = std::size_t {0}; index < emitted_parameter_count; ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << declaration.parameter_types[index];
        }
        if (declaration.adapter == CAbiAdapterKind::variadic) {
            if (emitted_parameter_count > 0) {
                output << ", ";
            }
            output << "...";
        }
        output << ")\n";
    }
    if (!foreign_declarations.empty()) {
        output << "\n";
    }

    auto emitted_runtime_symbols = std::vector<std::string_view> {};
    for (auto operation : concurrency_runtime_operations) {
        auto runtime_call = concurrency_runtime_call(operation);
        if (has_emitted_symbol(emitted_runtime_symbols, runtime_call.symbol_name)) {
            continue;
        }
        emit_declaration(
            output,
            runtime_call.return_type,
            runtime_call.symbol_name,
            runtime_call.parameter_types
        );
        emitted_runtime_symbols.push_back(runtime_call.symbol_name);
    }
    if (!emitted_runtime_symbols.empty()) {
        output << "\n";
    }

    auto emitted_drop_symbols = std::vector<std::string_view> {};
    for (auto const& declaration : drop_declarations) {
        if (!declaration.emit_declaration) {
            continue;
        }
        if (has_emitted_symbol(emitted_drop_symbols, declaration.symbol_name)) {
            continue;
        }
        emit_declaration(output, "void", declaration.symbol_name, {"ptr"});
        emitted_drop_symbols.push_back(declaration.symbol_name);
    }
    if (!emitted_drop_symbols.empty()) {
        output << "\n";
    }
    return output.str();
}

}  // namespace orison::lowering
