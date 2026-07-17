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
    assert(signature.source_return_type_name == "Int32");
    assert(signature.return_type == "i32");
    assert(signature.parameter_types == std::vector<std::string>({"ptr", "i16"}));
    assert(signature.parameter_source_type_names == std::vector<std::string>({"Pointer<Byte>", "Int16"}));

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

    auto dynamic_array_signature = orison::lowering::lower_function_signature(
        TypeSyntax {.name = "UInt32"},
        {
            ParameterSyntax {
                .name = "values",
                .type = TypeSyntax {
                    .name = "DynamicArray",
                    .generic_arguments = {TypeSyntax {.name = "UInt32"}},
                },
            },
        },
        "sum_dynamic"
    );
    assert(!orison::lowering::has_supported_function_signature_types(dynamic_array_signature));
    assert(dynamic_array_signature.source_return_type_name == "UInt32");
    assert(dynamic_array_signature.return_type == "i32");
    assert(dynamic_array_signature.parameter_types.size() == 1);
    assert(dynamic_array_signature.parameter_types.front().empty());
    assert(dynamic_array_signature.parameter_source_type_names == std::vector<std::string>({"DynamicArray<UInt32>"}));

    auto array_return = orison::lowering::lower_function_signature(
        TypeSyntax {
            .name = "Array",
            .generic_arguments = {TypeSyntax {.name = "UInt32"}, TypeSyntax {.name = "3"}},
        },
        {},
        "make_values"
    );
    assert(orison::lowering::has_supported_function_signature_types(array_return));
    assert(array_return.source_return_type_name == "Array<UInt32, 3>");
    assert(array_return.return_type == "[3 x i32]");
    return 0;
}
