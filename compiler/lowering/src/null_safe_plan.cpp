#include "orison/lowering/null_safe_plan.hpp"

#include "orison/lowering/source_type_queries.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace orison::lowering {
namespace {

auto make_failure(NullSafePlanFailureReason reason, std::string detail) -> NullSafePlanResult {
    return NullSafePlanResult {
        .plan = std::nullopt,
        .failure = NullSafePlanFailure {
            .reason = reason,
            .detail = std::move(detail),
        },
    };
}

void collect_null_safe_chain(
    syntax::ExpressionSyntax const& expression,
    std::vector<syntax::ExpressionSyntax const*>& chain
) {
    if (expression.kind != syntax::ExpressionKind::null_safe_member_access) {
        chain.push_back(&expression);
        return;
    }
    if (expression.left == nullptr) {
        chain.push_back(&expression);
        return;
    }
    collect_null_safe_chain(*expression.left, chain);
    chain.push_back(&expression);
}

auto maybe_type_name(std::string const& payload_type_name) -> std::string {
    return "Maybe<" + payload_type_name + ">";
}

}  // namespace

auto maybe_payload_source_type_name(std::string const& type_name) -> std::optional<std::string> {
    if (!type_name.starts_with("Maybe<") || !type_name.ends_with(">")) {
        return std::nullopt;
    }

    auto inner = type_name.substr(6, type_name.size() - 7);
    auto arguments = split_top_level_generic_arguments(inner);
    if (arguments.size() != 1 || arguments.front().empty()) {
        return std::nullopt;
    }
    return arguments.front();
}

auto plan_null_safe_member_access(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> NullSafePlanResult {
    if (expression.kind != syntax::ExpressionKind::null_safe_member_access) {
        return make_failure(NullSafePlanFailureReason::invalid_shape, "expected null-safe member access");
    }

    auto chain = std::vector<syntax::ExpressionSyntax const*> {};
    collect_null_safe_chain(expression, chain);
    if (chain.size() < 2 || chain.front() == nullptr ||
        std::any_of(chain.begin() + 1, chain.end(), [](auto const* segment) {
            return segment == nullptr || segment->kind != syntax::ExpressionKind::null_safe_member_access;
        })) {
        return make_failure(NullSafePlanFailureReason::invalid_shape, "malformed null-safe member chain");
    }

    auto base_type = source_type_name_for_expression(*chain.front(), context, state);
    if (!base_type.has_value()) {
        return make_failure(NullSafePlanFailureReason::unknown_base_type, "base expression");
    }

    auto current_maybe_type = *base_type;
    auto current_payload_type = maybe_payload_source_type_name(current_maybe_type);
    if (!current_payload_type.has_value()) {
        return make_failure(NullSafePlanFailureReason::expected_maybe_base, current_maybe_type);
    }

    auto plan = NullSafePlan {
        .base_expression = chain.front(),
        .base_maybe_type_name = current_maybe_type,
    };

    for (auto index = std::size_t {1}; index < chain.size(); ++index) {
        auto const& segment = *chain[index];
        auto record = context.records.find(*current_payload_type);
        if (record == context.records.end()) {
            return make_failure(NullSafePlanFailureReason::unknown_record, *current_payload_type);
        }

        auto const* field = find_record_field(record->second, segment.text);
        if (field == nullptr) {
            return make_failure(
                NullSafePlanFailureReason::unknown_field,
                *current_payload_type + "." + segment.text
            );
        }

        plan.segments.push_back(NullSafePlanSegment {
            .field_name = segment.text,
            .receiver_type_name = *current_payload_type,
            .field_type_name = field->source_type_name,
        });

        auto field_maybe_payload = maybe_payload_source_type_name(field->source_type_name);
        current_payload_type = field_maybe_payload.has_value()
            ? std::move(field_maybe_payload)
            : std::optional<std::string> {field->source_type_name};
        current_maybe_type = field_maybe_payload.has_value()
            ? field->source_type_name
            : maybe_type_name(field->source_type_name);
    }

    plan.result_payload_type_name = *current_payload_type;
    plan.result_maybe_type_name = current_maybe_type;
    return NullSafePlanResult {
        .plan = std::move(plan),
        .failure = {},
    };
}

auto render_null_safe_plan_failure(NullSafePlanFailure const& failure) -> std::string {
    auto prefix = std::string {};
    switch (failure.reason) {
    case NullSafePlanFailureReason::none:
        return {};
    case NullSafePlanFailureReason::invalid_shape:
        prefix = "invalid null-safe access shape";
        break;
    case NullSafePlanFailureReason::unknown_base_type:
        prefix = "unknown null-safe base type";
        break;
    case NullSafePlanFailureReason::expected_maybe_base:
        prefix = "null-safe access requires Maybe base";
        break;
    case NullSafePlanFailureReason::unknown_record:
        prefix = "unknown null-safe record type";
        break;
    case NullSafePlanFailureReason::unknown_field:
        prefix = "unknown null-safe field";
        break;
    }
    return failure.detail.empty() ? prefix : prefix + ": " + failure.detail;
}

}  // namespace orison::lowering
