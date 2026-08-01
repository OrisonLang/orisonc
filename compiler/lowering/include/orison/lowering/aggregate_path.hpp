#pragma once

#include "orison/lowering/function_lowering_state.hpp"
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

enum class AggregateProjectionAccessIntent {
    value_read,
    explicit_transfer,
    shared_borrow,
    exclusive_borrow,
    clone_value,
};

enum class AggregateProjectionAccessStatus {
    not_named_aggregate_path,
    non_owned_projection,
    allowed,
    requires_explicit_boundary,
    boundary_not_enabled,
};

struct AggregateProjectionAccessPlan {
    AggregateProjectionAccessIntent intent = AggregateProjectionAccessIntent::value_read;
    AggregateProjectionAccessStatus status = AggregateProjectionAccessStatus::not_named_aggregate_path;
    std::string binding_name;
    std::string source_type_name;
    bool receiver_projection = false;
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

auto advance_aggregate_path_member_with_temporary(
    AggregatePathCursor& cursor,
    std::string_view field_name,
    LoweringContext const& context,
    std::size_t& next_temporary_index,
    std::ostream& output
) -> AggregatePathResult;

auto advance_aggregate_path_index(
    AggregatePathCursor& cursor,
    std::string index_value,
    LoweringContext const& context,
    std::string pointer_name,
    std::ostream& output
) -> AggregatePathResult;

auto advance_aggregate_path_index_with_temporary(
    AggregatePathCursor& cursor,
    std::string index_value,
    LoweringContext const& context,
    std::size_t& next_temporary_index,
    std::ostream& output
) -> AggregatePathResult;

auto emit_aggregate_path_cursor_load(
    AggregatePathCursor const& cursor,
    std::string_view llvm_type,
    IntegerSignedness signedness,
    std::string result_name,
    std::ostream& output
) -> LoweredExpression;

auto describe_named_aggregate_projection_access(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state,
    AggregateProjectionAccessIntent intent
) -> AggregateProjectionAccessPlan;

auto render_aggregate_path_error(AggregatePathError error) -> std::string_view;

auto render_aggregate_projection_access_intent(
    AggregateProjectionAccessIntent intent
) -> std::string_view;

auto render_aggregate_projection_access_status(
    AggregateProjectionAccessStatus status
) -> std::string_view;

auto aggregate_projection_access_diagnostic(
    AggregateProjectionAccessPlan const& plan
) -> std::string;

auto aggregate_projection_access_plan_report(
    AggregateProjectionAccessPlan const& plan
) -> std::string;

}  // namespace orison::lowering
