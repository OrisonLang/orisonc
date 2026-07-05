#include "orison/lowering/source_type_queries.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

auto index(orison::syntax::ExpressionSyntax left) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::index_access;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    auto zero = orison::syntax::ExpressionSyntax {};
    zero.kind = orison::syntax::ExpressionKind::integer_literal;
    zero.text = "0";
    expression.arguments.push_back(std::move(zero));
    return expression;
}

auto call(std::string function_name) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(name(std::move(function_name)));
    return expression;
}

auto cast(orison::syntax::ExpressionSyntax operand, std::string type_name) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::cast;
    expression.text = std::move(type_name);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(operand));
    return expression;
}

auto integer_literal(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::integer_literal;
    expression.text = std::move(text);
    return expression;
}

auto record_constructor(std::string record_name) -> orison::syntax::ExpressionSyntax {
    auto expression = call(std::move(record_name));
    expression.arguments.push_back(cast(integer_literal("1"), "UInt32"));
    return expression;
}

auto array_literal(std::vector<orison::syntax::ExpressionSyntax> elements)
    -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::array_literal;
    expression.arguments = std::move(elements);
    return expression;
}

auto array_literal(
    orison::syntax::ExpressionSyntax first,
    orison::syntax::ExpressionSyntax second
) -> orison::syntax::ExpressionSyntax {
    auto elements = std::vector<orison::syntax::ExpressionSyntax> {};
    elements.push_back(std::move(first));
    elements.push_back(std::move(second));
    return array_literal(std::move(elements));
}

auto method_call(orison::syntax::ExpressionSyntax receiver, std::string method_name)
    -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = orison::syntax::ExpressionKind::member_access;
    expression.left->text = std::move(method_name);
    expression.left->left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(receiver));
    return expression;
}

auto ternary(
    orison::syntax::ExpressionSyntax condition,
    orison::syntax::ExpressionSyntax then_expression,
    orison::syntax::ExpressionSyntax else_expression
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::ternary;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(condition));
    expression.right = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(then_expression));
    expression.alternate = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(else_expression));
    return expression;
}

}  // namespace

int main() {
    auto context = orison::lowering::LoweringContext {};
    context.records.emplace("Bucket", orison::lowering::LoweredRecordLayout {
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
    context.records.emplace("Wrapper", orison::lowering::LoweredRecordLayout {
        .name = "Wrapper",
        .llvm_type_name = "%record.Wrapper",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "bucket",
                .source_type_name = "Bucket",
                .llvm_type = "%record.Bucket",
                .index = 0,
            },
        },
    });
    context.functions.emplace("make_bucket", orison::lowering::LoweredFunctionSignature {
        .return_type = "%record.Bucket",
        .return_signedness = orison::lowering::IntegerSignedness::not_integer,
        .symbol_name = "make_bucket",
    });
    context.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Bucket",
        .method_name = "view",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "[3 x i32]",
            .return_signedness = orison::lowering::IntegerSignedness::not_integer,
            .parameter_types = {"%record.Bucket"},
            .parameter_signedness = {orison::lowering::IntegerSignedness::not_integer},
            .symbol_name = "method.Bucket.view",
        },
    });

    auto state = orison::lowering::FunctionLoweringState {};
    state.source_type_names["wrapper"] = "Wrapper";
    state.source_type_names["buckets"] = "Array<Bucket, 2>";
    state.source_type_names["left_values"] = "Array<UInt32, 3>";
    state.source_type_names["right_values"] = "Array<UInt32, 3>";

    assert(orison::lowering::split_top_level_generic_arguments("Bucket, Array<UInt32, 3>").size() == 2);
    assert(orison::lowering::array_element_source_type_name("Array<Bucket, 2>") == "Bucket");
    assert(orison::lowering::pointer_pointee_source_type_name("Pointer<UInt32>") == "UInt32");

    auto array = orison::lowering::parse_llvm_array_type("[3 x i32]");
    assert(array.has_value());
    assert(array->element_type == "i32");
    assert(array->length == 3);

    assert(orison::lowering::source_type_name_for_llvm_type("%record.Bucket", context) == "Bucket");
    assert(orison::lowering::source_type_name_for_llvm_type("[3 x i32]", context) == "Array<UInt32, 3>");
    assert(orison::lowering::find_record_field(context.records.at("Wrapper"), "bucket") != nullptr);

    auto bucket_type = orison::lowering::lowered_type_for_source_type_name("Bucket", context);
    assert(bucket_type.has_value());
    assert(bucket_type->type == "%record.Bucket");

    auto array_type = orison::lowering::llvm_type_for_source_type_name("Array<Bucket, 2>", context);
    assert(array_type.has_value());
    assert(*array_type == "[2 x %record.Bucket]");

    assert(orison::lowering::source_type_name_for_expression(name("wrapper"), context, state) == "Wrapper");
    assert(
        orison::lowering::source_type_name_for_expression(
            member(name("wrapper"), "bucket"),
            context,
            state
        ) == "Bucket"
    );
    assert(orison::lowering::source_type_name_for_expression(index(name("buckets")), context, state) == "Bucket");
    assert(orison::lowering::source_type_name_for_expression(call("make_bucket"), context, state) == "Bucket");
    assert(
        orison::lowering::source_type_name_for_expression(
            cast(integer_literal("1"), "UInt32"),
            context,
            state
        ) == "UInt32"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            array_literal(cast(integer_literal("1"), "UInt32"), cast(integer_literal("2"), "UInt32")),
            context,
            state
        ) == "Array<UInt32, 2>"
    );
    assert(orison::lowering::source_type_name_for_expression(record_constructor("Bucket"), context, state) == "Bucket");
    assert(
        orison::lowering::source_type_name_for_expression(
            array_literal(record_constructor("Bucket"), record_constructor("Bucket")),
            context,
            state
        ) == "Array<Bucket, 2>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            method_call(member(name("wrapper"), "bucket"), "view"),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            ternary(name("flag"), name("left_values"), name("right_values")),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            ternary(
                name("flag"),
                array_literal(cast(integer_literal("1"), "UInt32"), cast(integer_literal("2"), "UInt32")),
                array_literal(cast(integer_literal("3"), "UInt32"), cast(integer_literal("4"), "UInt32"))
            ),
            context,
            state
        ) == "Array<UInt32, 2>"
    );
    assert(
        !orison::lowering::source_type_name_for_expression(
            array_literal(cast(integer_literal("1"), "UInt32"), cast(integer_literal("2"), "UInt64")),
            context,
            state
        ).has_value()
    );
    assert(
        !orison::lowering::source_type_name_for_expression(
            ternary(name("flag"), name("wrapper"), name("right_values")),
            context,
            state
        ).has_value()
    );

    return 0;
}
