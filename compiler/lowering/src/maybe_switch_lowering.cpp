#include "orison/lowering/maybe_switch_lowering.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

auto maybe_payload_type_for_switch_subject(std::string_view type) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"{ i1,"};
    if (!type.starts_with(prefix) || !type.ends_with("}")) {
        return std::nullopt;
    }

    auto payload = type.substr(prefix.size(), type.size() - prefix.size() - 1);
    while (!payload.empty() && payload.front() == ' ') {
        payload.remove_prefix(1);
    }
    while (!payload.empty() && payload.back() == ' ') {
        payload.remove_suffix(1);
    }
    return payload.empty() ? std::nullopt : std::optional<std::string> {std::string(payload)};
}

auto maybe_payload_binding_name(syntax::ExpressionSyntax const& pattern) -> std::optional<std::string> {
    if (pattern.kind != syntax::ExpressionKind::call ||
        pattern.left == nullptr ||
        pattern.left->kind != syntax::ExpressionKind::name ||
        pattern.left->text != "Some" ||
        pattern.arguments.size() != 1 ||
        pattern.arguments.front().kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }
    return pattern.arguments.front().text;
}

struct ChoicePayloadFieldBinding {
    std::string binding_name;
    std::string payload_name;
    std::string payload_type;
    std::string source_type_name;
    std::size_t payload_index = 0;
};

struct ChoicePayloadBinding {
    std::string variant_name;
    std::string variant_payload_type;
    std::vector<ChoicePayloadFieldBinding> payloads;
};

auto choice_payload_binding_for_switch_case(
    syntax::ExpressionSyntax const& pattern,
    LoweredExpression const& subject,
    LoweringContext const& context,
    std::optional<std::string_view> subject_source_type_name
) -> std::optional<ChoicePayloadBinding> {
    if (pattern.kind != syntax::ExpressionKind::call ||
        pattern.left == nullptr ||
        pattern.left->kind != syntax::ExpressionKind::name ||
        pattern.arguments.empty()) {
        return std::nullopt;
    }

    auto const* choice = static_cast<LoweredChoiceLayout const*>(nullptr);
    if (subject_source_type_name.has_value()) {
        auto found = context.choices.find(std::string(*subject_source_type_name));
        if (found != context.choices.end() && found->second.llvm_type_name == subject.type) {
            choice = &found->second;
        }
    }
    if (choice == nullptr) {
        choice = find_lowered_choice_layout_by_llvm_type(context, subject.type);
    }
    if (choice == nullptr) {
        return std::nullopt;
    }

    for (auto const& variant : choice->variants) {
        if (variant.name == pattern.left->text && variant.payloads.size() == pattern.arguments.size()) {
            auto binding = ChoicePayloadBinding {
                .variant_name = variant.name,
                .variant_payload_type = variant.lowered_payload_type,
            };
            binding.payloads.reserve(variant.payloads.size());
            for (auto index = std::size_t {0}; index < variant.payloads.size(); ++index) {
                if (pattern.arguments[index].kind != syntax::ExpressionKind::name) {
                    return std::nullopt;
                }
                binding.payloads.push_back(ChoicePayloadFieldBinding {
                    .binding_name = pattern.arguments[index].text,
                    .payload_name = variant.payloads[index].name,
                    .payload_type = variant.payloads[index].llvm_type,
                    .source_type_name = variant.payloads[index].source_type_name,
                    .payload_index = variant.payloads[index].index,
                });
            }
            return binding;
        }
    }
    return std::nullopt;
}

auto choice_layout_for_switch_subject(
    LoweringContext const& context,
    std::string_view llvm_type,
    std::optional<std::string_view> subject_source_type_name
) -> LoweredChoiceLayout const* {
    if (subject_source_type_name.has_value()) {
        auto found = context.choices.find(std::string(*subject_source_type_name));
        if (found != context.choices.end() && found->second.llvm_type_name == llvm_type) {
            return &found->second;
        }
    }
    return find_lowered_choice_layout_by_llvm_type(context, llvm_type);
}

auto choice_payload_field_type(LoweredChoiceLayout const& layout) -> std::optional<std::string_view> {
    constexpr auto prefix = std::string_view {"{ i32,"};
    if (!layout.llvm_type_name.starts_with(prefix) || !layout.llvm_type_name.ends_with("}")) {
        return std::nullopt;
    }
    auto field = std::string_view(layout.llvm_type_name).substr(
        prefix.size(),
        layout.llvm_type_name.size() - prefix.size() - 1
    );
    while (!field.empty() && field.front() == ' ') {
        field.remove_prefix(1);
    }
    while (!field.empty() && field.back() == ' ') {
        field.remove_suffix(1);
    }
    if (field.empty()) {
        return std::nullopt;
    }
    return field;
}

void seed_switch_payload_dynamic_array_cleanup(
    std::string const& binding_name,
    std::string const& source_type_name,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session
) {
    if (!context.options.enable_dynamic_array_construction_lowering ||
        !context.options.enable_dynamic_array_cleanup_emission ||
        dynamic_array_element_source_type_name(source_type_name) == std::nullopt) {
        return;
    }

    auto storage = aggregate_storage_for_name(binding_name, session.state);
    if (!storage.has_value()) {
        return;
    }

    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        binding_name,
        source_type_name,
        context.lowering
    );
    if (!cleanup_plan.has_value()) {
        return;
    }
    cleanup_plan->descriptor_storage_name = std::move(*storage);
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    session.state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));
}

void bind_switch_payload_value(
    std::string const& binding_name,
    std::string const& payload_type,
    std::string_view payload_field_type,
    std::optional<std::string> source_type_name,
    LoweredExpression const& subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) {
    auto payload = next_llvm_temporary_name(session.state.next_temporary_index);
    auto payload_signedness = IntegerSignedness::not_integer;
    if (source_type_name.has_value()) {
        if (auto lowered_type = lowered_type_for_source_type_name(*source_type_name, context.lowering)) {
            payload_signedness = lowered_type->signedness;
        }
        session.state.source_type_names[binding_name] = std::move(*source_type_name);
    }
    output << "  " << payload << " = extractvalue " << subject.type << " " << subject.value << ", 1\n";
    auto payload_value = std::move(payload);
    if (payload_field_type != payload_type) {
        auto payload_storage = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_storage << " = alloca " << payload_field_type << ", align 8\n";
        output << "  store " << payload_field_type << " " << payload_value << ", ptr "
               << payload_storage << ", align 8\n";
        payload_value = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_value << " = load " << payload_type << ", ptr "
               << payload_storage << ", align 8\n";
    }
    auto lowered_payload = LoweredExpression {
        .type = payload_type,
        .value = std::move(payload_value),
        .signedness = payload_signedness,
    };
    session.state.immutable_bindings[binding_name] = lowered_payload;
    bind_addressable_aggregate_value(binding_name, lowered_payload, session, output);
    if (source_type_name.has_value()) {
        seed_switch_payload_dynamic_array_cleanup(binding_name, *source_type_name, context, session);
    }
}

void bind_switch_payload_field_value(
    ChoicePayloadFieldBinding const& binding,
    std::string payload_value,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) {
    auto payload_signedness = IntegerSignedness::not_integer;
    if (auto lowered_type = lowered_type_for_source_type_name(binding.source_type_name, context.lowering)) {
        payload_signedness = lowered_type->signedness;
    }
    session.state.source_type_names[binding.binding_name] = binding.source_type_name;

    auto lowered_payload = LoweredExpression {
        .type = binding.payload_type,
        .value = std::move(payload_value),
        .signedness = payload_signedness,
    };
    session.state.immutable_bindings[binding.binding_name] = lowered_payload;
    bind_addressable_aggregate_value(binding.binding_name, lowered_payload, session, output);
    seed_switch_payload_dynamic_array_cleanup(
        binding.binding_name,
        binding.source_type_name,
        context,
        session
    );
}

void bind_choice_switch_payload_values(
    ChoicePayloadBinding const& binding,
    std::string_view payload_field_type,
    LoweredExpression const& subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) {
    if (binding.payloads.empty() || binding.variant_payload_type.empty()) {
        return;
    }

    auto payload = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << payload << " = extractvalue " << subject.type << " " << subject.value << ", 1\n";
    auto payload_value = std::move(payload);
    if (payload_field_type != binding.variant_payload_type) {
        auto payload_storage = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_storage << " = alloca " << payload_field_type << ", align 8\n";
        output << "  store " << payload_field_type << " " << payload_value << ", ptr "
               << payload_storage << ", align 8\n";
        payload_value = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_value << " = load " << binding.variant_payload_type << ", ptr "
               << payload_storage << ", align 8\n";
    }

    if (binding.payloads.size() == 1) {
        bind_switch_payload_field_value(binding.payloads.front(), std::move(payload_value), context, session, output);
        return;
    }

    for (auto const& payload_binding : binding.payloads) {
        auto payload_field_value = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_field_value << " = extractvalue " << binding.variant_payload_type
               << " " << payload_value << ", " << payload_binding.payload_index << "\n";
        bind_switch_payload_field_value(payload_binding, std::move(payload_field_value), context, session, output);
    }
}

void mark_consumed_choice_payload(
    syntax::ExpressionSyntax const& subject_expression,
    ChoicePayloadBinding const& binding,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::optional<std::string_view> subject_source_type_name
) {
    if (!subject_source_type_name.has_value() ||
        subject_expression.kind != syntax::ExpressionKind::name) {
        return;
    }

    for (auto const& payload : binding.payloads) {
        auto transfer = owned_choice_payload_transfer(
            subject_expression.text,
            *subject_source_type_name,
            binding.variant_name,
            payload.payload_name,
            context.lowering
        );
        if (!transfer.has_value()) {
            continue;
        }

        mark_owned_binding_consumed(session.state.ownership_transfers, std::move(transfer->binding_name));
    }
}

}  // namespace

auto is_maybe_switch_subject(LoweredType const& type) -> bool {
    return type.type.starts_with("{ i1,");
}

auto is_supported_switch_subject(
    LoweredType const& type,
    LoweringEmissionContext const& context,
    std::optional<std::string_view> subject_source_type_name
) -> bool {
    return type.type == "i1" ||
           is_integer_llvm_type(type.type) ||
           is_maybe_switch_subject(type) ||
           choice_layout_for_switch_subject(context.lowering, type.type, subject_source_type_name) != nullptr;
}

auto switch_subject_for_emit(
    LoweredExpression subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::optional<std::string_view> subject_source_type_name
) -> LoweredExpression {
    auto const* choice = choice_layout_for_switch_subject(
        context.lowering,
        subject.type,
        subject_source_type_name
    );
    if (!subject.type.starts_with("{ i1,") && choice == nullptr) {
        return subject;
    }
    if (choice != nullptr && choice->llvm_type_name == "i32") {
        return subject;
    }

    auto tag = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag << " = extractvalue " << subject.type << " " << subject.value << ", 0\n";
    return LoweredExpression {
        .type = subject.type.starts_with("{ i1,") ? "i1" : "i32",
        .value = std::move(tag),
        .signedness = subject.type.starts_with("{ i1,")
            ? IntegerSignedness::not_integer
            : IntegerSignedness::unsigned_integer,
    };
}

void bind_switch_payload(
    LoweredSwitchCasePlan const& planned_case,
    syntax::ExpressionSyntax const& subject_expression,
    LoweredExpression const& subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::optional<std::string_view> subject_source_type_name
) {
    if (planned_case.syntax == nullptr) {
        return;
    }
    auto payload_type = maybe_payload_type_for_switch_subject(subject.type);
    auto binding_name = maybe_payload_binding_name(planned_case.syntax->pattern);
    if (payload_type.has_value() && binding_name.has_value()) {
        auto source_type_name = subject_source_type_name.has_value()
            ? maybe_payload_source_type_name(*subject_source_type_name)
            : source_type_name_for_llvm_type(*payload_type, context.lowering);
        if (!source_type_name.has_value()) {
            source_type_name = source_type_name_for_llvm_type(*payload_type, context.lowering);
        }
        bind_switch_payload_value(
            *binding_name,
            *payload_type,
            *payload_type,
            std::move(source_type_name),
            subject,
            context,
            session,
            output
        );
        return;
    }

    auto choice_binding = choice_payload_binding_for_switch_case(
        planned_case.syntax->pattern,
        subject,
        context.lowering,
        subject_source_type_name
    );
    if (!choice_binding.has_value()) {
        return;
    }

    mark_consumed_choice_payload(
        subject_expression,
        *choice_binding,
        context,
        session,
        subject_source_type_name
    );
    auto const* choice_layout = choice_layout_for_switch_subject(
        context.lowering,
        subject.type,
        subject_source_type_name
    );
    auto payload_field_type = choice_layout == nullptr
        ? std::optional<std::string_view> {}
        : choice_payload_field_type(*choice_layout);
    if (!payload_field_type.has_value()) {
        return;
    }
    bind_choice_switch_payload_values(*choice_binding, *payload_field_type, subject, context, session, output);
}

}  // namespace orison::lowering
