#pragma once

#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"

#include <optional>
#include <string>
#include <vector>

namespace orison::lowering {

struct DynamicArrayCleanupObligation {
    std::string cleanup_symbol_name;
    DynamicArrayDescriptorCleanupPlan descriptor_cleanup;
    std::vector<PlannedDropAction> actions;
    bool requires_descriptor_deallocation = true;
};

struct DynamicArrayCleanupSequencePlan {
    DynamicArrayCleanupObligation obligation;
    std::vector<std::string> phases;
};

struct DynamicArrayCleanupSequenceVerification {
    std::string cleanup_symbol_name;
    std::vector<std::string> errors;
};

struct BoundDynamicArrayParameterCleanupPlan {
    DynamicArrayDescriptorCleanupPlan descriptor_cleanup;
    std::optional<std::string> element_drop_symbol_name;
    DynamicArrayCleanupSequencePlan sequence_plan;
    DynamicArrayCleanupSequenceVerification sequence_verification;
};

struct LocalDynamicArrayCleanupPlan {
    DynamicArrayDescriptorCleanupPlan descriptor_cleanup;
    std::optional<std::string> element_drop_symbol_name;
    DynamicArrayCleanupSequencePlan sequence_plan;
    DynamicArrayCleanupSequenceVerification sequence_verification;
};

struct DynamicArrayCleanupObligationRecord {
    std::string function_symbol_name;
    DynamicArrayCleanupObligation obligation;
};

struct DynamicArrayCleanupSequencePlanRecord {
    std::string function_symbol_name;
    DynamicArrayCleanupSequencePlan plan;
};

struct DynamicArrayCleanupSequenceVerificationRecord {
    std::string function_symbol_name;
    DynamicArrayCleanupSequenceVerification verification;
};

}  // namespace orison::lowering
