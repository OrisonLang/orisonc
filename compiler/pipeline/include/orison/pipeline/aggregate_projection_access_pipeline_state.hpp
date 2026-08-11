#pragma once

#include "orison/lowering/aggregate_projection_access_plan.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct AggregateProjectionAccessPlanState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::AggregateProjectionAccessIntent> intents;
    std::vector<lowering::AggregateProjectionAccessStatus> statuses;
    std::vector<std::string> binding_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> diagnostics;
    std::vector<bool> receiver_projections;
    bool access_plans_available = false;
    std::size_t plan_count = 0;
    std::size_t allowed_count = 0;
    std::size_t blocked_count = 0;
    std::size_t receiver_projection_count = 0;
};

}  // namespace orison::pipeline
