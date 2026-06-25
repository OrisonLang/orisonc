#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"

#include <iosfwd>
#include <string_view>

namespace orison::lowering {

auto is_aggregate_llvm_type(std::string_view type) -> bool;

void bind_addressable_aggregate_value(
    std::string_view name,
    LoweredExpression const& lowered,
    FunctionLoweringSession& session,
    std::ostream& output
);

}  // namespace orison::lowering
