#pragma once

#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/lowered_value.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    std::unordered_map<std::string, ConcurrencyBinding> thread_bindings;
    std::unordered_map<std::string, ConcurrencyBinding> task_bindings;
    std::vector<std::string> thread_binding_order;
    std::vector<std::string> task_binding_order;
    std::unordered_map<std::string, std::string> source_type_names;
    std::vector<std::string> parameter_names;
    std::vector<DynamicArrayDescriptorCleanupPlan> dynamic_array_local_cleanup_plans;
    std::unordered_set<std::string> consumed_owned_dynamic_array_locals;
    std::unordered_map<std::string, std::size_t> local_name_counts;
    std::vector<DeferredCleanupScopeState> defer_cleanup_scopes;
    std::vector<std::string> pending_function_definitions;
    std::size_t next_temporary_index = 0;
    std::size_t next_block_index = 0;
    std::size_t next_concurrency_ordinal = 0;
    std::string current_block = "entry";
    std::vector<LoopTargets> loop_targets;
};

}  // namespace orison::lowering
