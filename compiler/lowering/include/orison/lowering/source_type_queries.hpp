#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct ParsedLlvmArrayType {
    std::string element_type;
    std::size_t length = 0;
};

enum class DynamicSequenceKind {
    dynamic_array,
    shared_view,
    exclusive_view,
    view,
};

struct DynamicSequenceSourceType {
    DynamicSequenceKind kind = DynamicSequenceKind::view;
    std::string element_source_type_name;
    bool owns_storage = false;
    bool permits_element_mutation = false;
};

auto split_top_level_generic_arguments(std::string_view text) -> std::vector<std::string>;

auto parse_llvm_array_type(std::string_view type) -> std::optional<ParsedLlvmArrayType>;

auto array_element_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto dynamic_array_element_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto view_element_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto dynamic_sequence_source_type(std::string_view type_name) -> std::optional<DynamicSequenceSourceType>;

auto view_descriptor_llvm_type() -> std::string_view;

auto dynamic_array_descriptor_llvm_type() -> std::string_view;

auto pointer_pointee_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto maybe_payload_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto source_type_name_for_llvm_type(
    std::string_view llvm_type,
    LoweringContext const& context
) -> std::optional<std::string>;

auto find_record_field(
    LoweredRecordLayout const& layout,
    std::string_view field_name
) -> LoweredRecordField const*;

auto generic_record_constructor_inference_failure_detail(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto lowered_type_for_source_type_name(
    std::string_view type_name,
    LoweringContext const& context
) -> std::optional<LoweredType>;

auto llvm_type_for_source_type_name(
    std::string_view type_name,
    LoweringContext const& context
) -> std::optional<std::string>;

auto source_type_name_for_expression(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto source_type_name_for_initializer(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state,
    std::string_view lowered_llvm_type
) -> std::optional<std::string>;

auto source_type_name_for_value_statement(
    syntax::StatementSyntax const& statement,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto source_type_name_for_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto source_type_name_for_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

}  // namespace orison::lowering
