#pragma once

#include "orison/lowering/function_lowering_session.hpp"

#include <cstddef>
#include <iosfwd>
#include <string_view>

namespace orison::lowering {

void emit_fixed_array_runtime_index_bounds_check(
    std::string_view prefix_stem,
    std::string_view index_value,
    std::size_t length,
    FunctionLoweringSession& session,
    std::ostream& output
);

}  // namespace orison::lowering
