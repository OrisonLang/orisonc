#include "orison/lowering/lowering_context.hpp"

#include <cassert>
#include <memory>
#include <utility>

int main() {
    using orison::syntax::ForeignImportBlockSyntax;
    using orison::syntax::ForeignImportFunctionSyntax;
    using orison::syntax::FunctionSyntax;
    using orison::syntax::FieldSyntax;
    using orison::syntax::ImplementationSyntax;
    using orison::syntax::ExtensionSyntax;
    using orison::syntax::ExpressionKind;
    using orison::syntax::ExpressionSyntax;
    using orison::syntax::ModuleSyntax;
    using orison::syntax::ChoiceSyntax;
    using orison::syntax::ChoiceVariantSyntax;
    using orison::syntax::NamedTypeSyntax;
    using orison::syntax::ParameterSyntax;
    using orison::syntax::RecordSyntax;
    using orison::syntax::StatementKind;
    using orison::syntax::StatementSyntax;
    using orison::syntax::TypeSyntax;

    auto module = ModuleSyntax {};
    module.functions.push_back(FunctionSyntax {
        .name = "main",
        .return_type = TypeSyntax {.name = "Int32"},
    });
    module.functions.push_back(FunctionSyntax {
        .name = "make_status",
        .return_type = TypeSyntax {.name = "Status"},
    });
    module.functions.push_back(FunctionSyntax {
        .name = "make_nested_box",
        .return_type = TypeSyntax {
            .name = "Box",
            .generic_arguments = {
                TypeSyntax {
                    .name = "Box",
                    .generic_arguments = {TypeSyntax {.name = "UInt32"}},
                },
            },
        },
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
    module.records.push_back(RecordSyntax {
        .name = "Matrix",
        .fields = {
            FieldSyntax {
                .name = "rows",
                .type = TypeSyntax {
                    .name = "Array",
                    .generic_arguments = {
                        TypeSyntax {
                            .name = "Array",
                            .generic_arguments = {
                                TypeSyntax {.name = "Byte"},
                                TypeSyntax {.name = "4"},
                            },
                        },
                        TypeSyntax {.name = "2"},
                    },
                },
            },
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "Profile",
        .fields = {
            FieldSyntax {.name = "rating", .type = TypeSyntax {.name = "UInt32"}},
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "User",
        .fields = {
            FieldSyntax {
                .name = "profile",
                .type = TypeSyntax {
                    .name = "Maybe",
                    .generic_arguments = {TypeSyntax {.name = "Profile"}},
                },
            },
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "Box",
        .generic_parameters = {"T"},
        .fields = {
            FieldSyntax {.name = "value", .type = TypeSyntax {.name = "T"}},
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "NamedBox",
        .generic_parameters = {"T"},
        .fields = {
            FieldSyntax {.name = "value", .type = TypeSyntax {.name = "T"}},
        },
    });
    module.records.push_back(RecordSyntax {
        .name = "GenericLayoutEvidence",
        .fields = {
            FieldSyntax {.name = "selected", .type = TypeSyntax {.name = "UInt32"}},
            FieldSyntax {
                .name = "values",
                .type = TypeSyntax {
                    .name = "Array",
                    .generic_arguments = {
                        TypeSyntax {.name = "Byte"},
                        TypeSyntax {.name = "2"},
                    },
                },
            },
        },
    });
    auto named_constructor_function = FunctionSyntax {
        .name = "collect_named_constructor",
        .parameters = {
            ParameterSyntax {.name = "input", .type = TypeSyntax {.name = "UInt16"}},
        },
        .return_type = TypeSyntax {.name = "Unit"},
    };
    named_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "typed_value",
        .annotated_type = TypeSyntax {.name = "UInt16"},
        .expression = ExpressionSyntax {.kind = ExpressionKind::name, .text = "input"},
    });
    auto named_constructor_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "NamedBox",
        }),
    };
    named_constructor_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "typed_value",
    });
    named_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::expression_statement,
        .expression = std::move(named_constructor_call),
    });
    module.functions.push_back(std::move(named_constructor_function));
    module.functions.push_back(FunctionSyntax {
        .name = "normalize_index",
        .parameters = {
            ParameterSyntax {.name = "value", .type = TypeSyntax {.name = "UInt32"}},
        },
        .return_type = TypeSyntax {.name = "Int16"},
    });
    module.functions.push_back(FunctionSyntax {
        .name = "normalize_wide_index",
        .parameters = {
            ParameterSyntax {.name = "value", .type = TypeSyntax {.name = "UInt32"}},
        },
        .return_type = TypeSyntax {.name = "Int32"},
    });
    module.functions.push_back(FunctionSyntax {
        .name = "identity",
        .generic_parameters = {"T"},
        .parameters = {
            ParameterSyntax {.name = "value", .type = TypeSyntax {.name = "T"}},
        },
        .return_type = TypeSyntax {.name = "T"},
    });
    auto projected_constructor_function = FunctionSyntax {
        .name = "collect_projected_constructors",
        .parameters = {
            ParameterSyntax {
                .name = "evidence",
                .type = TypeSyntax {.name = "GenericLayoutEvidence"},
            },
            ParameterSyntax {
                .name = "generic_evidence",
                .type = TypeSyntax {
                    .name = "Box",
                    .generic_arguments = {TypeSyntax {.name = "UInt64"}},
                },
            },
            ParameterSyntax {.name = "slot", .type = TypeSyntax {.name = "UInt64"}},
            ParameterSyntax {.name = "flag", .type = TypeSyntax {.name = "Bool"}},
        },
        .return_type = TypeSyntax {.name = "Unit"},
    };
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "member_value",
        .expression = ExpressionSyntax {
            .kind = ExpressionKind::member_access,
            .text = "selected",
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "evidence",
            }),
        },
    });
    auto indexed_value = ExpressionSyntax {
        .kind = ExpressionKind::index_access,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::member_access,
            .text = "values",
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "evidence",
            }),
        }),
    };
    indexed_value.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "slot",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "indexed_value",
        .expression = std::move(indexed_value),
    });
    auto ordinary_return_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "normalize_index",
        }),
    };
    ordinary_return_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "member_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "ordinary_return_value",
        .expression = std::move(ordinary_return_call),
    });
    auto wide_return_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "normalize_wide_index",
        }),
    };
    wide_return_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "member_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "wide_return_value",
        .expression = std::move(wide_return_call),
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "ternary_return_value",
        .expression = ExpressionSyntax {
            .kind = ExpressionKind::ternary,
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "flag",
            }),
            .right = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "ordinary_return_value",
            }),
            .alternate = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "ordinary_return_value",
            }),
        },
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "rejected_ternary_value",
        .expression = ExpressionSyntax {
            .kind = ExpressionKind::ternary,
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "flag",
            }),
            .right = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "ordinary_return_value",
            }),
            .alternate = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "wide_return_value",
            }),
        },
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "method_return_value",
        .expression = ExpressionSyntax {
            .kind = ExpressionKind::call,
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::member_access,
                .text = "identity",
                .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                    .kind = ExpressionKind::name,
                    .text = "member_value",
                }),
            }),
        },
    });
    auto rejected_method_return_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::member_access,
            .text = "wide_identity",
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "member_value",
            }),
        }),
    };
    rejected_method_return_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "indexed_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "rejected_method_return_value",
        .expression = std::move(rejected_method_return_call),
    });
    auto generic_return_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "identity",
        }),
    };
    generic_return_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "indexed_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "generic_return_value",
        .expression = std::move(generic_return_call),
    });
    auto repeated_generic_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "identity",
        }),
    };
    repeated_generic_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "indexed_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::expression_statement,
        .expression = std::move(repeated_generic_call),
    });
    auto rejected_return_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "normalize_wide_index",
        }),
    };
    rejected_return_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "indexed_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "rejected_return_value",
        .expression = std::move(rejected_return_call),
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::let_binding,
        .name = "generic_member_value",
        .expression = ExpressionSyntax {
            .kind = ExpressionKind::call,
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::member_access,
                .text = "value_copy",
                .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                    .kind = ExpressionKind::name,
                    .text = "generic_evidence",
                }),
            }),
        },
    });
    for (auto const* value_name : {
             "ternary_return_value",
             "generic_return_value",
             "generic_member_value",
             "method_return_value",
         }) {
        auto constructor_call = ExpressionSyntax {
            .kind = ExpressionKind::call,
            .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
                .kind = ExpressionKind::name,
                .text = "NamedBox",
            }),
        };
        constructor_call.arguments.push_back(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = value_name,
        });
        projected_constructor_function.body_statements.push_back(StatementSyntax {
            .kind = StatementKind::expression_statement,
            .expression = std::move(constructor_call),
        });
    }
    auto rejected_constructor_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "NamedBox",
        }),
    };
    rejected_constructor_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "rejected_return_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::expression_statement,
        .expression = std::move(rejected_constructor_call),
    });
    auto rejected_ternary_constructor_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "NamedBox",
        }),
    };
    rejected_ternary_constructor_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "rejected_ternary_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::expression_statement,
        .expression = std::move(rejected_ternary_constructor_call),
    });
    auto rejected_method_constructor_call = ExpressionSyntax {
        .kind = ExpressionKind::call,
        .left = std::make_unique<ExpressionSyntax>(ExpressionSyntax {
            .kind = ExpressionKind::name,
            .text = "NamedBox",
        }),
    };
    rejected_method_constructor_call.arguments.push_back(ExpressionSyntax {
        .kind = ExpressionKind::name,
        .text = "rejected_method_return_value",
    });
    projected_constructor_function.body_statements.push_back(StatementSyntax {
        .kind = StatementKind::expression_statement,
        .expression = std::move(rejected_method_constructor_call),
    });
    module.functions.push_back(std::move(projected_constructor_function));
    module.choices.push_back(ChoiceSyntax {
        .name = "Maybe",
        .generic_parameters = {"T"},
        .variants = {
            ChoiceVariantSyntax {
                .name = "Some",
                .payloads = {
                    NamedTypeSyntax {
                        .name = "value",
                        .type = TypeSyntax {.name = "T"},
                    },
                },
            },
            ChoiceVariantSyntax {.name = "Empty"},
        },
    });
    module.choices.push_back(ChoiceSyntax {
        .name = "Expression",
        .variants = {
            ChoiceVariantSyntax {
                .name = "Int",
                .payloads = {
                    NamedTypeSyntax {
                        .name = "value",
                        .type = TypeSyntax {.name = "Int64"},
                    },
                },
            },
            ChoiceVariantSyntax {
                .name = "Add",
                .payloads = {
                    NamedTypeSyntax {
                        .name = "left",
                        .type = TypeSyntax {.name = "UInt32"},
                    },
                    NamedTypeSyntax {
                        .name = "right",
                        .type = TypeSyntax {.name = "Profile"},
                    },
                },
            },
        },
    });
    module.choices.push_back(ChoiceSyntax {
        .name = "Status",
        .variants = {
            ChoiceVariantSyntax {
                .name = "Ready",
                .payloads = {
                    NamedTypeSyntax {
                        .name = "code",
                        .type = TypeSyntax {.name = "UInt32"},
                    },
                },
            },
            ChoiceVariantSyntax {.name = "Empty"},
        },
    });
    module.choices.push_back(ChoiceSyntax {
        .name = "Buffered",
        .variants = {
            ChoiceVariantSyntax {
                .name = "Ready",
                .payloads = {
                    NamedTypeSyntax {
                        .name = "values",
                        .type = TypeSyntax {
                            .name = "DynamicArray",
                            .generic_arguments = {TypeSyntax {.name = "UInt32"}},
                        },
                    },
                },
            },
            ChoiceVariantSyntax {.name = "Empty"},
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
    scalar_extension.methods.push_back(FunctionSyntax {
        .name = "wide_identity",
        .parameters = {
            ParameterSyntax {.name = "this", .type = TypeSyntax {.name = "shared.This"}},
            ParameterSyntax {.name = "value", .type = TypeSyntax {.name = "UInt32"}},
        },
        .return_type = TypeSyntax {.name = "Int32"},
    });
    module.extensions.push_back(std::move(scalar_extension));
    auto generic_box_extension = ExtensionSyntax {
        .receiver_type = TypeSyntax {
            .name = "Box",
            .generic_arguments = {TypeSyntax {.name = "T"}},
        },
    };
    generic_box_extension.methods.push_back(FunctionSyntax {
        .name = "value_copy",
        .parameters = {
            ParameterSyntax {.name = "this", .type = TypeSyntax {.name = "shared.This"}},
        },
        .return_type = TypeSyntax {.name = "T"},
    });
    module.extensions.push_back(std::move(generic_box_extension));
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
    assert(context.functions.size() == 10);
    assert(context.records.size() == 16);
    assert(context.choices.size() == 4);
    assert(context.foreign_declarations.size() == 1);
    assert(context.generic_record_parameters.size() == 2);
    assert(context.generic_record_parameters.contains("Box"));
    assert(context.generic_record_parameters.at("Box").size() == 1);
    assert(context.generic_record_parameters.at("Box")[0] == "T");
    assert(context.generic_record_parameters.contains("NamedBox"));
    assert(context.generic_record_parameters.at("NamedBox").size() == 1);
    assert(context.generic_record_parameters.at("NamedBox")[0] == "T");
    assert(context.generic_function_specializations.size() == 1);
    assert(context.generic_function_specializations.front()->name == "identity__Byte");
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
    assert(context.records.contains("Matrix"));
    auto const& matrix_layout = context.records.at("Matrix");
    assert(matrix_layout.fields.size() == 1);
    assert(matrix_layout.fields[0].name == "rows");
    assert(matrix_layout.fields[0].source_type_name == "Array<Array<Byte, 4>, 2>");
    assert(matrix_layout.fields[0].llvm_type == "[2 x [4 x i8]]");
    assert(matrix_layout.fields[0].index == 0);
    assert(context.records.contains("User"));
    auto const& user_layout = context.records.at("User");
    assert(user_layout.fields.size() == 1);
    assert(user_layout.fields[0].name == "profile");
    assert(user_layout.fields[0].source_type_name == "Maybe<Profile>");
    assert(user_layout.fields[0].llvm_type == "{ i1, %record.Profile }");
    assert(user_layout.fields[0].index == 0);
    assert(!context.records.contains("Box"));
    assert(context.records.contains("Box<UInt32>"));
    auto const& box_uint32_layout = context.records.at("Box<UInt32>");
    assert(box_uint32_layout.llvm_type_name == "%record.Box_UInt32_");
    assert(box_uint32_layout.fields.size() == 1);
    assert(box_uint32_layout.fields[0].name == "value");
    assert(box_uint32_layout.fields[0].source_type_name == "UInt32");
    assert(box_uint32_layout.fields[0].llvm_type == "i32");
    assert(context.records.contains("Box<Box<UInt32>>"));
    auto const& nested_box_layout = context.records.at("Box<Box<UInt32>>");
    assert(nested_box_layout.llvm_type_name == "%record.Box_Box_UInt32__");
    assert(nested_box_layout.fields.size() == 1);
    assert(nested_box_layout.fields[0].name == "value");
    assert(nested_box_layout.fields[0].source_type_name == "Box<UInt32>");
    assert(nested_box_layout.fields[0].llvm_type == "%record.Box_UInt32_");
    assert(!context.records.contains("NamedBox"));
    assert(context.records.contains("NamedBox<UInt16>"));
    auto const& named_box_layout = context.records.at("NamedBox<UInt16>");
    assert(named_box_layout.llvm_type_name == "%record.NamedBox_UInt16_");
    assert(named_box_layout.fields.size() == 1);
    assert(named_box_layout.fields[0].source_type_name == "UInt16");
    assert(named_box_layout.fields[0].llvm_type == "i16");
    assert(context.records.contains("NamedBox<Int16>"));
    auto const& ordinary_return_box_layout = context.records.at("NamedBox<Int16>");
    assert(ordinary_return_box_layout.llvm_type_name == "%record.NamedBox_Int16_");
    assert(ordinary_return_box_layout.fields.size() == 1);
    assert(ordinary_return_box_layout.fields[0].source_type_name == "Int16");
    assert(ordinary_return_box_layout.fields[0].llvm_type == "i16");
    assert(context.records.contains("NamedBox<Byte>"));
    auto const& projected_index_box_layout = context.records.at("NamedBox<Byte>");
    assert(projected_index_box_layout.llvm_type_name == "%record.NamedBox_Byte_");
    assert(projected_index_box_layout.fields.size() == 1);
    assert(projected_index_box_layout.fields[0].source_type_name == "Byte");
    assert(projected_index_box_layout.fields[0].llvm_type == "i8");
    assert(context.records.contains("NamedBox<UInt64>"));
    auto const& projected_generic_member_box_layout = context.records.at("NamedBox<UInt64>");
    assert(projected_generic_member_box_layout.llvm_type_name == "%record.NamedBox_UInt64_");
    assert(projected_generic_member_box_layout.fields.size() == 1);
    assert(projected_generic_member_box_layout.fields[0].source_type_name == "UInt64");
    assert(projected_generic_member_box_layout.fields[0].llvm_type == "i64");
    assert(context.records.contains("NamedBox<UInt32>"));
    auto const& method_return_box_layout = context.records.at("NamedBox<UInt32>");
    assert(method_return_box_layout.llvm_type_name == "%record.NamedBox_UInt32_");
    assert(method_return_box_layout.fields.size() == 1);
    assert(method_return_box_layout.fields[0].source_type_name == "UInt32");
    assert(method_return_box_layout.fields[0].llvm_type == "i32");
    assert(!context.records.contains("NamedBox<Int32>"));
    assert(context.choices.contains("Maybe"));
    auto const& maybe_layout = context.choices.at("Maybe");
    assert(maybe_layout.name == "Maybe");
    assert(maybe_layout.source_type_name == "Maybe<T>");
    assert(maybe_layout.generic_parameters.size() == 1);
    assert(maybe_layout.generic_parameters[0] == "T");
    assert(maybe_layout.variants.size() == 2);
    assert(maybe_layout.variants[0].name == "Some");
    assert(maybe_layout.variants[0].tag == 0);
    assert(maybe_layout.variants[0].payloads.size() == 1);
    assert(maybe_layout.variants[0].payloads[0].name == "value");
    assert(maybe_layout.variants[0].payloads[0].source_type_name == "T");
    assert(maybe_layout.variants[0].payloads[0].llvm_type.empty());
    assert(maybe_layout.variants[0].payloads[0].index == 0);
    assert(maybe_layout.variants[1].name == "Empty");
    assert(maybe_layout.variants[1].tag == 1);
    assert(maybe_layout.variants[1].payloads.empty());
    assert(context.choices.contains("Expression"));
    auto const& expression_layout = context.choices.at("Expression");
    assert(expression_layout.name == "Expression");
    assert(expression_layout.source_type_name == "Expression");
    assert(expression_layout.llvm_type_name == "{ i32, [8 x i8] }");
    assert(expression_layout.generic_parameters.empty());
    assert(expression_layout.variants.size() == 2);
    assert(expression_layout.variants[0].name == "Int");
    assert(expression_layout.variants[0].lowered_payload_type == "i64");
    assert(expression_layout.variants[0].tag == 0);
    assert(expression_layout.variants[0].payloads.size() == 1);
    assert(expression_layout.variants[0].payloads[0].name == "value");
    assert(expression_layout.variants[0].payloads[0].source_type_name == "Int64");
    assert(expression_layout.variants[0].payloads[0].llvm_type == "i64");
    assert(expression_layout.variants[0].payloads[0].index == 0);
    assert(expression_layout.variants[1].name == "Add");
    assert(expression_layout.variants[1].lowered_payload_type == "{ i32, %record.Profile }");
    assert(expression_layout.variants[1].tag == 1);
    assert(expression_layout.variants[1].payloads.size() == 2);
    assert(expression_layout.variants[1].payloads[0].name == "left");
    assert(expression_layout.variants[1].payloads[0].source_type_name == "UInt32");
    assert(expression_layout.variants[1].payloads[0].llvm_type == "i32");
    assert(expression_layout.variants[1].payloads[0].index == 0);
    assert(expression_layout.variants[1].payloads[1].name == "right");
    assert(expression_layout.variants[1].payloads[1].source_type_name == "Profile");
    assert(expression_layout.variants[1].payloads[1].llvm_type == "%record.Profile");
    assert(expression_layout.variants[1].payloads[1].index == 1);
    assert(context.choices.contains("Status"));
    auto const& status_layout = context.choices.at("Status");
    assert(status_layout.name == "Status");
    assert(status_layout.source_type_name == "Status");
    assert(status_layout.llvm_type_name == "{ i32, i32 }");
    assert(status_layout.variants.size() == 2);
    assert(status_layout.variants[0].name == "Ready");
    assert(status_layout.variants[0].tag == 0);
    assert(status_layout.variants[0].payloads.size() == 1);
    assert(status_layout.variants[0].payloads[0].name == "code");
    assert(status_layout.variants[0].payloads[0].source_type_name == "UInt32");
    assert(status_layout.variants[0].payloads[0].llvm_type == "i32");
    assert(status_layout.variants[1].name == "Empty");
    assert(status_layout.variants[1].tag == 1);
    assert(status_layout.variants[1].payloads.empty());
    assert(context.choices.contains("Buffered"));
    auto const& buffered_layout = context.choices.at("Buffered");
    assert(buffered_layout.name == "Buffered");
    assert(buffered_layout.source_type_name == "Buffered");
    assert(buffered_layout.llvm_type_name == "{ i32, { ptr, i64, i64 } }");
    assert(buffered_layout.unsupported_abi_reason.empty());
    assert(buffered_layout.variants.size() == 2);
    assert(buffered_layout.variants[0].name == "Ready");
    assert(buffered_layout.variants[0].lowered_payload_type == "{ ptr, i64, i64 }");
    assert(buffered_layout.variants[0].payloads.size() == 1);
    assert(buffered_layout.variants[0].payloads[0].name == "values");
    assert(buffered_layout.variants[0].payloads[0].source_type_name == "DynamicArray<UInt32>");
    assert(buffered_layout.variants[0].payloads[0].llvm_type == "{ ptr, i64, i64 }");
    assert(context.functions.at("make_status").return_type == "{ i32, i32 }");
    assert(context.functions.at("make_status").return_signedness == orison::lowering::IntegerSignedness::not_integer);
    assert(context.methods[0].receiver_type_name == "Device");
    assert(context.methods[0].method_name == "read");
    assert(context.methods[0].signature.symbol_name == "method.Device.read");
    assert(context.methods[0].signature.return_type == "i32");
    assert(context.methods[0].signature.parameter_types.size() == 2);
    assert(context.methods[0].signature.parameter_types[0] == "%record.Device");
    assert(context.methods[0].signature.parameter_types[1] == "i32");
    assert(context.methods[1].receiver_type_name == "Box<UInt32>");
    assert(context.methods[1].method_name == "reset");
    assert(context.methods[1].signature.symbol_name == "method.Box_UInt32_.reset");
    assert(context.methods[1].signature.return_type == "void");
    assert(context.methods[1].signature.parameter_types.size() == 1);
    assert(context.methods[1].signature.parameter_types[0] == "%record.Box_UInt32_");
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
