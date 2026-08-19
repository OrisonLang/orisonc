#pragma once

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_metadata.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct DynamicArrayCleanupAvailability {
    std::vector<std::string> missing_element_drop_pairs;
    bool descriptor_origins_available = false;
    bool descriptor_origin_blockers_absent = false;
    bool descriptor_cleanup_plans_available = false;
    bool cleanup_obligations_available = false;
    bool sequence_verification_available = false;
    bool sequence_verification_passed = false;
    bool cleanup_capability_proven = false;
};

struct DynamicArrayCleanupEmissionCapabilityState {
    std::vector<std::string> function_symbol_names;
    std::vector<std::string> cleanup_pairs;
    std::vector<std::string> cleanup_operation_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> element_drop_pairs;
    std::vector<std::string> missing_element_drop_pairs;
    bool capability_metadata_available = false;
    bool proven = false;
    bool emission_enabled = false;
    bool descriptor_storage_bound = false;
    bool sequence_verified = false;
    bool element_cleanup_authorized_or_not_required = false;
    bool descriptor_deallocation_authorized = false;
};

struct DynamicArrayDescriptorCleanupPlanState {
    std::vector<lowering::DynamicArrayDescriptorCleanupPlan> plans;
};

struct DynamicArrayDescriptorLifetimePlan {
    std::string owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    semantics::DynamicArrayDescriptorOriginKind origin_kind =
        semantics::DynamicArrayDescriptorOriginKind::local_binding;
    lowering::DynamicArrayDescriptorStorageStatus descriptor_storage_status =
        lowering::DynamicArrayDescriptorStorageStatus::predicted_owner_local;
    std::string descriptor_storage_name;
    std::string cleanup_responsibility;
    bool cleanup_plan_available = false;
    std::size_t source_line = 0;
};

struct DynamicArrayDescriptorOriginBlocker {
    std::string owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    semantics::DynamicArrayDescriptorOriginKind origin_kind =
        semantics::DynamicArrayDescriptorOriginKind::local_binding;
    std::string reason;
    std::size_t source_line = 0;
};

struct DynamicArrayDescriptorLifetimePlanState {
    std::vector<DynamicArrayDescriptorLifetimePlan> plans;
    std::vector<DynamicArrayDescriptorOriginBlocker> origin_blockers;
    bool all_origins_have_cleanup_plans = false;
    bool all_cleanup_plans_have_origins = false;
};

struct DynamicArrayConstructionPlanState {
    std::vector<lowering::DynamicArrayConstructionPlan> plans;
};

struct DynamicArrayRuntimeRequestState {
    std::vector<lowering::DynamicArrayRuntimeOperation> operations;
};

struct DynamicArrayAllocationCallIrArtifactState {
    std::vector<std::string> rendered_ir_snippets;
};

struct DynamicArrayAllocationCallEmissionState {
    DynamicArrayAllocationCallIrArtifactState ir_artifact_state;
    bool allocation_calls_rendered = false;
    std::size_t rendered_call_count = 0;
};

struct PlannedDropDeclarationState {
    std::vector<lowering::PlannedDropDeclaration> declarations;
};

struct PlannedDropActionState {
    std::vector<lowering::PlannedDropAction> actions;
};

struct DropCleanupAuthorizationState {
    std::vector<lowering::ConcurrencyDropCleanupPlan> cleanups;
    std::vector<lowering::DropCleanupAuthorizationReport> authorizations;
};

struct SemanticDropImplementationDiscovery {
    semantics::DropImplementation implementation;
    std::string discovery_name;
};

struct SemanticDropState {
    std::vector<SemanticDropImplementationDiscovery> discovered_implementations;
    std::vector<semantics::DropImplementationResolutionSummary> resolution_summaries;
};

struct DynamicArrayCleanupObligationState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::DynamicArrayCleanupObligation> obligations;
};

struct DynamicArrayCleanupSequencePlanState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::DynamicArrayCleanupSequencePlan> plans;
};

struct DynamicArrayCleanupSequenceVerificationState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::DynamicArrayCleanupSequenceVerification> verifications;
};

struct DynamicArrayCleanupProductionReadiness {
    std::vector<std::string> missing_element_drop_pairs;
    bool descriptor_origins_available = false;
    bool descriptor_origin_blockers_absent = false;
    bool descriptor_cleanup_plans_available = false;
    bool cleanup_obligations_available = false;
    bool sequence_verification_available = false;
    bool sequence_verification_passed = false;
    bool cleanup_capability_proven = false;
    bool production_signature_lowering_enabled = false;
    bool production_construction_lowering_enabled = false;
    bool production_cleanup_emission_enabled = false;
};

}  // namespace orison::pipeline
