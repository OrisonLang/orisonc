#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace orison::pipeline {

struct InsertedCleanupOperation;
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

auto format_inserted_cleanup_transition(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string;

auto format_inserted_cleanup_state_verification(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string;

auto format_inserted_cleanup_state_verification_blocked(
    std::string_view reason,
    InsertedCleanupOperation const& operation
) -> std::string;

}  // namespace orison::pipeline
