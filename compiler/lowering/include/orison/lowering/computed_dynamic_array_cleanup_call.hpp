#pragma once

#include <cstddef>
#include <string>

namespace orison::lowering {

struct ComputedDynamicArrayCleanupCallOperands {
    std::string cleanup_operation_name;
    std::string data_pointer_name;
    std::size_t element_size_bytes = 0;
    std::string capacity_name;
};

}  // namespace orison::lowering
