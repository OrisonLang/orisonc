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
