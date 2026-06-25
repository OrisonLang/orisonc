#pragma once

#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

enum class AggregatePathStepKind {
    member,
    index,
};

struct AggregatePathStep {
    AggregatePathStepKind kind = AggregatePathStepKind::member;
    std::string field_name;
    syntax::ExpressionSyntax const* index_expression = nullptr;
};

struct AggregatePath {
    syntax::ExpressionSyntax const* base_expression = nullptr;
    std::vector<AggregatePathStep> steps;
};

struct AggregatePathCursor {
    std::string pointer;
    std::string source_type_name;
    std::string llvm_type_name;
    LoweredRecordLayout const* record_layout = nullptr;
    bool expects_record_layout = false;
};

enum class AggregatePathError {
    none,
    no_steps,
    unsupported_base,
    unsupported_source_type,
    expected_record,
    unknown_field,
    unsupported_field_type,
    expected_array,
    unsupported_element_source_type,
    unsupported_element_type,
};

struct AggregatePathResult {
    AggregatePathError error = AggregatePathError::none;
};

auto collect_aggregate_path(syntax::ExpressionSyntax const& expression) -> AggregatePath;

auto collect_named_aggregate_path(
    syntax::ExpressionSyntax const& expression
) -> std::optional<AggregatePath>;

auto collect_temporary_aggregate_path(
    syntax::ExpressionSyntax const& expression
) -> std::optional<AggregatePath>;

auto initialize_aggregate_path_cursor(
    std::string pointer,
    std::string source_type_name,
    LoweringContext const& context
) -> std::optional<AggregatePathCursor>;

auto advance_aggregate_path_member(
    AggregatePathCursor& cursor,
    std::string_view field_name,
    LoweringContext const& context,
    std::string pointer_name,
    std::ostream& output
) -> AggregatePathResult;

auto advance_aggregate_path_index(
    AggregatePathCursor& cursor,
    std::string index_value,
    LoweringContext const& context,
    std::string pointer_name,
    std::ostream& output
) -> AggregatePathResult;

auto emit_aggregate_path_cursor_load(
    AggregatePathCursor const& cursor,
    std::string_view llvm_type,
    IntegerSignedness signedness,
    std::string result_name,
    std::ostream& output
) -> LoweredExpression;

}  // namespace orison::lowering
