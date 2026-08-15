#pragma once

#include "orison/pipeline/compile_pipeline_result.hpp"

#include <string>
#include <vector>

namespace orison::pipeline {

auto runtime_indexed_member_cleanup_execution_summaries(
    CompilePipelineResult const& result
) -> std::vector<RuntimeIndexedMemberCleanupExecutionSummary>;

auto runtime_indexed_member_cleanup_execution_summary_report(
    RuntimeIndexedMemberCleanupExecutionSummary const& summary
) -> std::string;

auto runtime_indexed_member_cleanup_execution_summary_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string>;

}  // namespace orison::pipeline
