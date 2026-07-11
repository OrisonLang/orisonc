#pragma once

#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_failures.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace orison::lowering {

struct LoweredSwitchCasePlan {
    syntax::SwitchCaseSyntax const* syntax = nullptr;
    std::string block;
    std::optional<LoweredExpression> pattern;
};

struct SwitchPlan {
    std::string merge_block;
    std::string fallback_block;
    std::string default_block;
    bool has_default = false;
    std::vector<LoweredSwitchCasePlan> cases;
};

struct SwitchPlanningResult {
    std::optional<SwitchPlan> plan;
    ControlFlowLoweringFailureReason failure = ControlFlowLoweringFailureReason::none;
};

auto plan_switch(
    std::vector<syntax::SwitchCaseSyntax> const& cases,
    LoweredType const& subject_type,
    LoweringContext const& context,
    std::optional<std::string_view> subject_source_type_name,
    std::size_t block_index
) -> SwitchPlanningResult;

}  // namespace orison::lowering
