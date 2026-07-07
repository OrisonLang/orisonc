#pragma once

#include "orison/lowering/concurrency_plan.hpp"

#include <string>
#include <vector>

namespace orison::pipeline {

auto format_drop_readiness_source_correlation_report(
    lowering::DropReadinessSnapshot const& snapshot
) -> std::vector<std::string>;

}  // namespace orison::pipeline
