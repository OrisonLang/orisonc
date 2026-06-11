#pragma once

#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace orison::lowering {

struct LoweredExpression {
    std::string type;
    std::string value;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

struct LoweredType {
    std::string type;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

struct ExpressionEmissionContext {
    LoweringContext const& lowering;
    StringConstantTable const& string_constants;
};

enum class ExpressionLoweringFailureReason {
    none,
    unsupported_expression,
    missing_string_constant,
    unknown_name,
    type_mismatch,
    signedness_mismatch,
    unsupported_cast,
    unsupported_operator,
    cannot_infer_operand_type,
    branch_type_mismatch,
    unknown_function,
    call_return_type_mismatch,
    call_arity_mismatch,
    call_argument_failure,
};

struct ExpressionLoweringFailure {
    ExpressionLoweringFailureReason reason = ExpressionLoweringFailureReason::none;
    std::string detail;
};

struct ExpressionLoweringState {
    std::unordered_map<std::string, LoweredExpression> immutable_bindings;
    std::unordered_map<std::string, std::size_t> local_name_counts;
    std::size_t next_temporary_index = 0;
    std::size_t next_block_index = 0;
    std::string current_block = "entry";
    ExpressionLoweringFailure failure;
};

auto lower_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    ExpressionLoweringState& state,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto infer_expression_type(
    syntax::ExpressionSyntax const& expression,
    ExpressionEmissionContext const& context,
    ExpressionLoweringState const& state
) -> std::optional<LoweredType>;

auto lower_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness
) -> std::optional<LoweredExpression>;

auto lower_boolean_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression>;

auto is_integer_llvm_type(std::string_view type) -> bool;

auto render_expression_lowering_failure(
    ExpressionLoweringFailure const& failure
) -> std::string;

}  // namespace orison::lowering
