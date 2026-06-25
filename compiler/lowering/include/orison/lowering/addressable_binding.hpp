#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace orison::lowering {

auto is_aggregate_llvm_type(std::string_view type) -> bool;

void bind_addressable_aggregate_value(
    std::string_view name,
    LoweredExpression const& lowered,
    FunctionLoweringSession& session,
    std::ostream& output
);

auto spill_aggregate_value_to_temporary_storage(
    LoweredExpression const& lowered,
    FunctionLoweringSession& session,
    std::ostream& output
) -> std::optional<std::string>;

}  // namespace orison::lowering
