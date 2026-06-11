#include "orison/lowering/module_prelude.hpp"

#include <cassert>

int main() {
    auto strings = orison::lowering::StringConstantTable {};
    strings.constants.push_back(orison::lowering::StringConstant {
        .name = ".str.0",
        .bytes = std::string("ready\0", 6),
        .llvm_bytes = "ready\\00",
    });

    auto declarations = std::vector<orison::lowering::LoweredFunctionSignature> {
        orison::lowering::LoweredFunctionSignature {
            .return_type = "i32",
            .parameter_types = {"ptr", "i16"},
            .symbol_name = "printf",
            .adapter = orison::lowering::CAbiAdapterKind::variadic,
            .fixed_abi_parameter_count = 1,
        },
        orison::lowering::LoweredFunctionSignature {
            .return_type = "double",
            .parameter_types = {"double"},
            .symbol_name = "sin",
        },
    };

    auto prelude = orison::lowering::emit_module_prelude(strings, declarations);
    assert(
        prelude ==
        "@.str.0 = private unnamed_addr constant [6 x i8] c\"ready\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "declare double @sin(double)\n"
        "\n"
    );
    assert(orison::lowering::emit_module_prelude({}, {}).empty());
    return 0;
}
