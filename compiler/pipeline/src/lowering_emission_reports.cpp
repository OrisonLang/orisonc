#include "lowering_emission_reports.hpp"

#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include "dynamic_array_cleanup_readiness.hpp"

#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace orison::pipeline {

namespace {

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
};

auto trim(std::string_view value) -> std::string_view {
    while (!value.empty() && value.front() == ' ') {
        value.remove_prefix(1);
    }
    while (!value.empty() && value.back() == ' ') {
        value.remove_suffix(1);
    }
    return value;
}

auto operation_prefix_for_cleanup_resume(std::string_view operation_name) -> std::string {
    auto const suffix = std::string_view {".cleanup.resume"};
    if (operation_name.ends_with(suffix)) {
        return std::string {operation_name.substr(0, operation_name.size() - suffix.size())};
    }
    return std::string {operation_name};
}

auto scalar_llvm_type_size_bytes(std::string_view llvm_type) -> std::optional<std::size_t> {
    if (llvm_type == "i1" || llvm_type == "i8") {
        return 1;
    }
    if (llvm_type == "i16") {
        return 2;
    }
    if (llvm_type == "i32" || llvm_type == "float") {
        return 4;
    }
    if (llvm_type == "i64" || llvm_type == "double" || llvm_type == "ptr") {
        return 8;
    }
    return std::nullopt;
}

auto collect_computed_cleanup_call_operands(
    std::string_view ir_text,
    InsertedCleanupOperation const& resumption
) -> ComputedCleanupCallOperands {
    auto operands = ComputedCleanupCallOperands {};
    auto const operation_prefix = operation_prefix_for_cleanup_resume(resumption.operation_name);
    auto const data_prefix = "  %" + operation_prefix + ".data = extractvalue ";
    auto const capacity_prefix = "  %" + operation_prefix + ".capacity = extractvalue ";
    auto const cleanup_capacity_prefix = "  %" + operation_prefix + ".cleanup.capacity = extractvalue ";
    auto const item_load_prefix = "  %" + operation_prefix + ".item = load ";
    auto input = std::istringstream {std::string {ir_text}};
    auto line = std::string {};
    while (std::getline(input, line)) {
        if (operands.data_pointer_name.empty() && line.starts_with(data_prefix) &&
            line.ends_with(", 0")) {
            operands.data_pointer_name = "%" + operation_prefix + ".data";
            continue;
        }
        if (operands.capacity_name.empty() &&
            (line.starts_with(capacity_prefix) || line.starts_with(cleanup_capacity_prefix)) &&
            line.ends_with(", 2")) {
            auto const equals_position = line.find(" = ");
            if (equals_position != std::string::npos) {
                operands.capacity_name = std::string {trim(std::string_view {line}.substr(2, equals_position - 2))};
            }
            continue;
        }
        if (operands.element_size_bytes.empty() && line.starts_with(item_load_prefix)) {
            auto const type_start = item_load_prefix.size();
            auto const comma_position = line.find(",", type_start);
            if (comma_position == std::string::npos) {
                continue;
            }
            auto const llvm_type = trim(std::string_view {line}.substr(type_start, comma_position - type_start));
            if (auto size = scalar_llvm_type_size_bytes(llvm_type)) {
                operands.element_size_bytes = std::to_string(*size);
            }
        }
    }
    return operands;
}

auto parse_inserted_cleanup_operation(
    std::string_view line,
    std::string_view prefix
) -> std::optional<InsertedCleanupOperation> {
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    auto const payload = line.substr(prefix.size());
    auto const kind_separator = payload.find(" operation ");
    if (kind_separator == std::string_view::npos) {
        return std::nullopt;
    }
    auto const operation_start = kind_separator + std::string_view {" operation "}.size();
    auto const from_separator = payload.find(" from ", operation_start);
    if (from_separator == std::string_view::npos) {
        return std::nullopt;
    }
    auto const source_start = from_separator + std::string_view {" from "}.size();
    auto const target_separator = payload.find(" to ", source_start);
    if (target_separator == std::string_view::npos) {
        return std::nullopt;
    }
    auto const disabled_suffix = std::string_view {" [cleanup calls disabled]"};
    auto const enabled_suffix = std::string_view {" [cleanup calls enabled]"};
    auto suffix_position = payload.find(disabled_suffix, target_separator);
    auto cleanup_calls_enabled = false;
    if (suffix_position == std::string_view::npos) {
        suffix_position = payload.find(enabled_suffix, target_separator);
        cleanup_calls_enabled = suffix_position != std::string_view::npos;
    }
    if (suffix_position == std::string_view::npos) {
        return std::nullopt;
    }
    return InsertedCleanupOperation {
        .kind_name = std::string {payload.substr(0, kind_separator)},
        .operation_name = std::string {payload.substr(operation_start, from_separator - operation_start)},
        .source_owner_name = std::string {payload.substr(source_start, target_separator - source_start)},
        .target_owner_name = std::string {
            payload.substr(target_separator + std::string_view {" to "}.size(),
                           suffix_position - target_separator - std::string_view {" to "}.size())
        },
        .cleanup_calls_enabled = cleanup_calls_enabled,
    };
}

auto format_inserted_cleanup_transition(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup transition";
    output << " acquire-from " << acquisition.source_owner_name;
    output << " acquire-to " << acquisition.target_owner_name;
    output << " acquire-operation " << acquisition.operation_name;
    output << " resume-from " << resumption.source_owner_name;
    output << " resume-to " << resumption.target_owner_name;
    output << " resume-operation " << resumption.operation_name;
    output << " (inserted IR)";
    return output.str();
}

auto format_inserted_cleanup_state_verification(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup state verification";
    output << " acquire-operation " << acquisition.operation_name;
    output << " resume-operation " << resumption.operation_name;
    output << " acquire-from " << acquisition.source_owner_name;
    output << " acquire-to " << acquisition.target_owner_name;
    output << " resume-from " << resumption.source_owner_name;
    output << " resume-to " << resumption.target_owner_name;
    output << " [handoff paired]";
    output << (acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled ?
        " [cleanup calls enabled]" : " [cleanup calls disabled]");
    output << " (inserted IR)";
    return output.str();
}

auto format_inserted_cleanup_state_verification_blocked(
    std::string_view reason,
    InsertedCleanupOperation const& operation
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup state verification blocked";
    output << " reason " << reason;
    output << " operation " << operation.operation_name;
    output << " from " << operation.source_owner_name;
    output << " to " << operation.target_owner_name;
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_emission_gate(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const cleanup_calls_enabled =
        acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call emission gate ";
    output << (state_verified && cleanup_calls_enabled ? "ready" : "blocked");
    output << " acquire-operation " << acquisition.operation_name;
    output << " resume-operation " << resumption.operation_name;
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    output << (state_verified && cleanup_calls_enabled ? " [cleanup call emission ready]" :
        " [cleanup call emission blocked]");
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_plan(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const cleanup_calls_enabled =
        acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call plan ";
    output << (state_verified ? "planned" : "blocked");
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << " after-resume-operation " << resumption.operation_name;
    output << " owner " << resumption.target_owner_name;
    if (!operands.data_pointer_name.empty()) {
        output << " data " << operands.data_pointer_name;
    }
    if (!operands.element_size_bytes.empty()) {
        output << " element-size " << operands.element_size_bytes;
    }
    if (!operands.capacity_name.empty()) {
        output << " capacity " << operands.capacity_name;
    }
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    output << (operands.data_pointer_name.empty() ? " [data operand pending]" : " [data operand proven]");
    output << (operands.element_size_bytes.empty() ? " [element-size operand pending]" :
        " [element-size operand proven]");
    output << (operands.capacity_name.empty() ? " [capacity operand pending]" : " [capacity operand proven]");
    output << " [cleanup call disabled]";
    output << " snippets 1 (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_render(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const operands_proven =
        !operands.data_pointer_name.empty() &&
        !operands.element_size_bytes.empty() &&
        !operands.capacity_name.empty();
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call render ";
    output << (state_verified && operands_proven ? "rendered" : "blocked");
    output << " cleanup-operation " << resumption.operation_name << ".call";
    if (operands_proven) {
        output << " call \"call void @__orison_dynamic_array_deallocate(ptr ";
        output << operands.data_pointer_name;
        output << ", i64 " << operands.element_size_bytes;
        output << ", i64 " << operands.capacity_name << ")\"";
    }
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (operands.data_pointer_name.empty() ? " [data operand pending]" : " [data operand proven]");
    output << (operands.element_size_bytes.empty() ? " [element-size operand pending]" :
        " [element-size operand proven]");
    output << (operands.capacity_name.empty() ? " [capacity operand pending]" : " [capacity operand proven]");
    output << " [render disabled]";
    output << " [module IR unchanged]";
    output << " snippets " << (state_verified && operands_proven ? 1 : 0);
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_insertion_gate(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const operands_proven =
        !operands.data_pointer_name.empty() &&
        !operands.element_size_bytes.empty() &&
        !operands.capacity_name.empty();
    auto const cleanup_calls_authorized =
        acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled;
    auto const insertion_ready =
        state_verified && operands_proven && cleanup_calls_authorized;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call insertion gate ";
    output << (insertion_ready ? "ready" : "blocked");
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (operands_proven ? " [cleanup operands proven]" : " [cleanup operands blocked]");
    output << (cleanup_calls_authorized ? " [cleanup calls authorized]" : " [cleanup calls unauthorized]");
    output << (insertion_ready ? " [cleanup call insertion ready]" : " [cleanup call insertion blocked]");
    output << " (inserted IR)";
    return output.str();
}

auto format_inserted_cleanup_transition_report(std::string_view ir_text) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    auto pending_acquisition = std::optional<InsertedCleanupOperation> {};
    auto input = std::istringstream {std::string {ir_text}};
    auto line = std::string {};
    while (std::getline(input, line)) {
        auto handoff = parse_inserted_cleanup_operation(
                line,
                "  ; cleanup state handoff "
            );
        if (!handoff.has_value()) {
            continue;
        }
        if (handoff->kind_name == "acquire") {
            pending_acquisition = std::move(handoff);
            continue;
        }
        if (handoff->kind_name == "resume") {
            if (pending_acquisition.has_value() &&
                pending_acquisition->target_owner_name == handoff->source_owner_name &&
                pending_acquisition->source_owner_name == handoff->target_owner_name) {
                report.push_back(format_inserted_cleanup_transition(*pending_acquisition, *handoff));
            }
            pending_acquisition.reset();
        }
    }
    return report;
}

auto collect_verified_inserted_cleanup_state_pairs(std::string_view ir_text)
    -> std::vector<std::pair<InsertedCleanupOperation, InsertedCleanupOperation>> {
    auto pairs = std::vector<std::pair<InsertedCleanupOperation, InsertedCleanupOperation>> {};
    auto pending_acquisition = std::optional<InsertedCleanupOperation> {};
    auto input = std::istringstream {std::string {ir_text}};
    auto line = std::string {};
    while (std::getline(input, line)) {
        auto handoff = parse_inserted_cleanup_operation(
            line,
            "  ; cleanup state handoff "
        );
        if (!handoff.has_value()) {
            continue;
        }
        if (handoff->kind_name == "acquire") {
            pending_acquisition = std::move(handoff);
            continue;
        }
        if (handoff->kind_name == "resume") {
            if (pending_acquisition.has_value() &&
                pending_acquisition->target_owner_name == handoff->source_owner_name &&
                pending_acquisition->source_owner_name == handoff->target_owner_name) {
                pairs.push_back({*pending_acquisition, *handoff});
            }
            pending_acquisition.reset();
        }
    }
    return pairs;
}

auto format_inserted_cleanup_state_verification_report(std::string_view ir_text)
    -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    auto pending_acquisition = std::optional<InsertedCleanupOperation> {};
    auto input = std::istringstream {std::string {ir_text}};
    auto line = std::string {};
    while (std::getline(input, line)) {
        auto handoff = parse_inserted_cleanup_operation(
            line,
            "  ; cleanup state handoff "
        );
        if (!handoff.has_value()) {
            continue;
        }
        if (handoff->kind_name == "acquire") {
            if (pending_acquisition.has_value()) {
                report.push_back(format_inserted_cleanup_state_verification_blocked(
                    "nested-acquire",
                    *pending_acquisition
                ));
            }
            pending_acquisition = std::move(handoff);
            continue;
        }
        if (handoff->kind_name == "resume") {
            if (!pending_acquisition.has_value()) {
                report.push_back(format_inserted_cleanup_state_verification_blocked(
                    "resume-without-acquire",
                    *handoff
                ));
                continue;
            }
            if (pending_acquisition->target_owner_name != handoff->source_owner_name ||
                pending_acquisition->source_owner_name != handoff->target_owner_name) {
                report.push_back(format_inserted_cleanup_state_verification_blocked(
                    "owner-mismatch",
                    *handoff
                ));
                pending_acquisition.reset();
                continue;
            }
            report.push_back(format_inserted_cleanup_state_verification(*pending_acquisition, *handoff));
            pending_acquisition.reset();
        }
    }
    if (pending_acquisition.has_value()) {
        report.push_back(format_inserted_cleanup_state_verification_blocked(
            "acquire-without-resume",
            *pending_acquisition
        ));
    }
    return report;
}

auto format_computed_cleanup_call_emission_gate_report(std::string_view ir_text)
    -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& [acquisition, resumption] : collect_verified_inserted_cleanup_state_pairs(ir_text)) {
        report.push_back(format_computed_cleanup_call_emission_gate(acquisition, resumption));
    }
    return report;
}

auto format_computed_cleanup_call_plan_report(std::string_view ir_text)
    -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& [acquisition, resumption] : collect_verified_inserted_cleanup_state_pairs(ir_text)) {
        report.push_back(format_computed_cleanup_call_plan(
            acquisition,
            resumption,
            collect_computed_cleanup_call_operands(ir_text, resumption)
        ));
    }
    return report;
}

auto format_computed_cleanup_call_render_report(std::string_view ir_text)
    -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& [acquisition, resumption] : collect_verified_inserted_cleanup_state_pairs(ir_text)) {
        report.push_back(format_computed_cleanup_call_render(
            acquisition,
            resumption,
            collect_computed_cleanup_call_operands(ir_text, resumption)
        ));
    }
    return report;
}

auto format_computed_cleanup_call_insertion_gate_report(std::string_view ir_text)
    -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& [acquisition, resumption] : collect_verified_inserted_cleanup_state_pairs(ir_text)) {
        report.push_back(format_computed_cleanup_call_insertion_gate(
            acquisition,
            resumption,
            collect_computed_cleanup_call_operands(ir_text, resumption)
        ));
    }
    return report;
}

auto format_computed_cleanup_call_inserted(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup call";
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << " call \"call void @__orison_dynamic_array_deallocate(ptr ";
    output << operands.data_pointer_name;
    output << ", i64 " << operands.element_size_bytes;
    output << ", i64 " << operands.capacity_name << ")\"";
    output << " [inserted state verified]";
    output << (acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled ?
        " [cleanup calls authorized]" : " [cleanup calls unauthorized]");
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_inserted_report(std::string_view ir_text)
    -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& [acquisition, resumption] : collect_verified_inserted_cleanup_state_pairs(ir_text)) {
        auto const operands = collect_computed_cleanup_call_operands(ir_text, resumption);
        if (
            operands.data_pointer_name.empty() ||
            operands.element_size_bytes.empty() ||
            operands.capacity_name.empty()
        ) {
            continue;
        }
        auto call_text = std::ostringstream {};
        call_text << "  call void @__orison_dynamic_array_deallocate(ptr ";
        call_text << operands.data_pointer_name;
        call_text << ", i64 " << operands.element_size_bytes;
        call_text << ", i64 " << operands.capacity_name << ")\n";
        if (ir_text.find(call_text.str()) == std::string_view::npos) {
            continue;
        }
        report.push_back(format_computed_cleanup_call_inserted(acquisition, resumption, operands));
    }
    return report;
}

auto format_computed_consumed_cleanup_descriptor(
    InsertedCleanupOperation const& resumption,
    std::string_view descriptor_storage_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for consumed cleanup descriptor";
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << " owner " << resumption.target_owner_name;
    output << " descriptor " << descriptor_storage_name;
    output << " [inserted cleanup call proven]";
    output << " [descriptor finalized]";
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_consumed_cleanup_descriptor_report(std::string_view ir_text)
    -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& [acquisition, resumption] : collect_verified_inserted_cleanup_state_pairs(ir_text)) {
        auto const operands = collect_computed_cleanup_call_operands(ir_text, resumption);
        if (
            operands.data_pointer_name.empty() ||
            operands.element_size_bytes.empty() ||
            operands.capacity_name.empty()
        ) {
            continue;
        }
        auto call_text = std::ostringstream {};
        call_text << "  call void @__orison_dynamic_array_deallocate(ptr ";
        call_text << operands.data_pointer_name;
        call_text << ", i64 " << operands.element_size_bytes;
        call_text << ", i64 " << operands.capacity_name << ")\n";
        auto const call_position = ir_text.find(call_text.str());
        if (call_position == std::string_view::npos) {
            continue;
        }
        auto const descriptor_storage_name = "%" + resumption.target_owner_name + ".addr";
        auto clear_text = std::ostringstream {};
        clear_text << "  store { ptr, i64, i64 } zeroinitializer, ptr ";
        clear_text << descriptor_storage_name << "\n";
        auto const clear_position = ir_text.find(clear_text.str(), call_position + call_text.str().size());
        if (clear_position == std::string_view::npos) {
            continue;
        }
        (void)acquisition;
        report.push_back(
            format_computed_consumed_cleanup_descriptor(resumption, descriptor_storage_name)
        );
    }
    return report;
}

}  // namespace

void populate_lowering_emission_reports(
    CompilePipelineResult& result,
    lowering::LlvmIrEmissionResult&& emission,
    CompilePipelineOptions const& options
) {
    result.ir_text = std::move(emission.ir_text);
    result.dynamic_array_construction_plan_report =
        emission.dynamic_array_construction_plan_report();
    result.dynamic_array_runtime_request_report =
        emission.dynamic_array_runtime_request_report();
    result.dynamic_array_allocation_call_ir =
        std::move(emission.dynamic_array_allocation_call_ir);
    result.dynamic_array_descriptor_cleanup_plan_report =
        emission.dynamic_array_descriptor_cleanup_plan_report();
    result.dynamic_array_cleanup_obligation_report =
        emission.dynamic_array_cleanup_obligation_report();
    result.dynamic_array_cleanup_sequence_plan_report =
        emission.dynamic_array_cleanup_sequence_plan_report();
    result.dynamic_array_cleanup_sequence_verification_report =
        emission.dynamic_array_cleanup_sequence_verification_report();
    result.dynamic_array_cleanup_emission_gate_report =
        emission.dynamic_array_cleanup_emission_gate_report();
    result.dynamic_array_cleanup_emission_capability_report =
        emission.dynamic_array_cleanup_emission_capability_report();
    result.computed_dynamic_array_for_descriptor_render_report =
        emission.computed_dynamic_array_for_descriptor_render_report();
    result.computed_dynamic_array_for_loop_control_render_report =
        emission.computed_dynamic_array_for_loop_control_render_report();
    result.computed_dynamic_array_for_element_address_render_report =
        emission.computed_dynamic_array_for_element_address_render_report();
    result.computed_dynamic_array_for_element_load_render_report =
        emission.computed_dynamic_array_for_element_load_render_report();
    result.computed_dynamic_array_for_loop_continue_render_report =
        emission.computed_dynamic_array_for_loop_continue_render_report();
    result.computed_dynamic_array_for_loop_render_sequence_report =
        emission.computed_dynamic_array_for_loop_render_sequence_report();
    result.computed_dynamic_array_for_loop_exit_cleanup_report =
        emission.computed_dynamic_array_for_loop_exit_cleanup_report();
    result.computed_dynamic_array_for_cleanup_transition_report =
        emission.computed_dynamic_array_for_cleanup_transition_report();
    result.computed_dynamic_array_for_inserted_cleanup_transition_report =
        format_inserted_cleanup_transition_report(result.ir_text);
    result.computed_dynamic_array_for_inserted_cleanup_state_verification_report =
        format_inserted_cleanup_state_verification_report(result.ir_text);
    result.computed_dynamic_array_for_cleanup_call_emission_gate_report =
        format_computed_cleanup_call_emission_gate_report(result.ir_text);
    result.computed_dynamic_array_for_cleanup_call_plan_report =
        format_computed_cleanup_call_plan_report(result.ir_text);
    result.computed_dynamic_array_for_cleanup_call_render_report =
        format_computed_cleanup_call_render_report(result.ir_text);
    result.computed_dynamic_array_for_cleanup_call_insertion_gate_report =
        format_computed_cleanup_call_insertion_gate_report(result.ir_text);
    result.computed_dynamic_array_for_inserted_cleanup_call_report =
        format_computed_cleanup_call_inserted_report(result.ir_text);
    result.consumed_descriptor_finalization_plan_report =
        emission.consumed_descriptor_finalization_plan_report();
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report =
        emission.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report();
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_report =
        format_computed_consumed_cleanup_descriptor_report(result.ir_text);
    result.computed_dynamic_array_for_production_emission_gate_report =
        emission.computed_dynamic_array_for_production_emission_gate_report();
    result.computed_dynamic_array_for_production_sequence_report =
        emission.computed_dynamic_array_for_production_sequence_report();
    result.test_only_computed_dynamic_array_for_production_sequence_module_ir =
        std::move(emission.test_only_computed_dynamic_array_for_production_sequence_module_ir);
    result.dynamic_array_cleanup_production_readiness =
        plan_dynamic_array_cleanup_production_readiness(result, options);
    result.dynamic_array_cleanup_production_readiness_report = {
        format_dynamic_array_cleanup_production_readiness(result.dynamic_array_cleanup_production_readiness),
    };
    result.planned_drop_report = emission.planned_drop_report();
    result.emitted_drop_declaration_report =
        emission.emitted_drop_declaration_report();
    result.planned_drop_action_report =
        emission.planned_drop_action_report();
    result.drop_cleanup_authorization_report =
        emission.drop_cleanup_authorization_report();
    result.drop_readiness_snapshot = emission.drop_readiness_snapshot();
    result.drop_readiness_snapshot_report =
        emission.drop_readiness_snapshot_report();
    result.drop_readiness_summary = emission.drop_readiness_summary();
    result.drop_readiness_summary_report =
        emission.drop_readiness_summary_report();
    result.drop_readiness_relation_report =
        emission.drop_readiness_relation_report();
    result.drop_readiness_blocker_summary =
        lowering::summarize_drop_readiness_blockers(result.drop_readiness_snapshot);
    result.drop_readiness_blocker_report =
        lowering::format_drop_readiness_blocker_report(result.drop_readiness_blocker_summary);
    result.drop_readiness_source_correlation_report =
        format_drop_readiness_source_correlation_report(result.drop_readiness_snapshot);
    result.semantic_drop_lowering_authorizations = std::move(emission.semantic_drop_lowering_authorizations);
}

}  // namespace orison::pipeline
