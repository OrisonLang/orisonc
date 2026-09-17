#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include <cassert>

auto main() -> int {
    auto report = orison::pipeline::format_drop_readiness_source_correlation_report({});
    assert(report.size() == 1);
    assert(report.front() == "drop readiness source correlations actions 0 semantic sites 0");
    return 0;
}
