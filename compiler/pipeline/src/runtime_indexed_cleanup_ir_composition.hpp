#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace orison::pipeline {

auto occurrence_count(
    std::string const& text,
    std::string const& needle
) -> std::size_t;

auto block_label_found(
    std::string const& function_ir,
    std::string const& block_name
) -> bool;

auto predecessor_branch_pattern(
    std::string const& function_ir,
    std::string const& predecessor_block_name,
    std::string const& branch_text
) -> std::string;

auto predecessor_terminator_pattern(
    std::string const& function_ir,
    std::string const& predecessor_block_name
) -> std::string;

auto predecessor_terminator_position(
    std::string const& function_ir,
    std::string const& predecessor_block_name,
    std::string const& terminator
) -> std::string::size_type;

struct RuntimeIndexedCleanupFunctionIrInsertion {
    std::string predecessor_block_name;
    std::string inserted_branch_text;
    std::vector<std::string> cfg_lines;
};

struct RuntimeIndexedCleanupFunctionIrCompositionPart {
    std::string predecessor_block_name;
    std::string continuation_block_name;
    std::string replacement_branch_text;
    std::string cleanup_cfg_tail;
    std::size_t splice_start_offset = 0;
    std::size_t splice_end_offset = 0;
};

struct RuntimeIndexedCleanupFunctionIrRewriteResult {
    std::string rewritten_function_ir;
    RuntimeIndexedCleanupIrCompositionFailure failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;

    [[nodiscard]] auto succeeded() const -> bool {
        return failure == RuntimeIndexedCleanupIrCompositionFailure::none &&
            !rewritten_function_ir.empty();
    }
};

struct RuntimeIndexedCleanupFunctionIrCompositionPartResult {
    std::vector<RuntimeIndexedCleanupFunctionIrCompositionPart> parts;
    RuntimeIndexedCleanupIrCompositionFailure failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;

    [[nodiscard]] auto succeeded() const -> bool {
        return failure == RuntimeIndexedCleanupIrCompositionFailure::none &&
            !parts.empty();
    }
};

struct RuntimeIndexedCleanupFunctionIrRewriteOperation {
    std::vector<RuntimeIndexedCleanupFunctionIrCompositionPart> parts;
    std::string appended_cleanup_cfg;
};

struct RuntimeIndexedCleanupFunctionIrRewriteOperationResult {
    RuntimeIndexedCleanupFunctionIrRewriteOperation operation;
    RuntimeIndexedCleanupIrCompositionFailure failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;

    [[nodiscard]] auto succeeded() const -> bool {
        return failure == RuntimeIndexedCleanupIrCompositionFailure::none &&
            !operation.parts.empty();
    }
};

struct RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
    RuntimeIndexedCleanupIrCompositionFailure failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;

    [[nodiscard]] auto succeeded() const -> bool {
        return failure == RuntimeIndexedCleanupIrCompositionFailure::none;
    }
};

auto build_runtime_indexed_cleanup_function_ir_composition_part_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> const& candidates
) -> RuntimeIndexedCleanupFunctionIrCompositionPartResult;

auto build_runtime_indexed_cleanup_function_ir_rewrite_operation_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> RuntimeIndexedCleanupFunctionIrRewriteOperationResult;

auto validate_runtime_indexed_cleanup_function_ir_rewrite_operation(
    std::string const& original_function_ir,
    RuntimeIndexedCleanupFunctionIrRewriteOperation const& operation
) -> RuntimeIndexedCleanupFunctionIrRewriteOperationValidation;

auto apply_runtime_indexed_cleanup_function_ir_rewrite_operation(
    std::string const& original_function_ir,
    RuntimeIndexedCleanupFunctionIrRewriteOperation const& operation
) -> RuntimeIndexedCleanupFunctionIrRewriteResult;

auto rewrite_predecessor_terminator_and_insert_cfg_result(
    std::string const& function_ir,
    RuntimeIndexedCleanupFunctionIrInsertion const& insertion
) -> RuntimeIndexedCleanupFunctionIrRewriteResult;

auto compose_non_overlapping_function_ir_rewrite_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> RuntimeIndexedCleanupFunctionIrRewriteResult;

auto runtime_indexed_cleanup_ir_composition_failure_token(
    RuntimeIndexedCleanupIrCompositionFailure failure
) -> std::string_view;

} // namespace orison::pipeline
