#pragma once

#include "orison/lowering/computed_dynamic_array_cleanup_call.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_handoff.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::pipeline {

struct InsertedCleanupOperation {
    std::string kind_name;
    std::string operation_name;
    std::string source_owner_name;
    std::string target_owner_name;
    bool cleanup_calls_enabled = false;
};

struct ComputedCleanupCallOperands {
    std::string data_pointer_name;
    std::string capacity_name;
    std::string element_size_bytes;
    bool from_metadata = false;
};

struct InsertedCleanupStateAnalysis {
    std::vector<std::pair<InsertedCleanupOperation, InsertedCleanupOperation>> verified_pairs;
    std::vector<std::string> transition_report;
    std::vector<std::string> verification_report;
    bool from_metadata = false;
};

struct VerifiedComputedCleanupCall {
    InsertedCleanupOperation acquisition;
    InsertedCleanupOperation resumption;
    ComputedCleanupCallOperands operands;
    lowering::ComputedDynamicArrayCleanupCallOperands const* metadata = nullptr;
};

struct ComputedCleanupCallInsertionDecision {
    bool state_verified = false;
    bool operands_proven = false;
    bool cleanup_calls_authorized = false;
    bool insertion_ready = false;
};

struct ComputedCleanupProofModel {
    InsertedCleanupStateAnalysis inserted_cleanup_state;
    std::vector<VerifiedComputedCleanupCall> verified_cleanup_calls;
};

auto build_computed_cleanup_proof_model(
    std::string_view ir_text,
    std::vector<lowering::ComputedDynamicArrayCleanupStateHandoff> const& handoff_metadata,
    std::vector<lowering::ComputedDynamicArrayCleanupCallOperands> const& operand_metadata
) -> ComputedCleanupProofModel;

auto computed_cleanup_call_operands_complete(ComputedCleanupCallOperands const& operands) -> bool;

auto rendered_computed_cleanup_call_text(ComputedCleanupCallOperands const& operands) -> std::string;

auto computed_cleanup_call_insertion_decision(
    VerifiedComputedCleanupCall const& call
) -> ComputedCleanupCallInsertionDecision;

auto computed_cleanup_call_inserted_by_metadata(VerifiedComputedCleanupCall const& call) -> bool;

auto computed_cleanup_call_inserted_by_ir(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> bool;

auto computed_consumed_cleanup_descriptor_by_metadata(VerifiedComputedCleanupCall const& call) -> bool;

auto computed_consumed_cleanup_descriptor_by_ir(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> std::optional<std::string>;

}  // namespace orison::pipeline
