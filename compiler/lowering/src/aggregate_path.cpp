#include "orison/lowering/aggregate_path.hpp"

#include "orison/lowering/source_type_queries.hpp"

#include <algorithm>
#include <ostream>
#include <utility>

namespace orison::lowering {
namespace {

auto is_array_llvm_type(std::string_view type) -> bool {
    return type.starts_with("[");
}

auto refresh_cursor_layout(
    AggregatePathCursor& cursor,
    LoweringContext const& context
) -> AggregatePathError {
    if (auto layout = context.records.find(cursor.source_type_name); layout != context.records.end()) {
        cursor.record_layout = &layout->second;
        cursor.llvm_type_name = cursor.record_layout->llvm_type_name;
        cursor.expects_record_layout = true;
        return AggregatePathError::none;
    }

    auto lowered_type = llvm_type_for_source_type_name(cursor.source_type_name, context);
    if (!lowered_type.has_value()) {
        cursor.record_layout = nullptr;
        cursor.llvm_type_name.clear();
        cursor.expects_record_layout = false;
        return AggregatePathError::unsupported_element_type;
    }

    cursor.record_layout = nullptr;
    cursor.llvm_type_name = std::move(*lowered_type);
    cursor.expects_record_layout = false;
    return AggregatePathError::none;
}

}  // namespace

auto collect_aggregate_path(syntax::ExpressionSyntax const& expression) -> AggregatePath {
    auto path = AggregatePath {};
    auto const* base_expression = &expression;
    while (true) {
        if (base_expression->kind == syntax::ExpressionKind::member_access &&
            base_expression->left != nullptr) {
            path.steps.push_back(AggregatePathStep {
                .kind = AggregatePathStepKind::member,
                .field_name = base_expression->text,
            });
            base_expression = base_expression->left.get();
            continue;
        }
        if (base_expression->kind == syntax::ExpressionKind::index_access &&
            base_expression->left != nullptr && base_expression->arguments.size() == 1) {
            path.steps.push_back(AggregatePathStep {
                .kind = AggregatePathStepKind::index,
                .index_expression = &base_expression->arguments.front(),
            });
            base_expression = base_expression->left.get();
            continue;
        }
        break;
    }

    std::reverse(path.steps.begin(), path.steps.end());
    path.base_expression = base_expression;
    return path;
}

auto collect_named_aggregate_path(
    syntax::ExpressionSyntax const& expression
) -> std::optional<AggregatePath> {
    if (expression.kind != syntax::ExpressionKind::member_access &&
        expression.kind != syntax::ExpressionKind::index_access) {
        return std::nullopt;
    }

    auto path = collect_aggregate_path(expression);
    if (path.steps.empty() || path.base_expression == nullptr ||
        path.base_expression->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }
    return path;
}

auto collect_temporary_aggregate_path(
    syntax::ExpressionSyntax const& expression
) -> std::optional<AggregatePath> {
    if (expression.kind != syntax::ExpressionKind::member_access &&
        expression.kind != syntax::ExpressionKind::index_access) {
        return std::nullopt;
    }

    auto path = collect_aggregate_path(expression);
    if (path.steps.empty() || path.base_expression == nullptr ||
        path.base_expression->kind == syntax::ExpressionKind::name) {
        return std::nullopt;
    }
    return path;
}

auto initialize_aggregate_path_cursor(
    std::string pointer,
    std::string source_type_name,
    LoweringContext const& context
) -> std::optional<AggregatePathCursor> {
    auto cursor = AggregatePathCursor {
        .pointer = std::move(pointer),
        .source_type_name = std::move(source_type_name),
    };
    if (refresh_cursor_layout(cursor, context) != AggregatePathError::none) {
        return std::nullopt;
    }
    return cursor;
}

auto advance_aggregate_path_member(
    AggregatePathCursor& cursor,
    std::string_view field_name,
    LoweringContext const& context,
    std::string pointer_name,
    std::ostream& output
) -> AggregatePathResult {
    if (!cursor.expects_record_layout || cursor.record_layout == nullptr) {
        return {.error = AggregatePathError::expected_record};
    }

    auto const* field = find_record_field(*cursor.record_layout, field_name);
    if (field == nullptr) {
        return {.error = AggregatePathError::unknown_field};
    }
    if (field->llvm_type.empty() || field->llvm_type == "void") {
        return {.error = AggregatePathError::unsupported_field_type};
    }

    output << "  " << pointer_name << " = getelementptr " << cursor.llvm_type_name
           << ", ptr " << cursor.pointer << ", i32 0, i32 " << field->index << "\n";
    cursor.pointer = std::move(pointer_name);
    cursor.source_type_name = field->source_type_name;
    cursor.llvm_type_name = field->llvm_type;

    if (auto error = refresh_cursor_layout(cursor, context); error != AggregatePathError::none) {
        return {.error = error};
    }
    return {};
}

auto advance_aggregate_path_index(
    AggregatePathCursor& cursor,
    std::string index_value,
    LoweringContext const& context,
    std::string pointer_name,
    std::ostream& output
) -> AggregatePathResult {
    if (!is_array_llvm_type(cursor.llvm_type_name)) {
        return {.error = AggregatePathError::expected_array};
    }

    output << "  " << pointer_name << " = getelementptr " << cursor.llvm_type_name
           << ", ptr " << cursor.pointer << ", i64 0, i64 " << index_value << "\n";
    cursor.pointer = std::move(pointer_name);

    auto element_source_type = array_element_source_type_name(cursor.source_type_name);
    if (!element_source_type.has_value()) {
        return {.error = AggregatePathError::unsupported_element_source_type};
    }
    cursor.source_type_name = std::move(*element_source_type);

    if (auto error = refresh_cursor_layout(cursor, context); error != AggregatePathError::none) {
        return {.error = error};
    }
    return {};
}

auto emit_aggregate_path_cursor_load(
    AggregatePathCursor const& cursor,
    std::string_view llvm_type,
    IntegerSignedness signedness,
    std::string result_name,
    std::ostream& output
) -> LoweredExpression {
    output << "  " << result_name << " = load " << llvm_type << ", ptr " << cursor.pointer << "\n";
    return LoweredExpression {
        .type = std::string(llvm_type),
        .value = std::move(result_name),
        .signedness = signedness,
    };
}

}  // namespace orison::lowering
