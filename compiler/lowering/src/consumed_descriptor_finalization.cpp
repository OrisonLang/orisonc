#include "orison/lowering/consumed_descriptor_finalization.hpp"

#include <sstream>

namespace orison::lowering {

auto plan_consumed_descriptor_finalization(
    std::string_view cleanup_owner_name,
    std::string_view descriptor_storage_name,
    std::string_view cleanup_operation_name
) -> ConsumedDescriptorFinalizationPlan {
    auto plan = ConsumedDescriptorFinalizationPlan {
        .cleanup_owner_name = std::string {cleanup_owner_name},
        .descriptor_storage_name = std::string {descriptor_storage_name},
        .cleanup_operation_name = std::string {cleanup_operation_name},
    };
    plan.cleanup_owner_consumed = !plan.cleanup_owner_name.empty() && !plan.cleanup_operation_name.empty();
    plan.descriptor_finalization_planned = plan.cleanup_owner_consumed && !plan.descriptor_storage_name.empty();
    return plan;
}

auto plan_consumed_descriptor_finalization_readiness(
    ConsumedDescriptorFinalizationPlan const& plan
) -> ConsumedDescriptorFinalizationReadiness {
    auto readiness = ConsumedDescriptorFinalizationReadiness {
        .cleanup_owner_consumed = plan.cleanup_owner_consumed,
        .descriptor_finalization_planned = plan.descriptor_finalization_planned,
    };
    readiness.ready = readiness.cleanup_owner_consumed && readiness.descriptor_finalization_planned;
    return readiness;
}

auto consumed_descriptor_finalization_plan_ready(
    ConsumedDescriptorFinalizationPlan const& plan
) -> bool {
    return plan_consumed_descriptor_finalization_readiness(plan).ready;
}

auto format_consumed_descriptor_finalization_plan(
    ConsumedDescriptorFinalizationPlan const& plan
) -> std::string {
    auto const readiness = plan_consumed_descriptor_finalization_readiness(plan);
    auto output = std::ostringstream {};
    output << "consumed descriptor finalization plan";
    if (!plan.function_symbol_name.empty()) {
        output << " function " << plan.function_symbol_name;
    }
    if (!plan.cleanup_owner_name.empty()) {
        output << " owner " << plan.cleanup_owner_name;
    }
    if (!plan.descriptor_storage_name.empty()) {
        output << " descriptor " << plan.descriptor_storage_name;
    }
    if (!plan.cleanup_operation_name.empty()) {
        output << " cleanup-operation " << plan.cleanup_operation_name;
    }
    output << (readiness.cleanup_owner_consumed ? " [cleanup owner consumed]" : " [cleanup owner unproven]");
    output << (readiness.descriptor_finalization_planned ? " [descriptor finalization planned]" :
        " [descriptor finalization blocked]");
    output << " (metadata only)";
    return output.str();
}

}  // namespace orison::lowering
