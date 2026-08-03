#pragma once

#include <string>

namespace orison::lowering {

enum class ComputedDynamicArrayCleanupStateHandoffKind {
    acquire,
    resume,
};

enum class ComputedDynamicArrayCleanupCallAuthorizationOrigin {
    none,
    production_local_cleanup_plan,
    explicit_test_seam,
};

struct ComputedDynamicArrayCleanupStateHandoff {
    ComputedDynamicArrayCleanupStateHandoffKind kind =
        ComputedDynamicArrayCleanupStateHandoffKind::acquire;
    std::string operation_name;
    std::string source_owner_name;
    std::string target_owner_name;
    bool cleanup_calls_enabled = false;
    ComputedDynamicArrayCleanupCallAuthorizationOrigin cleanup_call_authorization_origin =
        ComputedDynamicArrayCleanupCallAuthorizationOrigin::none;
    std::string cleanup_calls_blocked_reason;
};

}  // namespace orison::lowering
