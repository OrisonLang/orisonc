#pragma once

#include <string>
#include <vector>

namespace orison::lowering {

struct DynamicArrayCleanupEmissionCapability {
    std::vector<std::string> cleanup_pairs;
    std::vector<std::string> cleanup_operation_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> element_drop_pairs;
    std::vector<std::string> missing_element_drop_pairs;
    bool emission_enabled = false;
    bool descriptor_storage_bound = false;
    bool sequence_verified = false;
    bool element_cleanup_authorized_or_not_required = false;
    bool descriptor_deallocation_authorized = false;
};

struct DynamicArrayCleanupEmissionCapabilityRecord {
    std::string function_symbol_name;
    DynamicArrayCleanupEmissionCapability capability;
};

}  // namespace orison::lowering
