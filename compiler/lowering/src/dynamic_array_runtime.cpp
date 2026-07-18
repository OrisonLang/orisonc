#include "orison/lowering/dynamic_array_runtime.hpp"

#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/target_layout.hpp"

#include <sstream>

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

auto format_dynamic_array_descriptor_storage_status(
    DynamicArrayDescriptorStorageStatus status
) -> std::string_view {
    switch (status) {
    case DynamicArrayDescriptorStorageStatus::predicted_owner_local:
        return "predicted";
    }
    return "unknown";
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
    output << "  " << result_name << " = call " << runtime_call.return_type;
    output << " @" << runtime_call.symbol_name;
    output << "(i64 " << plan.element_size_bytes;
    output << ", i64 " << plan.initial_capacity << ")\n";
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
    output << "  " << result_name << " = call " << runtime_call.return_type;
    output << " @" << runtime_call.symbol_name;
    output << "(" << dynamic_array_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", i64 " << plan.element_size_bytes;
    output << ", i64 " << next_capacity_name << ")\n";
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
