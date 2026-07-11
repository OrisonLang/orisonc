#pragma once

#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace orison::lowering {

struct MaybeValueAbi {
    std::string source_type_name;
    std::string payload_source_type_name;
    std::string llvm_type;
    std::string payload_llvm_type;
};

auto maybe_value_abi_for_source_type(
    std::string_view source_type_name,
    LoweringContext const& context
) -> std::optional<MaybeValueAbi>;

auto emit_empty_maybe_value(
    std::string_view source_type_name,
    LoweringContext const& context,
    std::size_t& next_temporary_index,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto emit_some_maybe_value(
    std::string_view source_type_name,
    LoweredExpression const& payload,
    LoweringContext const& context,
    std::size_t& next_temporary_index,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

}  // namespace orison::lowering
