#pragma once

#include <string>
#include <string_view>

namespace orison::lowering {

struct ConsumedDescriptorFinalizationPlan {
    std::string function_symbol_name;
    std::string cleanup_owner_name;
    std::string descriptor_storage_name;
    std::string cleanup_operation_name;
    bool cleanup_owner_consumed = false;
    bool descriptor_finalization_planned = false;
};

struct ConsumedDescriptorFinalizationReadiness {
    bool cleanup_owner_consumed = false;
    bool descriptor_finalization_planned = false;
    bool ready = false;
};

auto plan_consumed_descriptor_finalization(
    std::string_view cleanup_owner_name,
    std::string_view descriptor_storage_name,
    std::string_view cleanup_operation_name
) -> ConsumedDescriptorFinalizationPlan;

auto plan_consumed_descriptor_finalization_readiness(
    ConsumedDescriptorFinalizationPlan const& plan
) -> ConsumedDescriptorFinalizationReadiness;

auto consumed_descriptor_finalization_plan_ready(
    ConsumedDescriptorFinalizationPlan const& plan
) -> bool;

auto format_consumed_descriptor_finalization_plan(
    ConsumedDescriptorFinalizationPlan const& plan
) -> std::string;

}  // namespace orison::lowering
