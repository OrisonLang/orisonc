#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/ownership_transfer.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

int main() {
    using orison::lowering::CAbiAdapterKind;
    using orison::lowering::IntegerSignedness;
    using orison::lowering::LoweredExpression;
    using orison::lowering::LoweredFunctionSignature;

    auto add = LoweredFunctionSignature {
        .return_type = "i32",
        .return_signedness = IntegerSignedness::unsigned_integer,
        .parameter_types = {"i32", "i32"},
        .parameter_signedness = {
            IntegerSignedness::unsigned_integer,
            IntegerSignedness::unsigned_integer,
        },
        .symbol_name = "add",
    };
    auto add_arguments = std::vector<LoweredExpression> {
        LoweredExpression {
            .type = "i32",
            .value = "40",
            .signedness = IntegerSignedness::unsigned_integer,
        },
        LoweredExpression {
            .type = "i32",
            .value = "2",
            .signedness = IntegerSignedness::unsigned_integer,
        },
    };

    auto output = std::ostringstream {};
    auto result = orison::lowering::emit_value_call("%tmp0", add, add_arguments, output);
    assert(output.str() == "  %tmp0 = call i32 @add(i32 40, i32 2)\n");
    assert(result.type == "i32");
    assert(result.value == "%tmp0");
    assert(result.signedness == IntegerSignedness::unsigned_integer);

    auto observe = LoweredFunctionSignature {
        .return_type = "void",
        .parameter_types = {"i32"},
        .parameter_signedness = {IntegerSignedness::signed_integer},
        .symbol_name = "observe",
    };
    auto observe_arguments = std::vector<LoweredExpression> {
        LoweredExpression {
            .type = "i32",
            .value = "%value",
            .signedness = IntegerSignedness::signed_integer,
        },
    };
    auto void_output = std::ostringstream {};
    orison::lowering::emit_void_call(observe, observe_arguments, void_output);
    assert(void_output.str() == "  call void @observe(i32 %value)\n");

    auto printf = LoweredFunctionSignature {
        .return_type = "i32",
        .return_signedness = IntegerSignedness::signed_integer,
        .parameter_types = {"ptr", "i32"},
        .parameter_signedness = {
            IntegerSignedness::not_integer,
            IntegerSignedness::signed_integer,
        },
        .symbol_name = "printf",
        .adapter = CAbiAdapterKind::variadic,
        .fixed_abi_parameter_count = 1,
    };
    auto printf_arguments = std::vector<LoweredExpression> {
        LoweredExpression {
            .type = "ptr",
            .value = "@.str.0",
            .signedness = IntegerSignedness::not_integer,
        },
        LoweredExpression {
            .type = "i32",
            .value = "7",
            .signedness = IntegerSignedness::signed_integer,
        },
    };
    auto printf_output = std::ostringstream {};
    orison::lowering::emit_value_call("%tmp1", printf, printf_arguments, printf_output);
    assert(printf_output.str() == "  %tmp1 = call i32 (ptr, ...) @printf(ptr @.str.0, i32 7)\n");

    auto scale = LoweredFunctionSignature {
        .return_type = "i32",
        .return_signedness = IntegerSignedness::unsigned_integer,
        .parameter_types = {"i32", "i32"},
        .parameter_signedness = {
            IntegerSignedness::unsigned_integer,
            IntegerSignedness::unsigned_integer,
        },
        .symbol_name = "method.UInt32.scale",
    };
    auto receiver = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "value",
    };
    auto scale_arguments = std::vector<orison::syntax::ExpressionSyntax> {};
    scale_arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::integer_literal,
        .text = "2",
    });
    auto member_state = orison::lowering::FunctionLoweringState {};
    member_state.immutable_bindings.emplace("value", LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = IntegerSignedness::unsigned_integer,
    });
    member_state.source_type_names.emplace("value", "UInt32");
    auto member_lowering = orison::lowering::LoweringContext {};
    auto member_string_constants = orison::lowering::StringConstantTable {};
    auto member_context = orison::lowering::LoweringEmissionContext {
        .lowering = member_lowering,
        .string_constants = member_string_constants,
        .options = {},
    };
    auto member_failures = orison::lowering::LoweringFailures {};
    auto member_session = orison::lowering::FunctionLoweringSession {
        .state = member_state,
        .failures = member_failures,
    };
    auto member_output = std::ostringstream {};
    auto member_arguments = orison::lowering::lower_member_call_arguments(
        receiver,
        std::span<orison::syntax::ExpressionSyntax const>(scale_arguments),
        scale,
        member_context,
        member_session,
        member_output
    );
    assert(member_arguments.has_value());
    assert(member_arguments->size() == 2);
    assert((*member_arguments)[0].value == "%value");
    assert((*member_arguments)[1].value == "2");
    auto member_result = orison::lowering::emit_value_call("%tmp2", scale, *member_arguments, member_output);
    assert(member_result.type == "i32");
    assert(member_output.str() == "  %tmp2 = call i32 @method.UInt32.scale(i32 %value, i32 2)\n");

    auto transfer_context = orison::lowering::LoweringContext {};
    transfer_context.records.emplace(
        "Payload",
        orison::lowering::LoweredRecordLayout {
            .name = "Payload",
            .llvm_type_name = "%record.Payload",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "items",
                    .source_type_name = "DynamicArray<UInt32>",
                    .llvm_type = "{ ptr, i64, i64 }",
                    .index = 0,
                },
            },
        }
    );
    transfer_context.records.emplace(
        "Box",
        orison::lowering::LoweredRecordLayout {
            .name = "Box",
            .llvm_type_name = "%record.Box",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "payload",
                    .source_type_name = "Payload",
                    .llvm_type = "%record.Payload",
                    .index = 0,
                },
                orison::lowering::LoweredRecordField {
                    .name = "count",
                    .source_type_name = "UInt32",
                    .llvm_type = "i32",
                    .index = 1,
                },
            },
        }
    );
    transfer_context.records.emplace(
        "NestedBox",
        orison::lowering::LoweredRecordLayout {
            .name = "NestedBox",
            .llvm_type_name = "%record.NestedBox",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "inner",
                    .source_type_name = "Box",
                    .llvm_type = "%record.Box",
                    .index = 0,
                },
            },
        }
    );
    auto transfer_emission_context = orison::lowering::LoweringEmissionContext {
        .lowering = transfer_context,
        .string_constants = member_string_constants,
        .options = {},
    };
    auto consume_payload = LoweredFunctionSignature {
        .return_type = "void",
        .parameter_types = {"%record.Payload"},
        .parameter_source_type_names = {"Payload"},
        .parameter_signedness = {IntegerSignedness::not_integer},
        .symbol_name = "consume_payload",
    };
    auto transfer_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "consume_payload",
            }
        ),
    };
    transfer_call.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::member_access,
        .text = "payload",
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "box",
            }
        ),
    });
    auto transfer_state = orison::lowering::FunctionLoweringState {};
    transfer_state.addressable_bindings.emplace("box", orison::lowering::AddressableBinding {
        .type = orison::lowering::LoweredType {
            .type = "%record.Box",
            .signedness = IntegerSignedness::not_integer,
        },
        .storage = "%box.addr",
    });
    transfer_state.source_type_names.emplace("box", "Box");
    auto transfer_failures = orison::lowering::LoweringFailures {};
    auto transfer_session = orison::lowering::FunctionLoweringSession {
        .state = transfer_state,
        .failures = transfer_failures,
    };
    auto transfer_output = std::ostringstream {};
    auto transfer_arguments = orison::lowering::lower_call_arguments(
        transfer_call,
        consume_payload,
        transfer_emission_context,
        transfer_session,
        transfer_output
    );
    assert(transfer_arguments.has_value());
    assert(transfer_arguments->size() == 1);
    assert((*transfer_arguments)[0].type == "%record.Payload");
    assert((*transfer_arguments)[0].value == "%tmp1");
    assert(orison::lowering::is_owned_binding_consumed(
        transfer_state.ownership_transfers,
        "box.payload"
    ));
    assert(
        transfer_output.str() ==
        "  %tmp0 = getelementptr %record.Box, ptr %box.addr, i32 0, i32 0\n"
        "  %tmp1 = load %record.Payload, ptr %tmp0\n"
    );

    auto nested_transfer_call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "consume_payload",
            }
        ),
    };
    auto nested_payload_access = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::member_access,
        .text = "payload",
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::member_access,
                .text = "inner",
                .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                    orison::syntax::ExpressionSyntax {
                        .kind = orison::syntax::ExpressionKind::name,
                        .text = "nested",
                    }
                ),
            }
        ),
    };
    nested_transfer_call.arguments.push_back(std::move(nested_payload_access));
    auto nested_transfer_state = orison::lowering::FunctionLoweringState {};
    nested_transfer_state.addressable_bindings.emplace("nested", orison::lowering::AddressableBinding {
        .type = orison::lowering::LoweredType {
            .type = "%record.NestedBox",
            .signedness = IntegerSignedness::not_integer,
        },
        .storage = "%nested.addr",
    });
    nested_transfer_state.source_type_names.emplace("nested", "NestedBox");
    auto nested_transfer_failures = orison::lowering::LoweringFailures {};
    auto nested_transfer_session = orison::lowering::FunctionLoweringSession {
        .state = nested_transfer_state,
        .failures = nested_transfer_failures,
    };
    transfer_output = {};
    auto nested_transfer_arguments = orison::lowering::lower_call_arguments(
        nested_transfer_call,
        consume_payload,
        transfer_emission_context,
        nested_transfer_session,
        transfer_output
    );
    assert(nested_transfer_arguments.has_value());
    assert(nested_transfer_arguments->size() == 1);
    assert((*nested_transfer_arguments)[0].type == "%record.Payload");
    assert((*nested_transfer_arguments)[0].value == "%tmp2");
    assert(orison::lowering::is_owned_binding_consumed(
        nested_transfer_state.ownership_transfers,
        "nested.inner.payload"
    ));
    assert(
        transfer_output.str() ==
        "  %tmp0 = getelementptr %record.NestedBox, ptr %nested.addr, i32 0, i32 0\n"
        "  %tmp1 = getelementptr %record.Box, ptr %tmp0, i32 0, i32 0\n"
        "  %tmp2 = load %record.Payload, ptr %tmp1\n"
    );

    auto make_nested_member_call = [](
        std::vector<std::string_view> member_names
    ) {
        assert(!member_names.empty());
        auto expression = orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::name,
            .text = "nested",
        };
        for (auto member_name : member_names) {
            expression = orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::member_access,
                .text = std::string {member_name},
                .left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(expression)),
            };
        }

        auto call = orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::call,
            .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                orison::syntax::ExpressionSyntax {
                    .kind = orison::syntax::ExpressionKind::name,
                    .text = "consume",
                }
            ),
        };
        call.arguments.push_back(std::move(expression));
        return call;
    };
    auto seed_nested_call_state = [] {
        auto state = orison::lowering::FunctionLoweringState {};
        state.addressable_bindings.emplace("nested", orison::lowering::AddressableBinding {
            .type = orison::lowering::LoweredType {
                .type = "%record.NestedBox",
                .signedness = IntegerSignedness::not_integer,
            },
            .storage = "%nested.addr",
        });
        state.source_type_names.emplace("nested", "NestedBox");
        return state;
    };
    auto consume_scalar = LoweredFunctionSignature {
        .return_type = "void",
        .parameter_types = {"i32"},
        .parameter_source_type_names = {"UInt32"},
        .parameter_signedness = {IntegerSignedness::unsigned_integer},
        .symbol_name = "consume_scalar",
    };

    auto scalar_terminal_state = seed_nested_call_state();
    auto scalar_terminal_failures = orison::lowering::LoweringFailures {};
    auto scalar_terminal_session = orison::lowering::FunctionLoweringSession {
        .state = scalar_terminal_state,
        .failures = scalar_terminal_failures,
    };
    auto scalar_terminal_output = std::ostringstream {};
    auto scalar_terminal_call = make_nested_member_call({"inner", "count"});
    auto scalar_terminal_arguments = orison::lowering::lower_call_arguments(
        scalar_terminal_call,
        consume_scalar,
        transfer_emission_context,
        scalar_terminal_session,
        scalar_terminal_output
    );
    assert(scalar_terminal_arguments.has_value());
    assert(scalar_terminal_arguments->size() == 1);
    assert((*scalar_terminal_arguments)[0].type == "i32");
    assert(!orison::lowering::is_owned_binding_consumed(
        scalar_terminal_state.ownership_transfers,
        "nested.inner.count"
    ));
    assert(scalar_terminal_state.ownership_transfers.consumed_owned_bindings.empty());

    auto missing_outer_state = seed_nested_call_state();
    auto missing_outer_failures = orison::lowering::LoweringFailures {};
    auto missing_outer_session = orison::lowering::FunctionLoweringSession {
        .state = missing_outer_state,
        .failures = missing_outer_failures,
    };
    auto missing_outer_output = std::ostringstream {};
    auto missing_outer_call = make_nested_member_call({"missing", "payload"});
    auto missing_outer_arguments = orison::lowering::lower_call_arguments(
        missing_outer_call,
        consume_payload,
        transfer_emission_context,
        missing_outer_session,
        missing_outer_output
    );
    assert(!missing_outer_arguments.has_value());
    assert(missing_outer_state.ownership_transfers.consumed_owned_bindings.empty());

    auto missing_inner_state = seed_nested_call_state();
    auto missing_inner_failures = orison::lowering::LoweringFailures {};
    auto missing_inner_session = orison::lowering::FunctionLoweringSession {
        .state = missing_inner_state,
        .failures = missing_inner_failures,
    };
    auto missing_inner_output = std::ostringstream {};
    auto missing_inner_call = make_nested_member_call({"inner", "missing"});
    auto missing_inner_arguments = orison::lowering::lower_call_arguments(
        missing_inner_call,
        consume_payload,
        transfer_emission_context,
        missing_inner_session,
        missing_inner_output
    );
    assert(!missing_inner_arguments.has_value());
    assert(missing_inner_state.ownership_transfers.consumed_owned_bindings.empty());

    auto cross_scalar_state = seed_nested_call_state();
    auto cross_scalar_failures = orison::lowering::LoweringFailures {};
    auto cross_scalar_session = orison::lowering::FunctionLoweringSession {
        .state = cross_scalar_state,
        .failures = cross_scalar_failures,
    };
    auto cross_scalar_output = std::ostringstream {};
    auto cross_scalar_call = make_nested_member_call({"inner", "count", "payload"});
    auto cross_scalar_arguments = orison::lowering::lower_call_arguments(
        cross_scalar_call,
        consume_payload,
        transfer_emission_context,
        cross_scalar_session,
        cross_scalar_output
    );
    assert(!cross_scalar_arguments.has_value());
    assert(cross_scalar_state.ownership_transfers.consumed_owned_bindings.empty());
    return 0;
}
