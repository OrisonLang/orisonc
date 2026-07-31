#include "orison/lowering/generic_call_resolution.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>

namespace {

auto type(std::string name) -> orison::syntax::TypeSyntax {
    return orison::syntax::TypeSyntax {.name = std::move(name)};
}

auto dynamic_array_type(std::string element_name) -> orison::syntax::TypeSyntax {
    return orison::syntax::TypeSyntax {
        .name = "DynamicArray",
        .generic_arguments = {type(std::move(element_name))},
    };
}

auto name_expression(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::name;
    expression.text = std::move(text);
    return expression;
}

auto call_expression(std::string function_name) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(name_expression(std::move(function_name)));
    return expression;
}

auto call_expression(
    std::string function_name,
    orison::syntax::ExpressionSyntax argument
) -> orison::syntax::ExpressionSyntax {
    auto expression = call_expression(std::move(function_name));
    expression.arguments.push_back(std::move(argument));
    return expression;
}

auto generic_function(
    std::string name,
    std::string parameter_name,
    orison::syntax::TypeSyntax parameter_type,
    orison::syntax::TypeSyntax return_type
) -> orison::syntax::FunctionSyntax {
    return orison::syntax::FunctionSyntax {
        .name = std::move(name),
        .generic_parameters = {"T"},
        .parameters = {
            orison::syntax::ParameterSyntax {
                .name = std::move(parameter_name),
                .type = std::move(parameter_type),
            },
        },
        .return_type = std::move(return_type),
    };
}

}  // namespace

int main() {
    auto first = generic_function("first", "values", dynamic_array_type("T"), type("T"));
    auto consume = generic_function("consume", "value", type("T"), type("T"));

    auto generic_functions = std::unordered_map<std::string, orison::syntax::FunctionSyntax const*> {
        {"first", &first},
        {"consume", &consume},
    };
    auto functions = std::unordered_map<std::string, orison::lowering::LoweredFunctionSignature> {
        {
            "make_values",
            orison::lowering::LoweredFunctionSignature {
                .return_type = std::string {orison::lowering::dynamic_array_descriptor_llvm_type()},
                .source_return_type_name = "DynamicArray<UInt32>",
                .symbol_name = "make_values",
            },
        },
    };
    auto local_source_types = std::unordered_map<std::string, std::string> {};
    auto collector_resolver = orison::lowering::GenericCallSourceResolver {
        .generic_functions = &generic_functions,
        .functions = &functions,
        .local_source_types = &local_source_types,
    };

    auto first_call = call_expression("first", call_expression("make_values"));
    auto first_result = orison::lowering::source_type_name_for_generic_call_argument(
        first_call,
        collector_resolver
    );
    assert(first_result.has_value());
    assert(*first_result == "UInt32");

    auto nested_call = call_expression("consume", std::move(first_call));
    auto consume_substitutions = orison::lowering::bind_generic_function_call_substitutions(
        consume,
        nested_call,
        collector_resolver
    );
    assert(consume_substitutions.has_value());
    assert(consume_substitutions->at("T").name == "UInt32");

    local_source_types["value"] = "UInt32";
    auto local_call = call_expression("consume", name_expression("value"));
    auto local_substitutions = orison::lowering::bind_generic_function_call_substitutions(
        consume,
        local_call,
        collector_resolver
    );
    assert(local_substitutions.has_value());
    assert(local_substitutions->at("T").name == "UInt32");

    auto context = orison::lowering::LoweringContext {};
    context.functions.emplace(
        "consume__UInt32",
        orison::lowering::LoweredFunctionSignature {
            .return_type = "i32",
            .source_return_type_name = "UInt32",
            .parameter_types = {"i32"},
            .parameter_source_type_names = {"UInt32"},
            .symbol_name = "consume__UInt32",
        }
    );
    context.generic_function_specializations.push_back(
        std::make_shared<orison::syntax::FunctionSyntax>(orison::syntax::FunctionSyntax {
            .name = "consume__UInt32",
        })
    );
    auto state = orison::lowering::FunctionLoweringState {};
    state.source_type_names["value"] = "UInt32";
    auto matched = orison::lowering::find_matching_generic_specialization(
        "consume",
        local_call,
        "i32",
        context,
        state
    );
    assert(matched != nullptr);
    assert(matched->symbol_name == "consume__UInt32");

    return 0;
}
