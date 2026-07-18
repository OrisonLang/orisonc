#include "orison/lowering/dynamic_array_runtime.hpp"

#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/target_layout.hpp"

#include <sstream>

namespace orison::lowering {
namespace {

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

}  // namespace

auto dynamic_array_runtime_call(
    DynamicArrayRuntimeOperation operation
) -> DynamicArrayRuntimeCall {
    switch (operation) {
    case DynamicArrayRuntimeOperation::allocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_allocate",
            .return_type = dynamic_array_descriptor_llvm_type(),
            .parameter_types = {"i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::grow:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_grow",
            .return_type = dynamic_array_descriptor_llvm_type(),
            .parameter_types = {dynamic_array_descriptor_llvm_type(), "i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::deallocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_deallocate",
            .return_type = "void",
            .parameter_types = {"ptr", "i64", "i64"},
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

auto format_dynamic_array_construction_plan(
    DynamicArrayConstructionPlan const& plan
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(plan.operation);
    auto output = std::ostringstream {};
    output << "dynamic array construction " << plan.source_type_name;
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

auto emit_dynamic_array_allocation_call(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(DynamicArrayRuntimeOperation::allocate);
    auto output = std::ostringstream {};
    output << "  " << result_name << " = call " << runtime_call.return_type;
    output << " @" << runtime_call.symbol_name;
    output << "(i64 " << plan.element_size_bytes;
    output << ", i64 " << plan.initial_capacity << ")\n";
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
