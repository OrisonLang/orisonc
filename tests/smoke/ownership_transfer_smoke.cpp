#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/lowering_context.hpp"

#include <cassert>
#include <utility>
#include <vector>

int main() {
    auto transfers = orison::lowering::OwnershipTransferState {};
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "items"));

    orison::lowering::mark_owned_binding_consumed(transfers, "items");
    assert(orison::lowering::is_owned_binding_consumed(transfers, "items"));
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "other"));

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
    return 0;
}
