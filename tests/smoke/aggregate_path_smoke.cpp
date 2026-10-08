#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/for_loop_lowering.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace {

auto name(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::name;
    expression.text = std::move(text);
    return expression;
}

auto member(orison::syntax::ExpressionSyntax left, std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::member_access;
    expression.text = std::move(text);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    return expression;
}

auto index(orison::syntax::ExpressionSyntax left, std::string index_text = "0")
    -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::index_access;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    auto index = orison::syntax::ExpressionSyntax {};
    index.kind = orison::syntax::ExpressionKind::integer_literal;
    index.text = std::move(index_text);
    expression.arguments.push_back(std::move(index));
    return expression;
}

auto index_name(orison::syntax::ExpressionSyntax left, std::string index_name)
    -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::index_access;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    auto index = orison::syntax::ExpressionSyntax {};
    index.kind = orison::syntax::ExpressionKind::name;
    index.text = std::move(index_name);
    expression.arguments.push_back(std::move(index));
    return expression;
}

auto call(std::string callee) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    auto name = orison::syntax::ExpressionSyntax {};
    name.kind = orison::syntax::ExpressionKind::name;
    name.text = std::move(callee);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(name));
    return expression;
}

auto context() -> orison::lowering::LoweringContext {
    auto lowering = orison::lowering::LoweringContext {};
    lowering.records.emplace("Payload", orison::lowering::LoweredRecordLayout {
        .name = "Payload",
        .llvm_type_name = "%record.Payload",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "value",
                .source_type_name = "UInt32",
                .llvm_type = "i32",
                .index = 0,
            },
        },
    });
    lowering.records.emplace("Box", orison::lowering::LoweredRecordLayout {
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
    });
    lowering.records.emplace("Nested", orison::lowering::LoweredRecordLayout {
        .name = "Nested",
        .llvm_type_name = "%record.Nested",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "box",
                .source_type_name = "Box",
                .llvm_type = "%record.Box",
                .index = 0,
            },
        },
    });
    lowering.records.emplace("Bucket", orison::lowering::LoweredRecordLayout {
        .name = "Bucket",
        .llvm_type_name = "%record.Bucket",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "values",
                .source_type_name = "Array<UInt32, 3>",
                .llvm_type = "[3 x i32]",
                .index = 0,
            },
        },
    });
    lowering.records.emplace("Shelf", orison::lowering::LoweredRecordLayout {
        .name = "Shelf",
        .llvm_type_name = "%record.Shelf",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "buckets",
                .source_type_name = "Array<Bucket, 2>",
                .llvm_type = "[2 x %record.Bucket]",
                .index = 0,
            },
        },
    });
    lowering.records.emplace("RuntimeBucket", orison::lowering::LoweredRecordLayout {
        .name = "RuntimeBucket",
        .llvm_type_name = "%record.RuntimeBucket",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "values",
                .source_type_name = "DynamicArray<Payload>",
                .llvm_type = "{ ptr, i64, i64 }",
                .index = 0,
            },
        },
    });
    lowering.records.emplace("Holder", orison::lowering::LoweredRecordLayout {
        .name = "Holder",
        .llvm_type_name = "%record.Holder",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "grid",
                .source_type_name = "Array<Array<RuntimeBucket, 2>, 2>",
                .llvm_type = "[2 x [2 x %record.RuntimeBucket]]",
                .index = 0,
            },
            orison::lowering::LoweredRecordField {
                .name = "maybe_values",
                .source_type_name = "Maybe<DynamicArray<Payload>>",
                .llvm_type = "{ i1, { ptr, i64, i64 } }",
                .index = 1,
            },
            orison::lowering::LoweredRecordField {
                .name = "packet",
                .source_type_name = "Packet",
                .llvm_type = "{ i32, { ptr, i64, i64 } }",
                .index = 2,
            },
        },
    });
    lowering.choices.emplace("Packet", orison::lowering::LoweredChoiceLayout {
        .name = "Packet",
        .source_type_name = "Packet",
        .llvm_type_name = "{ i32, { ptr, i64, i64 } }",
        .variants = {
            orison::lowering::LoweredChoiceVariant {
                .name = "Primary",
                .lowered_payload_type = "{ ptr, i64, i64 }",
                .tag = 0,
                .payloads = {
                    orison::lowering::LoweredChoicePayload {
                        .name = "values",
                        .source_type_name = "DynamicArray<Payload>",
                        .llvm_type = "{ ptr, i64, i64 }",
                        .index = 0,
                    },
                },
            },
        },
    });
    return lowering;
}

}  // namespace

int main() {
    auto lowering = context();

    auto target = member(index(member(name("shelf"), "buckets"), "1"), "values");
    auto path = orison::lowering::collect_aggregate_path(target);
    assert(path.base_expression != nullptr);
    assert(path.base_expression->kind == orison::syntax::ExpressionKind::name);
    assert(path.base_expression->text == "shelf");
    assert(path.steps.size() == 3);
    assert(path.steps[0].kind == orison::lowering::AggregatePathStepKind::member);
    assert(path.steps[0].field_name == "buckets");
    assert(path.steps[1].kind == orison::lowering::AggregatePathStepKind::index);
    assert(path.steps[1].index_expression != nullptr);
    assert(path.steps[1].index_expression->text == "1");
    assert(path.steps[2].kind == orison::lowering::AggregatePathStepKind::member);
    assert(path.steps[2].field_name == "values");

    auto named_path = orison::lowering::collect_named_aggregate_path(target);
    assert(named_path.has_value());
    assert(named_path->base_expression != nullptr);
    assert(named_path->base_expression->text == "shelf");
    assert(named_path->steps.size() == 3);
    assert(!orison::lowering::collect_temporary_aggregate_path(target).has_value());

    auto temporary_target = member(call("make_shelf"), "buckets");
    auto temporary_path = orison::lowering::collect_temporary_aggregate_path(temporary_target);
    assert(temporary_path.has_value());
    assert(temporary_path->base_expression != nullptr);
    assert(temporary_path->base_expression->kind == orison::syntax::ExpressionKind::call);
    assert(temporary_path->steps.size() == 1);
    assert(!orison::lowering::collect_named_aggregate_path(temporary_target).has_value());
    assert(!orison::lowering::collect_named_aggregate_path(name("shelf")).has_value());
    assert(!orison::lowering::collect_temporary_aggregate_path(name("shelf")).has_value());

    auto access_state = orison::lowering::FunctionLoweringState {};
    access_state.source_type_names.emplace("box", "Box");
    access_state.source_type_names.emplace("nested", "Nested");
    access_state.source_type_names.emplace("this", "Box");

    auto owned_projection = member(name("box"), "payload");
    auto value_read_plan = orison::lowering::describe_named_aggregate_projection_access(
        owned_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::value_read
    );
    assert(
        value_read_plan.status ==
        orison::lowering::AggregateProjectionAccessStatus::requires_explicit_boundary
    );
    assert(value_read_plan.binding_name == "box.payload");
    assert(value_read_plan.source_type_name == "Payload");
    assert(!value_read_plan.receiver_projection);
    assert(
        orison::lowering::aggregate_projection_access_diagnostic(value_read_plan) ==
        "aggregate path read of owned projection requires an explicit ownership transfer"
    );
    assert(
        orison::lowering::aggregate_projection_access_plan_report(value_read_plan) ==
        "aggregate projection access intent value_read status requires_explicit_boundary "
        "binding box.payload source Payload receiver false diagnostic "
        "aggregate path read of owned projection requires an explicit ownership transfer"
    );

    auto transfer_plan = orison::lowering::describe_named_aggregate_projection_access(
        owned_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::explicit_transfer
    );
    assert(transfer_plan.status == orison::lowering::AggregateProjectionAccessStatus::allowed);
    assert(transfer_plan.binding_name == "box.payload");
    assert(orison::lowering::aggregate_projection_access_diagnostic(transfer_plan).empty());
    assert(
        orison::lowering::aggregate_projection_access_plan_report(transfer_plan) ==
        "aggregate projection access intent explicit_transfer status allowed "
        "binding box.payload source Payload receiver false"
    );

    auto projection_state = orison::lowering::FunctionLoweringState {};
    projection_state.addressable_bindings.emplace("holder", orison::lowering::AddressableBinding {
        .type = orison::lowering::LoweredType {
            .type = "%record.Holder",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        .storage = "%holder.addr",
    });
    projection_state.source_type_names.emplace("holder", "Holder");
    projection_state.immutable_bindings.emplace("row", orison::lowering::LoweredExpression {
        .type = "i64",
        .value = "%row",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    projection_state.immutable_bindings.emplace("column", orison::lowering::LoweredExpression {
        .type = "i64",
        .value = "%column",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto projection_failures = orison::lowering::LoweringFailures {};
    auto projection_session = orison::lowering::FunctionLoweringSession {
        .state = projection_state,
        .failures = projection_failures,
        .semantics = nullptr,
        .enclosing_symbol_name = "main",
    };

    auto direct_runtime_projection = member(
        index_name(index_name(member(name("holder"), "grid"), "row"), "column"),
        "values"
    );
    auto direct_projection_plan = orison::lowering::plan_runtime_index_aggregate_descriptor_projection(
        direct_runtime_projection,
        "holder.grid[row][column].values",
        projection_session
    );
    assert(direct_projection_plan.has_value());
    assert(direct_projection_plan->root_name == "holder");
    assert(direct_projection_plan->root_storage == "%holder.addr");
    assert(direct_projection_plan->root_source_type_name == "Holder");
    assert(direct_projection_plan->aggregate_path.base_expression != nullptr);
    assert(direct_projection_plan->aggregate_path.base_expression->kind == orison::syntax::ExpressionKind::name);
    assert(direct_projection_plan->aggregate_path.steps.size() == 4);

    auto forwarded_runtime_projection = member(
        index_name(index_name(member(call("forward_holder"), "grid"), "row"), "column"),
        "values"
    );
    auto forwarded_projection_plan = orison::lowering::plan_runtime_index_aggregate_descriptor_projection(
        forwarded_runtime_projection,
        "holder.grid[row][column].values",
        projection_session
    );
    assert(forwarded_projection_plan.has_value());
    assert(forwarded_projection_plan->root_name == "holder");
    assert(forwarded_projection_plan->root_storage == "%holder.addr");
    assert(forwarded_projection_plan->root_source_type_name == "Holder");
    assert(forwarded_projection_plan->aggregate_path.base_expression != nullptr);
    assert(forwarded_projection_plan->aggregate_path.base_expression->kind == orison::syntax::ExpressionKind::call);
    assert(forwarded_projection_plan->aggregate_path.steps.size() == 4);

    auto missing_projection_plan = orison::lowering::plan_runtime_index_aggregate_descriptor_projection(
        forwarded_runtime_projection,
        ".grid[row][column].values",
        projection_session
    );
    assert(!missing_projection_plan.has_value());

    auto string_constants = orison::lowering::StringConstantTable {};
    auto projection_context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = string_constants,
        .options = {},
    };
    auto projection_output = std::ostringstream {};
    projection_state.next_temporary_index = 0;
    projection_state.next_block_index = 0;
    projection_state.current_block = "entry";
    auto direct_descriptor_storage = orison::lowering::emit_runtime_index_aggregate_descriptor_projection(
        *direct_projection_plan,
        "DynamicArray<Payload>",
        projection_context,
        projection_session,
        projection_output
    );
    assert(direct_descriptor_storage.has_value());
    assert(*direct_descriptor_storage == "%tmp5");
    assert(projection_state.current_block == "fixed_array.index.in_bounds.1");
    assert(
        projection_output.str() ==
        "  %tmp0 = getelementptr %record.Holder, ptr %holder.addr, i32 0, i32 0\n"
        "  %computed_dynamic_array_runtime_aggregate_index1.in_bounds = icmp ult i64 %row, 2\n"
        "  br i1 %computed_dynamic_array_runtime_aggregate_index1.in_bounds, "
        "label %fixed_array.index.in_bounds.0, label %fixed_array.index.out_of_bounds.0\n"
        "fixed_array.index.out_of_bounds.0:\n"
        "  call void @__orison_dynamic_array_bounds_failed()\n"
        "  unreachable\n"
        "fixed_array.index.in_bounds.0:\n"
        "  %tmp2 = getelementptr [2 x [2 x %record.RuntimeBucket]], ptr %tmp0, i64 0, i64 %row\n"
        "  %computed_dynamic_array_runtime_aggregate_index3.in_bounds = icmp ult i64 %column, 2\n"
        "  br i1 %computed_dynamic_array_runtime_aggregate_index3.in_bounds, "
        "label %fixed_array.index.in_bounds.1, label %fixed_array.index.out_of_bounds.1\n"
        "fixed_array.index.out_of_bounds.1:\n"
        "  call void @__orison_dynamic_array_bounds_failed()\n"
        "  unreachable\n"
        "fixed_array.index.in_bounds.1:\n"
        "  %tmp4 = getelementptr [2 x %record.RuntimeBucket], ptr %tmp2, i64 0, i64 %column\n"
        "  %tmp5 = getelementptr %record.RuntimeBucket, ptr %tmp4, i32 0, i32 0\n"
    );

    projection_state.next_temporary_index = 0;
    projection_state.next_block_index = 0;
    projection_state.current_block = "entry";
    auto failed_projection_output = std::ostringstream {};
    auto mismatched_descriptor_storage = orison::lowering::emit_runtime_index_aggregate_descriptor_projection(
        *direct_projection_plan,
        "DynamicArray<UInt32>",
        projection_context,
        projection_session,
        failed_projection_output
    );
    assert(!mismatched_descriptor_storage.has_value());

    auto returned_sibling_paths = orison::lowering::collect_returned_aggregate_sibling_projection_paths(
        "returned.holder",
        "Holder",
        "%record.Holder",
        lowering
    );
    assert(returned_sibling_paths.has_value());
    assert(returned_sibling_paths->descriptor_paths.size() == 4);
    assert(returned_sibling_paths->maybe_owner_paths.size() == 1);
    assert(returned_sibling_paths->choice_owner_paths.size() == 1);

    auto returned_descriptor_paths = orison::lowering::collect_returned_aggregate_descriptor_projection_paths(
        "returned.holder",
        "Holder",
        "%record.Holder",
        lowering
    );
    assert(returned_descriptor_paths.has_value());
    assert(returned_descriptor_paths->size() == returned_sibling_paths->descriptor_paths.size());
    assert((*returned_descriptor_paths)[0].owner_name == "returned.holder.grid.element0.element0.values");
    assert(returned_sibling_paths->descriptor_paths[0].owner_name == (*returned_descriptor_paths)[0].owner_name);
    assert((*returned_descriptor_paths)[0].source_type_name == "DynamicArray<Payload>");
    assert((*returned_descriptor_paths)[0].steps.size() == 4);
    assert(
        (*returned_descriptor_paths)[0].steps[0].kind ==
        orison::lowering::ReturnedAggregateDescriptorProjectionStepKind::field
    );
    assert((*returned_descriptor_paths)[0].steps[0].index_value == "0");
    assert(
        (*returned_descriptor_paths)[0].steps[1].kind ==
        orison::lowering::ReturnedAggregateDescriptorProjectionStepKind::array_element
    );
    assert((*returned_descriptor_paths)[0].steps[1].index_value == "0");

    auto returned_maybe_paths = orison::lowering::collect_returned_aggregate_maybe_projection_paths(
        "returned.holder",
        "Holder",
        "%record.Holder",
        lowering
    );
    assert(returned_maybe_paths.has_value());
    assert(returned_maybe_paths->size() == returned_sibling_paths->maybe_owner_paths.size());
    assert((*returned_maybe_paths)[0].owner_name == "returned.holder.maybe_values");
    assert(returned_sibling_paths->maybe_owner_paths[0].owner_name == (*returned_maybe_paths)[0].owner_name);
    assert((*returned_maybe_paths)[0].source_type_name == "Maybe<DynamicArray<Payload>>");
    assert((*returned_maybe_paths)[0].steps.size() == 1);
    assert((*returned_maybe_paths)[0].steps[0].index_value == "1");

    auto returned_choice_paths = orison::lowering::collect_returned_aggregate_choice_projection_paths(
        "returned.holder",
        "Holder",
        "%record.Holder",
        lowering
    );
    assert(returned_choice_paths.has_value());
    assert(returned_choice_paths->size() == returned_sibling_paths->choice_owner_paths.size());
    assert((*returned_choice_paths)[0].owner_name == "returned.holder.packet");
    assert(returned_sibling_paths->choice_owner_paths[0].owner_name == (*returned_choice_paths)[0].owner_name);
    assert((*returned_choice_paths)[0].source_type_name == "Packet");
    assert((*returned_choice_paths)[0].steps.size() == 1);
    assert((*returned_choice_paths)[0].steps[0].index_value == "2");

    projection_state.next_temporary_index = 0;
    auto returned_projection_output = std::ostringstream {};
    auto maybe_pointer = orison::lowering::emit_returned_aggregate_descriptor_projection_pointer(
        "%returned.holder.addr",
        (*returned_maybe_paths)[0],
        "%returned.holder.maybe_values",
        projection_session,
        returned_projection_output
    );
    assert(maybe_pointer == "%returned.holder.maybe_values.path0");
    assert(
        returned_projection_output.str() ==
        "  %returned.holder.maybe_values.path0 = getelementptr %record.Holder, "
        "ptr %returned.holder.addr, i32 0, i32 1\n"
    );

    auto returned_materialization_output = std::ostringstream {};
    auto returned_aggregate_storage = orison::lowering::emit_returned_aggregate_materialized_storage(
        "returned.holder.grid[row][column].values",
        "%record.Holder",
        "%holder.value",
        returned_materialization_output
    );
    assert(returned_aggregate_storage == "%returned.holder.grid.row..column..values.aggregate.addr");
    assert(
        returned_materialization_output.str() ==
        "  %returned.holder.grid.row..column..values.aggregate.addr = alloca %record.Holder\n"
        "  store %record.Holder %holder.value, ptr "
        "%returned.holder.grid.row..column..values.aggregate.addr\n"
    );

    auto selected_descriptor_output = std::ostringstream {};
    auto selected_descriptor_storage =
        orison::lowering::move_returned_aggregate_selected_descriptor_to_cleanup_storage(
            "returned.holder.grid[row][column].values",
            "DynamicArray<Payload>",
            "%selected.descriptor.ptr",
            42,
            projection_context,
            projection_session,
            selected_descriptor_output
        );
    assert(selected_descriptor_storage.has_value());
    assert(projection_state.source_type_names["returned.holder.grid[row][column].values"] == "DynamicArray<Payload>");
    assert(projection_state.addressable_bindings.contains("returned.holder.grid[row][column].values"));
    assert(
        projection_state.addressable_bindings["returned.holder.grid[row][column].values"].storage ==
        *selected_descriptor_storage
    );
    auto const selected_descriptor_ir = selected_descriptor_output.str();
    assert(
        selected_descriptor_ir.find(
            "%returned.holder.grid.row..column..values.descriptor = load { ptr, i64, i64 }, "
            "ptr %selected.descriptor.ptr\n"
        ) != std::string::npos
    );
    assert(
        selected_descriptor_ir.find(
            "  store { ptr, i64, i64 } %returned.holder.grid.row..column..values.descriptor, ptr "
        ) != std::string::npos
    );
    assert(
        selected_descriptor_ir.find(
            "  store { ptr, i64, i64 } zeroinitializer, ptr %selected.descriptor.ptr\n"
        ) != std::string::npos
    );

    auto borrow_plan = orison::lowering::describe_named_aggregate_projection_access(
        owned_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::shared_borrow
    );
    assert(borrow_plan.status == orison::lowering::AggregateProjectionAccessStatus::boundary_not_enabled);
    assert(
        orison::lowering::aggregate_projection_access_diagnostic(borrow_plan) ==
        "aggregate projection shared_borrow boundary is not enabled"
    );
    assert(
        orison::lowering::aggregate_projection_access_plan_report(borrow_plan) ==
        "aggregate projection access intent shared_borrow status boundary_not_enabled "
        "binding box.payload source Payload receiver false diagnostic "
        "aggregate projection shared_borrow boundary is not enabled"
    );

    auto clone_plan = orison::lowering::describe_named_aggregate_projection_access(
        owned_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::clone_value
    );
    assert(clone_plan.status == orison::lowering::AggregateProjectionAccessStatus::boundary_not_enabled);
    assert(
        orison::lowering::aggregate_projection_access_diagnostic(clone_plan) ==
        "aggregate projection clone_value boundary is not enabled"
    );

    auto receiver_projection = member(name("this"), "payload");
    auto receiver_plan = orison::lowering::describe_named_aggregate_projection_access(
        receiver_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::value_read
    );
    assert(receiver_plan.status == orison::lowering::AggregateProjectionAccessStatus::allowed);
    assert(receiver_plan.binding_name == "this.payload");
    assert(receiver_plan.receiver_projection);

    auto scalar_projection = member(name("box"), "count");
    auto scalar_plan = orison::lowering::describe_named_aggregate_projection_access(
        scalar_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::value_read
    );
    assert(scalar_plan.status == orison::lowering::AggregateProjectionAccessStatus::non_owned_projection);
    assert(scalar_plan.binding_name == "box.count");
    assert(scalar_plan.source_type_name == "UInt32");
    assert(orison::lowering::aggregate_projection_access_diagnostic(scalar_plan).empty());

    auto nested_projection = member(member(name("nested"), "box"), "payload");
    auto nested_plan = orison::lowering::describe_named_aggregate_projection_access(
        nested_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::explicit_transfer
    );
    assert(nested_plan.status == orison::lowering::AggregateProjectionAccessStatus::allowed);
    assert(nested_plan.binding_name == "nested.box.payload");
    assert(nested_plan.source_type_name == "Payload");

    auto non_path_plan = orison::lowering::describe_named_aggregate_projection_access(
        name("box"),
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::value_read
    );
    assert(non_path_plan.status == orison::lowering::AggregateProjectionAccessStatus::not_named_aggregate_path);
    assert(orison::lowering::aggregate_projection_access_diagnostic(non_path_plan).empty());
    assert(
        orison::lowering::aggregate_projection_access_plan_report(non_path_plan) ==
        "aggregate projection access intent value_read status not_named_aggregate_path "
        "binding <none> source <unknown> receiver false"
    );

    assert(
        orison::lowering::render_aggregate_projection_access_intent(
            orison::lowering::AggregateProjectionAccessIntent::exclusive_borrow
        ) == "exclusive_borrow"
    );
    assert(
        orison::lowering::render_aggregate_projection_access_status(
            orison::lowering::AggregateProjectionAccessStatus::requires_explicit_boundary
        ) == "requires_explicit_boundary"
    );

    auto cursor = orison::lowering::initialize_aggregate_path_cursor("%shelf.addr", "Shelf", lowering);
    assert(cursor.has_value());
    assert(cursor->pointer == "%shelf.addr");
    assert(cursor->source_type_name == "Shelf");
    assert(cursor->llvm_type_name == "%record.Shelf");
    assert(cursor->record_layout == &lowering.records.at("Shelf"));
    assert(cursor->expects_record_layout);

    auto output = std::ostringstream {};
    auto member_result = orison::lowering::advance_aggregate_path_member(
        *cursor,
        "buckets",
        lowering,
        "%tmp0",
        output
    );
    assert(member_result.error == orison::lowering::AggregatePathError::none);
    assert(cursor->pointer == "%tmp0");
    assert(cursor->source_type_name == "Array<Bucket, 2>");
    assert(cursor->llvm_type_name == "[2 x %record.Bucket]");
    assert(cursor->record_layout == nullptr);
    assert(!cursor->expects_record_layout);

    auto index_result = orison::lowering::advance_aggregate_path_index(
        *cursor,
        "1",
        lowering,
        "%tmp1",
        output
    );
    assert(index_result.error == orison::lowering::AggregatePathError::none);
    assert(cursor->pointer == "%tmp1");
    assert(cursor->source_type_name == "Bucket");
    assert(cursor->llvm_type_name == "%record.Bucket");
    assert(cursor->record_layout == &lowering.records.at("Bucket"));
    assert(cursor->expects_record_layout);

    auto value_result = orison::lowering::advance_aggregate_path_member(
        *cursor,
        "values",
        lowering,
        "%tmp2",
        output
    );
    assert(value_result.error == orison::lowering::AggregatePathError::none);
    assert(cursor->pointer == "%tmp2");
    assert(cursor->source_type_name == "Array<UInt32, 3>");
    assert(cursor->llvm_type_name == "[3 x i32]");
    assert(cursor->record_layout == nullptr);
    assert(!cursor->expects_record_layout);

    assert(
        output.str() ==
        "  %tmp0 = getelementptr %record.Shelf, ptr %shelf.addr, i32 0, i32 0\n"
        "  %tmp1 = getelementptr [2 x %record.Bucket], ptr %tmp0, i64 0, i64 1\n"
        "  %tmp2 = getelementptr %record.Bucket, ptr %tmp1, i32 0, i32 0\n"
    );
    auto loaded_value = orison::lowering::emit_aggregate_path_cursor_load(
        *cursor,
        "[3 x i32]",
        orison::lowering::IntegerSignedness::not_integer,
        "%tmp3",
        output
    );
    assert(loaded_value.type == "[3 x i32]");
    assert(loaded_value.value == "%tmp3");
    assert(loaded_value.signedness == orison::lowering::IntegerSignedness::not_integer);
    assert(
        output.str() ==
        "  %tmp0 = getelementptr %record.Shelf, ptr %shelf.addr, i32 0, i32 0\n"
        "  %tmp1 = getelementptr [2 x %record.Bucket], ptr %tmp0, i64 0, i64 1\n"
        "  %tmp2 = getelementptr %record.Bucket, ptr %tmp1, i32 0, i32 0\n"
        "  %tmp3 = load [3 x i32], ptr %tmp2\n"
    );

    auto scalar_cursor = orison::lowering::initialize_aggregate_path_cursor("%value.addr", "UInt32", lowering);
    assert(scalar_cursor.has_value());
    auto scalar_member_result = orison::lowering::advance_aggregate_path_member(
        *scalar_cursor,
        "field",
        lowering,
        "%tmp3",
        output
    );
    assert(scalar_member_result.error == orison::lowering::AggregatePathError::expected_record);

    auto missing_field_cursor = orison::lowering::initialize_aggregate_path_cursor("%bucket.addr", "Bucket", lowering);
    assert(missing_field_cursor.has_value());
    auto missing_field_result = orison::lowering::advance_aggregate_path_member(
        *missing_field_cursor,
        "missing",
        lowering,
        "%tmp4",
        output
    );
    assert(missing_field_result.error == orison::lowering::AggregatePathError::unknown_field);

    auto temporary_member_cursor =
        orison::lowering::initialize_aggregate_path_cursor("%shelf.addr.1", "Shelf", lowering);
    assert(temporary_member_cursor.has_value());
    auto next_temporary_index = std::size_t {6};
    auto temporary_member_output = std::ostringstream {};
    auto temporary_member_result = orison::lowering::advance_aggregate_path_member_with_temporary(
        *temporary_member_cursor,
        "buckets",
        lowering,
        next_temporary_index,
        temporary_member_output
    );
    assert(temporary_member_result.error == orison::lowering::AggregatePathError::none);
    assert(next_temporary_index == 7);
    assert(temporary_member_cursor->pointer == "%tmp6");
    assert(temporary_member_cursor->source_type_name == "Array<Bucket, 2>");
    assert(
        temporary_member_output.str() ==
        "  %tmp6 = getelementptr %record.Shelf, ptr %shelf.addr.1, i32 0, i32 0\n"
    );

    auto temporary_index_cursor =
        orison::lowering::initialize_aggregate_path_cursor("%buckets.addr", "Array<Bucket, 2>", lowering);
    assert(temporary_index_cursor.has_value());
    auto next_index_temporary_index = std::size_t {7};
    auto temporary_index_output = std::ostringstream {};
    auto temporary_index_result = orison::lowering::advance_aggregate_path_index_with_temporary(
        *temporary_index_cursor,
        "1",
        lowering,
        next_index_temporary_index,
        temporary_index_output
    );
    assert(temporary_index_result.error == orison::lowering::AggregatePathError::none);
    assert(next_index_temporary_index == 8);
    assert(temporary_index_cursor->pointer == "%tmp7");
    assert(temporary_index_cursor->source_type_name == "Bucket");
    assert(temporary_index_cursor->expects_record_layout);
    assert(
        temporary_index_output.str() ==
        "  %tmp7 = getelementptr [2 x %record.Bucket], ptr %buckets.addr, i64 0, i64 1\n"
    );

    auto bad_index_cursor = orison::lowering::initialize_aggregate_path_cursor("%bucket.addr", "Bucket", lowering);
    assert(bad_index_cursor.has_value());
    auto bad_index_result = orison::lowering::advance_aggregate_path_index(
        *bad_index_cursor,
        "0",
        lowering,
        "%tmp5",
        output
    );
    assert(bad_index_result.error == orison::lowering::AggregatePathError::expected_array);

    auto unsupported_cursor =
        orison::lowering::initialize_aggregate_path_cursor("%unknown.addr", "Unknown", lowering);
    assert(!unsupported_cursor.has_value());

    return 0;
}
