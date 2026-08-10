#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

#include <cstddef>
#include <string>
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

enum class RuntimeIndexedCleanupIrCompositionFailure {
    none,
    empty_input,
    missing_function_closing_brace,
    missing_predecessor_block,
    missing_predecessor_terminator,
    ambiguous_predecessor_terminator,
    missing_cleanup_exit_block,
    phi_retarget_failed,
    invalid_candidate,
    unexpected_splice_text,
    missing_cleanup_cfg_tail,
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

auto build_runtime_indexed_cleanup_function_ir_composition_parts(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> const& candidates
) -> std::vector<RuntimeIndexedCleanupFunctionIrCompositionPart>;

auto build_runtime_indexed_cleanup_function_ir_composition_part_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> const& candidates
) -> RuntimeIndexedCleanupFunctionIrCompositionPartResult;

auto rewrite_predecessor_terminator_and_insert_cfg(
    std::string const& function_ir,
    RuntimeIndexedCleanupFunctionIrInsertion const& insertion
) -> std::string;

auto rewrite_predecessor_terminator_and_insert_cfg_result(
    std::string const& function_ir,
    RuntimeIndexedCleanupFunctionIrInsertion const& insertion
) -> RuntimeIndexedCleanupFunctionIrRewriteResult;

auto compose_non_overlapping_function_ir_rewrite(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> std::string;

auto compose_non_overlapping_function_ir_rewrite_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> RuntimeIndexedCleanupFunctionIrRewriteResult;

} // namespace orison::pipeline
