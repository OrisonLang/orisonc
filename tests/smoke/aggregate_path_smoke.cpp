#include "orison/lowering/aggregate_path.hpp"

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

    auto transfer_plan = orison::lowering::describe_named_aggregate_projection_access(
        owned_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::explicit_transfer
    );
    assert(transfer_plan.status == orison::lowering::AggregateProjectionAccessStatus::allowed);
    assert(transfer_plan.binding_name == "box.payload");

    auto borrow_plan = orison::lowering::describe_named_aggregate_projection_access(
        owned_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::shared_borrow
    );
    assert(borrow_plan.status == orison::lowering::AggregateProjectionAccessStatus::boundary_not_enabled);

    auto clone_plan = orison::lowering::describe_named_aggregate_projection_access(
        owned_projection,
        lowering,
        access_state,
        orison::lowering::AggregateProjectionAccessIntent::clone_value
    );
    assert(clone_plan.status == orison::lowering::AggregateProjectionAccessStatus::boundary_not_enabled);

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
