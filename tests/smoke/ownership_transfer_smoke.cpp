#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/maybe_switch_lowering.hpp"
#include "orison/lowering/string_constants.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

int main() {
    auto transfers = orison::lowering::OwnershipTransferState {};
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "items"));

    orison::lowering::mark_owned_binding_consumed(transfers, "items");
    assert(orison::lowering::is_owned_binding_consumed(transfers, "items"));
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "other"));
    assert(orison::lowering::consumed_owned_binding_or_descendant_name(transfers, "items") == "items");
    assert(!orison::lowering::consumed_owned_binding_or_descendant_name(transfers, "other").has_value());

    orison::lowering::mark_owned_binding_consumed(transfers, "maybe.Some.value");
    assert(
        orison::lowering::consumed_owned_binding_or_descendant_name(transfers, "maybe") ==
        "maybe.Some.value"
    );

    auto matching = orison::lowering::merge_ownership_transfer_states({
        transfers,
        transfers,
    });
    assert(matching.has_value());
    assert(orison::lowering::is_owned_binding_consumed(*matching, "items"));

    auto different = orison::lowering::OwnershipTransferState {};
    orison::lowering::mark_owned_binding_consumed(different, "other");
    auto mismatched = orison::lowering::merge_ownership_transfer_states({
        std::move(transfers),
        std::move(different),
    });
    assert(!mismatched.has_value());

    auto empty = orison::lowering::merge_ownership_transfer_states({});
    assert(empty.has_value());
    assert(!orison::lowering::is_owned_binding_consumed(*empty, "items"));

    auto context = orison::lowering::LoweringContext {};
    context.records.emplace(
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
    context.records.emplace(
        "Box",
        orison::lowering::LoweredRecordLayout {
            .name = "Box",
            .llvm_type_name = "%record.Box",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "count",
                    .source_type_name = "UInt32",
                    .llvm_type = "i32",
                    .index = 0,
                },
                orison::lowering::LoweredRecordField {
                    .name = "payload",
                    .source_type_name = "Payload",
                    .llvm_type = "%record.Payload",
                    .index = 1,
                },
            },
        }
    );
    context.records.emplace(
        "NestedBox",
        orison::lowering::LoweredRecordLayout {
            .name = "NestedBox",
            .llvm_type_name = "%record.NestedBox",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "box",
                    .source_type_name = "Box",
                    .llvm_type = "%record.Box",
                    .index = 0,
                },
            },
        }
    );
    context.choices.emplace(
        "MaybePayload",
        orison::lowering::LoweredChoiceLayout {
            .name = "MaybePayload",
            .source_type_name = "MaybePayload",
            .llvm_type_name = "{ i32, %record.Payload }",
            .variants = {
                orison::lowering::LoweredChoiceVariant {
                    .name = "Some",
                    .tag = 0,
                    .payloads = {
                        orison::lowering::LoweredChoicePayload {
                            .name = "value",
                            .source_type_name = "Payload",
                            .llvm_type = "%record.Payload",
                            .index = 0,
                        },
                    },
                },
                orison::lowering::LoweredChoiceVariant {
                    .name = "Empty",
                    .tag = 1,
                },
            },
        }
    );

    assert(!orison::lowering::is_owned_transfer_source_type("UInt32", context));
    assert(!orison::lowering::is_owned_transfer_source_type("Array<UInt32, 4>", context));
    assert(!orison::lowering::is_owned_transfer_source_type("Maybe<UInt32>", context));
    assert(orison::lowering::is_owned_transfer_source_type("DynamicArray<UInt32>", context));
    assert(orison::lowering::is_owned_transfer_source_type("Payload", context));
    assert(orison::lowering::is_owned_transfer_source_type("Box", context));

    auto scalar_field = orison::lowering::owned_record_field_transfer("box", "Box", "count", context);
    assert(!scalar_field.has_value());

    auto owned_field = orison::lowering::owned_record_field_transfer("box", "Box", "payload", context);
    assert(owned_field.has_value());
    assert(owned_field->binding_name == "box.payload");
    assert(owned_field->source_type_name == "Payload");

    auto nested_field_names = std::vector<std::string> {"box", "payload"};
    auto nested_field = orison::lowering::owned_record_member_path_transfer(
        "nested",
        "NestedBox",
        nested_field_names,
        context
    );
    assert(nested_field.has_value());
    assert(nested_field->binding_name == "nested.box.payload");
    assert(nested_field->source_type_name == "Payload");

    auto owned_payload = orison::lowering::owned_choice_payload_transfer(
        "maybe",
        "MaybePayload",
        "Some",
        "value",
        context
    );
    assert(owned_payload.has_value());
    assert(owned_payload->binding_name == "maybe.Some.value");
    assert(owned_payload->source_type_name == "Payload");

    auto payload_name = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "payload",
    };
    auto some_pattern = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "Some",
            }
        ),
    };
    some_pattern.arguments.push_back(std::move(payload_name));
    auto switch_case = orison::syntax::SwitchCaseSyntax {
        .pattern = std::move(some_pattern),
    };
    auto planned_case = orison::lowering::LoweredSwitchCasePlan {
        .syntax = &switch_case,
        .block = "switch.case.0",
    };
    auto subject_expression = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "maybe",
    };
    auto switch_state = orison::lowering::FunctionLoweringState {};
    switch_state.source_type_names.emplace("maybe", "MaybePayload");
    auto switch_failures = orison::lowering::LoweringFailures {};
    auto switch_session = orison::lowering::FunctionLoweringSession {
        .state = switch_state,
        .failures = switch_failures,
    };
    auto strings = orison::lowering::StringConstantTable {};
    auto emission_context = orison::lowering::LoweringEmissionContext {
        .lowering = context,
        .string_constants = strings,
        .options = {},
    };
    auto output = std::ostringstream {};
    orison::lowering::bind_switch_payload(
        planned_case,
        subject_expression,
        orison::lowering::LoweredExpression {
            .type = "{ i32, %record.Payload }",
            .value = "%maybe",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        emission_context,
        switch_session,
        output,
        std::string_view {"MaybePayload"}
    );
    assert(orison::lowering::is_owned_binding_consumed(
        switch_state.ownership_transfers,
        "maybe.Some.value"
    ));
    assert(switch_state.source_type_names.at("payload") == "Payload");
    assert(switch_state.immutable_bindings.at("payload").type == "%record.Payload");
    assert(
        output.str() ==
        "  %tmp0 = extractvalue { i32, %record.Payload } %maybe, 1\n"
        "  %payload.addr = alloca %record.Payload\n"
        "  store %record.Payload %tmp0, ptr %payload.addr\n"
    );
    return 0;
}
