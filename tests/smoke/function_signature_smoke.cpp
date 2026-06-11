#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/function_signature.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    using orison::syntax::ParameterSyntax;
    using orison::syntax::TypeSyntax;

    auto parameters = std::vector<ParameterSyntax> {
        ParameterSyntax {
            .name = "format",
            .type = TypeSyntax {
                .name = "Pointer",
                .generic_arguments = {TypeSyntax {.name = "Byte"}},
            },
        },
        ParameterSyntax {.name = "value", .type = TypeSyntax {.name = "Int16"}},
    };
    auto signature = orison::lowering::lower_function_signature(
        TypeSyntax {.name = "Int32"},
        parameters,
        "printf"
    );
    assert(orison::lowering::has_supported_function_signature_types(signature));
    assert(signature.return_type == "i32");
    assert(signature.parameter_types == std::vector<std::string>({"ptr", "i16"}));

    auto const* adapter = orison::lowering::find_c_abi_adapter("printf");
    assert(adapter != nullptr);
    auto validation = orison::lowering::apply_c_abi_adapter(signature, *adapter);
    assert(validation.error == orison::lowering::CAbiAdapterValidationError::none);
    assert(signature.adapter == orison::lowering::CAbiAdapterKind::variadic);
    assert(signature.fixed_abi_parameter_count == 1);

    auto unsupported = orison::lowering::lower_function_signature(
        TypeSyntax {.name = "Int32"},
        {ParameterSyntax {.name = "value", .type = TypeSyntax {.name = "Text"}}},
        "unsupported"
    );
    assert(!orison::lowering::has_supported_function_signature_types(unsupported));
    assert(unsupported.parameter_types.size() == 1);
    assert(unsupported.parameter_types.front().empty());
    return 0;
}
