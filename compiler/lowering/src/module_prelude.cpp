#include "orison/lowering/module_prelude.hpp"

#include <sstream>

namespace orison::lowering {

auto emit_module_prelude(
    StringConstantTable const& string_constants,
    std::vector<LoweredFunctionSignature> const& foreign_declarations
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
    return output.str();
}

}  // namespace orison::lowering
