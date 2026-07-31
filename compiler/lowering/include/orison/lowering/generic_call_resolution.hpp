#pragma once

#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace orison::lowering {

struct FunctionLoweringState;

struct GenericCallSourceResolver {
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const* generic_functions = nullptr;
    std::unordered_map<std::string, LoweredFunctionSignature> const* functions = nullptr;
    std::unordered_map<std::string, std::string> const* local_source_types = nullptr;
    LoweringContext const* lowering_context = nullptr;
    FunctionLoweringState const* state = nullptr;
};

auto generic_specialization_base_name(std::string_view symbol_name) -> std::optional<std::string>;

auto source_type_name_for_generic_call_argument(
    syntax::ExpressionSyntax const& expression,
    GenericCallSourceResolver const& resolver
) -> std::optional<std::string>;

auto bind_generic_function_call_substitutions(
    syntax::FunctionSyntax const& function,
    syntax::ExpressionSyntax const& call,
    GenericCallSourceResolver const& resolver
) -> std::optional<std::unordered_map<std::string, syntax::TypeSyntax>>;

auto call_arguments_match_source_types(
    syntax::ExpressionSyntax const& expression,
    LoweredFunctionSignature const& signature,
    GenericCallSourceResolver const& resolver
) -> bool;

auto find_matching_generic_specialization(
    std::string_view function_name,
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> LoweredFunctionSignature const*;

}  // namespace orison::lowering
