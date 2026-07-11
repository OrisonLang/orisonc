#include "orison/lowering/maybe_switch_lowering.hpp"

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

}  // namespace

auto is_maybe_switch_subject(LoweredType const& type) -> bool {
    return type.type.starts_with("{ i1,");
}

auto is_supported_switch_subject(LoweredType const& type) -> bool {
    return type.type == "i1" || is_integer_llvm_type(type.type) || is_maybe_switch_subject(type);
}

auto switch_subject_for_emit(
    LoweredExpression subject,
    FunctionLoweringSession& session,
    std::ostream& output
) -> LoweredExpression {
    if (!subject.type.starts_with("{ i1,")) {
        return subject;
    }

    auto tag = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag << " = extractvalue " << subject.type << " " << subject.value << ", 0\n";
    return LoweredExpression {
        .type = "i1",
        .value = std::move(tag),
        .signedness = IntegerSignedness::not_integer,
    };
}

void bind_maybe_switch_payload(
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
    if (!payload_type.has_value() || !binding_name.has_value()) {
        return;
    }

    auto payload = next_llvm_temporary_name(session.state.next_temporary_index);
    auto payload_signedness = IntegerSignedness::not_integer;
    if (auto source_type = source_type_name_for_llvm_type(*payload_type, context.lowering)) {
        if (auto lowered_type = lowered_type_for_source_type_name(*source_type, context.lowering)) {
            payload_signedness = lowered_type->signedness;
        }
        session.state.source_type_names[*binding_name] = std::move(*source_type);
    }
    output << "  " << payload << " = extractvalue " << subject.type << " " << subject.value << ", 1\n";
    session.state.immutable_bindings[*binding_name] = LoweredExpression {
        .type = *payload_type,
        .value = std::move(payload),
        .signedness = payload_signedness,
    };
}

}  // namespace orison::lowering
