#pragma once

#include "orison/lowering/function_lowering_state.hpp"

#include <string>
#include <unordered_map>

namespace orison::lowering {

class ImmutableBindingScope {
public:
    explicit ImmutableBindingScope(FunctionLoweringState& state);
    ~ImmutableBindingScope() noexcept;

    ImmutableBindingScope(ImmutableBindingScope const&) = delete;
    auto operator=(ImmutableBindingScope const&) -> ImmutableBindingScope& = delete;
    ImmutableBindingScope(ImmutableBindingScope&&) = delete;
    auto operator=(ImmutableBindingScope&&) -> ImmutableBindingScope& = delete;

    void reset();

private:
    FunctionLoweringState& state_;
    std::unordered_map<std::string, LoweredExpression> saved_bindings_;
};

}  // namespace orison::lowering
