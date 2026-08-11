#include "runtime_indexed_cleanup_ir_composition.hpp"

#include <algorithm>
#include <utility>

namespace orison::pipeline {

namespace {

auto block_start_position(
    std::string const& function_ir,
    std::string const& block_name
) -> std::string::size_type {
    if (function_ir.empty() || block_name.empty()) {
        return std::string::npos;
    }

    auto const label = "\n" + block_name + ":\n";
    auto const label_position = function_ir.find(label);
    if (label_position == std::string::npos) {
        return std::string::npos;
    }
    return label_position + 1;
}

auto block_end_position(
    std::string const& function_ir,
    std::string::size_type block_start
) -> std::string::size_type {
    if (function_ir.empty() || block_start == std::string::npos) {
        return std::string::npos;
    }

    auto search_position = function_ir.find('\n', block_start);
    if (search_position == std::string::npos) {
        return std::string::npos;
    }
    ++search_position;
    while (search_position < function_ir.size()) {
        auto const line_end = function_ir.find('\n', search_position);
        if (line_end == std::string::npos) {
            return function_ir.size();
        }
        auto const line = function_ir.substr(search_position, line_end - search_position);
        if (!line.empty() && line.front() != ' ' && line.back() == ':') {
            return search_position;
        }
        if (line == "}") {
            return search_position;
        }
        search_position = line_end + 1;
    }
    return function_ir.size();
}

auto trailing_label_name(std::vector<std::string> const& lines) -> std::string {
    for (auto line = lines.rbegin(); line != lines.rend(); ++line) {
        if (line->empty() || line->back() != '\n') {
            continue;
        }
        auto label = line->substr(0, line->size() - 1);
        if (!label.empty() && label.back() == ':') {
            label.pop_back();
            return label;
        }
    }
    return {};
}

auto inserted_cleanup_cfg_tail(
    RuntimeIndexedCleanupFunctionIrRewriteCandidate const& candidate
) -> std::string {
    auto const insertion_start =
        block_start_position(candidate.candidate_function_ir_text, candidate.insertion_block_name);
    auto const closing_position = candidate.candidate_function_ir_text.rfind("\n}\n");
    if (insertion_start == std::string::npos || closing_position == std::string::npos ||
        insertion_start >= closing_position) {
        return {};
    }
    return candidate.candidate_function_ir_text.substr(
        insertion_start,
        closing_position + 1 - insertion_start
    );
}

auto retarget_phi_incoming_predecessor(
    std::string const& function_ir,
    std::string const& old_predecessor_name,
    std::string const& new_predecessor_name
) -> std::string {
    if (old_predecessor_name.empty() || new_predecessor_name.empty()) {
        return {};
    }

    auto rewritten = std::string {};
    auto search_position = std::string::size_type {0};
    auto const old_incoming = ", %" + old_predecessor_name + " ]";
    auto const new_incoming = ", %" + new_predecessor_name + " ]";
    while (search_position < function_ir.size()) {
        auto const line_end = function_ir.find('\n', search_position);
        auto const line =
            line_end == std::string::npos
                ? function_ir.substr(search_position)
                : function_ir.substr(search_position, line_end - search_position);
        auto next_line = line;
        if (next_line.find(" = phi ") != std::string::npos) {
            auto incoming_position = std::string::size_type {0};
            while ((incoming_position = next_line.find(old_incoming, incoming_position)) != std::string::npos) {
                next_line.replace(incoming_position, old_incoming.size(), new_incoming);
                incoming_position += new_incoming.size();
            }
        }
        rewritten += next_line;
        if (line_end == std::string::npos) {
            break;
        }
        rewritten += '\n';
        search_position = line_end + 1;
    }
    return rewritten;
}

auto validation_failure_for_splice_range(
    RuntimeIndexedCleanupIrCompositionFailure failure,
    std::size_t part_index,
    RuntimeIndexedCleanupFunctionIrTextSpliceRange splice_range
) -> RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
    return RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
        .failure = failure,
        .part_available = true,
        .part_index = part_index,
        .splice_range = splice_range,
    };
}

} // namespace

auto occurrence_count(
    std::string const& text,
    std::string const& needle
) -> std::size_t {
    if (text.empty() || needle.empty()) {
        return 0;
    }

    auto count = std::size_t {0};
    auto position = std::string::size_type {0};
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

auto block_label_found(
    std::string const& function_ir,
    std::string const& block_name
) -> bool {
    return !function_ir.empty() &&
        !block_name.empty() &&
        function_ir.find("\n" + block_name + ":\n") != std::string::npos;
}

auto predecessor_branch_pattern(
    std::string const& function_ir,
    std::string const& predecessor_block_name,
    std::string const& branch_text
) -> std::string {
    auto const block_start = block_start_position(function_ir, predecessor_block_name);
    auto const block_end = block_end_position(function_ir, block_start);
    if (block_start == std::string::npos || block_end == std::string::npos) {
        return {};
    }

    auto const block_text = function_ir.substr(block_start, block_end - block_start);
    auto const branch_pattern = "  " + branch_text + "\n";
    if (block_text.find(branch_pattern) == std::string::npos) {
        return {};
    }
    return branch_pattern;
}

auto predecessor_terminator_pattern(
    std::string const& function_ir,
    std::string const& predecessor_block_name
) -> std::string {
    auto const block_start = block_start_position(function_ir, predecessor_block_name);
    auto const block_end = block_end_position(function_ir, block_start);
    if (block_start == std::string::npos || block_end == std::string::npos) {
        return {};
    }

    auto const block_text = function_ir.substr(block_start, block_end - block_start);
    auto search_position = std::string::size_type {0};
    auto terminator = std::string {};
    while (search_position < block_text.size()) {
        auto const line_end = block_text.find('\n', search_position);
        if (line_end == std::string::npos) {
            break;
        }
        auto const line = block_text.substr(search_position, line_end - search_position);
        if (line.rfind("  br ", 0) == 0 || line.rfind("  ret ", 0) == 0) {
            terminator = line + "\n";
        }
        search_position = line_end + 1;
    }
    return terminator;
}

auto predecessor_terminator_position(
    std::string const& function_ir,
    std::string const& predecessor_block_name,
    std::string const& terminator
) -> std::string::size_type {
    auto const block_start = block_start_position(function_ir, predecessor_block_name);
    auto const block_end = block_end_position(function_ir, block_start);
    if (block_start == std::string::npos || block_end == std::string::npos || terminator.empty()) {
        return std::string::npos;
    }

    auto const block_text = function_ir.substr(block_start, block_end - block_start);
    if (occurrence_count(block_text, terminator) != 1) {
        return std::string::npos;
    }
    return block_start + block_text.find(terminator);
}

auto rewrite_predecessor_terminator_and_insert_cfg_result(
    std::string const& function_ir,
    RuntimeIndexedCleanupFunctionIrInsertion const& insertion
) -> RuntimeIndexedCleanupFunctionIrRewriteResult {
    if (function_ir.empty() || insertion.predecessor_block_name.empty() ||
        insertion.inserted_branch_text.empty() || insertion.cfg_lines.empty()) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::empty_input,
        };
    }

    auto const closing_position = function_ir.rfind("\n}\n");
    if (closing_position == std::string::npos) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::missing_function_closing_brace,
        };
    }

    auto const block_start = block_start_position(function_ir, insertion.predecessor_block_name);
    auto const block_end = block_end_position(function_ir, block_start);
    if (block_start == std::string::npos || block_end == std::string::npos) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::missing_predecessor_block,
        };
    }

    auto const block_text = function_ir.substr(block_start, block_end - block_start);
    auto const replaced_branch = predecessor_terminator_pattern(
        function_ir,
        insertion.predecessor_block_name
    );
    auto const inserted_branch = "  " + insertion.inserted_branch_text + "\n";
    if (replaced_branch.empty()) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::missing_predecessor_terminator,
        };
    }
    auto const terminator_position_in_block = block_text.find(replaced_branch);
    if (terminator_position_in_block == std::string::npos ||
        occurrence_count(block_text, replaced_branch) != 1) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::ambiguous_predecessor_terminator,
        };
    }

    auto cleanup_cfg_tail = std::string {};
    for (auto line_index = std::size_t {1}; line_index < insertion.cfg_lines.size(); ++line_index) {
        cleanup_cfg_tail += insertion.cfg_lines[line_index];
    }
    auto const cleanup_exit_block_name = trailing_label_name(insertion.cfg_lines);
    if (cleanup_exit_block_name.empty()) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::missing_cleanup_exit_block,
        };
    }
    cleanup_cfg_tail += replaced_branch;
    return apply_runtime_indexed_cleanup_function_ir_rewrite_operation(
        function_ir,
        RuntimeIndexedCleanupFunctionIrRewriteOperation {
            .parts = {
                RuntimeIndexedCleanupFunctionIrCompositionPart {
                    .predecessor_block_name = insertion.predecessor_block_name,
                    .continuation_block_name = cleanup_exit_block_name,
                    .replaced_branch_text = replaced_branch,
                    .replacement_branch_text = inserted_branch,
                    .cleanup_cfg_tail = cleanup_cfg_tail,
                    .splice_range = RuntimeIndexedCleanupFunctionIrTextSpliceRange {
                        .start_offset = block_start + terminator_position_in_block,
                        .end_offset = block_start + terminator_position_in_block + replaced_branch.size(),
                    },
                },
            },
            .appended_cleanup_cfg = std::move(cleanup_cfg_tail),
        }
    );
}

auto build_runtime_indexed_cleanup_function_ir_composition_part_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> const& candidates
) -> RuntimeIndexedCleanupFunctionIrCompositionPartResult {
    if (original_function_ir.empty() || candidates.empty()) {
        return RuntimeIndexedCleanupFunctionIrCompositionPartResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::empty_input,
        };
    }

    auto parts = std::vector<RuntimeIndexedCleanupFunctionIrCompositionPart> {};
    parts.reserve(candidates.size());
    for (auto const* candidate : candidates) {
        if (candidate == nullptr || !candidate->candidate_available || !candidate->splice_range_available ||
            candidate->splice_start_offset >= candidate->splice_end_offset ||
            candidate->splice_end_offset > original_function_ir.size()) {
            return RuntimeIndexedCleanupFunctionIrCompositionPartResult {
                .failure = RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
            };
        }
        auto const expected_terminator = "  " + candidate->replaced_terminator_text + "\n";
        if (original_function_ir.substr(
                candidate->splice_start_offset,
                candidate->splice_end_offset - candidate->splice_start_offset
            ) != expected_terminator) {
            return RuntimeIndexedCleanupFunctionIrCompositionPartResult {
                .failure = RuntimeIndexedCleanupIrCompositionFailure::unexpected_splice_text,
            };
        }
        auto cleanup_tail = inserted_cleanup_cfg_tail(*candidate);
        if (cleanup_tail.empty()) {
            return RuntimeIndexedCleanupFunctionIrCompositionPartResult {
                .failure = RuntimeIndexedCleanupIrCompositionFailure::missing_cleanup_cfg_tail,
            };
        }
        parts.push_back(RuntimeIndexedCleanupFunctionIrCompositionPart {
            .predecessor_block_name = candidate->predecessor_block_name,
            .continuation_block_name = candidate->continuation_block_name,
            .replaced_branch_text = expected_terminator,
            .replacement_branch_text = "  " + candidate->inserted_branch_text + "\n",
            .cleanup_cfg_tail = std::move(cleanup_tail),
            .splice_range = RuntimeIndexedCleanupFunctionIrTextSpliceRange {
                .start_offset = candidate->splice_start_offset,
                .end_offset = candidate->splice_end_offset,
            },
        });
    }
    return RuntimeIndexedCleanupFunctionIrCompositionPartResult {
        .parts = std::move(parts),
    };
}

auto build_runtime_indexed_cleanup_function_ir_rewrite_operation_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> RuntimeIndexedCleanupFunctionIrRewriteOperationResult {
    if (original_function_ir.empty() || candidates.empty()) {
        return RuntimeIndexedCleanupFunctionIrRewriteOperationResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::empty_input,
        };
    }
    std::sort(
        candidates.begin(),
        candidates.end(),
        [](auto const* left, auto const* right) {
            return left->splice_start_offset > right->splice_start_offset;
        }
    );

    auto const composition_part_result =
        build_runtime_indexed_cleanup_function_ir_composition_part_result(original_function_ir, candidates);
    if (!composition_part_result.succeeded()) {
        return RuntimeIndexedCleanupFunctionIrRewriteOperationResult {
            .failure = composition_part_result.failure,
        };
    }
    auto appended_cleanup_cfg = std::string {};
    for (auto const& part : composition_part_result.parts) {
        appended_cleanup_cfg = part.cleanup_cfg_tail + appended_cleanup_cfg;
    }
    return RuntimeIndexedCleanupFunctionIrRewriteOperationResult {
        .operation = RuntimeIndexedCleanupFunctionIrRewriteOperation {
            .parts = composition_part_result.parts,
            .appended_cleanup_cfg = std::move(appended_cleanup_cfg),
        },
    };
}

auto validate_runtime_indexed_cleanup_function_ir_rewrite_operation(
    std::string const& original_function_ir,
    RuntimeIndexedCleanupFunctionIrRewriteOperation const& operation
) -> RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
    auto const edit_script_result = build_runtime_indexed_cleanup_function_ir_edit_script_result(operation);
    if (!edit_script_result.succeeded()) {
        return RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
            .failure = edit_script_result.failure,
        };
    }
    return validate_runtime_indexed_cleanup_function_ir_edit_script(
        original_function_ir,
        edit_script_result.edit_script
    );
}

auto build_runtime_indexed_cleanup_function_ir_edit_script_result(
    RuntimeIndexedCleanupFunctionIrRewriteOperation const& operation
) -> RuntimeIndexedCleanupFunctionIrEditScriptResult {
    if (operation.parts.empty()) {
        return RuntimeIndexedCleanupFunctionIrEditScriptResult {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::empty_input,
        };
    }
    auto branch_replacements = std::vector<RuntimeIndexedCleanupFunctionIrBranchReplacement> {};
    branch_replacements.reserve(operation.parts.size());
    auto phi_predecessor_retargets = std::vector<RuntimeIndexedCleanupFunctionIrPhiPredecessorRetarget> {};
    phi_predecessor_retargets.reserve(operation.parts.size());
    for (auto const& part : operation.parts) {
        branch_replacements.push_back(RuntimeIndexedCleanupFunctionIrBranchReplacement {
            .predecessor_block_name = part.predecessor_block_name,
            .continuation_block_name = part.continuation_block_name,
            .expected_branch_text = part.replaced_branch_text,
            .replacement_branch_text = part.replacement_branch_text,
            .splice_range = part.splice_range,
        });
        phi_predecessor_retargets.push_back(RuntimeIndexedCleanupFunctionIrPhiPredecessorRetarget {
            .old_predecessor_block_name = part.predecessor_block_name,
            .new_predecessor_block_name = part.continuation_block_name,
        });
    }
    return RuntimeIndexedCleanupFunctionIrEditScriptResult {
        .edit_script = RuntimeIndexedCleanupFunctionIrEditScript {
            .branch_replacements = std::move(branch_replacements),
            .cleanup_cfg_append = RuntimeIndexedCleanupFunctionIrCleanupCfgAppend {
                .placement = RuntimeIndexedCleanupFunctionIrCleanupCfgAppendPlacement::before_function_closing_brace,
                .expected_closing_text = "\n}\n",
                .append_text = operation.appended_cleanup_cfg,
            },
            .phi_predecessor_retargets = std::move(phi_predecessor_retargets),
        },
    };
}

auto validate_runtime_indexed_cleanup_function_ir_edit_script(
    std::string const& original_function_ir,
    RuntimeIndexedCleanupFunctionIrEditScript const& edit_script
) -> RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
    if (original_function_ir.empty() || edit_script.branch_replacements.empty()) {
        return RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::empty_input,
        };
    }
    if (edit_script.cleanup_cfg_append.append_text.empty()) {
        return RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
        };
    }
    if (edit_script.cleanup_cfg_append.placement !=
            RuntimeIndexedCleanupFunctionIrCleanupCfgAppendPlacement::before_function_closing_brace ||
        edit_script.cleanup_cfg_append.expected_closing_text.empty()) {
        return RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
        };
    }
    if (edit_script.phi_predecessor_retargets.size() != edit_script.branch_replacements.size()) {
        return RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {
            .failure = RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
        };
    }
    for (auto part_index = std::size_t {0}; part_index < edit_script.branch_replacements.size(); ++part_index) {
        auto const& part = edit_script.branch_replacements[part_index];
        if (part.splice_range.start_offset >= part.splice_range.end_offset ||
            part.splice_range.end_offset > original_function_ir.size() ||
            part.expected_branch_text.empty() ||
            part.replacement_branch_text.empty()) {
            return validation_failure_for_splice_range(
                RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
                part_index,
                part.splice_range
            );
        }
        if (original_function_ir.substr(
                part.splice_range.start_offset,
                part.splice_range.end_offset - part.splice_range.start_offset
            ) != part.expected_branch_text) {
            return validation_failure_for_splice_range(
                RuntimeIndexedCleanupIrCompositionFailure::unexpected_splice_text,
                part_index,
                part.splice_range
            );
        }
        auto const& phi_retarget = edit_script.phi_predecessor_retargets[part_index];
        if (phi_retarget.old_predecessor_block_name != part.predecessor_block_name ||
            phi_retarget.new_predecessor_block_name != part.continuation_block_name) {
            return validation_failure_for_splice_range(
                RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
                part_index,
                part.splice_range
            );
        }
    }
    return RuntimeIndexedCleanupFunctionIrRewriteOperationValidation {};
}

auto apply_runtime_indexed_cleanup_function_ir_rewrite_operation(
    std::string const& original_function_ir,
    RuntimeIndexedCleanupFunctionIrRewriteOperation const& operation
) -> RuntimeIndexedCleanupFunctionIrRewriteResult {
    auto const stage_result = apply_runtime_indexed_cleanup_function_ir_rewrite_operation_stages(
        original_function_ir,
        operation
    );
    if (!stage_result.succeeded()) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = stage_result.failure,
            .validation_part_available = stage_result.validation_part_available,
            .validation_part_index = stage_result.validation_part_index,
            .validation_splice_range = stage_result.validation_splice_range,
        };
    }
    return RuntimeIndexedCleanupFunctionIrRewriteResult {
        .rewritten_function_ir = stage_result.staged_function_ir,
    };
}

auto apply_runtime_indexed_cleanup_function_ir_rewrite_operation_stages(
    std::string const& original_function_ir,
    RuntimeIndexedCleanupFunctionIrRewriteOperation const& operation
) -> RuntimeIndexedCleanupFunctionIrRewriteStageResult {
    auto const edit_script_result = build_runtime_indexed_cleanup_function_ir_edit_script_result(operation);
    if (!edit_script_result.succeeded()) {
        return RuntimeIndexedCleanupFunctionIrRewriteStageResult {
            .failure = edit_script_result.failure,
        };
    }
    return apply_runtime_indexed_cleanup_function_ir_edit_script_stages(
        original_function_ir,
        edit_script_result.edit_script
    );
}

auto apply_runtime_indexed_cleanup_function_ir_edit_script_stages(
    std::string const& original_function_ir,
    RuntimeIndexedCleanupFunctionIrEditScript const& edit_script
) -> RuntimeIndexedCleanupFunctionIrRewriteStageResult {
    auto const validation = validate_runtime_indexed_cleanup_function_ir_edit_script(
        original_function_ir,
        edit_script
    );
    if (!validation.succeeded()) {
        return RuntimeIndexedCleanupFunctionIrRewriteStageResult {
            .failure = validation.failure,
            .validation_part_available = validation.part_available,
            .validation_part_index = validation.part_index,
            .validation_splice_range = validation.splice_range,
        };
    }
    auto composed = original_function_ir;
    for (auto const& part : edit_script.branch_replacements) {
        composed.replace(
            part.splice_range.start_offset,
            part.splice_range.end_offset - part.splice_range.start_offset,
            part.replacement_branch_text
        );
    }

    auto const closing_position = composed.rfind(edit_script.cleanup_cfg_append.expected_closing_text);
    if (closing_position == std::string::npos) {
        return RuntimeIndexedCleanupFunctionIrRewriteStageResult {
            .staged_function_ir = std::move(composed),
            .failure = RuntimeIndexedCleanupIrCompositionFailure::missing_function_closing_brace,
            .branch_replacements_applied = true,
        };
    }
    composed.insert(closing_position + 1, edit_script.cleanup_cfg_append.append_text);
    for (auto const& retarget : edit_script.phi_predecessor_retargets) {
        composed = retarget_phi_incoming_predecessor(
            composed,
            retarget.old_predecessor_block_name,
            retarget.new_predecessor_block_name
        );
        if (composed.empty()) {
            return RuntimeIndexedCleanupFunctionIrRewriteStageResult {
                .failure = RuntimeIndexedCleanupIrCompositionFailure::phi_retarget_failed,
                .branch_replacements_applied = true,
                .cleanup_cfg_appended = true,
            };
        }
    }
    return RuntimeIndexedCleanupFunctionIrRewriteStageResult {
        .staged_function_ir = std::move(composed),
        .branch_replacements_applied = true,
        .cleanup_cfg_appended = true,
        .phi_predecessors_retargeted = true,
    };
}

auto compose_non_overlapping_function_ir_rewrite_result(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> RuntimeIndexedCleanupFunctionIrRewriteResult {
    auto const operation_result =
        build_runtime_indexed_cleanup_function_ir_rewrite_operation_result(original_function_ir, std::move(candidates));
    if (!operation_result.succeeded()) {
        return RuntimeIndexedCleanupFunctionIrRewriteResult {
            .failure = operation_result.failure,
        };
    }
    return apply_runtime_indexed_cleanup_function_ir_rewrite_operation(
        original_function_ir,
        operation_result.operation
    );
}

auto runtime_indexed_cleanup_ir_composition_failure_token(
    RuntimeIndexedCleanupIrCompositionFailure failure
) -> std::string_view {
    switch (failure) {
    case RuntimeIndexedCleanupIrCompositionFailure::none:
        return "none";
    case RuntimeIndexedCleanupIrCompositionFailure::empty_input:
        return "empty-input";
    case RuntimeIndexedCleanupIrCompositionFailure::missing_function_closing_brace:
        return "missing-function-closing-brace";
    case RuntimeIndexedCleanupIrCompositionFailure::missing_predecessor_block:
        return "missing-predecessor-block";
    case RuntimeIndexedCleanupIrCompositionFailure::missing_predecessor_terminator:
        return "missing-predecessor-terminator";
    case RuntimeIndexedCleanupIrCompositionFailure::ambiguous_predecessor_terminator:
        return "ambiguous-predecessor-terminator";
    case RuntimeIndexedCleanupIrCompositionFailure::missing_cleanup_exit_block:
        return "missing-cleanup-exit-block";
    case RuntimeIndexedCleanupIrCompositionFailure::phi_retarget_failed:
        return "phi-retarget-failed";
    case RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate:
        return "invalid-candidate";
    case RuntimeIndexedCleanupIrCompositionFailure::unexpected_splice_text:
        return "unexpected-splice-text";
    case RuntimeIndexedCleanupIrCompositionFailure::missing_cleanup_cfg_tail:
        return "missing-cleanup-cfg-tail";
    }
    return "unknown";
}

} // namespace orison::pipeline
