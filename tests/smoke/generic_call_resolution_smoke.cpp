#include "orison/lowering/generic_call_resolution.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

auto cast_expression(
    orison::syntax::ExpressionSyntax inner,
    std::string target_type
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::cast;
    expression.text = std::move(target_type);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(inner));
    return expression;
}

auto integer_literal(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::integer_literal;
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

auto call_expression(
    std::string function_name,
    orison::syntax::ExpressionSyntax first_argument,
    orison::syntax::ExpressionSyntax second_argument
) -> orison::syntax::ExpressionSyntax {
    auto expression = call_expression(std::move(function_name));
    expression.arguments.push_back(std::move(first_argument));
    expression.arguments.push_back(std::move(second_argument));
    return expression;
}

auto member_call_expression(
    orison::syntax::ExpressionSyntax receiver,
    std::string method_name,
    orison::syntax::ExpressionSyntax argument
) -> orison::syntax::ExpressionSyntax {
    auto member = orison::syntax::ExpressionSyntax {};
    member.kind = orison::syntax::ExpressionKind::member_access;
    member.text = std::move(method_name);
    member.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(receiver));

    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(member));
    expression.arguments.push_back(std::move(argument));
    return expression;
}

auto member_call_expression(
    orison::syntax::ExpressionSyntax receiver,
    std::string method_name
) -> orison::syntax::ExpressionSyntax {
    auto member = orison::syntax::ExpressionSyntax {};
    member.kind = orison::syntax::ExpressionKind::member_access;
    member.text = std::move(method_name);
    member.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(receiver));

    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(member));
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

auto binary_generic_function(
    std::string name,
    orison::syntax::TypeSyntax parameter_type,
    orison::syntax::TypeSyntax return_type
) -> orison::syntax::FunctionSyntax {
    return orison::syntax::FunctionSyntax {
        .name = std::move(name),
        .generic_parameters = {"T"},
        .parameters = {
            orison::syntax::ParameterSyntax {
                .name = "left",
                .type = parameter_type,
            },
            orison::syntax::ParameterSyntax {
                .name = "right",
                .type = parameter_type,
            },
        },
        .return_type = std::move(return_type),
    };
}

}  // namespace

int main() {
    auto first = generic_function("first", "values", dynamic_array_type("T"), type("T"));
    auto consume = generic_function("consume", "value", type("T"), type("T"));
    auto choose_same = binary_generic_function("choose_same", type("T"), type("T"));

    auto generic_functions = std::unordered_map<std::string, orison::syntax::FunctionSyntax const*> {
        {"first", &first},
        {"consume", &consume},
        {"choose_same", &choose_same},
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
        {
            "make_u64",
            orison::lowering::LoweredFunctionSignature {
                .return_type = "i64",
                .source_return_type_name = "UInt64",
                .symbol_name = "make_u64",
            },
        },
    };
    auto local_source_types = std::unordered_map<std::string, std::string> {};
    auto record_names = std::unordered_set<std::string> {"Payload"};
    auto collector_resolver = orison::lowering::GenericCallSourceResolver {
        .generic_functions = &generic_functions,
        .functions = &functions,
        .local_source_types = &local_source_types,
        .record_names = &record_names,
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

    auto payload_constructor_type = orison::lowering::source_type_name_for_generic_call_argument(
        call_expression("Payload", cast_expression(integer_literal("11"), "UInt32")),
        collector_resolver
    );
    assert(payload_constructor_type.has_value());
    assert(*payload_constructor_type == "Payload");

    auto cast_call = call_expression("consume", cast_expression(integer_literal("9"), "UInt64"));
    auto cast_substitutions = orison::lowering::bind_generic_function_call_substitutions(
        consume,
        cast_call,
        collector_resolver
    );
    assert(cast_substitutions.has_value());
    assert(cast_substitutions->at("T").name == "UInt64");

    auto mismatched_generic_call = call_expression(
        "choose_same",
        name_expression("value"),
        call_expression("make_u64")
    );
    auto mismatched_substitutions = orison::lowering::bind_generic_function_call_substitutions(
        choose_same,
        mismatched_generic_call,
        collector_resolver
    );
    assert(!mismatched_substitutions.has_value());

    auto generic_pick = generic_function("pick", "value", type("T"), type("T"));
    generic_pick.parameters.insert(
        generic_pick.parameters.begin(),
        orison::syntax::ParameterSyntax {
            .name = "this",
            .type = type("shared.This"),
        }
    );
    auto generic_method_call = member_call_expression(
        name_expression("box"),
        "pick",
        cast_expression(integer_literal("9"), "UInt64")
    );
    local_source_types["box"] = "Box";
    auto method_substitutions = orison::lowering::bind_generic_method_call_substitutions(
        type("Box"),
        generic_pick,
        generic_method_call,
        collector_resolver
    );
    assert(method_substitutions.has_value());
    assert(method_substitutions->at("T").name == "UInt64");

    auto receiver_value = orison::syntax::FunctionSyntax {
        .name = "value",
        .parameters = {
            orison::syntax::ParameterSyntax {
                .name = "this",
                .type = type("shared.This"),
            },
        },
        .return_type = type("T"),
    };
    local_source_types["box"] = "Box<UInt32>";
    auto receiver_method_substitutions = orison::lowering::bind_generic_method_call_substitutions(
        dynamic_array_type("T"),
        receiver_value,
        member_call_expression(name_expression("box"), "value"),
        collector_resolver
    );
    assert(!receiver_method_substitutions.has_value());
    receiver_method_substitutions = orison::lowering::bind_generic_method_call_substitutions(
        orison::syntax::TypeSyntax {
            .name = "Box",
            .generic_arguments = {type("T")},
        },
        receiver_value,
        member_call_expression(name_expression("box"), "value"),
        collector_resolver
    );
    assert(receiver_method_substitutions.has_value());
    assert(receiver_method_substitutions->at("T").name == "UInt32");

    local_source_types["pair"] = "Pair<UInt32, UInt64>";
    auto pair_method_substitutions = orison::lowering::bind_generic_method_call_substitutions(
        orison::syntax::TypeSyntax {
            .name = "Pair",
            .generic_arguments = {type("A"), type("B")},
        },
        receiver_value,
        member_call_expression(name_expression("pair"), "value"),
        collector_resolver
    );
    assert(pair_method_substitutions.has_value());
    assert(pair_method_substitutions->at("A").name == "UInt32");
    assert(pair_method_substitutions->at("B").name == "UInt64");

    local_source_types["nested_box"] = "Box<Pair<UInt32, UInt64>>";
    auto nested_receiver_method_substitutions = orison::lowering::bind_generic_method_call_substitutions(
        orison::syntax::TypeSyntax {
            .name = "Box",
            .generic_arguments = {type("T")},
        },
        receiver_value,
        member_call_expression(name_expression("nested_box"), "value"),
        collector_resolver
    );
    assert(nested_receiver_method_substitutions.has_value());
    assert(nested_receiver_method_substitutions->at("T").name == "Pair");
    assert(nested_receiver_method_substitutions->at("T").generic_arguments.size() == 2);
    assert(nested_receiver_method_substitutions->at("T").generic_arguments[0].name == "UInt32");
    assert(nested_receiver_method_substitutions->at("T").generic_arguments[1].name == "UInt64");

    local_source_types["items"] = "DynamicArray<UInt32>";
    auto dynamic_array_receiver_substitutions = orison::lowering::bind_generic_method_call_substitutions(
        dynamic_array_type("T"),
        receiver_value,
        member_call_expression(name_expression("items"), "value"),
        collector_resolver
    );
    assert(dynamic_array_receiver_substitutions.has_value());
    assert(dynamic_array_receiver_substitutions->at("T").name == "UInt32");

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
    context.functions.emplace(
        "consume__UInt64",
        orison::lowering::LoweredFunctionSignature {
            .return_type = "i64",
            .source_return_type_name = "UInt64",
            .parameter_types = {"i64"},
            .parameter_source_type_names = {"UInt64"},
            .symbol_name = "consume__UInt64",
        }
    );
    context.generic_function_specializations.push_back(
        std::make_shared<orison::syntax::FunctionSyntax>(orison::syntax::FunctionSyntax {
            .name = "consume__UInt32",
        })
    );
    auto state = orison::lowering::FunctionLoweringState {};
    state.source_type_names["value"] = "UInt32";
    state.source_type_names["box"] = "Box<UInt32>";
    auto matched = orison::lowering::find_matching_generic_specialization(
        "consume",
        local_call,
        "i32",
        context,
        state
    );
    assert(matched != nullptr);
    assert(matched->symbol_name == "consume__UInt32");

    auto mismatched = orison::lowering::find_matching_generic_specialization(
        "consume",
        local_call,
        "i64",
        context,
        state
    );
    assert(mismatched == nullptr);

    context.functions.emplace(
        "consume__UInt32_duplicate",
        orison::lowering::LoweredFunctionSignature {
            .return_type = "i32",
            .source_return_type_name = "UInt32",
            .parameter_types = {"i32"},
            .parameter_source_type_names = {"UInt32"},
            .symbol_name = "consume__UInt32_duplicate",
        }
    );
    context.generic_function_specializations.push_back(
        std::make_shared<orison::syntax::FunctionSyntax>(orison::syntax::FunctionSyntax {
            .name = "consume__UInt32_duplicate",
        })
    );
    auto ambiguous = orison::lowering::find_matching_generic_specialization(
        "consume",
        local_call,
        "i32",
        context,
        state
    );
    assert(ambiguous == nullptr);

    context.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Box<UInt32>",
        .method_name = "pick",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "i32",
            .source_return_type_name = "UInt32",
            .parameter_types = {"%record.Box_UInt32", "i32"},
            .parameter_source_type_names = {"Box<UInt32>", "UInt32"},
            .symbol_name = "method.Box_UInt32.pick",
        },
    });
    auto method_call = member_call_expression(name_expression("box"), "pick", name_expression("value"));
    auto method_source_type = orison::lowering::source_type_name_for_expression(
        method_call,
        context,
        state
    );
    assert(method_source_type.has_value());
    assert(*method_source_type == "UInt32");

    auto matched_method = orison::lowering::find_matching_generic_method_specialization(
        "Box<UInt32>",
        "pick",
        method_call,
        "i32",
        context,
        state
    );
    assert(matched_method != nullptr);
    assert(matched_method->symbol_name == "method.Box_UInt32.pick");

    auto no_matching_method = orison::lowering::find_matching_generic_method_specialization(
        "Box<UInt32>",
        "pick",
        method_call,
        "i64",
        context,
        state
    );
    assert(no_matching_method == nullptr);

    context.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Box<UInt32>",
        .method_name = "pick",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "i32",
            .source_return_type_name = "UInt32",
            .parameter_types = {"%record.Box_UInt32", "i32"},
            .parameter_source_type_names = {"Box<UInt32>", "UInt32"},
            .symbol_name = "method.Box_UInt32.pick_duplicate",
        },
    });
    auto ambiguous_method = orison::lowering::find_matching_generic_method_specialization(
        "Box<UInt32>",
        "pick",
        method_call,
        "i32",
        context,
        state
    );
    assert(ambiguous_method == nullptr);

    return 0;
}
