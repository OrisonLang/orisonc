#pragma once

#include <cstddef>
#include <string>

namespace orison::lowering {

struct ComputedDynamicArrayCleanupCallOperands {
    std::string cleanup_operation_name;
    std::string data_pointer_name;
    std::size_t element_size_bytes = 0;
    std::string capacity_name;
    std::string descriptor_storage_name;
    bool cleanup_call_inserted = false;
    bool descriptor_finalized = false;
};

}  // namespace orison::lowering
