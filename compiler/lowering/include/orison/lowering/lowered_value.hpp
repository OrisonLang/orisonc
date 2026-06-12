#pragma once

#include "orison/lowering/type_lowering.hpp"

#include <string>

namespace orison::lowering {

struct LoweredExpression {
    std::string type;
    std::string value;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

struct LoweredType {
    std::string type;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

}  // namespace orison::lowering
