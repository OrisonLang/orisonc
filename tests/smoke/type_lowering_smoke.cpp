#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/target_layout.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    using orison::lowering::IntegerSignedness;
    using orison::syntax::ParameterSyntax;
    using orison::syntax::TypeSyntax;

    auto int32_type = TypeSyntax {.name = "Int32"};
    assert(orison::lowering::llvm_type_for(int32_type) == "i32");
    assert(orison::lowering::integer_signedness_for(int32_type) == IntegerSignedness::signed_integer);

    auto uint64_type = TypeSyntax {.name = "UInt64"};
    assert(orison::lowering::llvm_type_for(uint64_type) == "i64");
    assert(orison::lowering::integer_signedness_for(uint64_type) == IntegerSignedness::unsigned_integer);

    auto float32_type = TypeSyntax {.name = "Float32"};
    assert(orison::lowering::llvm_type_for(float32_type) == "float");
    assert(orison::lowering::integer_signedness_for(float32_type) == IntegerSignedness::not_integer);

    auto pointer_type = TypeSyntax {
        .name = "Pointer",
        .generic_arguments = {TypeSyntax {.name = "Byte"}},
    };
    assert(orison::lowering::llvm_type_for(pointer_type) == "ptr");

    auto unsupported_generic = TypeSyntax {
        .name = "View",
        .generic_arguments = {TypeSyntax {.name = "Byte"}},
    };
    assert(!orison::lowering::llvm_type_for(unsupported_generic).has_value());

    auto parameters = std::vector<ParameterSyntax> {
        ParameterSyntax {.name = "text", .type = pointer_type},
        ParameterSyntax {.name = "value", .type = int32_type},
    };
    auto parameter_types = orison::lowering::llvm_parameter_types_for(parameters);
    assert(parameter_types.has_value());
    assert(*parameter_types == std::vector<std::string>({"ptr", "i32"}));

    auto parameter_signedness = orison::lowering::parameter_signedness_for(parameters);
    assert(parameter_signedness.size() == 2);
    assert(parameter_signedness[0] == IntegerSignedness::not_integer);
    assert(parameter_signedness[1] == IntegerSignedness::signed_integer);

    auto unit_parameter = std::vector<ParameterSyntax> {
        ParameterSyntax {.name = "invalid", .type = TypeSyntax {.name = "Unit"}},
    };
    assert(!orison::lowering::llvm_parameter_types_for(unit_parameter).has_value());

    auto pointer_size = orison::lowering::lowered_type_size_bytes("ptr");
    assert(pointer_size.has_value());
    assert(*pointer_size == 8);

    auto array_size = orison::lowering::lowered_type_size_bytes("[4 x i16]");
    assert(array_size.has_value());
    assert(*array_size == 8);

    auto padded_struct_size = orison::lowering::lowered_struct_size_bytes({"i8", "i64"});
    assert(padded_struct_size.has_value());
    assert(*padded_struct_size == 16);

    auto view_descriptor_size =
        orison::lowering::lowered_type_size_bytes(orison::lowering::view_descriptor_llvm_type());
    assert(view_descriptor_size.has_value());
    assert(*view_descriptor_size == 16);

    auto dynamic_array_descriptor_size =
        orison::lowering::lowered_type_size_bytes(orison::lowering::dynamic_array_descriptor_llvm_type());
    assert(dynamic_array_descriptor_size.has_value());
    assert(*dynamic_array_descriptor_size == 24);

    auto dynamic_array_allocate = orison::lowering::dynamic_array_runtime_call(
        orison::lowering::DynamicArrayRuntimeOperation::allocate
    );
    assert(dynamic_array_allocate.symbol_name == "__orison_dynamic_array_allocate");
    assert(dynamic_array_allocate.return_type == orison::lowering::dynamic_array_descriptor_llvm_type());
    assert(dynamic_array_allocate.parameter_types == std::vector<std::string_view>({"i64", "i64"}));

    auto dynamic_array_grow = orison::lowering::dynamic_array_runtime_call(
        orison::lowering::DynamicArrayRuntimeOperation::grow
    );
    assert(dynamic_array_grow.symbol_name == "__orison_dynamic_array_grow");
    assert(dynamic_array_grow.return_type == orison::lowering::dynamic_array_descriptor_llvm_type());
    assert(dynamic_array_grow.parameter_types == std::vector<std::string_view>({
        orison::lowering::dynamic_array_descriptor_llvm_type(),
        "i64",
        "i64",
    }));

    auto dynamic_array_deallocate = orison::lowering::dynamic_array_runtime_call(
        orison::lowering::DynamicArrayRuntimeOperation::deallocate
    );
    assert(dynamic_array_deallocate.symbol_name == "__orison_dynamic_array_deallocate");
    assert(dynamic_array_deallocate.return_type == "void");
    assert(dynamic_array_deallocate.parameter_types == std::vector<std::string_view>({"ptr", "i64", "i64"}));

    auto dynamic_array_report = orison::lowering::format_dynamic_array_runtime_request_report({
        orison::lowering::DynamicArrayRuntimeOperation::allocate,
        orison::lowering::DynamicArrayRuntimeOperation::grow,
        orison::lowering::DynamicArrayRuntimeOperation::allocate,
        orison::lowering::DynamicArrayRuntimeOperation::deallocate,
    });
    assert(dynamic_array_report == std::vector<std::string>({
        "dynamic array runtime __orison_dynamic_array_allocate returns { ptr, i64, i64 } params i64 i64",
        "dynamic array runtime __orison_dynamic_array_grow returns { ptr, i64, i64 } params { ptr, i64, i64 } i64 i64",
        "dynamic array runtime __orison_dynamic_array_deallocate returns void params ptr i64 i64",
    }));
    assert(orison::lowering::format_dynamic_array_runtime_request_report({}).empty());

    auto nested_literal_struct_size = orison::lowering::lowered_type_size_bytes("{ i8, { ptr, i64 } }");
    assert(nested_literal_struct_size.has_value());
    assert(*nested_literal_struct_size == 24);

    assert(!orison::lowering::lowered_type_size_bytes("%Unknown").has_value());

    auto context = orison::lowering::LoweringContext {};
    context.records.emplace("Payload", orison::lowering::LoweredRecordLayout {
        .name = "Payload",
        .llvm_type_name = "%record.Payload",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "tag",
                .source_type_name = "UInt8",
                .llvm_type = "i8",
                .index = 0,
            },
            orison::lowering::LoweredRecordField {
                .name = "value",
                .source_type_name = "Int64",
                .llvm_type = "i64",
                .index = 1,
            },
        },
    });
    auto record_size = orison::lowering::lowered_type_size_bytes("%record.Payload", context);
    assert(record_size.has_value());
    assert(*record_size == 16);

    auto dynamic_array_plan = orison::lowering::plan_dynamic_array_construction(
        "DynamicArray<Payload>",
        8,
        context
    );
    assert(dynamic_array_plan.has_value());
    assert(dynamic_array_plan->source_type_name == "DynamicArray<Payload>");
    assert(dynamic_array_plan->element_source_type_name == "Payload");
    assert(dynamic_array_plan->element_llvm_type == "%record.Payload");
    assert(dynamic_array_plan->element_size_bytes == 16);
    assert(dynamic_array_plan->initial_capacity == 8);
    assert(dynamic_array_plan->operation == orison::lowering::DynamicArrayRuntimeOperation::allocate);
    assert(
        orison::lowering::format_dynamic_array_construction_plan(*dynamic_array_plan) ==
        "dynamic array construction DynamicArray<Payload> element Payload lowers to %record.Payload "
        "element_size 16 initial_capacity 8 requests __orison_dynamic_array_allocate (metadata only)"
    );
    auto dynamic_array_cleanup_plan = orison::lowering::plan_dynamic_array_descriptor_cleanup(
        "items",
        "DynamicArray<Payload>",
        context
    );
    assert(dynamic_array_cleanup_plan.has_value());
    assert(dynamic_array_cleanup_plan->owner_name == "items");
    assert(dynamic_array_cleanup_plan->source_type_name == "DynamicArray<Payload>");
    assert(dynamic_array_cleanup_plan->element_source_type_name == "Payload");
    assert(dynamic_array_cleanup_plan->element_llvm_type == "%record.Payload");
    assert(dynamic_array_cleanup_plan->descriptor_storage_name == "%items.addr");
    assert(
        dynamic_array_cleanup_plan->descriptor_storage_status ==
        orison::lowering::DynamicArrayDescriptorStorageStatus::predicted_owner_local
    );
    assert(dynamic_array_cleanup_plan->element_size_bytes == 16);
    assert(
        orison::lowering::format_dynamic_array_descriptor_cleanup_plan(*dynamic_array_cleanup_plan) ==
        "dynamic array descriptor cleanup DynamicArray<Payload> owner items element Payload "
        "lowers to %record.Payload descriptor %items.addr predicted element_size 16 (metadata only)"
    );
    assert(
        orison::lowering::format_dynamic_array_descriptor_cleanup_plan_report({*dynamic_array_cleanup_plan}) ==
        std::vector<std::string>({
            "dynamic array descriptor cleanup DynamicArray<Payload> owner items element Payload "
            "lowers to %record.Payload descriptor %items.addr predicted element_size 16 (metadata only)"
        })
    );
    assert(
        orison::lowering::emit_dynamic_array_descriptor_load("%items.descriptor", "%items.addr") ==
        "  %items.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_descriptor_load_cleanup_sequence(
            *dynamic_array_cleanup_plan,
            "%items.descriptor",
            "%items"
        ) ==
        "  %items.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        "  %items.cleanup.data = extractvalue { ptr, i64, i64 } %items.descriptor, 0\n"
        "  %items.cleanup.length = extractvalue { ptr, i64, i64 } %items.descriptor, 1\n"
        "  %items.cleanup.capacity = extractvalue { ptr, i64, i64 } %items.descriptor, 2\n"
        "  br label %items.drop.walk\n"
        "items.drop.walk:\n"
        "  %items.drop.index = phi i64 [ 0, %items.cleanup.entry ], [ %items.drop.next, %items.drop.body ]\n"
        "  %items.drop.more = icmp ult i64 %items.drop.index, %items.cleanup.length\n"
        "  br i1 %items.drop.more, label %items.drop.body, label %items.drop.done\n"
        "items.drop.body:\n"
        "  %items.drop.element.addr = getelementptr %record.Payload, ptr %items.cleanup.data, i64 %items.drop.index\n"
        "  ; planned drop for Payload at %items.drop.element.addr remains disabled\n"
        "  %items.drop.next = add i64 %items.drop.index, 1\n"
        "  br label %items.drop.walk\n"
        "items.drop.done:\n"
        "  call void @__orison_dynamic_array_deallocate(ptr %items.cleanup.data, i64 16, i64 %items.cleanup.capacity)\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_allocation_call(*dynamic_array_plan, "%array") ==
        "  %array = call { ptr, i64, i64 } @__orison_dynamic_array_allocate(i64 16, i64 8)\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_descriptor_binding(*dynamic_array_plan, "%array.addr", "%array") ==
        "  %array.addr = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %array, ptr %array.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_grow_call(
            *dynamic_array_plan,
            "%array.grown",
            "%array",
            "%array.grow.next.capacity"
        ) ==
        "  %array.grown = call { ptr, i64, i64 } @__orison_dynamic_array_grow"
        "({ ptr, i64, i64 } %array, i64 16, i64 %array.grow.next.capacity)\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_deallocation_call(
            *dynamic_array_plan,
            "%array.data",
            "%array.capacity"
        ) ==
        "  call void @__orison_dynamic_array_deallocate(ptr %array.data, i64 16, i64 %array.capacity)\n"
    );
    assert(
        orison::lowering::dynamic_array_descriptor_field_index(
            orison::lowering::DynamicArrayDescriptorField::data
        ) == 0
    );
    assert(
        orison::lowering::dynamic_array_descriptor_field_llvm_type(
            orison::lowering::DynamicArrayDescriptorField::length
        ) == "i64"
    );
    assert(
        orison::lowering::emit_dynamic_array_descriptor_field_projection(
            "%array.length",
            "%array",
            orison::lowering::DynamicArrayDescriptorField::length
        ) ==
        "  %array.length = extractvalue { ptr, i64, i64 } %array, 1\n"
    );
    assert(
        orison::lowering::dynamic_array_bounds_check_predicate(
            orison::lowering::DynamicArrayBoundsCheckKind::index_within_length
        ) == "ult"
    );
    assert(
        orison::lowering::dynamic_array_bounds_check_predicate(
            orison::lowering::DynamicArrayBoundsCheckKind::length_within_capacity
        ) == "ule"
    );
    assert(
        orison::lowering::emit_dynamic_array_bounds_check(
            "%array.index.in_bounds",
            "%index",
            "%array.length",
            orison::lowering::DynamicArrayBoundsCheckKind::index_within_length
        ) ==
        "  %array.index.in_bounds = icmp ult i64 %index, %array.length\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_element_address(
            *dynamic_array_plan,
            "%array.element.addr",
            "%array.data",
            "%index"
        ) ==
        "  %array.element.addr = getelementptr %record.Payload, ptr %array.data, i64 %index\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_element_load(
            *dynamic_array_plan,
            "%array.element",
            "%array.element.addr"
        ) ==
        "  %array.element = load %record.Payload, ptr %array.element.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_element_store(
            *dynamic_array_plan,
            "%value",
            "%array.element.addr"
        ) ==
        "  store %record.Payload %value, ptr %array.element.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_descriptor_length_update(
            "%array.updated",
            "%array.next.length",
            "%array.descriptor",
            "%array.length"
        ) ==
        "  %array.next.length = add i64 %array.length, 1\n"
        "  %array.updated = insertvalue { ptr, i64, i64 } %array.descriptor, i64 %array.next.length, 1\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_descriptor_write_back(
            "%array.updated",
            "%array.addr"
        ) ==
        "  store { ptr, i64, i64 } %array.updated, ptr %array.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_append_sequence(
            *dynamic_array_plan,
            "%array.descriptor",
            "%array.addr",
            "%array.data",
            "%array.length",
            "%array.capacity",
            "%value",
            "%array"
        ) ==
        "  %array.append.has_capacity = icmp ult i64 %array.length, %array.capacity\n"
        "  %array.append.element.addr = getelementptr %record.Payload, ptr %array.data, i64 %array.length\n"
        "  store %record.Payload %value, ptr %array.append.element.addr\n"
        "  %array.append.next.length = add i64 %array.length, 1\n"
        "  %array.append.updated = insertvalue { ptr, i64, i64 } %array.descriptor, "
        "i64 %array.append.next.length, 1\n"
        "  store { ptr, i64, i64 } %array.append.updated, ptr %array.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_grow_sequence(
            *dynamic_array_plan,
            "%array",
            "%array.addr",
            "%array.capacity",
            "%array"
        ) ==
        "  %array.grow.next.capacity = mul i64 %array.capacity, 2\n"
        "  %array.grown = call { ptr, i64, i64 } @__orison_dynamic_array_grow"
        "({ ptr, i64, i64 } %array, i64 16, i64 %array.grow.next.capacity)\n"
        "  store { ptr, i64, i64 } %array.grown, ptr %array.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_append_with_grow_sequence(
            *dynamic_array_plan,
            "%array",
            "%array.addr",
            "%array.data",
            "%array.length",
            "%array.capacity",
            "%value",
            "%array"
        ) ==
        "array.append.entry:\n"
        "  %array.append.has_capacity = icmp ult i64 %array.length, %array.capacity\n"
        "  br i1 %array.append.has_capacity, label %array.append.ready, label %array.grow\n"
        "array.grow:\n"
        "  %array.grow.next.capacity = mul i64 %array.capacity, 2\n"
        "  %array.grown = call { ptr, i64, i64 } @__orison_dynamic_array_grow"
        "({ ptr, i64, i64 } %array, i64 16, i64 %array.grow.next.capacity)\n"
        "  store { ptr, i64, i64 } %array.grown, ptr %array.addr\n"
        "  br label %array.append.ready\n"
        "array.append.ready:\n"
        "  %array.active = phi { ptr, i64, i64 } [ %array, %array.append.entry ], "
        "[ %array.grown, %array.grow ]\n"
        "  %array.active.data = extractvalue { ptr, i64, i64 } %array.active, 0\n"
        "  %array.active.length = extractvalue { ptr, i64, i64 } %array.active, 1\n"
        "  %array.active.append.element.addr = getelementptr %record.Payload, ptr %array.active.data, "
        "i64 %array.active.length\n"
        "  store %record.Payload %value, ptr %array.active.append.element.addr\n"
        "  %array.active.append.next.length = add i64 %array.active.length, 1\n"
        "  %array.active.append.updated = insertvalue { ptr, i64, i64 } %array.active, "
        "i64 %array.active.append.next.length, 1\n"
        "  store { ptr, i64, i64 } %array.active.append.updated, ptr %array.addr\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_cleanup_sequence(
            *dynamic_array_plan,
            "%array",
            "%array"
        ) ==
        "  %array.cleanup.data = extractvalue { ptr, i64, i64 } %array, 0\n"
        "  %array.cleanup.length = extractvalue { ptr, i64, i64 } %array, 1\n"
        "  %array.cleanup.capacity = extractvalue { ptr, i64, i64 } %array, 2\n"
        "  br label %array.drop.walk\n"
        "array.drop.walk:\n"
        "  %array.drop.index = phi i64 [ 0, %array.cleanup.entry ], [ %array.drop.next, %array.drop.body ]\n"
        "  %array.drop.more = icmp ult i64 %array.drop.index, %array.cleanup.length\n"
        "  br i1 %array.drop.more, label %array.drop.body, label %array.drop.done\n"
        "array.drop.body:\n"
        "  %array.drop.element.addr = getelementptr %record.Payload, ptr %array.cleanup.data, i64 %array.drop.index\n"
        "  ; planned drop for Payload at %array.drop.element.addr remains disabled\n"
        "  %array.drop.next = add i64 %array.drop.index, 1\n"
        "  br label %array.drop.walk\n"
        "array.drop.done:\n"
        "  call void @__orison_dynamic_array_deallocate(ptr %array.cleanup.data, i64 16, i64 %array.cleanup.capacity)\n"
    );
    assert(
        orison::lowering::emit_dynamic_array_element_drop_walk(
            *dynamic_array_plan,
            "%array.data",
            "%array.length",
            "%array"
        ) ==
        "  br label %array.drop.walk\n"
        "array.drop.walk:\n"
        "  %array.drop.index = phi i64 [ 0, %array.cleanup.entry ], [ %array.drop.next, %array.drop.body ]\n"
        "  %array.drop.more = icmp ult i64 %array.drop.index, %array.length\n"
        "  br i1 %array.drop.more, label %array.drop.body, label %array.drop.done\n"
        "array.drop.body:\n"
        "  %array.drop.element.addr = getelementptr %record.Payload, ptr %array.data, i64 %array.drop.index\n"
        "  ; planned drop for Payload at %array.drop.element.addr remains disabled\n"
        "  %array.drop.next = add i64 %array.drop.index, 1\n"
        "  br label %array.drop.walk\n"
        "array.drop.done:\n"
    );
    auto dynamic_array_scalar_plan = orison::lowering::plan_dynamic_array_construction(
        "DynamicArray<UInt32>",
        4,
        context
    );
    assert(dynamic_array_scalar_plan.has_value());
    assert(dynamic_array_scalar_plan->element_llvm_type == "i32");
    assert(dynamic_array_scalar_plan->element_size_bytes == 4);
    assert(dynamic_array_scalar_plan->initial_capacity == 4);
    assert(!orison::lowering::plan_dynamic_array_construction("Array<UInt32, 4>", 4, context).has_value());
    assert(!orison::lowering::plan_dynamic_array_construction("DynamicArray<Unknown>", 4, context).has_value());

    auto record_struct_size = orison::lowering::lowered_struct_size_bytes({"i8", "%record.Payload"}, context);
    assert(record_struct_size.has_value());
    assert(*record_struct_size == 24);

    assert(!orison::lowering::lowered_type_size_bytes("%record.Payload").has_value());
    return 0;
}
