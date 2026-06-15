#include "orison/lowering/lowering_context.hpp"

#include <cassert>
#include <utility>

int main() {
    using orison::syntax::ForeignImportBlockSyntax;
    using orison::syntax::ForeignImportFunctionSyntax;
    using orison::syntax::FunctionSyntax;
    using orison::syntax::ImplementationSyntax;
    using orison::syntax::ExtensionSyntax;
    using orison::syntax::ModuleSyntax;
    using orison::syntax::ParameterSyntax;
    using orison::syntax::TypeSyntax;

    auto module = ModuleSyntax {};
    module.functions.push_back(FunctionSyntax {
        .name = "main",
        .return_type = TypeSyntax {.name = "Int32"},
    });
    auto implementation = ImplementationSyntax {
        .interface_type = TypeSyntax {.name = "Reader"},
        .receiver_type = TypeSyntax {.name = "Device"},
    };
    implementation.methods.push_back(FunctionSyntax {
        .name = "read",
        .parameters = {
            ParameterSyntax {.name = "this", .type = TypeSyntax {.name = "shared.This"}},
            ParameterSyntax {.name = "offset", .type = TypeSyntax {.name = "UInt32"}},
        },
        .return_type = TypeSyntax {.name = "UInt32"},
    });
    module.implementations.push_back(std::move(implementation));

    auto extension = ExtensionSyntax {
        .receiver_type = TypeSyntax {
            .name = "Box",
            .generic_arguments = {TypeSyntax {.name = "UInt32"}},
        },
    };
    extension.methods.push_back(FunctionSyntax {
        .name = "reset",
        .parameters = {
            ParameterSyntax {.name = "this", .type = TypeSyntax {.name = "exclusive.This"}},
        },
        .return_type = TypeSyntax {.name = "Unit"},
    });
    module.extensions.push_back(std::move(extension));
    module.foreign_imports.push_back(ForeignImportBlockSyntax {
        .abi = "\"c\"",
        .functions = {
            ForeignImportFunctionSyntax {
                .name = "print_checked",
                .parameters = {
                    ParameterSyntax {
                        .name = "format",
                        .type = TypeSyntax {
                            .name = "Pointer",
                            .generic_arguments = {TypeSyntax {.name = "Byte"}},
                        },
                    },
                    ParameterSyntax {.name = "value", .type = TypeSyntax {.name = "Int16"}},
                },
                .return_type = TypeSyntax {.name = "Int32"},
                .external_name = "\"printf\"",
            },
        },
    });

    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    auto context = orison::lowering::build_lowering_context(module, diagnostics);
    assert(!diagnostics.has_errors());
    assert(context.functions.size() == 2);
    assert(context.methods.size() == 2);
    assert(context.foreign_declarations.size() == 1);
    assert(context.methods[0].receiver_type_name == "Device");
    assert(context.methods[0].method_name == "read");
    assert(context.methods[0].signature.return_type == "i32");
    assert(context.methods[0].signature.parameter_types.size() == 2);
    assert(context.methods[0].signature.parameter_types[0].empty());
    assert(context.methods[0].signature.parameter_types[1] == "i32");
    assert(context.methods[1].receiver_type_name == "Box<UInt32>");
    assert(context.methods[1].method_name == "reset");
    assert(context.methods[1].signature.return_type == "void");
    assert(context.methods[1].signature.parameter_types.size() == 1);
    assert(context.methods[1].signature.parameter_types[0].empty());
    auto read_lookup = orison::lowering::find_lowered_method_signature(context, "Device", "read");
    assert(read_lookup.result == orison::lowering::LoweredMethodLookupResult::found);
    assert(read_lookup.method == &context.methods[0]);
    assert(read_lookup.method->signature.return_type == "i32");
    auto missing_receiver_lookup =
        orison::lowering::find_lowered_method_signature(context, "MissingDevice", "read");
    assert(missing_receiver_lookup.result == orison::lowering::LoweredMethodLookupResult::not_found);
    assert(missing_receiver_lookup.method == nullptr);
    auto missing_name_lookup = orison::lowering::find_lowered_method_signature(context, "Device", "write");
    assert(missing_name_lookup.result == orison::lowering::LoweredMethodLookupResult::not_found);
    assert(missing_name_lookup.method == nullptr);

    auto duplicate_context = context;
    duplicate_context.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Device",
        .method_name = "read",
        .signature = context.methods[0].signature,
    });
    auto ambiguous_lookup =
        orison::lowering::find_lowered_method_signature(duplicate_context, "Device", "read");
    assert(ambiguous_lookup.result == orison::lowering::LoweredMethodLookupResult::ambiguous);
    assert(ambiguous_lookup.method == nullptr);

    auto const& foreign = context.foreign_declarations.front();
    assert(foreign.symbol_name == "printf");
    assert(foreign.adapter == orison::lowering::CAbiAdapterKind::variadic);

    module.functions.front().return_type = TypeSyntax {.name = "Text"};
    diagnostics = {};
    context = orison::lowering::build_lowering_context(module, diagnostics);
    assert(!diagnostics.has_errors());
    assert(context.functions.contains("main"));
    assert(context.functions.at("main").return_type.empty());

    module.foreign_imports.front().functions.front().parameters[1].type = TypeSyntax {.name = "Text"};
    diagnostics = {};
    context = orison::lowering::build_lowering_context(module, diagnostics);
    assert(diagnostics.has_errors());
    assert(
        diagnostics.entries().front().message ==
        "foreign symbol 'printf' parameter 'value' has no supported C variadic ABI representation"
    );
    return 0;
}
