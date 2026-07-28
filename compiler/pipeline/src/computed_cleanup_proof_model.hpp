#pragma once

#include "orison/lowering/computed_dynamic_array_cleanup_call.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_handoff.hpp"

#include <cstddef>
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

struct ComputedCleanupCallInsertionDecision {
    bool state_verified = false;
    bool operands_proven = false;
    bool cleanup_calls_authorized = false;
    bool insertion_ready = false;
};

struct ComputedInsertedCleanupCallDecision {
    bool operands_proven = false;
    bool proven_by_metadata = false;
    bool proven_by_ir = false;
    bool inserted = false;
};

struct ComputedConsumedCleanupDescriptorDecision {
    bool operands_proven = false;
    bool finalized_by_metadata = false;
    bool finalized_by_ir = false;
    bool finalized = false;
    std::optional<std::string> descriptor_storage_name;
};

struct VerifiedComputedCleanupCall {
    InsertedCleanupOperation acquisition;
    InsertedCleanupOperation resumption;
    ComputedCleanupCallOperands operands;
    lowering::ComputedDynamicArrayCleanupCallOperands const* metadata = nullptr;
    ComputedCleanupCallInsertionDecision insertion_decision;
    ComputedInsertedCleanupCallDecision inserted_call_decision;
    ComputedConsumedCleanupDescriptorDecision consumed_descriptor_decision;
};

struct ComputedCleanupProofSummary {
    std::size_t cleanup_proof_model_count = 0;
    std::size_t verified_inserted_cleanup_pair_count = 0;
    std::size_t structured_inserted_cleanup_handoff_count = 0;
    std::size_t structured_inserted_cleanup_handoff_use_count = 0;
    std::size_t ir_inserted_cleanup_handoff_fallback_count = 0;
    std::size_t structured_cleanup_operand_count = 0;
    std::size_t structured_cleanup_operand_use_count = 0;
    std::size_t ir_cleanup_operand_fallback_count = 0;
    std::size_t structured_inserted_cleanup_call_count = 0;
    std::size_t structured_consumed_cleanup_descriptor_count = 0;
    std::size_t ir_inserted_cleanup_call_fallback_count = 0;
    std::size_t ir_consumed_cleanup_descriptor_fallback_count = 0;
};

struct ComputedCleanupProofModel {
    InsertedCleanupStateAnalysis inserted_cleanup_state;
    std::vector<VerifiedComputedCleanupCall> verified_cleanup_calls;
    ComputedCleanupProofSummary summary;
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

auto computed_inserted_cleanup_call_decision(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> ComputedInsertedCleanupCallDecision;

auto computed_consumed_cleanup_descriptor_decision(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> ComputedConsumedCleanupDescriptorDecision;

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
