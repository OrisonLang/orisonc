#pragma once

#include "orison/lowering/function_lowering_state.hpp"

#include <utility>

namespace orison::lowering {

enum class StatementFlow {
    falls_through,
    terminated,
    failed,
};

class LoopTargetScope {
public:
    LoopTargetScope(FunctionLoweringState& state, LoopTargets targets)
        : state_(state) {
        state_.loop_targets.push_back(std::move(targets));
    }

    ~LoopTargetScope() {
        state_.loop_targets.pop_back();
    }

    LoopTargetScope(LoopTargetScope const&) = delete;
    auto operator=(LoopTargetScope const&) -> LoopTargetScope& = delete;

private:
    FunctionLoweringState& state_;
};

}  // namespace orison::lowering
