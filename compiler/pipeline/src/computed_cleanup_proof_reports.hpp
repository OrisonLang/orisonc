#pragma once

#include <string>
#include <vector>

namespace orison::pipeline {

struct ComputedCleanupProofModel;

struct ComputedCleanupProofReportBundle {
    std::vector<std::string> cleanup_call_emission_gate_report;
    std::vector<std::string> cleanup_call_plan_report;
    std::vector<std::string> cleanup_call_render_report;
    std::vector<std::string> cleanup_call_insertion_gate_report;
    std::vector<std::string> inserted_cleanup_call_report;
    std::vector<std::string> consumed_cleanup_descriptor_report;
};

auto build_computed_cleanup_proof_report_bundle(
    ComputedCleanupProofModel const& model
) -> ComputedCleanupProofReportBundle;

}  // namespace orison::pipeline
