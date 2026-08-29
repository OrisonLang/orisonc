#include "orison/lowering/dynamic_array_runtime.hpp"

#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/target_layout.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace orison::lowering {
namespace {

auto label_name(std::string_view name_prefix) -> std::string {
    if (!name_prefix.empty() && name_prefix.front() == '%') {
        return std::string {name_prefix.substr(1)};
    }
    return std::string {name_prefix};
}

auto has_reported_symbol(
    std::vector<std::string_view> const& reported_symbols,
    std::string_view symbol_name
) -> bool {
    for (auto reported_symbol : reported_symbols) {
        if (reported_symbol == symbol_name) {
            return true;
        }
    }
    return false;
}

auto descriptor_storage_name_for_owner(std::string_view owner_name) -> std::string {
    if (owner_name.empty()) {
        return {};
    }
    return llvm_local_value_name(std::string {owner_name} + ".addr");
}

auto dynamic_array_descriptor_sret_parameter() -> std::string {
    auto output = std::ostringstream {};
    output << "ptr sret(" << dynamic_array_descriptor_llvm_type() << ")";
    return output.str();
}

auto dynamic_array_descriptor_byval_parameter() -> std::string {
    auto output = std::ostringstream {};
    output << "ptr byval(" << dynamic_array_descriptor_llvm_type() << ")";
    return output.str();
}

auto format_dynamic_array_descriptor_storage_status(
    DynamicArrayDescriptorStorageStatus status
) -> std::string_view {
    switch (status) {
    case DynamicArrayDescriptorStorageStatus::predicted_owner_local:
        return "predicted";
    case DynamicArrayDescriptorStorageStatus::audit_parameter_descriptor:
        return "audit";
    case DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor:
        return "bound";
    case DynamicArrayDescriptorStorageStatus::lowered_local_descriptor:
        return "local";
    }
    return "unknown";
}

auto dynamic_array_descriptor_lifetime_cleanup_responsibility(
    semantics::DynamicArrayDescriptorBindingKind binding_kind
) -> std::string {
    switch (binding_kind) {
    case semantics::DynamicArrayDescriptorBindingKind::local_binding:
        return "caller-owned-local-cleanup";
    case semantics::DynamicArrayDescriptorBindingKind::parameter_binding:
        return "callee-owned-parameter-cleanup";
    case semantics::DynamicArrayDescriptorBindingKind::returned_binding:
        return "caller-owned-returned-cleanup";
    }
    return "unknown-cleanup";
}

auto is_single_generic_parameter_placeholder(std::string_view source_type_name) -> bool {
    return source_type_name.size() == 1 &&
        std::isupper(static_cast<unsigned char>(source_type_name.front())) != 0;
}

auto source_type_constructor_name(std::string_view source_type_name) -> std::string_view {
    auto const generic_start = source_type_name.find('<');
    if (generic_start == std::string_view::npos) {
        return source_type_name;
    }
    return source_type_name.substr(0, generic_start);
}

auto source_type_generic_argument_text(std::string_view source_type_name) -> std::optional<std::string_view> {
    auto const generic_start = source_type_name.find('<');
    if (generic_start == std::string_view::npos || !source_type_name.ends_with(">")) {
        return std::nullopt;
    }
    return source_type_name.substr(generic_start + 1, source_type_name.size() - generic_start - 2);
}

auto generic_source_type_pattern_matches(
    std::string_view origin_source_type_name,
    std::string_view lowered_source_type_name
) -> bool {
    if (origin_source_type_name == lowered_source_type_name ||
        is_single_generic_parameter_placeholder(origin_source_type_name)) {
        return true;
    }

    if (source_type_constructor_name(origin_source_type_name) != source_type_constructor_name(lowered_source_type_name)) {
        return false;
    }

    auto origin_arguments_text = source_type_generic_argument_text(origin_source_type_name);
    auto lowered_arguments_text = source_type_generic_argument_text(lowered_source_type_name);
    if (!origin_arguments_text.has_value() || !lowered_arguments_text.has_value()) {
        return false;
    }

    auto origin_arguments = split_top_level_generic_arguments(*origin_arguments_text);
    auto lowered_arguments = split_top_level_generic_arguments(*lowered_arguments_text);
    if (origin_arguments.size() != lowered_arguments.size()) {
        return false;
    }

    for (auto index = std::size_t {0}; index < origin_arguments.size(); ++index) {
        if (!generic_source_type_pattern_matches(origin_arguments[index], lowered_arguments[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace

auto dynamic_array_runtime_call(
    DynamicArrayRuntimeOperation operation
) -> DynamicArrayRuntimeCall {
    switch (operation) {
    case DynamicArrayRuntimeOperation::allocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_allocate",
            .return_type = "void",
            .parameter_types = {dynamic_array_descriptor_sret_parameter(), "i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::grow:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_grow",
            .return_type = "void",
            .parameter_types = {
                dynamic_array_descriptor_sret_parameter(),
                dynamic_array_descriptor_byval_parameter(),
                "i64",
                "i64",
            },
        };
    case DynamicArrayRuntimeOperation::deallocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_deallocate",
            .return_type = "void",
            .parameter_types = {"ptr", "i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::bounds_failed:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_bounds_failed",
            .return_type = "void",
            .parameter_types = {},
        };
    case DynamicArrayRuntimeOperation::capacity_failed:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_capacity_failed",
            .return_type = "void",
            .parameter_types = {},
        };
    }
    return {};
}

auto plan_dynamic_array_construction(
    std::string_view source_type_name,
    std::size_t initial_capacity,
    LoweringContext const& context,
    TargetLayout const& layout
) -> std::optional<DynamicArrayConstructionPlan> {
    auto sequence = dynamic_sequence_source_type(source_type_name);
    if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array) {
        return std::nullopt;
    }

    auto element_llvm_type = llvm_type_for_source_type_name(sequence->element_source_type_name, context);
    if (!element_llvm_type.has_value()) {
        return std::nullopt;
    }

    auto element_size = lowered_type_size_bytes(*element_llvm_type, context, layout);
    if (!element_size.has_value()) {
        return std::nullopt;
    }

    return DynamicArrayConstructionPlan {
        .source_type_name = std::string {source_type_name},
        .element_source_type_name = sequence->element_source_type_name,
        .element_llvm_type = *element_llvm_type,
        .element_size_bytes = *element_size,
        .initial_capacity = initial_capacity,
    };
}

auto plan_dynamic_array_descriptor_cleanup(
    std::string_view owner_name,
    std::string_view source_type_name,
    LoweringContext const& context,
    TargetLayout const& layout
) -> std::optional<DynamicArrayDescriptorCleanupPlan> {
    auto sequence = dynamic_sequence_source_type(source_type_name);
    if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array) {
        return std::nullopt;
    }

    auto element_llvm_type = llvm_type_for_source_type_name(sequence->element_source_type_name, context);
    if (!element_llvm_type.has_value()) {
        return std::nullopt;
    }

    auto element_size = lowered_type_size_bytes(*element_llvm_type, context, layout);
    if (!element_size.has_value()) {
        return std::nullopt;
    }

    return DynamicArrayDescriptorCleanupPlan {
        .owner_name = std::string {owner_name},
        .source_type_name = std::string {source_type_name},
        .element_source_type_name = sequence->element_source_type_name,
        .element_llvm_type = *element_llvm_type,
        .descriptor_storage_name = descriptor_storage_name_for_owner(owner_name),
        .element_size_bytes = *element_size,
    };
}

auto plan_dynamic_array_bound_parameter_lifetime(
    std::string_view parameter_name,
    std::string_view source_type_name,
    std::string_view descriptor_storage_name,
    bool drop_proof_available,
    LoweringContext const& context,
    TargetLayout const& layout
) -> std::optional<DynamicArrayBoundParameterLifetimePlan> {
    if (parameter_name.empty() || descriptor_storage_name.empty() || !drop_proof_available) {
        return std::nullopt;
    }

    auto descriptor_cleanup = plan_dynamic_array_descriptor_cleanup(
        parameter_name,
        source_type_name,
        context,
        layout
    );
    if (!descriptor_cleanup.has_value()) {
        return std::nullopt;
    }

    descriptor_cleanup->descriptor_storage_name = descriptor_storage_name;
    descriptor_cleanup->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor;

    return DynamicArrayBoundParameterLifetimePlan {
        .descriptor_cleanup = std::move(*descriptor_cleanup),
        .cleanup_responsibility = dynamic_array_descriptor_lifetime_cleanup_responsibility(
            semantics::DynamicArrayDescriptorBindingKind::parameter_binding
        ),
        .drop_proof_available = drop_proof_available,
    };
}

auto plan_dynamic_array_bound_parameter_lifetime(
    semantics::SemanticDynamicArrayDescriptorSummary const& descriptor,
    std::string_view descriptor_storage_name,
    bool drop_proof_available,
    LoweringContext const& context,
    TargetLayout const& layout
) -> std::optional<DynamicArrayBoundParameterLifetimePlan> {
    if (descriptor.binding_kind != semantics::DynamicArrayDescriptorBindingKind::parameter_binding) {
        return std::nullopt;
    }
    return plan_dynamic_array_bound_parameter_lifetime(
        descriptor.owner_name,
        descriptor.source_type_name,
        descriptor_storage_name,
        drop_proof_available,
        context,
        layout
    );
}

auto plan_dynamic_array_returned_descriptor_lifetime(
    std::string_view returned_owner_name,
    std::string_view returned_source_type_name,
    DynamicArrayDescriptorCleanupPlan const& candidate_cleanup
) -> std::optional<DynamicArrayReturnedDescriptorLifetimePlan> {
    if (returned_owner_name.empty() ||
        returned_source_type_name.empty() ||
        dynamic_array_element_source_type_name(returned_source_type_name) == std::nullopt) {
        return std::nullopt;
    }
    if (candidate_cleanup.owner_name != returned_owner_name ||
        candidate_cleanup.source_type_name != returned_source_type_name ||
        candidate_cleanup.descriptor_storage_status !=
            DynamicArrayDescriptorStorageStatus::lowered_local_descriptor) {
        return std::nullopt;
    }

    return DynamicArrayReturnedDescriptorLifetimePlan {
        .descriptor_cleanup = candidate_cleanup,
        .cleanup_responsibility = dynamic_array_descriptor_lifetime_cleanup_responsibility(
            semantics::DynamicArrayDescriptorBindingKind::returned_binding
        ),
        .caller_owns_returned_cleanup = true,
    };
}

auto plan_dynamic_array_returned_descriptor_lifetime(
    semantics::SemanticDynamicArrayDescriptorSummary const& descriptor,
    DynamicArrayDescriptorCleanupPlan const& candidate_cleanup
) -> std::optional<DynamicArrayReturnedDescriptorLifetimePlan> {
    if (descriptor.binding_kind != semantics::DynamicArrayDescriptorBindingKind::returned_binding) {
        return std::nullopt;
    }
    return plan_dynamic_array_returned_descriptor_lifetime(
        descriptor.owner_name,
        descriptor.source_type_name,
        candidate_cleanup
    );
}

auto plan_dynamic_array_descriptor_lifetime(
    semantics::SemanticDynamicArrayDescriptorSummary const& descriptor,
    DynamicArrayDescriptorCleanupPlan const* cleanup_plan
) -> DynamicArrayDescriptorLifetimePlan {
    auto plan = DynamicArrayDescriptorLifetimePlan {
        .owner_name = descriptor.owner_name,
        .source_type_name = descriptor.source_type_name,
        .element_source_type_name = descriptor.element_source_type_name,
        .binding_kind = descriptor.binding_kind,
        .cleanup_responsibility =
            dynamic_array_descriptor_lifetime_cleanup_responsibility(descriptor.binding_kind),
        .cleanup_plan_available = cleanup_plan != nullptr,
        .source_line = descriptor.line,
    };
    if (cleanup_plan != nullptr) {
        plan.descriptor_storage_status = cleanup_plan->descriptor_storage_status;
        plan.descriptor_storage_name = cleanup_plan->descriptor_storage_name;
        plan.cleanup_owner_name = cleanup_plan->owner_name;
    }
    return plan;
}

auto dynamic_array_descriptor_lifetime_source_type_matches(
    std::string_view origin_source_type_name,
    std::string_view lowered_source_type_name
) -> bool {
    if (origin_source_type_name == lowered_source_type_name) {
        return true;
    }

    auto origin_sequence = dynamic_sequence_source_type(origin_source_type_name);
    auto lowered_sequence = dynamic_sequence_source_type(lowered_source_type_name);
    return origin_sequence.has_value() &&
        lowered_sequence.has_value() &&
        origin_sequence->kind == DynamicSequenceKind::dynamic_array &&
        lowered_sequence->kind == DynamicSequenceKind::dynamic_array &&
        generic_source_type_pattern_matches(
            origin_sequence->element_source_type_name,
            lowered_sequence->element_source_type_name
        );
}

auto matching_dynamic_array_descriptor_lifetime_plan(
    std::vector<DynamicArrayDescriptorLifetimePlan> const& lifetime_plans,
    semantics::SemanticDynamicArrayDescriptorSummary const& descriptor
) -> DynamicArrayDescriptorLifetimePlan const* {
    auto const match = std::find_if(
        lifetime_plans.begin(),
        lifetime_plans.end(),
        [&](DynamicArrayDescriptorLifetimePlan const& lifetime_plan) {
            return lifetime_plan.owner_name == descriptor.owner_name &&
                dynamic_array_descriptor_lifetime_source_type_matches(
                    lifetime_plan.source_type_name,
                    descriptor.source_type_name
                ) &&
                generic_source_type_pattern_matches(
                    lifetime_plan.element_source_type_name,
                    descriptor.element_source_type_name
                ) &&
                lifetime_plan.binding_kind == descriptor.binding_kind;
        }
    );
    return match == lifetime_plans.end() ? nullptr : &*match;
}

auto matching_bound_dynamic_array_parameter_lifetime_plan(
    std::vector<DynamicArrayDescriptorLifetimePlan> const& lifetime_plans,
    std::string_view parameter_name,
    std::string_view source_type_name,
    std::string_view descriptor_storage_name
) -> DynamicArrayDescriptorLifetimePlan const* {
    auto const match = std::find_if(
        lifetime_plans.begin(),
        lifetime_plans.end(),
        [&](DynamicArrayDescriptorLifetimePlan const& lifetime_plan) {
            return lifetime_plan.binding_kind ==
                    semantics::DynamicArrayDescriptorBindingKind::parameter_binding &&
                lifetime_plan.cleanup_plan_available &&
                lifetime_plan.owner_name == parameter_name &&
                dynamic_array_descriptor_lifetime_source_type_matches(
                    lifetime_plan.source_type_name,
                    source_type_name
                ) &&
                lifetime_plan.descriptor_storage_status ==
                    DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor &&
                lifetime_plan.descriptor_storage_name == descriptor_storage_name &&
                lifetime_plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
        }
    );
    return match == lifetime_plans.end() ? nullptr : &*match;
}

auto matching_returned_dynamic_array_descriptor_lifetime_plan(
    std::vector<DynamicArrayDescriptorLifetimePlan> const& lifetime_plans,
    std::string_view returned_owner_name,
    DynamicArrayDescriptorCleanupPlan const& cleanup_plan
) -> DynamicArrayDescriptorLifetimePlan const* {
    auto const match = std::find_if(
        lifetime_plans.begin(),
        lifetime_plans.end(),
        [&](DynamicArrayDescriptorLifetimePlan const& lifetime_plan) {
            return lifetime_plan.binding_kind ==
                    semantics::DynamicArrayDescriptorBindingKind::returned_binding &&
                lifetime_plan.cleanup_plan_available &&
                lifetime_plan.owner_name == returned_owner_name &&
                lifetime_plan.owner_name == cleanup_plan.owner_name &&
                dynamic_array_descriptor_lifetime_source_type_matches(
                    lifetime_plan.source_type_name,
                    cleanup_plan.source_type_name
                ) &&
                generic_source_type_pattern_matches(
                    lifetime_plan.element_source_type_name,
                    cleanup_plan.element_source_type_name
                ) &&
                lifetime_plan.descriptor_storage_status == cleanup_plan.descriptor_storage_status &&
                lifetime_plan.descriptor_storage_name == cleanup_plan.descriptor_storage_name &&
                lifetime_plan.cleanup_owner_name == cleanup_plan.owner_name &&
                lifetime_plan.cleanup_responsibility == "caller-owned-returned-cleanup";
        }
    );
    return match == lifetime_plans.end() ? nullptr : &*match;
}

auto format_dynamic_array_construction_plan(
    DynamicArrayConstructionPlan const& plan
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(plan.operation);
    auto output = std::ostringstream {};
    output << "dynamic array construction " << plan.source_type_name;
    if (!plan.owner_name.empty()) {
        output << " owner " << plan.owner_name;
    }
    output << " element " << plan.element_source_type_name;
    output << " lowers to " << plan.element_llvm_type;
    output << " element_size " << plan.element_size_bytes;
    output << " initial_capacity " << plan.initial_capacity;
    output << " requests " << runtime_call.symbol_name;
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_construction_plan_report(
    std::vector<DynamicArrayConstructionPlan> const& plans
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(plans.size());
    for (auto const& plan : plans) {
        report.push_back(format_dynamic_array_construction_plan(plan));
    }
    return report;
}

auto format_dynamic_array_descriptor_cleanup_plan(
    DynamicArrayDescriptorCleanupPlan const& plan
) -> std::string {
    auto output = std::ostringstream {};
    output << "dynamic array descriptor cleanup " << plan.source_type_name;
    if (!plan.owner_name.empty()) {
        output << " owner " << plan.owner_name;
    }
    output << " element " << plan.element_source_type_name;
    output << " lowers to " << plan.element_llvm_type;
    if (!plan.descriptor_storage_name.empty()) {
        output << " descriptor " << plan.descriptor_storage_name;
        output << " " << format_dynamic_array_descriptor_storage_status(plan.descriptor_storage_status);
    }
    output << " element_size " << plan.element_size_bytes;
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_bound_parameter_lifetime_plan(
    DynamicArrayBoundParameterLifetimePlan const& plan
) -> std::string {
    auto output = std::ostringstream {};
    output << "dynamic array bound parameter lifetime "
           << plan.descriptor_cleanup.source_type_name;
    if (!plan.descriptor_cleanup.owner_name.empty()) {
        output << " owner " << plan.descriptor_cleanup.owner_name;
    }
    output << " element " << plan.descriptor_cleanup.element_source_type_name;
    output << " lowers to " << plan.descriptor_cleanup.element_llvm_type;
    if (!plan.descriptor_cleanup.descriptor_storage_name.empty()) {
        output << " descriptor " << plan.descriptor_cleanup.descriptor_storage_name;
        output << " "
               << format_dynamic_array_descriptor_storage_status(
                      plan.descriptor_cleanup.descriptor_storage_status
                  );
    }
    output << " cleanup " << plan.cleanup_responsibility;
    output << " drop-proof " << (plan.drop_proof_available ? "available" : "missing");
    output << " element_size " << plan.descriptor_cleanup.element_size_bytes;
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_returned_descriptor_lifetime_plan(
    DynamicArrayReturnedDescriptorLifetimePlan const& plan
) -> std::string {
    auto output = std::ostringstream {};
    output << "dynamic array returned descriptor lifetime "
           << plan.descriptor_cleanup.source_type_name;
    if (!plan.descriptor_cleanup.owner_name.empty()) {
        output << " owner " << plan.descriptor_cleanup.owner_name;
    }
    output << " element " << plan.descriptor_cleanup.element_source_type_name;
    output << " lowers to " << plan.descriptor_cleanup.element_llvm_type;
    if (!plan.descriptor_cleanup.descriptor_storage_name.empty()) {
        output << " descriptor " << plan.descriptor_cleanup.descriptor_storage_name;
        output << " "
               << format_dynamic_array_descriptor_storage_status(
                      plan.descriptor_cleanup.descriptor_storage_status
                  );
    }
    output << " cleanup " << plan.cleanup_responsibility;
    output << " caller-owned " << (plan.caller_owns_returned_cleanup ? "yes" : "no");
    output << " element_size " << plan.descriptor_cleanup.element_size_bytes;
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_descriptor_cleanup_plan_report(
    std::vector<DynamicArrayDescriptorCleanupPlan> const& plans
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(plans.size());
    for (auto const& plan : plans) {
        report.push_back(format_dynamic_array_descriptor_cleanup_plan(plan));
    }
    return report;
}

auto emit_dynamic_array_allocation_call(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(DynamicArrayRuntimeOperation::allocate);
    auto output = std::ostringstream {};
    output << "  " << result_name << ".addr = alloca " << dynamic_array_descriptor_llvm_type() << "\n";
    output << "  call " << runtime_call.return_type << " @" << runtime_call.symbol_name;
    output << "(ptr sret(" << dynamic_array_descriptor_llvm_type() << ") " << result_name << ".addr";
    output << ", i64 " << plan.element_size_bytes;
    output << ", i64 " << plan.initial_capacity << ")\n";
    output << "  " << result_name << " = load " << dynamic_array_descriptor_llvm_type();
    output << ", ptr " << result_name << ".addr\n";
    return output.str();
}

auto emit_dynamic_array_grow_call(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name,
    std::string_view descriptor_value_name,
    std::string_view next_capacity_name
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(DynamicArrayRuntimeOperation::grow);
    auto output = std::ostringstream {};
    output << "  " << result_name << ".input = alloca " << dynamic_array_descriptor_llvm_type() << "\n";
    output << "  store " << dynamic_array_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", ptr " << result_name << ".input\n";
    output << "  " << result_name << ".addr = alloca " << dynamic_array_descriptor_llvm_type() << "\n";
    output << "  call " << runtime_call.return_type << " @" << runtime_call.symbol_name;
    output << "(ptr sret(" << dynamic_array_descriptor_llvm_type() << ") " << result_name << ".addr";
    output << ", ptr byval(" << dynamic_array_descriptor_llvm_type() << ") " << result_name << ".input";
    output << ", i64 " << plan.element_size_bytes;
    output << ", i64 " << next_capacity_name << ")\n";
    output << "  " << result_name << " = load " << dynamic_array_descriptor_llvm_type();
    output << ", ptr " << result_name << ".addr\n";
    return output.str();
}

auto emit_dynamic_array_deallocation_call(
    DynamicArrayConstructionPlan const& plan,
    std::string_view data_pointer_name,
    std::string_view capacity_name
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(DynamicArrayRuntimeOperation::deallocate);
    auto output = std::ostringstream {};
    output << "  call " << runtime_call.return_type << " @" << runtime_call.symbol_name;
    output << "(ptr " << data_pointer_name;
    output << ", i64 " << plan.element_size_bytes;
    output << ", i64 " << capacity_name << ")\n";
    return output.str();
}

auto emit_dynamic_array_descriptor_binding(
    DynamicArrayConstructionPlan const& plan,
    std::string_view local_address_name,
    std::string_view descriptor_value_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << local_address_name << " = alloca " << dynamic_array_descriptor_llvm_type() << "\n";
    output << "  store " << dynamic_array_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", ptr " << local_address_name << "\n";
    (void)plan;
    return output.str();
}

auto dynamic_array_descriptor_field_index(
    DynamicArrayDescriptorField field
) -> std::size_t {
    switch (field) {
    case DynamicArrayDescriptorField::data:
        return 0;
    case DynamicArrayDescriptorField::length:
        return 1;
    case DynamicArrayDescriptorField::capacity:
        return 2;
    }
    return 0;
}

auto dynamic_array_descriptor_field_llvm_type(
    DynamicArrayDescriptorField field
) -> std::string_view {
    switch (field) {
    case DynamicArrayDescriptorField::data:
        return "ptr";
    case DynamicArrayDescriptorField::length:
    case DynamicArrayDescriptorField::capacity:
        return "i64";
    }
    return "";
}

auto emit_dynamic_array_descriptor_field_projection(
    std::string_view result_name,
    std::string_view descriptor_value_name,
    DynamicArrayDescriptorField field
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_name << " = extractvalue ";
    output << dynamic_array_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", " << dynamic_array_descriptor_field_index(field) << "\n";
    return output.str();
}

auto dynamic_array_bounds_check_predicate(
    DynamicArrayBoundsCheckKind kind
) -> std::string_view {
    switch (kind) {
    case DynamicArrayBoundsCheckKind::index_within_length:
    case DynamicArrayBoundsCheckKind::append_has_capacity:
        return "ult";
    case DynamicArrayBoundsCheckKind::length_within_capacity:
        return "ule";
    }
    return "";
}

auto emit_dynamic_array_bounds_check(
    std::string_view result_name,
    std::string_view left_value_name,
    std::string_view right_value_name,
    DynamicArrayBoundsCheckKind kind
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_name << " = icmp " << dynamic_array_bounds_check_predicate(kind);
    output << " i64 " << left_value_name << ", " << right_value_name << "\n";
    return output.str();
}

auto emit_dynamic_array_element_address(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name,
    std::string_view data_pointer_name,
    std::string_view index_value_name
) -> std::string {
    auto descriptor_plan = DynamicArrayDescriptorCleanupPlan {
        .source_type_name = plan.source_type_name,
        .element_source_type_name = plan.element_source_type_name,
        .element_llvm_type = plan.element_llvm_type,
        .element_size_bytes = plan.element_size_bytes,
    };
    return emit_dynamic_array_element_address(
        descriptor_plan,
        result_name,
        data_pointer_name,
        index_value_name
    );
}

auto emit_dynamic_array_element_address(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::string_view result_name,
    std::string_view data_pointer_name,
    std::string_view index_value_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_name << " = getelementptr " << plan.element_llvm_type;
    output << ", ptr " << data_pointer_name << ", i64 " << index_value_name << "\n";
    return output.str();
}

auto emit_dynamic_array_element_load(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name,
    std::string_view element_address_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_name << " = load " << plan.element_llvm_type;
    output << ", ptr " << element_address_name << "\n";
    return output.str();
}

auto emit_dynamic_array_element_store(
    DynamicArrayConstructionPlan const& plan,
    std::string_view value_name,
    std::string_view element_address_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  store " << plan.element_llvm_type << " " << value_name;
    output << ", ptr " << element_address_name << "\n";
    return output.str();
}

auto emit_dynamic_array_descriptor_length_update(
    std::string_view result_descriptor_name,
    std::string_view next_length_name,
    std::string_view descriptor_value_name,
    std::string_view current_length_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << next_length_name << " = add i64 " << current_length_name << ", 1\n";
    output << "  " << result_descriptor_name << " = insertvalue ";
    output << dynamic_array_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", i64 " << next_length_name;
    output << ", " << dynamic_array_descriptor_field_index(DynamicArrayDescriptorField::length) << "\n";
    return output.str();
}

auto emit_dynamic_array_descriptor_write_back(
    std::string_view descriptor_value_name,
    std::string_view local_address_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  store " << dynamic_array_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", ptr " << local_address_name << "\n";
    return output.str();
}

auto emit_dynamic_array_descriptor_finalization(
    std::string_view descriptor_storage_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  store " << dynamic_array_descriptor_llvm_type();
    output << " zeroinitializer, ptr " << descriptor_storage_name << "\n";
    return output.str();
}

auto emit_dynamic_array_append_sequence(
    DynamicArrayConstructionPlan const& plan,
    std::string_view descriptor_value_name,
    std::string_view local_address_name,
    std::string_view data_pointer_name,
    std::string_view length_name,
    std::string_view capacity_name,
    std::string_view value_name,
    std::string_view name_prefix
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    output << emit_dynamic_array_bounds_check(
        prefix + ".append.has_capacity",
        length_name,
        capacity_name,
        DynamicArrayBoundsCheckKind::append_has_capacity
    );
    output << emit_dynamic_array_element_address(
        plan,
        prefix + ".append.element.addr",
        data_pointer_name,
        length_name
    );
    output << emit_dynamic_array_element_store(
        plan,
        value_name,
        prefix + ".append.element.addr"
    );
    output << emit_dynamic_array_descriptor_length_update(
        prefix + ".append.updated",
        prefix + ".append.next.length",
        descriptor_value_name,
        length_name
    );
    output << emit_dynamic_array_descriptor_write_back(
        prefix + ".append.updated",
        local_address_name
    );
    return output.str();
}

auto emit_dynamic_array_grow_sequence(
    DynamicArrayConstructionPlan const& plan,
    std::string_view descriptor_value_name,
    std::string_view local_address_name,
    std::string_view capacity_name,
    std::string_view name_prefix
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    output << "  " << prefix << ".grow.next.capacity = mul i64 " << capacity_name << ", 2\n";
    output << emit_dynamic_array_grow_call(
        plan,
        prefix + ".grown",
        descriptor_value_name,
        prefix + ".grow.next.capacity"
    );
    output << emit_dynamic_array_descriptor_write_back(
        prefix + ".grown",
        local_address_name
    );
    return output.str();
}

auto emit_dynamic_array_append_with_grow_sequence(
    DynamicArrayConstructionPlan const& plan,
    std::string_view descriptor_value_name,
    std::string_view local_address_name,
    std::string_view data_pointer_name,
    std::string_view length_name,
    std::string_view capacity_name,
    std::string_view value_name,
    std::string_view name_prefix
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    auto label_prefix = label_name(name_prefix);
    output << label_prefix << ".append.entry:\n";
    output << emit_dynamic_array_bounds_check(
        prefix + ".append.has_capacity",
        length_name,
        capacity_name,
        DynamicArrayBoundsCheckKind::append_has_capacity
    );
    output << "  br i1 " << prefix << ".append.has_capacity";
    output << ", label %" << label_prefix << ".append.ready";
    output << ", label %" << label_prefix << ".grow\n";
    output << label_prefix << ".grow:\n";
    output << emit_dynamic_array_grow_sequence(
        plan,
        descriptor_value_name,
        local_address_name,
        capacity_name,
        name_prefix
    );
    output << "  br label %" << label_prefix << ".append.ready\n";
    output << label_prefix << ".append.ready:\n";
    output << "  " << prefix << ".active = phi " << dynamic_array_descriptor_llvm_type();
    output << " [ " << descriptor_value_name << ", %" << label_prefix << ".append.entry ],";
    output << " [ " << prefix << ".grown, %" << label_prefix << ".grow ]\n";
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".active.data",
        prefix + ".active",
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".active.length",
        prefix + ".active",
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_element_address(
        plan,
        prefix + ".active.append.element.addr",
        prefix + ".active.data",
        prefix + ".active.length"
    );
    output << emit_dynamic_array_element_store(
        plan,
        value_name,
        prefix + ".active.append.element.addr"
    );
    output << emit_dynamic_array_descriptor_length_update(
        prefix + ".active.append.updated",
        prefix + ".active.append.next.length",
        prefix + ".active",
        prefix + ".active.length"
    );
    output << emit_dynamic_array_descriptor_write_back(
        prefix + ".active.append.updated",
        local_address_name
    );
    (void)data_pointer_name;
    return output.str();
}

auto emit_dynamic_array_cleanup_sequence(
    DynamicArrayConstructionPlan const& plan,
    std::string_view descriptor_value_name,
    std::string_view name_prefix
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.data",
        descriptor_value_name,
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.length",
        descriptor_value_name,
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.capacity",
        descriptor_value_name,
        DynamicArrayDescriptorField::capacity
    );
    output << emit_dynamic_array_element_drop_walk(
        plan,
        prefix + ".cleanup.data",
        prefix + ".cleanup.length",
        prefix
    );
    output << emit_dynamic_array_deallocation_call(
        plan,
        prefix + ".cleanup.data",
        prefix + ".cleanup.capacity"
    );
    return output.str();
}

auto emit_dynamic_array_descriptor_load(
    std::string_view result_descriptor_name,
    std::string_view descriptor_storage_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_descriptor_name << " = load ";
    output << dynamic_array_descriptor_llvm_type() << ", ptr " << descriptor_storage_name << "\n";
    return output.str();
}

auto emit_dynamic_array_descriptor_cleanup_sequence(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::string_view descriptor_value_name,
    std::string_view name_prefix
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.data",
        descriptor_value_name,
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.length",
        descriptor_value_name,
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".cleanup.capacity",
        descriptor_value_name,
        DynamicArrayDescriptorField::capacity
    );
    if (!is_scalar_or_nonowning_source_type(plan.element_source_type_name)) {
        output << emit_dynamic_array_element_drop_walk(
            plan,
            prefix + ".cleanup.data",
            prefix + ".cleanup.length",
            prefix
        );
    }
    output << "  call void @__orison_dynamic_array_deallocate(ptr ";
    output << prefix << ".cleanup.data";
    output << ", i64 " << plan.element_size_bytes;
    output << ", i64 " << prefix << ".cleanup.capacity)\n";
    return output.str();
}

auto emit_dynamic_array_descriptor_load_cleanup_sequence(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::string_view descriptor_value_name,
    std::string_view name_prefix
) -> std::string {
    auto output = std::ostringstream {};
    output << emit_dynamic_array_descriptor_load(
        descriptor_value_name,
        plan.descriptor_storage_name
    );
    output << emit_dynamic_array_descriptor_cleanup_sequence(
        plan,
        descriptor_value_name,
        name_prefix
    );
    return output.str();
}

auto emit_dynamic_array_element_drop_walk(
    DynamicArrayConstructionPlan const& plan,
    std::string_view data_pointer_name,
    std::string_view length_name,
    std::string_view name_prefix
) -> std::string {
    auto descriptor_plan = DynamicArrayDescriptorCleanupPlan {
        .source_type_name = plan.source_type_name,
        .element_source_type_name = plan.element_source_type_name,
        .element_llvm_type = plan.element_llvm_type,
        .element_size_bytes = plan.element_size_bytes,
    };
    return emit_dynamic_array_element_drop_walk(
        descriptor_plan,
        data_pointer_name,
        length_name,
        name_prefix
    );
}

auto emit_dynamic_array_element_drop_walk(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::string_view data_pointer_name,
    std::string_view length_name,
    std::string_view name_prefix
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    auto label_prefix = label_name(name_prefix);
    output << "  br label %" << label_prefix << ".drop.walk\n";
    output << label_prefix << ".drop.walk:\n";
    output << "  " << prefix << ".drop.index = phi i64 [ 0, %" << label_prefix << ".cleanup.entry ],";
    output << " [ " << prefix << ".drop.next, %" << label_prefix << ".drop.body ]\n";
    output << "  " << prefix << ".drop.more = icmp ult i64 " << prefix << ".drop.index";
    output << ", " << length_name << "\n";
    output << "  br i1 " << prefix << ".drop.more";
    output << ", label %" << label_prefix << ".drop.body";
    output << ", label %" << label_prefix << ".drop.done\n";
    output << label_prefix << ".drop.body:\n";
    output << emit_dynamic_array_element_address(
        plan,
        prefix + ".drop.element.addr",
        data_pointer_name,
        prefix + ".drop.index"
    );
    output << "  ; planned drop for " << plan.element_source_type_name;
    output << " at " << prefix << ".drop.element.addr remains disabled\n";
    output << "  " << prefix << ".drop.next = add i64 " << prefix << ".drop.index, 1\n";
    output << "  br label %" << label_prefix << ".drop.walk\n";
    output << label_prefix << ".drop.done:\n";
    return output.str();
}

auto format_dynamic_array_runtime_request(
    DynamicArrayRuntimeOperation operation
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(operation);
    auto output = std::ostringstream {};
    output << "dynamic array runtime " << runtime_call.symbol_name;
    output << " returns " << runtime_call.return_type;
    output << " params";
    for (auto parameter_type : runtime_call.parameter_types) {
        output << " " << parameter_type;
    }
    return output.str();
}

auto format_dynamic_array_runtime_request_report(
    std::vector<DynamicArrayRuntimeOperation> const& operations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    auto reported_symbols = std::vector<std::string_view> {};
    for (auto operation : operations) {
        auto runtime_call = dynamic_array_runtime_call(operation);
        if (has_reported_symbol(reported_symbols, runtime_call.symbol_name)) {
            continue;
        }
        report.push_back(format_dynamic_array_runtime_request(operation));
        reported_symbols.push_back(runtime_call.symbol_name);
    }
    return report;
}

}  // namespace orison::lowering
