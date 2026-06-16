#include "orison/lowering/lowering_context.hpp"

#include <cassert>
#include <utility>

int main() {
    using orison::syntax::ForeignImportBlockSyntax;
    using orison::syntax::ForeignImportFunctionSyntax;
    using orison::syntax::FunctionSyntax;
    using orison::syntax::FieldSyntax;
    using orison::syntax::ImplementationSyntax;
    using orison::syntax::ExtensionSyntax;
    using orison::syntax::ModuleSyntax;
    using orison::syntax::ParameterSyntax;
    using orison::syntax::RecordSyntax;
    using orison::syntax::TypeSyntax;

    auto module = ModuleSyntax {};
    module.functions.push_back(FunctionSyntax {
        .name = "main",
        .return_type = TypeSyntax {.name = "Int32"},
    });
    module.records.push_back(RecordSyntax {
        .name = "UartRegisters",
        .fields = {
            FieldSyntax {.name = "data", .type = TypeSyntax {.name = "UInt32"}},
            FieldSyntax {.name = "status", .type = TypeSyntax {.name = "UInt32"}},
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "Packet",
        .fields = {
            FieldSyntax {
                .name = "bytes",
                .type = TypeSyntax {
                    .name = "Array",
                    .generic_arguments = {
                        TypeSyntax {.name = "Byte"},
                        TypeSyntax {.name = "4"},
                    },
                },
            },
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "Device",
        .fields = {
            FieldSyntax {.name = "registers", .type = TypeSyntax {.name = "UartRegisters"}},
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "Log",
        .fields = {
            FieldSyntax {
                .name = "entries",
                .type = TypeSyntax {
                    .name = "Array",
                    .generic_arguments = {
                        TypeSyntax {.name = "UartRegisters"},
                        TypeSyntax {.name = "2"},
                    },
                },
            },
        },
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

    auto scalar_extension = ExtensionSyntax {
        .receiver_type = TypeSyntax {.name = "UInt32"},
    };
    scalar_extension.methods.push_back(FunctionSyntax {
        .name = "identity",
        .parameters = {
            ParameterSyntax {.name = "this", .type = TypeSyntax {.name = "shared.This"}},
        },
        .return_type = TypeSyntax {.name = "UInt32"},
    });
    module.extensions.push_back(std::move(scalar_extension));
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
    assert(context.records.size() == 4);
    assert(context.methods.size() == 3);
    assert(context.foreign_declarations.size() == 1);
    assert(context.records.contains("UartRegisters"));
    auto const& uart_layout = context.records.at("UartRegisters");
    assert(uart_layout.name == "UartRegisters");
    assert(uart_layout.llvm_type_name == "%record.UartRegisters");
    assert(uart_layout.fields.size() == 2);
    assert(uart_layout.fields[0].name == "data");
    assert(uart_layout.fields[0].source_type_name == "UInt32");
    assert(uart_layout.fields[0].llvm_type == "i32");
    assert(uart_layout.fields[0].index == 0);
    assert(uart_layout.fields[1].name == "status");
    assert(uart_layout.fields[1].source_type_name == "UInt32");
    assert(uart_layout.fields[1].llvm_type == "i32");
    assert(uart_layout.fields[1].index == 1);
    assert(context.records.contains("Packet"));
    auto const& packet_layout = context.records.at("Packet");
    assert(packet_layout.llvm_type_name == "%record.Packet");
    assert(packet_layout.fields.size() == 1);
    assert(packet_layout.fields[0].name == "bytes");
    assert(packet_layout.fields[0].source_type_name == "Array<Byte, 4>");
    assert(packet_layout.fields[0].llvm_type == "[4 x i8]");
    assert(packet_layout.fields[0].index == 0);
    assert(context.records.contains("Device"));
    auto const& device_layout = context.records.at("Device");
    assert(device_layout.fields.size() == 1);
    assert(device_layout.fields[0].name == "registers");
    assert(device_layout.fields[0].source_type_name == "UartRegisters");
    assert(device_layout.fields[0].llvm_type == "%record.UartRegisters");
    assert(device_layout.fields[0].index == 0);
    assert(context.records.contains("Log"));
    auto const& log_layout = context.records.at("Log");
    assert(log_layout.fields.size() == 1);
    assert(log_layout.fields[0].name == "entries");
    assert(log_layout.fields[0].source_type_name == "Array<UartRegisters, 2>");
    assert(log_layout.fields[0].llvm_type == "[2 x %record.UartRegisters]");
    assert(log_layout.fields[0].index == 0);
    assert(context.methods[0].receiver_type_name == "Device");
    assert(context.methods[0].method_name == "read");
    assert(context.methods[0].signature.symbol_name == "method.Device.read");
    assert(context.methods[0].signature.return_type == "i32");
    assert(context.methods[0].signature.parameter_types.size() == 2);
    assert(context.methods[0].signature.parameter_types[0].empty());
    assert(context.methods[0].signature.parameter_types[1] == "i32");
    assert(context.methods[1].receiver_type_name == "Box<UInt32>");
    assert(context.methods[1].method_name == "reset");
    assert(context.methods[1].signature.symbol_name == "method.Box_UInt32_.reset");
    assert(context.methods[1].signature.return_type == "void");
    assert(context.methods[1].signature.parameter_types.size() == 1);
    assert(context.methods[1].signature.parameter_types[0].empty());
    assert(context.methods[2].receiver_type_name == "UInt32");
    assert(context.methods[2].method_name == "identity");
    assert(context.methods[2].signature.symbol_name == "method.UInt32.identity");
    assert(context.methods[2].signature.return_type == "i32");
    assert(context.methods[2].signature.parameter_types.size() == 1);
    assert(context.methods[2].signature.parameter_types[0] == "i32");
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
    assert(
        orison::lowering::lowered_method_symbol_name("Pair<Header, UInt16>", "read_value") ==
        "method.Pair_Header__UInt16_.read_value"
    );

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
