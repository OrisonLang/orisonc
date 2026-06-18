#include "orison/lowering/source_type_queries.hpp"

#include <cassert>
#include <memory>
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
            method_call(member(name("wrapper"), "bucket"), "view"),
            context,
            state
        ) == "Array<UInt32, 3>"
    );

    return 0;
}
