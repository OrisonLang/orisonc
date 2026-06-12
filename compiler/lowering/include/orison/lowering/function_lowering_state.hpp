#pragma once

#include "orison/lowering/type_lowering.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace orison::lowering {

struct LoweredExpression {
    std::string type;
    std::string value;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

struct FunctionLoweringState {
    std::unordered_map<std::string, LoweredExpression> immutable_bindings;
    std::unordered_map<std::string, std::size_t> local_name_counts;
    std::size_t next_temporary_index = 0;
    std::size_t next_block_index = 0;
    std::string current_block = "entry";
};

}  // namespace orison::lowering
