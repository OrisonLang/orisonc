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

struct ChoicePayloadBinding {
    std::string binding_name;
    std::string variant_name;
    std::string payload_name;
    std::string payload_type;
    std::string source_type_name;
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
        pattern.arguments.size() != 1 ||
        pattern.arguments.front().kind != syntax::ExpressionKind::name) {
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
        if (variant.name == pattern.left->text && variant.payloads.size() == 1) {
            return ChoicePayloadBinding {
                .binding_name = pattern.arguments.front().text,
                .variant_name = variant.name,
                .payload_name = variant.payloads.front().name,
                .payload_type = variant.payloads.front().llvm_type,
                .source_type_name = variant.payloads.front().source_type_name,
            };
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

void bind_switch_payload_value(
    std::string const& binding_name,
    std::string const& payload_type,
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
    auto lowered_payload = LoweredExpression {
        .type = payload_type,
        .value = std::move(payload),
        .signedness = payload_signedness,
    };
    session.state.immutable_bindings[binding_name] = lowered_payload;
    bind_addressable_aggregate_value(binding_name, lowered_payload, session, output);
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

    auto transfer = owned_choice_payload_transfer(
        subject_expression.text,
        *subject_source_type_name,
        binding.variant_name,
        binding.payload_name,
        context.lowering
    );
    if (!transfer.has_value()) {
        return;
    }

    mark_owned_binding_consumed(session.state.ownership_transfers, std::move(transfer->binding_name));
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
    bind_switch_payload_value(
        choice_binding->binding_name,
        choice_binding->payload_type,
        std::move(choice_binding->source_type_name),
        subject,
        context,
        session,
        output
    );
}

}  // namespace orison::lowering
