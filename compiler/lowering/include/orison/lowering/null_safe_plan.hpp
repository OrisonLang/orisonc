#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <string>
#include <vector>

namespace orison::lowering {

enum class NullSafePlanFailureReason {
    none,
    invalid_shape,
    unknown_base_type,
    expected_maybe_base,
    unknown_record,
    unknown_field,
};

struct NullSafePlanFailure {
    NullSafePlanFailureReason reason = NullSafePlanFailureReason::none;
    std::string detail;
};

struct NullSafePlanSegment {
    std::string field_name;
    std::string receiver_type_name;
    std::string field_type_name;
};

struct NullSafePlan {
    syntax::ExpressionSyntax const* base_expression = nullptr;
    std::string base_maybe_type_name;
    std::string result_payload_type_name;
    std::string result_maybe_type_name;
    std::vector<NullSafePlanSegment> segments;
};

struct NullSafePlanResult {
    std::optional<NullSafePlan> plan;
    NullSafePlanFailure failure;
};

auto plan_null_safe_member_access(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> NullSafePlanResult;

auto render_null_safe_plan_failure(NullSafePlanFailure const& failure) -> std::string;

}  // namespace orison::lowering
