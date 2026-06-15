#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>

namespace orison::lowering {

enum class MemberCallReceiverInferenceResult {
    unsupported_shape,
    not_found,
    found,
};

struct MemberCallReceiverInference {
    MemberCallReceiverInferenceResult result = MemberCallReceiverInferenceResult::unsupported_shape;
    std::string receiver_type_name;
    std::string method_name;
};

auto infer_member_call_receiver(
    syntax::ExpressionSyntax const& call_expression,
    FunctionLoweringState const& state
) -> MemberCallReceiverInference;

auto render_source_type_name(syntax::TypeSyntax const& type) -> std::string;

}  // namespace orison::lowering
