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

auto call(std::string function_name, orison::syntax::ExpressionSyntax argument)
    -> orison::syntax::ExpressionSyntax {
    auto expression = call(std::move(function_name));
    expression.arguments.push_back(std::move(argument));
    return expression;
}

auto call(
    std::string function_name,
    orison::syntax::ExpressionSyntax first_argument,
    orison::syntax::ExpressionSyntax second_argument
) -> orison::syntax::ExpressionSyntax {
    auto expression = call(std::move(function_name));
    expression.arguments.push_back(std::move(first_argument));
    expression.arguments.push_back(std::move(second_argument));
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

auto expression_statement(orison::syntax::ExpressionSyntax expression) -> orison::syntax::StatementSyntax {
    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::expression_statement;
    statement.expression = std::move(expression);
    return statement;
}

auto let_statement(std::string name, orison::syntax::ExpressionSyntax expression)
    -> orison::syntax::StatementSyntax {
    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::let_binding;
    statement.name = std::move(name);
    statement.expression = std::move(expression);
    return statement;
}

auto two_statement_block(
    orison::syntax::StatementSyntax first,
    orison::syntax::StatementSyntax second
) -> std::vector<orison::syntax::StatementSyntax> {
    auto statements = std::vector<orison::syntax::StatementSyntax> {};
    statements.push_back(std::move(first));
    statements.push_back(std::move(second));
    return statements;
}

auto one_statement_block(orison::syntax::StatementSyntax statement)
    -> std::vector<orison::syntax::StatementSyntax> {
    auto statements = std::vector<orison::syntax::StatementSyntax> {};
    statements.push_back(std::move(statement));
    return statements;
}

auto if_statement(
    std::vector<orison::syntax::StatementSyntax> then_statements,
    std::vector<orison::syntax::StatementSyntax> else_statements
) -> orison::syntax::StatementSyntax {
    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::if_statement;
    statement.expression = name("flag");
    statement.nested_statements = std::move(then_statements);
    statement.alternate_statements = std::move(else_statements);
    return statement;
}

auto switch_statement(
    std::vector<orison::syntax::StatementSyntax> first_case,
    std::vector<orison::syntax::StatementSyntax> second_case
) -> orison::syntax::StatementSyntax {
    auto first_case_pointers = std::vector<std::unique_ptr<orison::syntax::StatementSyntax>> {};
    first_case_pointers.reserve(first_case.size());
    for (auto& case_statement : first_case) {
        first_case_pointers.push_back(
            std::make_unique<orison::syntax::StatementSyntax>(std::move(case_statement))
        );
    }

    auto second_case_pointers = std::vector<std::unique_ptr<orison::syntax::StatementSyntax>> {};
    second_case_pointers.reserve(second_case.size());
    for (auto& case_statement : second_case) {
        second_case_pointers.push_back(
            std::make_unique<orison::syntax::StatementSyntax>(std::move(case_statement))
        );
    }

    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::switch_statement;
    statement.expression = name("selector");
    statement.switch_cases.push_back(orison::syntax::SwitchCaseSyntax {
        .is_default = false,
        .pattern = integer_literal("0"),
        .statements = std::move(first_case_pointers),
    });
    statement.switch_cases.push_back(orison::syntax::SwitchCaseSyntax {
        .is_default = true,
        .pattern = {},
        .statements = std::move(second_case_pointers),
    });
    return statement;
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
    state.source_type_names["bucket_array_pointer"] = "Pointer<Array<Bucket, 2>>";
    state.source_type_names["wrapper_pointer"] = "Pointer<Wrapper>";
    state.source_type_names["left_values"] = "Array<UInt32, 3>";
    state.source_type_names["right_values"] = "Array<UInt32, 3>";

    assert(orison::lowering::split_top_level_generic_arguments("Bucket, Array<UInt32, 3>").size() == 2);
    assert(orison::lowering::array_element_source_type_name("Array<Bucket, 2>") == "Bucket");
    assert(orison::lowering::dynamic_array_element_source_type_name("DynamicArray<Array<UInt32, 3>>") == "Array<UInt32, 3>");
    assert(!orison::lowering::dynamic_array_element_source_type_name("Array<UInt32, 3>").has_value());
    assert(orison::lowering::view_element_source_type_name("View<UInt32>") == "UInt32");
    assert(orison::lowering::view_element_source_type_name("shared.View<Array<UInt32, 3>>") == "Array<UInt32, 3>");
    assert(orison::lowering::view_element_source_type_name("exclusive.View<Bucket>") == "Bucket");
    assert(orison::lowering::pointer_pointee_source_type_name("Pointer<UInt32>") == "UInt32");
    assert(orison::lowering::maybe_payload_source_type_name("Maybe<Array<UInt32, 3>>") == "Array<UInt32, 3>");
    assert(!orison::lowering::maybe_payload_source_type_name("Array<UInt32, 3>").has_value());

    auto dynamic_array_sequence = orison::lowering::dynamic_sequence_source_type("DynamicArray<UInt32>");
    assert(dynamic_array_sequence.has_value());
    assert(dynamic_array_sequence->kind == orison::lowering::DynamicSequenceKind::dynamic_array);
    assert(dynamic_array_sequence->element_source_type_name == "UInt32");
    assert(dynamic_array_sequence->owns_storage);
    assert(dynamic_array_sequence->permits_element_mutation);
    assert(orison::lowering::dynamic_array_descriptor_llvm_type() == "{ ptr, i64, i64 }");

    auto shared_view_sequence = orison::lowering::dynamic_sequence_source_type("shared.View<Byte>");
    assert(shared_view_sequence.has_value());
    assert(shared_view_sequence->kind == orison::lowering::DynamicSequenceKind::shared_view);
    assert(shared_view_sequence->element_source_type_name == "Byte");
    assert(!shared_view_sequence->owns_storage);
    assert(!shared_view_sequence->permits_element_mutation);

    auto plain_view_sequence = orison::lowering::dynamic_sequence_source_type("View<Byte>");
    assert(plain_view_sequence.has_value());
    assert(plain_view_sequence->kind == orison::lowering::DynamicSequenceKind::view);
    assert(plain_view_sequence->element_source_type_name == "Byte");
    assert(!plain_view_sequence->owns_storage);
    assert(!plain_view_sequence->permits_element_mutation);

    auto exclusive_view_sequence = orison::lowering::dynamic_sequence_source_type("exclusive.View<Bucket>");
    assert(exclusive_view_sequence.has_value());
    assert(exclusive_view_sequence->kind == orison::lowering::DynamicSequenceKind::exclusive_view);
    assert(exclusive_view_sequence->element_source_type_name == "Bucket");
    assert(!exclusive_view_sequence->owns_storage);
    assert(exclusive_view_sequence->permits_element_mutation);
    assert(!orison::lowering::dynamic_sequence_source_type("Array<UInt32, 3>").has_value());

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

    auto maybe_scalar_type = orison::lowering::llvm_type_for_source_type_name("Maybe<UInt32>", context);
    assert(maybe_scalar_type.has_value());
    assert(*maybe_scalar_type == "{ i1, i32 }");

    auto maybe_record_type = orison::lowering::llvm_type_for_source_type_name("Maybe<Bucket>", context);
    assert(maybe_record_type.has_value());
    assert(*maybe_record_type == "{ i1, %record.Bucket }");

    auto maybe_array_type =
        orison::lowering::llvm_type_for_source_type_name("Maybe<Array<UInt32, 3>>", context);
    assert(maybe_array_type.has_value());
    assert(*maybe_array_type == "{ i1, [3 x i32] }");

    auto view_type = orison::lowering::llvm_type_for_source_type_name("shared.View<UInt32>", context);
    assert(view_type.has_value());
    assert(*view_type == std::string {orison::lowering::view_descriptor_llvm_type()});
    assert(!orison::lowering::llvm_type_for_source_type_name("DynamicArray<UInt32>", context).has_value());

    assert(!orison::lowering::llvm_type_for_source_type_name("Maybe<Unit>", context).has_value());

    assert(orison::lowering::source_type_name_for_expression(name("wrapper"), context, state) == "Wrapper");
    assert(
        orison::lowering::source_type_name_for_expression(
            member(name("wrapper"), "bucket"),
            context,
            state
        ) == "Bucket"
    );
    assert(orison::lowering::source_type_name_for_expression(index(name("buckets")), context, state) == "Bucket");
    assert(orison::lowering::source_type_name_for_expression(index(name("bucket_array_pointer")), context, state) == "Bucket");
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
            call("Pointer", call("address_of", member(name("wrapper"), "bucket"))),
            context,
            state
        ) == "Pointer<Bucket>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            call("Pointer", call("address_of", member(member(name("wrapper_pointer"), "bucket"), "values"))),
            context,
            state
        ) == "Pointer<Array<UInt32, 3>>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            call(
                "raw_offset",
                call("Pointer", call("address_of", member(name("wrapper"), "bucket"))),
                cast(integer_literal("1"), "UInt64")
            ),
            context,
            state
        ) == "Pointer<Bucket>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            array_literal(record_constructor("Bucket"), record_constructor("Bucket")),
            context,
            state
        ) == "Array<Bucket, 2>"
    );
    assert(
        orison::lowering::source_type_name_for_initializer(
            name("wrapper"),
            context,
            state,
            "%record.Wrapper"
        ) == "Wrapper"
    );
    assert(
        orison::lowering::source_type_name_for_initializer(
            cast(integer_literal("1"), "UInt32"),
            context,
            state,
            "i32"
        ) == "UInt32"
    );
    assert(
        orison::lowering::source_type_name_for_initializer(
            integer_literal("1"),
            context,
            state,
            "i32"
        ) == "UInt32"
    );
    assert(
        orison::lowering::source_type_name_for_value_statement(
            if_statement(
                two_statement_block(
                    let_statement("bucket", record_constructor("Bucket")),
                    expression_statement(member(name("bucket"), "values"))
                ),
                two_statement_block(
                    let_statement("bucket", record_constructor("Bucket")),
                    expression_statement(member(name("bucket"), "values"))
                )
            ),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        orison::lowering::source_type_name_for_value_statement(
            switch_statement(
                one_statement_block(expression_statement(name("left_values"))),
                one_statement_block(expression_statement(name("right_values")))
            ),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        !orison::lowering::source_type_name_for_value_statement(
            if_statement(
                one_statement_block(expression_statement(name("wrapper"))),
                one_statement_block(expression_statement(name("right_values")))
            ),
            context,
            state
        ).has_value()
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
