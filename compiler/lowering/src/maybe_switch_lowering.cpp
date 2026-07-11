#include "orison/lowering/maybe_switch_lowering.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_names.hpp"
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
    std::string payload_type;
    std::string source_type_name;
};

auto choice_payload_binding_for_switch_case(
    syntax::ExpressionSyntax const& pattern,
    LoweredExpression const& subject,
    LoweringContext const& context
) -> std::optional<ChoicePayloadBinding> {
    if (pattern.kind != syntax::ExpressionKind::call ||
        pattern.left == nullptr ||
        pattern.left->kind != syntax::ExpressionKind::name ||
        pattern.arguments.size() != 1 ||
        pattern.arguments.front().kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto const* choice = find_lowered_choice_layout_by_llvm_type(context, subject.type);
    if (choice == nullptr) {
        return std::nullopt;
    }

    for (auto const& variant : choice->variants) {
        if (variant.name == pattern.left->text && variant.payloads.size() == 1) {
            return ChoicePayloadBinding {
                .binding_name = pattern.arguments.front().text,
                .payload_type = variant.payloads.front().llvm_type,
                .source_type_name = variant.payloads.front().source_type_name,
            };
        }
    }
    return std::nullopt;
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

}  // namespace

auto is_maybe_switch_subject(LoweredType const& type) -> bool {
    return type.type.starts_with("{ i1,");
}

auto is_supported_switch_subject(
    LoweredType const& type,
    LoweringEmissionContext const& context
) -> bool {
    return type.type == "i1" ||
           is_integer_llvm_type(type.type) ||
           is_maybe_switch_subject(type) ||
           find_lowered_choice_layout_by_llvm_type(context.lowering, type.type) != nullptr;
}

auto switch_subject_for_emit(
    LoweredExpression subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> LoweredExpression {
    auto const* choice = find_lowered_choice_layout_by_llvm_type(context.lowering, subject.type);
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
    LoweredExpression const& subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) {
    if (planned_case.syntax == nullptr) {
        return;
    }
    auto payload_type = maybe_payload_type_for_switch_subject(subject.type);
    auto binding_name = maybe_payload_binding_name(planned_case.syntax->pattern);
    if (payload_type.has_value() && binding_name.has_value()) {
        auto source_type_name = source_type_name_for_llvm_type(*payload_type, context.lowering);
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
        context.lowering
    );
    if (!choice_binding.has_value()) {
        return;
    }

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
