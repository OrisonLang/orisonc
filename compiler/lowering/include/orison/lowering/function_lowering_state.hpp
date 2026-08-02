#pragma once

#include "orison/lowering/aggregate_projection_access_plan.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_call.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_handoff.hpp"
#include "orison/lowering/consumed_descriptor_finalization.hpp"
#include "orison/lowering/dynamic_array_cleanup_capability.hpp"
#include "orison/lowering/dynamic_array_cleanup_metadata.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/ownership_transfer.hpp"

#include <cstddef>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace orison::syntax {
struct StatementSyntax;
}

namespace orison::lowering {

struct MutableBinding {
    LoweredType type;
    std::string storage;
};

struct AddressableBinding {
    LoweredType type;
    std::string storage;
};

struct ConcurrencyBinding {
    std::string handle;
    std::string result_storage;
    LoweredType result_type;
    bool handle_destroyed = false;
};

struct DeferredCleanupBlock {
    std::size_t line = 0;
    std::vector<syntax::StatementSyntax const*> statements;
};

struct DeferredCleanupScopeState {
    std::vector<DeferredCleanupBlock> blocks;
};

struct LoopTargets {
    std::string break_target;
    std::string continue_target;
    std::size_t defer_cleanup_depth = 0;
};

struct FunctionLoweringState {
    std::unordered_map<std::string, LoweredExpression> immutable_bindings;
    std::unordered_map<std::string, MutableBinding> mutable_bindings;
    std::unordered_map<std::string, AddressableBinding> addressable_bindings;
    std::unordered_set<std::string> exclusive_receiver_bindings;
    std::unordered_map<std::string, ConcurrencyBinding> thread_bindings;
    std::unordered_map<std::string, ConcurrencyBinding> task_bindings;
    std::vector<std::string> thread_binding_order;
    std::vector<std::string> task_binding_order;
    std::unordered_map<std::string, std::string> source_type_names;
    std::vector<std::string> parameter_names;
    std::vector<DynamicArrayDescriptorCleanupPlan> dynamic_array_local_cleanup_plans;
    std::vector<DynamicArrayDescriptorCleanupPlan> dynamic_array_iterable_cleanup_owner_plans;
    std::vector<DynamicArrayCleanupObligation> emitted_dynamic_array_cleanup_obligations;
    std::vector<DynamicArrayCleanupSequencePlan> emitted_dynamic_array_cleanup_sequence_plans;
    std::vector<DynamicArrayCleanupSequenceVerification> emitted_dynamic_array_cleanup_sequence_verifications;
    std::vector<DynamicArrayCleanupEmissionCapability> emitted_dynamic_array_cleanup_emission_capabilities;
    std::vector<AggregateProjectionAccessPlan> aggregate_projection_access_plans;
    std::vector<ComputedDynamicArrayCleanupStateHandoff> computed_dynamic_array_inserted_cleanup_handoffs;
    std::vector<ComputedDynamicArrayCleanupCallOperands> computed_dynamic_array_cleanup_call_operands;
    std::vector<ConsumedDescriptorFinalizationPlan> consumed_descriptor_finalization_plans;
    OwnershipTransferState ownership_transfers;
    std::vector<syntax::StatementSyntax const*> sibling_statements_after_current;
    std::vector<syntax::StatementSyntax const*> function_statements_after_current;
    std::unordered_map<std::string, std::size_t> local_name_counts;
    std::vector<DeferredCleanupScopeState> defer_cleanup_scopes;
    std::vector<std::string> pending_function_definitions;
    std::size_t next_temporary_index = 0;
    std::size_t next_block_index = 0;
    std::size_t next_concurrency_ordinal = 0;
    std::string computed_dynamic_array_for_unique_suffix;
    std::string current_block = "entry";
    std::vector<LoopTargets> loop_targets;
};

}  // namespace orison::lowering
