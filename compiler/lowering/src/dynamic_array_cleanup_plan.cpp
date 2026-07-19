#include "orison/lowering/dynamic_array_cleanup_plan.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include "orison/semantics/drop_model.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <utility>

namespace orison::lowering {
namespace {

auto dynamic_array_cleanup_symbol_name(std::size_t ordinal) -> std::string {
    auto output = std::ostringstream {};
    output << "__orison_dynamic_array_cleanup." << ordinal;
    return output.str();
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
    if (!context.options.test_only_enable_dynamic_array_parameter_descriptors ||
        !context.options.test_only_emit_bound_dynamic_array_parameter_cleanups) {
        return plans;
    }

    auto names = std::vector<std::string> {};
    names.reserve(session.state.source_type_names.size());
    for (auto const& [name, _] : session.state.source_type_names) {
        names.push_back(name);
    }
    std::ranges::sort(names);

    for (auto const& name : names) {
        auto const& source_type_name = session.state.source_type_names.at(name);
        auto sequence = dynamic_sequence_source_type(source_type_name);
        if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array) {
            continue;
        }

        auto storage = aggregate_storage_for_name(name, session.state);
        if (!storage.has_value()) {
            continue;
        }

        auto descriptor_cleanup = plan_dynamic_array_descriptor_cleanup(name, source_type_name, context.lowering);
        if (!descriptor_cleanup.has_value()) {
            return std::nullopt;
        }
        descriptor_cleanup->descriptor_storage_name = std::move(*storage);
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

auto emit_bound_dynamic_array_parameter_cleanup_plans(
    std::vector<BoundDynamicArrayParameterCleanupPlan> const& plans,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool {
    if (!std::ranges::all_of(plans, [](auto const& plan) {
            return dynamic_array_cleanup_sequence_verification_passed(plan.sequence_verification);
        })) {
        return false;
    }

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
    }
    return true;
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

    return emit_bound_dynamic_array_parameter_cleanup_plans(*plans, session, output);
}

}  // namespace orison::lowering
