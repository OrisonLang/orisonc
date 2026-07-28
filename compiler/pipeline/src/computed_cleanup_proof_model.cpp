#include "computed_cleanup_proof_model.hpp"

#include <sstream>
#include <utility>

namespace orison::pipeline {

namespace {

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

auto computed_cleanup_call_operands_from_metadata(
    std::vector<lowering::ComputedDynamicArrayCleanupCallOperands> const& metadata,
    InsertedCleanupOperation const& resumption
) -> std::optional<ComputedCleanupCallOperands> {
    for (auto const& operands : metadata) {
        if (operands.cleanup_operation_name != resumption.operation_name) {
            continue;
        }
        return ComputedCleanupCallOperands {
            .data_pointer_name = operands.data_pointer_name,
            .capacity_name = operands.capacity_name,
            .element_size_bytes = std::to_string(operands.element_size_bytes),
            .from_metadata = true,
        };
    }
    return std::nullopt;
}

auto computed_cleanup_call_metadata_for_resumption(
    std::vector<lowering::ComputedDynamicArrayCleanupCallOperands> const& metadata,
    InsertedCleanupOperation const& resumption
) -> lowering::ComputedDynamicArrayCleanupCallOperands const* {
    for (auto const& operands : metadata) {
        if (operands.cleanup_operation_name == resumption.operation_name) {
            return &operands;
        }
    }
    return nullptr;
}

auto collect_computed_cleanup_call_operands(
    std::string_view ir_text,
    std::vector<lowering::ComputedDynamicArrayCleanupCallOperands> const& metadata,
    InsertedCleanupOperation const& resumption
) -> ComputedCleanupCallOperands {
    if (auto operands = computed_cleanup_call_operands_from_metadata(metadata, resumption)) {
        return *operands;
    }
    return collect_computed_cleanup_call_operands(ir_text, resumption);
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

auto inserted_cleanup_operation_from_metadata(
    lowering::ComputedDynamicArrayCleanupStateHandoff const& handoff
) -> InsertedCleanupOperation {
    return InsertedCleanupOperation {
        .kind_name = handoff.kind == lowering::ComputedDynamicArrayCleanupStateHandoffKind::acquire ?
            "acquire" : "resume",
        .operation_name = handoff.operation_name,
        .source_owner_name = handoff.source_owner_name,
        .target_owner_name = handoff.target_owner_name,
        .cleanup_calls_enabled = handoff.cleanup_calls_enabled,
    };
}

auto analyze_inserted_cleanup_state_handoff_operations(
    std::vector<InsertedCleanupOperation> const& handoffs
) -> InsertedCleanupStateAnalysis {
    auto analysis = InsertedCleanupStateAnalysis {};
    auto pending_acquisition = std::optional<InsertedCleanupOperation> {};
    for (auto const& handoff : handoffs) {
        if (handoff.kind_name == "acquire") {
            if (pending_acquisition.has_value()) {
                analysis.verification_events.push_back(InsertedCleanupStateVerificationEvent {
                    .kind = InsertedCleanupStateVerificationKind::blocked,
                    .reason = "nested-acquire",
                    .operation = *pending_acquisition,
                });
            }
            pending_acquisition = handoff;
            continue;
        }
        if (handoff.kind_name == "resume") {
            if (!pending_acquisition.has_value()) {
                analysis.verification_events.push_back(InsertedCleanupStateVerificationEvent {
                    .kind = InsertedCleanupStateVerificationKind::blocked,
                    .reason = "resume-without-acquire",
                    .operation = handoff,
                });
                continue;
            }
            if (pending_acquisition->target_owner_name != handoff.source_owner_name ||
                pending_acquisition->source_owner_name != handoff.target_owner_name) {
                analysis.verification_events.push_back(InsertedCleanupStateVerificationEvent {
                    .kind = InsertedCleanupStateVerificationKind::blocked,
                    .reason = "owner-mismatch",
                    .operation = handoff,
                });
                pending_acquisition.reset();
                continue;
            }
            analysis.verified_pairs.push_back({*pending_acquisition, handoff});
            analysis.transition_events.push_back(InsertedCleanupTransitionEvent {
                .acquisition = *pending_acquisition,
                .resumption = handoff,
            });
            analysis.verification_events.push_back(InsertedCleanupStateVerificationEvent {
                .kind = InsertedCleanupStateVerificationKind::paired,
                .acquisition = *pending_acquisition,
                .operation = handoff,
            });
            pending_acquisition.reset();
        }
    }
    if (pending_acquisition.has_value()) {
        analysis.verification_events.push_back(InsertedCleanupStateVerificationEvent {
            .kind = InsertedCleanupStateVerificationKind::blocked,
            .reason = "acquire-without-resume",
            .operation = *pending_acquisition,
        });
    }
    return analysis;
}

auto analyze_inserted_cleanup_state_handoffs_from_ir(std::string_view ir_text) -> InsertedCleanupStateAnalysis {
    auto handoffs = std::vector<InsertedCleanupOperation> {};
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
        handoffs.push_back(std::move(*handoff));
    }
    return analyze_inserted_cleanup_state_handoff_operations(handoffs);
}

auto analyze_inserted_cleanup_state_handoffs(
    std::string_view ir_text,
    std::vector<lowering::ComputedDynamicArrayCleanupStateHandoff> const& metadata
) -> InsertedCleanupStateAnalysis {
    if (!metadata.empty()) {
        auto handoffs = std::vector<InsertedCleanupOperation> {};
        handoffs.reserve(metadata.size());
        for (auto const& handoff : metadata) {
            handoffs.push_back(inserted_cleanup_operation_from_metadata(handoff));
        }
        auto analysis = analyze_inserted_cleanup_state_handoff_operations(handoffs);
        analysis.from_metadata = true;
        return analysis;
    }
    return analyze_inserted_cleanup_state_handoffs_from_ir(ir_text);
}

auto build_computed_cleanup_call_insertion_decision(
    VerifiedComputedCleanupCall const& call
) -> ComputedCleanupCallInsertionDecision {
    auto decision = ComputedCleanupCallInsertionDecision {
        .state_verified =
            call.acquisition.target_owner_name == call.resumption.source_owner_name &&
            call.acquisition.source_owner_name == call.resumption.target_owner_name,
        .operands_proven = computed_cleanup_call_operands_complete(call.operands),
        .cleanup_calls_authorized =
            call.acquisition.cleanup_calls_enabled && call.resumption.cleanup_calls_enabled,
    };
    decision.insertion_ready =
        decision.state_verified && decision.operands_proven && decision.cleanup_calls_authorized;
    return decision;
}

auto build_computed_inserted_cleanup_call_decision(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> ComputedInsertedCleanupCallDecision {
    auto decision = ComputedInsertedCleanupCallDecision {
        .operands_proven = computed_cleanup_call_operands_complete(call.operands),
        .proven_by_metadata = call.metadata != nullptr && call.metadata->cleanup_call_inserted,
    };
    decision.proven_by_ir =
        call.metadata == nullptr &&
        decision.operands_proven &&
        ir_text.find(rendered_computed_cleanup_call_text(call.operands)) != std::string_view::npos;
    decision.inserted = decision.proven_by_metadata || decision.proven_by_ir;
    return decision;
}

auto build_computed_consumed_cleanup_descriptor_decision(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> ComputedConsumedCleanupDescriptorDecision {
    auto decision = ComputedConsumedCleanupDescriptorDecision {
        .operands_proven = computed_cleanup_call_operands_complete(call.operands),
        .finalized_by_metadata =
            call.metadata != nullptr &&
            call.metadata->cleanup_call_inserted &&
            call.metadata->descriptor_finalized &&
            !call.metadata->descriptor_storage_name.empty(),
    };
    if (decision.finalized_by_metadata) {
        decision.descriptor_storage_name = call.metadata->descriptor_storage_name;
    } else if (call.metadata == nullptr && decision.operands_proven) {
        auto const call_text = rendered_computed_cleanup_call_text(call.operands);
        auto const call_position = ir_text.find(call_text);
        if (call_position != std::string_view::npos) {
            auto const descriptor_storage_name = "%" + call.resumption.target_owner_name + ".addr";
            auto clear_text = std::ostringstream {};
            clear_text << "  store { ptr, i64, i64 } zeroinitializer, ptr ";
            clear_text << descriptor_storage_name << "\n";
            auto const clear_position = ir_text.find(clear_text.str(), call_position + call_text.size());
            if (clear_position != std::string_view::npos) {
                decision.finalized_by_ir = true;
                decision.descriptor_storage_name = descriptor_storage_name;
            }
        }
    }
    decision.finalized = decision.finalized_by_metadata || decision.finalized_by_ir;
    return decision;
}

auto collect_verified_computed_cleanup_calls(
    std::string_view ir_text,
    std::vector<lowering::ComputedDynamicArrayCleanupCallOperands> const& operand_metadata,
    std::vector<std::pair<InsertedCleanupOperation, InsertedCleanupOperation>> const& verified_pairs
) -> std::vector<VerifiedComputedCleanupCall> {
    auto calls = std::vector<VerifiedComputedCleanupCall> {};
    calls.reserve(verified_pairs.size());
    for (auto const& [acquisition, resumption] : verified_pairs) {
        auto call = VerifiedComputedCleanupCall {
            .acquisition = acquisition,
            .resumption = resumption,
            .operands = collect_computed_cleanup_call_operands(ir_text, operand_metadata, resumption),
            .metadata = computed_cleanup_call_metadata_for_resumption(operand_metadata, resumption),
        };
        call.insertion_decision = build_computed_cleanup_call_insertion_decision(call);
        call.inserted_call_decision = build_computed_inserted_cleanup_call_decision(ir_text, call);
        call.consumed_descriptor_decision =
            build_computed_consumed_cleanup_descriptor_decision(ir_text, call);
        calls.push_back(std::move(call));
    }
    return calls;
}

auto build_computed_cleanup_proof_summary(
    ComputedCleanupProofModel const& model,
    std::vector<lowering::ComputedDynamicArrayCleanupStateHandoff> const& handoff_metadata,
    std::vector<lowering::ComputedDynamicArrayCleanupCallOperands> const& operand_metadata
) -> ComputedCleanupProofSummary {
    auto summary = ComputedCleanupProofSummary {
        .cleanup_proof_model_count = model.verified_cleanup_calls.size(),
        .verified_inserted_cleanup_pair_count = model.inserted_cleanup_state.verified_pairs.size(),
        .structured_inserted_cleanup_handoff_count = handoff_metadata.size(),
        .structured_cleanup_operand_count = operand_metadata.size(),
    };
    if (model.inserted_cleanup_state.from_metadata) {
        summary.structured_inserted_cleanup_handoff_use_count =
            model.inserted_cleanup_state.verified_pairs.size() * 2;
    } else {
        summary.ir_inserted_cleanup_handoff_fallback_count =
            model.inserted_cleanup_state.verified_pairs.size() * 2;
    }
    for (auto const& call : model.verified_cleanup_calls) {
        if (call.operands.from_metadata) {
            ++summary.structured_cleanup_operand_use_count;
        } else {
            ++summary.ir_cleanup_operand_fallback_count;
        }
        if (call.inserted_call_decision.proven_by_ir) {
            ++summary.ir_inserted_cleanup_call_fallback_count;
        }
        if (call.consumed_descriptor_decision.finalized_by_ir) {
            ++summary.ir_consumed_cleanup_descriptor_fallback_count;
        }
    }
    for (auto const& operands : operand_metadata) {
        if (operands.cleanup_call_inserted) {
            ++summary.structured_inserted_cleanup_call_count;
        }
        if (operands.cleanup_call_inserted &&
            operands.descriptor_finalized &&
            !operands.descriptor_storage_name.empty()) {
            ++summary.structured_consumed_cleanup_descriptor_count;
        }
    }
    return summary;
}

}  // namespace

auto build_computed_cleanup_proof_model(
    std::string_view ir_text,
    std::vector<lowering::ComputedDynamicArrayCleanupStateHandoff> const& handoff_metadata,
    std::vector<lowering::ComputedDynamicArrayCleanupCallOperands> const& operand_metadata
) -> ComputedCleanupProofModel {
    auto model = ComputedCleanupProofModel {};
    model.inserted_cleanup_state = analyze_inserted_cleanup_state_handoffs(ir_text, handoff_metadata);
    model.verified_cleanup_calls = collect_verified_computed_cleanup_calls(
        ir_text,
        operand_metadata,
        model.inserted_cleanup_state.verified_pairs
    );
    model.summary = build_computed_cleanup_proof_summary(model, handoff_metadata, operand_metadata);
    model.reports = build_computed_cleanup_proof_report_bundle(model);
    return model;
}

auto computed_cleanup_call_operands_complete(ComputedCleanupCallOperands const& operands) -> bool {
    return
        !operands.data_pointer_name.empty() &&
        !operands.element_size_bytes.empty() &&
        !operands.capacity_name.empty();
}

auto rendered_computed_cleanup_call_text(ComputedCleanupCallOperands const& operands) -> std::string {
    auto call_text = std::ostringstream {};
    call_text << "  call void @__orison_dynamic_array_deallocate(ptr ";
    call_text << operands.data_pointer_name;
    call_text << ", i64 " << operands.element_size_bytes;
    call_text << ", i64 " << operands.capacity_name << ")\n";
    return call_text.str();
}

auto computed_cleanup_call_insertion_decision(
    VerifiedComputedCleanupCall const& call
) -> ComputedCleanupCallInsertionDecision {
    return call.insertion_decision;
}

auto computed_inserted_cleanup_call_decision(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> ComputedInsertedCleanupCallDecision {
    (void)ir_text;
    return call.inserted_call_decision;
}

auto computed_consumed_cleanup_descriptor_decision(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> ComputedConsumedCleanupDescriptorDecision {
    (void)ir_text;
    return call.consumed_descriptor_decision;
}

auto computed_cleanup_call_inserted_by_metadata(VerifiedComputedCleanupCall const& call) -> bool {
    return computed_inserted_cleanup_call_decision("", call).proven_by_metadata;
}

auto computed_cleanup_call_inserted_by_ir(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> bool {
    return computed_inserted_cleanup_call_decision(ir_text, call).proven_by_ir;
}

auto computed_consumed_cleanup_descriptor_by_metadata(VerifiedComputedCleanupCall const& call) -> bool {
    return computed_consumed_cleanup_descriptor_decision("", call).finalized_by_metadata;
}

auto computed_consumed_cleanup_descriptor_by_ir(
    std::string_view ir_text,
    VerifiedComputedCleanupCall const& call
) -> std::optional<std::string> {
    auto decision = computed_consumed_cleanup_descriptor_decision(ir_text, call);
    if (!decision.finalized_by_ir) {
        return std::nullopt;
    }
    return decision.descriptor_storage_name;
}

}  // namespace orison::pipeline
