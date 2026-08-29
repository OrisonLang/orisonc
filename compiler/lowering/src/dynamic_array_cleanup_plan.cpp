#include "orison/lowering/dynamic_array_cleanup_plan.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/consumed_descriptor_finalization.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include "orison/semantics/drop_model.hpp"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace orison::lowering {
namespace {

auto dynamic_array_cleanup_symbol_name(std::size_t ordinal) -> std::string {
    auto output = std::ostringstream {};
    output << "__orison_dynamic_array_cleanup." << ordinal;
    return output.str();
}

auto has_matching_dynamic_array_descriptor_summary_binding(
    semantics::SemanticAnalysisResult const* semantic_result,
    std::string_view owner_name,
    std::string_view source_type_name,
    semantics::DynamicArrayDescriptorBindingKind binding_kind
) -> bool {
    if (semantic_result == nullptr) {
        return false;
    }

    return std::ranges::any_of(
        semantic_result->semantic_module.dynamic_array_descriptors,
        [&](semantics::SemanticDynamicArrayDescriptorSummary const& descriptor) {
            return descriptor.owner_name == owner_name &&
                dynamic_array_descriptor_lifetime_source_type_matches(descriptor.source_type_name, source_type_name) &&
                descriptor.binding_kind == binding_kind;
        }
    );
}

auto dynamic_array_parameter_element_cleanup_proven(
    std::string_view source_type_name,
    LlvmIrEmissionOptions const& options
) -> bool {
    auto sequence = dynamic_sequence_source_type(source_type_name);
    if (!sequence.has_value() ||
        sequence->kind != DynamicSequenceKind::dynamic_array ||
        is_scalar_or_nonowning_source_type(sequence->element_source_type_name)) {
        return false;
    }

    auto const expected_symbol_name = semantics::drop_abi_symbol_name(sequence->element_source_type_name);
    return std::ranges::find(options.source_drop_definition_symbols, expected_symbol_name) !=
        options.source_drop_definition_symbols.end();
}

auto dynamic_array_descriptor_element_drop_action(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::size_t ordinal
) -> PlannedDropAction {
    auto capture_name = !plan.owner_name.empty()
        ? plan.owner_name + ".element"
        : "dynamic_array_descriptor" + std::to_string(ordinal) + ".element";
    return PlannedDropAction {
        .capture_name = std::move(capture_name),
        .source_type_name = plan.element_source_type_name,
        .symbol_name = semantics::drop_abi_symbol_name(plan.element_source_type_name),
        .field_index = ordinal,
        .discovery_line = plan.source_line,
    };
}

auto dynamic_array_parameter_drop_action(
    std::string_view name,
    DynamicArrayDescriptorCleanupPlan const& plan
) -> PlannedDropAction {
    return PlannedDropAction {
        .capture_name = std::string {name} + ".element",
        .source_type_name = plan.element_source_type_name,
        .symbol_name = semantics::drop_abi_symbol_name(plan.element_source_type_name),
    };
}

auto emit_dynamic_array_element_drop_walk_with_calls(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::string_view data_pointer_name,
    std::string_view length_name,
    std::string_view name_prefix,
    std::string_view drop_symbol_name
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    auto label_prefix = prefix;
    if (!label_prefix.empty() && label_prefix.front() == '%') {
        label_prefix.erase(label_prefix.begin());
    }
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
    output << "  ; drop element " << plan.element_source_type_name;
    output << " at " << prefix << ".drop.element.addr using " << drop_symbol_name << "\n";
    output << "  call void @" << drop_symbol_name << "(ptr " << prefix << ".drop.element.addr)\n";
    output << "  " << prefix << ".drop.next = add i64 " << prefix << ".drop.index, 1\n";
    output << "  br label %" << label_prefix << ".drop.walk\n";
    output << label_prefix << ".drop.done:\n";
    return output.str();
}

auto emit_dynamic_array_descriptor_cleanup_sequence_with_optional_drop_calls(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::string_view descriptor_value_name,
    std::string_view name_prefix,
    std::optional<std::string> const& drop_symbol_name
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
    if (drop_symbol_name.has_value()) {
        output << emit_dynamic_array_element_drop_walk_with_calls(
            plan,
            prefix + ".cleanup.data",
            prefix + ".cleanup.length",
            prefix,
            *drop_symbol_name
        );
    }
    output << "  call void @__orison_dynamic_array_deallocate(ptr ";
    output << prefix << ".cleanup.data";
    output << ", i64 " << plan.element_size_bytes;
    output << ", i64 " << prefix << ".cleanup.capacity)\n";
    return output.str();
}

auto choice_payload_field_type(LoweredChoiceLayout const& layout) -> std::optional<std::string_view> {
    auto type = std::string_view {layout.llvm_type_name};
    if (type == "i32") {
        return std::nullopt;
    }
    if (!type.starts_with("{ i32, ") || !type.ends_with(" }")) {
        return std::nullopt;
    }
    return type.substr(7, type.size() - 9);
}

auto authorized_element_drop_symbol_name(
    std::string_view name,
    DynamicArrayDescriptorCleanupPlan const& plan,
    LlvmIrEmissionOptions const& options
) -> std::optional<std::string> {
    auto action = dynamic_array_parameter_drop_action(name, plan);
    auto cleanup = ConcurrencyDropCleanupPlan {
        .cleanup_symbol_name = "__orison_dynamic_array_cleanup",
        .actions = {action},
        .requires_semantic_authorization = true,
        .requires_descriptor_deallocation = true,
    };
    auto declarations = declared_drop_declarations_for_authorized_semantic_drops(
        options.semantic_drop_lowering_authorizations
    );
    auto authorization = plan_drop_cleanup_authorization(
        cleanup,
        declarations,
        options.semantic_drop_lowering_authorizations
    );
    if (!authorization.authorized) {
        return std::nullopt;
    }
    return action.symbol_name;
}

auto dynamic_array_cleanup_action_authorized(
    PlannedDropAction const& action,
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> bool {
    return std::ranges::any_of(authorizations, [&](auto const& authorization) {
        return authorization.authorized &&
            authorization.site.abi_symbol_name == action.symbol_name &&
            authorization.site.source_type_name == action.source_type_name &&
            authorization.site.owner_name == action.capture_name;
    });
}

auto synthetic_dynamic_array_parameter_cleanup_authorizations(
    std::vector<BoundDynamicArrayParameterCleanupPlan> const& plans
) -> std::vector<semantics::DropLoweringAuthorization> {
    auto authorizations = std::vector<semantics::DropLoweringAuthorization> {};
    for (auto const& plan : plans) {
        if (!plan.element_drop_symbol_name.has_value()) {
            continue;
        }
        for (auto const& action : plan.sequence_plan.obligation.actions) {
            authorizations.push_back(semantics::DropLoweringAuthorization {
                .site = semantics::PlannedDropSite {
                    .source_type_name = action.source_type_name,
                    .abi_symbol_name = action.symbol_name,
                    .owner_name = action.capture_name,
                    .site_line = action.discovery_line,
                },
                .semantic_resolved = true,
                .source_drop_lowering_enabled = true,
                .authorized = true,
            });
        }
    }
    return authorizations;
}

auto descriptor_storage_finalized_by_computed_cleanup(
    FunctionLoweringState const& state,
    std::string_view descriptor_storage_name
) -> bool {
    return std::ranges::any_of(state.computed_dynamic_array_cleanup_call_operands, [&](auto const& operands) {
        return operands.descriptor_finalized &&
            operands.descriptor_storage_name == descriptor_storage_name;
    });
}

auto authorized_descriptor_element_drop_symbol_name(
    DynamicArrayCleanupObligation const& obligation,
    LlvmIrEmissionOptions const& options
) -> std::optional<std::string> {
    if (obligation.actions.empty()) {
        return std::nullopt;
    }
    auto cleanup = drop_cleanup_for_dynamic_array_cleanup_obligation(obligation);
    auto declarations = declared_drop_declarations_for_authorized_semantic_drops(
        options.semantic_drop_lowering_authorizations
    );
    auto authorization = plan_drop_cleanup_authorization(
        cleanup,
        declarations,
        options.semantic_drop_lowering_authorizations
    );
    if (!authorization.authorized) {
        return std::nullopt;
    }
    return obligation.actions.front().symbol_name;
}

auto authorized_choice_payload_element_drop_symbol_name(
    DynamicArrayCleanupObligation const& obligation,
    LlvmIrEmissionOptions const& options
) -> std::optional<std::string> {
    auto authorized = authorized_descriptor_element_drop_symbol_name(obligation, options);
    if (authorized.has_value() || obligation.actions.empty()) {
        return authorized;
    }

    auto const& action = obligation.actions.front();
    for (auto const& authorization : options.semantic_drop_lowering_authorizations) {
        if (authorization.authorized &&
            authorization.site.source_type_name == action.source_type_name &&
            authorization.site.abi_symbol_name == action.symbol_name) {
            return action.symbol_name;
        }
    }
    return std::nullopt;
}

template <typename CleanupPlan>
void record_emitted_dynamic_array_cleanup_reports(
    DynamicArrayCleanupEmissionCapability const& capability,
    std::vector<CleanupPlan> const& plans,
    FunctionLoweringSession& session
) {
    auto obligations = std::vector<DynamicArrayCleanupObligation> {};
    auto sequence_plans = std::vector<DynamicArrayCleanupSequencePlan> {};
    auto verifications = std::vector<DynamicArrayCleanupSequenceVerification> {};
    obligations.reserve(plans.size());
    sequence_plans.reserve(plans.size());
    verifications.reserve(plans.size());
    for (auto const& plan : plans) {
        obligations.push_back(plan.sequence_plan.obligation);
        sequence_plans.push_back(plan.sequence_plan);
        verifications.push_back(plan.sequence_verification);
    }
    session.state.emitted_dynamic_array_cleanup_obligations.insert(
        session.state.emitted_dynamic_array_cleanup_obligations.end(),
        obligations.begin(),
        obligations.end()
    );
    session.state.emitted_dynamic_array_cleanup_sequence_plans.insert(
        session.state.emitted_dynamic_array_cleanup_sequence_plans.end(),
        sequence_plans.begin(),
        sequence_plans.end()
    );
    session.state.emitted_dynamic_array_cleanup_sequence_verifications.insert(
        session.state.emitted_dynamic_array_cleanup_sequence_verifications.end(),
        verifications.begin(),
        verifications.end()
    );
    session.state.emitted_dynamic_array_cleanup_emission_capabilities.push_back(capability);
}

struct AggregateExtractionStep {
    std::string aggregate_llvm_type;
    std::string extracted_llvm_type;
    std::size_t index = 0;
};

struct ChoicePayloadDescriptorCleanup {
    DynamicArrayDescriptorCleanupPlan descriptor_cleanup;
    std::vector<AggregateExtractionStep> extraction_steps;
};

auto collect_choice_payload_descriptor_cleanups(
    std::string_view owner_name,
    std::string_view source_type_name,
    std::string_view llvm_type,
    LoweringContext const& context,
    std::vector<AggregateExtractionStep> extraction_steps = {}
) -> std::optional<std::vector<ChoicePayloadDescriptorCleanup>> {
    auto cleanups = std::vector<ChoicePayloadDescriptorCleanup> {};

    if (dynamic_array_element_source_type_name(source_type_name).has_value()) {
        auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(owner_name, source_type_name, context);
        if (!cleanup_plan.has_value()) {
            return std::nullopt;
        }
        cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
        cleanups.push_back(ChoicePayloadDescriptorCleanup {
            .descriptor_cleanup = std::move(*cleanup_plan),
            .extraction_steps = std::move(extraction_steps),
        });
        return cleanups;
    }

    if (auto element_source_type = array_element_source_type_name(source_type_name)) {
        auto array_type = parse_llvm_array_type(llvm_type);
        if (!array_type.has_value()) {
            return std::nullopt;
        }
        for (auto index = std::size_t {0}; index < array_type->length; ++index) {
            auto element_steps = extraction_steps;
            element_steps.push_back(AggregateExtractionStep {
                .aggregate_llvm_type = std::string {llvm_type},
                .extracted_llvm_type = array_type->element_type,
                .index = index,
            });
            auto element_owner = std::string {owner_name} + ".element" + std::to_string(index);
            auto nested = collect_choice_payload_descriptor_cleanups(
                element_owner,
                *element_source_type,
                array_type->element_type,
                context,
                std::move(element_steps)
            );
            if (!nested.has_value()) {
                return std::nullopt;
            }
            cleanups.insert(
                cleanups.end(),
                std::make_move_iterator(nested->begin()),
                std::make_move_iterator(nested->end())
            );
        }
        return cleanups;
    }

    auto record = context.records.find(std::string {source_type_name});
    if (record != context.records.end()) {
        for (auto const& field : record->second.fields) {
            auto field_steps = extraction_steps;
            field_steps.push_back(AggregateExtractionStep {
                .aggregate_llvm_type = std::string {llvm_type},
                .extracted_llvm_type = field.llvm_type,
                .index = field.index,
            });
            auto field_owner = std::string {owner_name} + "." + field.name;
            auto nested = collect_choice_payload_descriptor_cleanups(
                field_owner,
                field.source_type_name,
                field.llvm_type,
                context,
                std::move(field_steps)
            );
            if (!nested.has_value()) {
                return std::nullopt;
            }
            cleanups.insert(
                cleanups.end(),
                std::make_move_iterator(nested->begin()),
                std::make_move_iterator(nested->end())
            );
        }
    }

    return cleanups;
}

auto emit_aggregate_extraction_chain(
    std::string_view root_value_name,
    std::vector<AggregateExtractionStep> const& extraction_steps,
    std::string_view result_prefix,
    FunctionLoweringSession& session,
    std::ostream& output
) -> std::string {
    auto current_value = std::string {root_value_name};
    for (auto const& step : extraction_steps) {
        auto next_value = std::string {result_prefix} + ".extract" +
            std::to_string(session.state.next_temporary_index++);
        output << "  " << next_value << " = extractvalue " << step.aggregate_llvm_type
               << " " << current_value << ", " << step.index << "\n";
        current_value = std::move(next_value);
    }
    return current_value;
}

}  // namespace

auto plan_dynamic_array_descriptor_cleanup_obligation(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::size_t ordinal
) -> DynamicArrayCleanupObligation {
    auto obligation = DynamicArrayCleanupObligation {
        .cleanup_symbol_name = dynamic_array_cleanup_symbol_name(ordinal),
        .descriptor_cleanup = plan,
        .requires_descriptor_deallocation = true,
    };
    if (!is_scalar_or_nonowning_source_type(plan.element_source_type_name)) {
        obligation.actions.push_back(dynamic_array_descriptor_element_drop_action(plan, ordinal));
    }
    return obligation;
}

auto plan_dynamic_array_descriptor_cleanup_obligations(
    std::vector<DynamicArrayDescriptorCleanupPlan> const& plans,
    std::size_t ordinal_offset
) -> std::vector<DynamicArrayCleanupObligation> {
    auto obligations = std::vector<DynamicArrayCleanupObligation> {};
    obligations.reserve(plans.size());
    for (auto index = std::size_t {0}; index < plans.size(); ++index) {
        obligations.push_back(plan_dynamic_array_descriptor_cleanup_obligation(plans[index], ordinal_offset + index));
    }
    return obligations;
}

auto drop_cleanup_for_dynamic_array_cleanup_obligation(
    DynamicArrayCleanupObligation const& obligation
) -> ConcurrencyDropCleanupPlan {
    return ConcurrencyDropCleanupPlan {
        .cleanup_symbol_name = obligation.cleanup_symbol_name,
        .actions = obligation.actions,
        .requires_semantic_authorization = true,
        .requires_descriptor_deallocation = obligation.requires_descriptor_deallocation,
    };
}

auto plan_dynamic_array_cleanup_sequence(
    DynamicArrayCleanupObligation const& obligation
) -> DynamicArrayCleanupSequencePlan {
    auto phases = std::vector<std::string> {};
    phases.push_back("load descriptor");
    if (!obligation.actions.empty()) {
        phases.push_back("drop initialized elements");
    }
    if (obligation.requires_descriptor_deallocation) {
        phases.push_back("deallocate descriptor storage");
    }
    return DynamicArrayCleanupSequencePlan {
        .obligation = obligation,
        .phases = std::move(phases),
    };
}

auto plan_dynamic_array_cleanup_sequences(
    std::vector<DynamicArrayCleanupObligation> const& obligations
) -> std::vector<DynamicArrayCleanupSequencePlan> {
    auto plans = std::vector<DynamicArrayCleanupSequencePlan> {};
    plans.reserve(obligations.size());
    for (auto const& obligation : obligations) {
        plans.push_back(plan_dynamic_array_cleanup_sequence(obligation));
    }
    return plans;
}

auto format_dynamic_array_cleanup_obligation(
    DynamicArrayCleanupObligation const& obligation
) -> std::string {
    auto const& plan = obligation.descriptor_cleanup;
    auto output = std::ostringstream {};
    output << "dynamic array cleanup obligation " << obligation.cleanup_symbol_name;
    output << " owner " << plan.owner_name;
    output << " source " << plan.source_type_name;
    output << " element " << plan.element_source_type_name;
    output << " descriptor " << plan.descriptor_storage_name;
    if (plan.source_line != 0) {
        output << " origin line " << plan.source_line;
    }
    output << " actions " << obligation.actions.size();
    output << " descriptor deallocation ";
    output << (obligation.requires_descriptor_deallocation ? "required" : "not required");
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_cleanup_obligation_report(
    std::vector<DynamicArrayCleanupObligation> const& obligations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(obligations.size());
    for (auto const& obligation : obligations) {
        report.push_back(format_dynamic_array_cleanup_obligation(obligation));
    }
    return report;
}

auto format_dynamic_array_cleanup_sequence_plan(
    DynamicArrayCleanupSequencePlan const& plan
) -> std::string {
    auto output = std::ostringstream {};
    output << "dynamic array cleanup sequence " << plan.obligation.cleanup_symbol_name;
    output << " owner " << plan.obligation.descriptor_cleanup.owner_name;
    output << " phases";
    for (auto const& phase : plan.phases) {
        output << " [" << phase << "]";
    }
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_cleanup_sequence_plan_report(
    std::vector<DynamicArrayCleanupSequencePlan> const& plans
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(plans.size());
    for (auto const& plan : plans) {
        report.push_back(format_dynamic_array_cleanup_sequence_plan(plan));
    }
    return report;
}

auto verify_dynamic_array_cleanup_sequence_plan(
    DynamicArrayCleanupSequencePlan const& plan
) -> DynamicArrayCleanupSequenceVerification {
    auto verification = DynamicArrayCleanupSequenceVerification {
        .cleanup_symbol_name = plan.obligation.cleanup_symbol_name,
    };
    auto const& phases = plan.phases;
    if (phases.empty()) {
        verification.errors.push_back("missing cleanup phases");
        return verification;
    }
    if (phases.front() != "load descriptor") {
        verification.errors.push_back("first phase must load descriptor");
    }
    if (plan.obligation.requires_descriptor_deallocation) {
        if (phases.back() != "deallocate descriptor storage") {
            verification.errors.push_back("last phase must deallocate descriptor storage");
        }
    }
    auto drop_phase = std::ranges::find(phases, "drop initialized elements");
    auto has_drop_phase = drop_phase != phases.end();
    if (!plan.obligation.actions.empty() && !has_drop_phase) {
        verification.errors.push_back("owned element cleanup requires initialized-element drop phase");
    }
    if (plan.obligation.actions.empty() && has_drop_phase) {
        verification.errors.push_back("scalar or non-owning cleanup must not drop initialized elements");
    }
    if (has_drop_phase) {
        if (drop_phase == phases.begin()) {
            verification.errors.push_back("initialized-element drop phase must follow descriptor load");
        }
        auto deallocate_phase = std::ranges::find(phases, "deallocate descriptor storage");
        if (deallocate_phase != phases.end() && deallocate_phase < drop_phase) {
            verification.errors.push_back("initialized-element drop phase must precede descriptor deallocation");
        }
    }
    return verification;
}

auto verify_dynamic_array_cleanup_sequence_plans(
    std::vector<DynamicArrayCleanupSequencePlan> const& plans
) -> std::vector<DynamicArrayCleanupSequenceVerification> {
    auto verifications = std::vector<DynamicArrayCleanupSequenceVerification> {};
    verifications.reserve(plans.size());
    for (auto const& plan : plans) {
        verifications.push_back(verify_dynamic_array_cleanup_sequence_plan(plan));
    }
    return verifications;
}

auto dynamic_array_cleanup_sequence_verification_passed(
    DynamicArrayCleanupSequenceVerification const& verification
) -> bool {
    return verification.errors.empty();
}

auto dynamic_array_cleanup_sequence_verification_report_passed(
    std::vector<DynamicArrayCleanupSequenceVerification> const& verifications
) -> bool {
    return std::ranges::all_of(verifications, dynamic_array_cleanup_sequence_verification_passed);
}

auto format_dynamic_array_cleanup_sequence_verification(
    DynamicArrayCleanupSequenceVerification const& verification
) -> std::string {
    auto output = std::ostringstream {};
    output << "dynamic array cleanup sequence verification " << verification.cleanup_symbol_name;
    if (verification.errors.empty()) {
        output << " passed (metadata only)";
        return output.str();
    }
    output << " failed";
    for (auto const& error : verification.errors) {
        output << " [" << error << "]";
    }
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_cleanup_sequence_verification_report(
    std::vector<DynamicArrayCleanupSequenceVerification> const& verifications
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(verifications.size());
    for (auto const& verification : verifications) {
        report.push_back(format_dynamic_array_cleanup_sequence_verification(verification));
    }
    return report;
}

auto format_dynamic_array_cleanup_emission_gate(
    DynamicArrayCleanupSequenceVerification const& verification
) -> std::string {
    auto output = std::ostringstream {};
    output << "dynamic array cleanup emission gate " << verification.cleanup_symbol_name;
    if (dynamic_array_cleanup_sequence_verification_passed(verification)) {
        output << " allowed (metadata only)";
        return output.str();
    }
    output << " blocked";
    for (auto const& error : verification.errors) {
        output << " [" << error << "]";
    }
    output << " (metadata only)";
    return output.str();
}

auto format_dynamic_array_cleanup_emission_gate_report(
    std::vector<DynamicArrayCleanupSequenceVerification> const& verifications
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(verifications.size());
    for (auto const& verification : verifications) {
        report.push_back(format_dynamic_array_cleanup_emission_gate(verification));
    }
    return report;
}

auto plan_bound_dynamic_array_parameter_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::optional<std::vector<BoundDynamicArrayParameterCleanupPlan>> {
    auto plans = std::vector<BoundDynamicArrayParameterCleanupPlan> {};
    if (!dynamic_array_parameter_descriptors_enabled(context.options) ||
        !dynamic_array_cleanup_emission_enabled(context.options)) {
        return plans;
    }

    auto names = session.state.parameter_names;
    std::ranges::sort(names);

    for (auto const& name : names) {
        if (name == "this") {
            continue;
        }
        if (is_owned_binding_consumed(session.state.ownership_transfers, name)) {
            continue;
        }
        auto const& source_type_name = session.state.source_type_names.at(name);
        auto sequence = dynamic_sequence_source_type(source_type_name);
        if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array) {
            continue;
        }

        auto storage = aggregate_storage_for_name(name, session.state);
        if (!storage.has_value()) {
            continue;
        }

        if (!context.options.dynamic_array_descriptor_lifetime_plans.empty()) {
            auto const has_lifetime_plan = matching_bound_dynamic_array_parameter_lifetime_plan(
                    context.options.dynamic_array_descriptor_lifetime_plans,
                    name,
                    source_type_name,
                    *storage
                ) != nullptr;
            if (!has_lifetime_plan &&
                !dynamic_array_parameter_element_cleanup_proven(source_type_name, context.options)) {
                continue;
            }
        } else if (!has_matching_dynamic_array_descriptor_summary_binding(
                       session.semantics,
                       name,
                       source_type_name,
                       semantics::DynamicArrayDescriptorBindingKind::parameter_binding
                   ) &&
                   !dynamic_array_parameter_element_cleanup_proven(source_type_name, context.options)) {
            continue;
        }

        auto descriptor_cleanup = plan_dynamic_array_descriptor_cleanup(name, source_type_name, context.lowering);
        if (!descriptor_cleanup.has_value()) {
            return std::nullopt;
        }
        descriptor_cleanup->descriptor_storage_name = *storage;
        descriptor_cleanup->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor;

        auto drop_symbol_name = std::optional<std::string> {};
        if (!is_scalar_or_nonowning_source_type(sequence->element_source_type_name)) {
            drop_symbol_name = authorized_element_drop_symbol_name(
                name,
                *descriptor_cleanup,
                context.options
            );
            if (!drop_symbol_name.has_value()) {
                continue;
            }
        }

        auto lifetime_plan = plan_dynamic_array_bound_parameter_lifetime(
            name,
            source_type_name,
            *storage,
            is_scalar_or_nonowning_source_type(sequence->element_source_type_name) || drop_symbol_name.has_value(),
            context.lowering
        );
        if (!lifetime_plan.has_value()) {
            continue;
        }
        descriptor_cleanup = std::move(lifetime_plan->descriptor_cleanup);

        auto actions = std::vector<PlannedDropAction> {};
        if (drop_symbol_name.has_value()) {
            actions.push_back(dynamic_array_parameter_drop_action(name, *descriptor_cleanup));
        }
        auto obligation = DynamicArrayCleanupObligation {
            .cleanup_symbol_name = dynamic_array_cleanup_symbol_name(plans.size()),
            .descriptor_cleanup = *descriptor_cleanup,
            .actions = std::move(actions),
            .requires_descriptor_deallocation = true,
        };
        auto sequence_plan = plan_dynamic_array_cleanup_sequence(obligation);
        auto sequence_verification = verify_dynamic_array_cleanup_sequence_plan(sequence_plan);
        plans.push_back(BoundDynamicArrayParameterCleanupPlan {
            .descriptor_cleanup = std::move(*descriptor_cleanup),
            .element_drop_symbol_name = std::move(drop_symbol_name),
            .sequence_plan = std::move(sequence_plan),
            .sequence_verification = std::move(sequence_verification),
        });
    }
    return plans;
}

auto prove_dynamic_array_cleanup_emission_capability(
    bool emission_enabled,
    std::vector<DynamicArrayDescriptorCleanupPlan> const& descriptor_cleanup_plans,
    std::vector<DynamicArrayCleanupSequenceVerification> const& sequence_verifications,
    std::vector<DynamicArrayCleanupObligation> const& obligations,
    std::vector<semantics::DropLoweringAuthorization> const& semantic_drop_lowering_authorizations
) -> DynamicArrayCleanupEmissionCapability {
    auto cleanup_pairs = std::vector<std::string> {};
    auto cleanup_operation_names = std::vector<std::string> {};
    auto cleanup_owner_names = std::vector<std::string> {};
    auto element_drop_pairs = std::vector<std::string> {};
    auto missing_element_drop_pairs = std::vector<std::string> {};
    cleanup_pairs.reserve(obligations.size());
    cleanup_operation_names.reserve(obligations.size());
    cleanup_owner_names.reserve(obligations.size());
    for (auto const& obligation : obligations) {
        cleanup_pairs.push_back(
            obligation.descriptor_cleanup.owner_name + ":" + obligation.cleanup_symbol_name
        );
        cleanup_operation_names.push_back(obligation.cleanup_symbol_name);
        cleanup_owner_names.push_back(obligation.descriptor_cleanup.owner_name);
        for (auto const& action : obligation.actions) {
            auto const action_pair =
                obligation.descriptor_cleanup.owner_name + ":" + action.capture_name + ":" + action.symbol_name;
            if (dynamic_array_cleanup_action_authorized(action, semantic_drop_lowering_authorizations)) {
                element_drop_pairs.push_back(action_pair);
            } else {
                missing_element_drop_pairs.push_back(action_pair);
            }
        }
    }
    return DynamicArrayCleanupEmissionCapability {
        .cleanup_pairs = std::move(cleanup_pairs),
        .cleanup_operation_names = std::move(cleanup_operation_names),
        .cleanup_owner_names = std::move(cleanup_owner_names),
        .element_drop_pairs = std::move(element_drop_pairs),
        .missing_element_drop_pairs = std::move(missing_element_drop_pairs),
        .emission_enabled = emission_enabled,
        .descriptor_storage_bound = std::ranges::all_of(descriptor_cleanup_plans, [](auto const& plan) {
        auto storage_status_bound =
            plan.descriptor_storage_status ==
                    DynamicArrayDescriptorStorageStatus::audit_parameter_descriptor ||
                plan.descriptor_storage_status ==
                    DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor ||
                plan.descriptor_storage_status ==
                    DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
            return storage_status_bound && !plan.descriptor_storage_name.empty();
        }),
        .sequence_verified = dynamic_array_cleanup_sequence_verification_report_passed(sequence_verifications),
        .element_cleanup_authorized_or_not_required = std::ranges::all_of(obligations, [&](auto const& obligation) {
            return obligation.actions.empty() ||
                std::ranges::all_of(obligation.actions, [&](auto const& action) {
                    return dynamic_array_cleanup_action_authorized(
                        action,
                        semantic_drop_lowering_authorizations
                    );
                });
        }),
        .descriptor_deallocation_authorized = std::ranges::all_of(obligations, [](auto const& obligation) {
            return obligation.requires_descriptor_deallocation;
        }),
    };
}

auto prove_bound_dynamic_array_parameter_cleanup_emission_capability(
    LoweringEmissionContext const& context,
    std::vector<BoundDynamicArrayParameterCleanupPlan> const& plans
) -> DynamicArrayCleanupEmissionCapability {
    auto descriptor_cleanup_plans = std::vector<DynamicArrayDescriptorCleanupPlan> {};
    auto sequence_verifications = std::vector<DynamicArrayCleanupSequenceVerification> {};
    auto obligations = std::vector<DynamicArrayCleanupObligation> {};
    descriptor_cleanup_plans.reserve(plans.size());
    sequence_verifications.reserve(plans.size());
    obligations.reserve(plans.size());
    for (auto const& plan : plans) {
        descriptor_cleanup_plans.push_back(plan.descriptor_cleanup);
        sequence_verifications.push_back(plan.sequence_verification);
        obligations.push_back(plan.sequence_plan.obligation);
    }
    return prove_dynamic_array_cleanup_emission_capability(
        dynamic_array_parameter_descriptors_enabled(context.options) &&
            dynamic_array_cleanup_emission_enabled(context.options),
        descriptor_cleanup_plans,
        sequence_verifications,
        obligations,
        synthetic_dynamic_array_parameter_cleanup_authorizations(plans)
    );
}

auto plan_local_dynamic_array_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::optional<std::vector<LocalDynamicArrayCleanupPlan>> {
    auto plans = std::vector<LocalDynamicArrayCleanupPlan> {};
    if (!context.options.enable_dynamic_array_construction_lowering ||
        !dynamic_array_cleanup_emission_enabled(context.options)) {
        return plans;
    }

    for (auto const& descriptor_cleanup : session.state.dynamic_array_local_cleanup_plans) {
        if (is_owned_binding_consumed(session.state.ownership_transfers, descriptor_cleanup.owner_name)) {
            continue;
        }
        if (descriptor_storage_finalized_by_computed_cleanup(
                session.state,
                descriptor_cleanup.descriptor_storage_name
            )) {
            continue;
        }
        auto obligation = plan_dynamic_array_descriptor_cleanup_obligation(
            descriptor_cleanup,
            plans.size()
        );
        auto drop_symbol_name = std::optional<std::string> {};
        if (!obligation.actions.empty()) {
            drop_symbol_name = authorized_descriptor_element_drop_symbol_name(
                obligation,
                context.options
            );
            if (!drop_symbol_name.has_value()) {
                continue;
            }
        }
        auto sequence_plan = plan_dynamic_array_cleanup_sequence(obligation);
        auto sequence_verification = verify_dynamic_array_cleanup_sequence_plan(sequence_plan);
        plans.push_back(LocalDynamicArrayCleanupPlan {
            .descriptor_cleanup = descriptor_cleanup,
            .element_drop_symbol_name = std::move(drop_symbol_name),
            .sequence_plan = std::move(sequence_plan),
            .sequence_verification = std::move(sequence_verification),
        });
    }
    return plans;
}

auto prove_local_dynamic_array_cleanup_emission_capability(
    LoweringEmissionContext const& context,
    std::vector<LocalDynamicArrayCleanupPlan> const& plans
) -> DynamicArrayCleanupEmissionCapability {
    auto descriptor_cleanup_plans = std::vector<DynamicArrayDescriptorCleanupPlan> {};
    auto sequence_verifications = std::vector<DynamicArrayCleanupSequenceVerification> {};
    auto obligations = std::vector<DynamicArrayCleanupObligation> {};
    descriptor_cleanup_plans.reserve(plans.size());
    sequence_verifications.reserve(plans.size());
    obligations.reserve(plans.size());
    for (auto const& plan : plans) {
        descriptor_cleanup_plans.push_back(plan.descriptor_cleanup);
        sequence_verifications.push_back(plan.sequence_verification);
        obligations.push_back(plan.sequence_plan.obligation);
    }
    return prove_dynamic_array_cleanup_emission_capability(
        context.options.enable_dynamic_array_construction_lowering &&
            dynamic_array_cleanup_emission_enabled(context.options),
        descriptor_cleanup_plans,
        sequence_verifications,
        obligations,
        context.options.semantic_drop_lowering_authorizations
    );
}

auto dynamic_array_cleanup_emission_capability_proven(
    DynamicArrayCleanupEmissionCapability const& capability
) -> bool {
    return !capability.cleanup_pairs.empty() &&
        !capability.cleanup_operation_names.empty() &&
        !capability.cleanup_owner_names.empty() &&
        capability.emission_enabled &&
        capability.descriptor_storage_bound &&
        capability.sequence_verified &&
        capability.element_cleanup_authorized_or_not_required &&
        capability.descriptor_deallocation_authorized;
}

auto format_dynamic_array_cleanup_emission_capability(
    DynamicArrayCleanupEmissionCapability const& capability
) -> std::string {
    auto const status = [](bool value) {
        return value ? "ok" : "missing";
    };
    auto output = std::ostringstream {};
    output << "dynamic array cleanup emission capability ";
    output << (dynamic_array_cleanup_emission_capability_proven(capability) ? "proven" : "blocked");
    if (!capability.cleanup_pairs.empty()) {
        output << " cleanup-pairs";
        for (auto const& cleanup_pair : capability.cleanup_pairs) {
            output << " [" << cleanup_pair << "]";
        }
    }
    if (!capability.cleanup_operation_names.empty()) {
        output << " cleanup-operations";
        for (auto const& cleanup_operation_name : capability.cleanup_operation_names) {
            output << " [" << cleanup_operation_name << "]";
        }
    }
    if (!capability.cleanup_owner_names.empty()) {
        output << " cleanup-owners";
        for (auto const& cleanup_owner_name : capability.cleanup_owner_names) {
            output << " [" << cleanup_owner_name << "]";
        }
    }
    if (!capability.element_drop_pairs.empty()) {
        output << " element-drop-pairs";
        for (auto const& element_drop_pair : capability.element_drop_pairs) {
            output << " [" << element_drop_pair << "]";
        }
    }
    if (!capability.missing_element_drop_pairs.empty()) {
        output << " missing-element-drop-pairs";
        for (auto const& missing_element_drop_pair : capability.missing_element_drop_pairs) {
            output << " [" << missing_element_drop_pair << "]";
        }
    }
    output << " [emission " << status(capability.emission_enabled) << "]";
    output << " [descriptor storage " << status(capability.descriptor_storage_bound) << "]";
    output << " [sequence verification " << status(capability.sequence_verified) << "]";
    output << " [element cleanup " << status(capability.element_cleanup_authorized_or_not_required) << "]";
    output << " [descriptor deallocation " << status(capability.descriptor_deallocation_authorized) << "]";
    output << " (metadata only)";
    return output.str();
}

auto emit_bound_dynamic_array_parameter_cleanup_plans(
    DynamicArrayCleanupEmissionCapability const& capability,
    std::vector<BoundDynamicArrayParameterCleanupPlan> const& plans,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool {
    if (!dynamic_array_cleanup_emission_capability_proven(capability)) {
        return false;
    }

    record_emitted_dynamic_array_cleanup_reports(capability, plans, session);
    for (auto const& plan : plans) {
        auto prefix = "%" + plan.descriptor_cleanup.owner_name + ".dynamic_array_cleanup" +
            std::to_string(session.state.next_temporary_index++);
        auto label_prefix = prefix.substr(1);
        output << "  br label %" << label_prefix << ".cleanup.entry\n";
        output << label_prefix << ".cleanup.entry:\n";
        output << emit_dynamic_array_descriptor_load(
            prefix + ".descriptor",
            plan.descriptor_cleanup.descriptor_storage_name
        );
        output << emit_dynamic_array_descriptor_cleanup_sequence_with_optional_drop_calls(
            plan.descriptor_cleanup,
            prefix + ".descriptor",
            prefix,
            plan.element_drop_symbol_name
        );
        auto finalization_plan = plan_consumed_descriptor_finalization(
            plan.descriptor_cleanup.owner_name,
            plan.descriptor_cleanup.descriptor_storage_name,
            plan.sequence_plan.obligation.cleanup_symbol_name
        );
        finalization_plan.function_symbol_name = session.enclosing_symbol_name;
        auto const finalization_readiness = plan_consumed_descriptor_finalization_readiness(finalization_plan);
        if (finalization_readiness.ready) {
            session.state.consumed_descriptor_finalization_plans.push_back(finalization_plan);
            output << emit_dynamic_array_descriptor_finalization(
                finalization_plan.descriptor_storage_name
            );
        }
    }
    return true;
}

auto emit_choice_dynamic_array_payload_cleanups_for_owner_filter(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::unordered_set<std::string> const* owner_filter
) -> bool {
    if (!context.options.enable_dynamic_array_construction_lowering ||
        !dynamic_array_cleanup_emission_enabled(context.options)) {
        return true;
    }

    auto names = std::vector<std::string> {};
    names.reserve(session.state.source_type_names.size());
    for (auto const& [name, source_type_name] : session.state.source_type_names) {
        if (context.lowering.choices.contains(source_type_name)) {
            names.push_back(name);
        }
    }
    std::ranges::sort(names);

    for (auto const& name : names) {
        if (owner_filter != nullptr && !owner_filter->contains(name)) {
            continue;
        }
        if (is_owned_binding_consumed(session.state.ownership_transfers, name)) {
            continue;
        }
        auto const source_type = session.state.source_type_names.find(name);
        if (source_type == session.state.source_type_names.end()) {
            continue;
        }
        auto const choice = context.lowering.choices.find(source_type->second);
        if (choice == context.lowering.choices.end() || choice->second.llvm_type_name == "i32") {
            continue;
        }
        auto storage = aggregate_storage_for_name(name, session.state);
        if (!storage.has_value()) {
            continue;
        }

        auto choice_value = "%" + name + ".choice_dynamic_array_cleanup" +
            std::to_string(session.state.next_temporary_index++);
        output << "  " << choice_value << " = load " << choice->second.llvm_type_name
               << ", ptr " << *storage << "\n";
        auto tag_value = "%" + name + ".choice_dynamic_array_cleanup" +
            std::to_string(session.state.next_temporary_index++) + ".tag";
        output << "  " << tag_value << " = extractvalue " << choice->second.llvm_type_name
               << " " << choice_value << ", 0\n";

        for (auto const& variant : choice->second.variants) {
            for (auto const& payload : variant.payloads) {
                auto const payload_field_type = choice_payload_field_type(choice->second);
                if (!payload_field_type.has_value() || variant.lowered_payload_type.empty()) {
                    continue;
                }
                auto const owner_name = name + "." + variant.name + "." + payload.name;
                auto descriptor_cleanups = collect_choice_payload_descriptor_cleanups(
                    owner_name,
                    payload.source_type_name,
                    payload.llvm_type,
                    context.lowering
                );
                if (!descriptor_cleanups.has_value()) {
                    return false;
                }
                if (descriptor_cleanups->empty()) {
                    continue;
                }

                for (auto& descriptor_cleanup : *descriptor_cleanups) {
                    if (is_owned_binding_consumed(
                            session.state.ownership_transfers,
                            descriptor_cleanup.descriptor_cleanup.owner_name
                        )) {
                        continue;
                    }
                    descriptor_cleanup.descriptor_cleanup.descriptor_storage_name = *storage;
                    auto obligation = plan_dynamic_array_descriptor_cleanup_obligation(
                        descriptor_cleanup.descriptor_cleanup,
                        session.state.emitted_dynamic_array_cleanup_obligations.size()
                    );
                    auto drop_symbol_name = std::optional<std::string> {};
                    if (!obligation.actions.empty()) {
                        drop_symbol_name = authorized_choice_payload_element_drop_symbol_name(
                            obligation,
                            context.options
                        );
                        if (!drop_symbol_name.has_value()) {
                            return false;
                        }
                    }

                    auto sequence_plan = plan_dynamic_array_cleanup_sequence(obligation);
                    auto sequence_verification = verify_dynamic_array_cleanup_sequence_plan(sequence_plan);
                    if (!dynamic_array_cleanup_sequence_verification_passed(sequence_verification)) {
                        return false;
                    }

                    session.state.emitted_dynamic_array_cleanup_obligations.push_back(obligation);
                    session.state.emitted_dynamic_array_cleanup_sequence_plans.push_back(sequence_plan);
                    session.state.emitted_dynamic_array_cleanup_sequence_verifications.push_back(sequence_verification);

                    auto const descriptor_owner_name = descriptor_cleanup.descriptor_cleanup.owner_name;
                    auto tag_check = "%" + descriptor_owner_name + ".choice_dynamic_array_cleanup" +
                        std::to_string(session.state.next_temporary_index++) + ".is_active";
                    auto block_prefix = descriptor_owner_name + ".choice_dynamic_array_cleanup" +
                        std::to_string(next_llvm_block_index(session.state.next_block_index));
                    auto cleanup_block = block_prefix + ".cleanup.entry";
                    auto after_block = block_prefix + ".after";
                    output << "  " << tag_check << " = icmp eq i32 " << tag_value
                           << ", " << variant.tag << "\n";
                    output << "  br i1 " << tag_check << ", label %" << cleanup_block
                           << ", label %" << after_block << "\n";
                    output << cleanup_block << ":\n";
                    auto variant_payload_value =
                        "%" + descriptor_owner_name + ".choice_dynamic_array_cleanup.payload";
                    if (*payload_field_type == variant.lowered_payload_type) {
                        output << "  " << variant_payload_value << " = extractvalue "
                               << choice->second.llvm_type_name << " " << choice_value << ", 1\n";
                    } else {
                        auto payload_storage_value =
                            "%" + descriptor_owner_name + ".choice_dynamic_array_cleanup.payload.storage.value";
                        auto payload_storage_addr =
                            "%" + descriptor_owner_name + ".choice_dynamic_array_cleanup.payload.storage.addr";
                        output << "  " << payload_storage_value << " = extractvalue "
                               << choice->second.llvm_type_name << " " << choice_value << ", 1\n";
                        output << "  " << payload_storage_addr << " = alloca " << *payload_field_type
                               << ", align 8\n";
                        output << "  store " << *payload_field_type << " " << payload_storage_value
                               << ", ptr " << payload_storage_addr << ", align 8\n";
                        output << "  " << variant_payload_value << " = load "
                               << variant.lowered_payload_type << ", ptr " << payload_storage_addr
                               << ", align 8\n";
                    }
                    auto payload_value = variant_payload_value;
                    if (variant.payloads.size() != 1 || variant.lowered_payload_type != payload.llvm_type) {
                        payload_value = "%" + descriptor_owner_name + ".choice_dynamic_array_cleanup.payload.value";
                        output << "  " << payload_value << " = extractvalue "
                               << variant.lowered_payload_type << " " << variant_payload_value
                               << ", " << payload.index << "\n";
                    }
                    auto descriptor_value = emit_aggregate_extraction_chain(
                        payload_value,
                        descriptor_cleanup.extraction_steps,
                        "%" + descriptor_owner_name + ".choice_dynamic_array_cleanup.descriptor",
                        session,
                        output
                    );
                    auto cleanup_prefix = "%" + block_prefix;
                    output << emit_dynamic_array_descriptor_cleanup_sequence_with_optional_drop_calls(
                        descriptor_cleanup.descriptor_cleanup,
                        descriptor_value,
                        cleanup_prefix,
                        drop_symbol_name
                    );
                    output << "  br label %" << after_block << "\n";
                    output << after_block << ":\n";
                    session.state.current_block = after_block;
                }
            }
        }
        mark_owned_binding_consumed(session.state.ownership_transfers, name);
    }
    return true;
}

auto emit_local_dynamic_array_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool {
    auto plans = plan_local_dynamic_array_cleanups(context, session);
    if (!plans.has_value()) {
        return false;
    }
    if (plans->empty()) {
        return true;
    }

    auto capability = prove_local_dynamic_array_cleanup_emission_capability(context, *plans);
    if (!dynamic_array_cleanup_emission_capability_proven(capability)) {
        return false;
    }

    record_emitted_dynamic_array_cleanup_reports(capability, *plans, session);
    for (auto const& plan : *plans) {
        auto prefix = "%" + plan.descriptor_cleanup.owner_name + ".dynamic_array_cleanup" +
            std::to_string(session.state.next_temporary_index++);
        auto label_prefix = prefix.substr(1);
        output << "  br label %" << label_prefix << ".cleanup.entry\n";
        output << label_prefix << ".cleanup.entry:\n";
        output << emit_dynamic_array_descriptor_load(
            prefix + ".descriptor",
            plan.descriptor_cleanup.descriptor_storage_name
        );
        output << emit_dynamic_array_descriptor_cleanup_sequence_with_optional_drop_calls(
            plan.descriptor_cleanup,
            prefix + ".descriptor",
            prefix,
            plan.element_drop_symbol_name
        );
        auto finalization_plan = plan_consumed_descriptor_finalization(
            plan.descriptor_cleanup.owner_name,
            plan.descriptor_cleanup.descriptor_storage_name,
            plan.sequence_plan.obligation.cleanup_symbol_name
        );
        finalization_plan.function_symbol_name = session.enclosing_symbol_name;
        auto const finalization_readiness = plan_consumed_descriptor_finalization_readiness(finalization_plan);
        if (finalization_readiness.ready) {
            session.state.consumed_descriptor_finalization_plans.push_back(finalization_plan);
            output << emit_dynamic_array_descriptor_finalization(
                finalization_plan.descriptor_storage_name
            );
        }
    }
    return true;
}

auto emit_local_dynamic_array_cleanups_for_names(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::vector<std::string> const& owner_names
) -> bool {
    if (owner_names.empty()) {
        return true;
    }

    auto owner_filter = std::unordered_set<std::string> {owner_names.begin(), owner_names.end()};
    auto saved_cleanup_plans = session.state.dynamic_array_local_cleanup_plans;
    auto filtered_cleanup_plans = std::vector<DynamicArrayDescriptorCleanupPlan> {};
    filtered_cleanup_plans.reserve(saved_cleanup_plans.size());
    for (auto const& cleanup_plan : saved_cleanup_plans) {
        if (owner_filter.contains(cleanup_plan.owner_name)) {
            filtered_cleanup_plans.push_back(cleanup_plan);
        }
    }
    if (filtered_cleanup_plans.empty()) {
        return true;
    }

    auto planned_session = session;
    planned_session.state.dynamic_array_local_cleanup_plans = filtered_cleanup_plans;
    auto plans = plan_local_dynamic_array_cleanups(context, planned_session);
    if (!plans.has_value()) {
        return false;
    }
    if (plans->empty()) {
        return true;
    }

    auto const cleanup_ordinal_start = session.state.next_temporary_index;
    auto const& final_cleanup_plan = plans->back().descriptor_cleanup;
    auto final_cleanup_ordinal = cleanup_ordinal_start + plans->size() - 1;
    auto cleanup_exit_block = final_cleanup_plan.owner_name + ".dynamic_array_cleanup" +
        std::to_string(final_cleanup_ordinal);
    cleanup_exit_block += is_scalar_or_nonowning_source_type(final_cleanup_plan.element_source_type_name)
        ? ".cleanup.entry"
        : ".drop.done";

    session.state.dynamic_array_local_cleanup_plans = filtered_cleanup_plans;
    if (!emit_local_dynamic_array_cleanups(context, session, output)) {
        session.state.dynamic_array_local_cleanup_plans = std::move(saved_cleanup_plans);
        return false;
    }
    session.state.dynamic_array_local_cleanup_plans = std::move(saved_cleanup_plans);
    for (auto const& plan : *plans) {
        mark_owned_binding_consumed(session.state.ownership_transfers, plan.descriptor_cleanup.owner_name);
    }
    session.state.current_block = std::move(cleanup_exit_block);
    return true;
}

auto emit_choice_dynamic_array_payload_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool {
    return emit_choice_dynamic_array_payload_cleanups_for_owner_filter(
        context,
        session,
        output,
        nullptr
    );
}

auto emit_choice_dynamic_array_payload_cleanups_for_names(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::vector<std::string> const& owner_names
) -> bool {
    auto owner_filter = std::unordered_set<std::string> {owner_names.begin(), owner_names.end()};
    return emit_choice_dynamic_array_payload_cleanups_for_owner_filter(
        context,
        session,
        output,
        &owner_filter
    );
}

auto emit_bound_dynamic_array_parameter_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool {
    auto plans = plan_bound_dynamic_array_parameter_cleanups(context, session);
    if (!plans.has_value()) {
        return false;
    }
    if (plans->empty()) {
        return true;
    }

    auto capability = prove_bound_dynamic_array_parameter_cleanup_emission_capability(context, *plans);
    return emit_bound_dynamic_array_parameter_cleanup_plans(capability, *plans, session, output);
}

auto emit_bound_dynamic_array_parameter_cleanups_for_names(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::vector<std::string> const& owner_names
) -> bool {
    if (owner_names.empty()) {
        return true;
    }
    auto plans = plan_bound_dynamic_array_parameter_cleanups(context, session);
    if (!plans.has_value()) {
        return false;
    }
    if (plans->empty()) {
        return true;
    }

    auto owner_filter = std::unordered_set<std::string> {owner_names.begin(), owner_names.end()};
    auto filtered_plans = std::vector<BoundDynamicArrayParameterCleanupPlan> {};
    for (auto& plan : *plans) {
        if (owner_filter.contains(plan.descriptor_cleanup.owner_name)) {
            filtered_plans.push_back(std::move(plan));
        }
    }
    if (filtered_plans.empty()) {
        return true;
    }

    auto const cleanup_ordinal_start = session.state.next_temporary_index;
    auto const& final_cleanup_plan = filtered_plans.back().descriptor_cleanup;
    auto final_cleanup_ordinal = cleanup_ordinal_start + filtered_plans.size() - 1;
    auto cleanup_exit_block = final_cleanup_plan.owner_name + ".dynamic_array_cleanup" +
        std::to_string(final_cleanup_ordinal);
    cleanup_exit_block += is_scalar_or_nonowning_source_type(final_cleanup_plan.element_source_type_name)
        ? ".cleanup.entry"
        : ".drop.done";

    auto capability = prove_bound_dynamic_array_parameter_cleanup_emission_capability(context, filtered_plans);
    if (!emit_bound_dynamic_array_parameter_cleanup_plans(capability, filtered_plans, session, output)) {
        return false;
    }
    for (auto const& plan : filtered_plans) {
        mark_owned_binding_consumed(session.state.ownership_transfers, plan.descriptor_cleanup.owner_name);
    }
    session.state.current_block = std::move(cleanup_exit_block);
    return true;
}

}  // namespace orison::lowering
