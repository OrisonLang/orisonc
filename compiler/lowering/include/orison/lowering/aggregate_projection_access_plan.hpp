#pragma once

#include <string>

namespace orison::lowering {

enum class AggregateProjectionAccessIntent {
    value_read,
    explicit_transfer,
    shared_borrow,
    exclusive_borrow,
    clone_value,
};

enum class AggregateProjectionAccessStatus {
    not_named_aggregate_path,
    non_owned_projection,
    allowed,
    requires_explicit_boundary,
    boundary_not_enabled,
};

struct AggregateProjectionAccessPlan {
    AggregateProjectionAccessIntent intent = AggregateProjectionAccessIntent::value_read;
    AggregateProjectionAccessStatus status = AggregateProjectionAccessStatus::not_named_aggregate_path;
    std::string binding_name;
    std::string source_type_name;
    bool receiver_projection = false;
};

struct AggregateProjectionAccessPlanRecord {
    std::string function_symbol_name;
    AggregateProjectionAccessPlan plan;
};

}  // namespace orison::lowering
