#pragma once

#include <string>

namespace orison::lowering {

enum class ComputedDynamicArrayCleanupStateHandoffKind {
    acquire,
    resume,
};

struct ComputedDynamicArrayCleanupStateHandoff {
    ComputedDynamicArrayCleanupStateHandoffKind kind =
        ComputedDynamicArrayCleanupStateHandoffKind::acquire;
    std::string operation_name;
    std::string source_owner_name;
    std::string target_owner_name;
    bool cleanup_calls_enabled = false;
    std::string cleanup_calls_blocked_reason;
};

}  // namespace orison::lowering
