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

} // namespace

auto build_runtime_indexed_cleanup_function_ir_composition_parts(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> const& candidates
) -> std::vector<RuntimeIndexedCleanupFunctionIrCompositionPart> {
    auto parts = std::vector<RuntimeIndexedCleanupFunctionIrCompositionPart> {};
    parts.reserve(candidates.size());
    for (auto const* candidate : candidates) {
        if (!candidate->candidate_available || !candidate->splice_range_available ||
            candidate->splice_start_offset >= candidate->splice_end_offset ||
            candidate->splice_end_offset > original_function_ir.size()) {
            return {};
        }
        auto const expected_terminator = "  " + candidate->replaced_terminator_text + "\n";
        if (original_function_ir.substr(
                candidate->splice_start_offset,
                candidate->splice_end_offset - candidate->splice_start_offset
            ) != expected_terminator) {
            return {};
        }
        auto cleanup_tail = inserted_cleanup_cfg_tail(*candidate);
        if (cleanup_tail.empty()) {
            return {};
        }
        parts.push_back(RuntimeIndexedCleanupFunctionIrCompositionPart {
            .predecessor_block_name = candidate->predecessor_block_name,
            .continuation_block_name = candidate->continuation_block_name,
            .replacement_branch_text = "  " + candidate->inserted_branch_text + "\n",
            .cleanup_cfg_tail = std::move(cleanup_tail),
            .splice_start_offset = candidate->splice_start_offset,
            .splice_end_offset = candidate->splice_end_offset,
        });
    }
    return parts;
}

auto compose_non_overlapping_function_ir_rewrite(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> std::string {
    if (original_function_ir.empty() || candidates.empty()) {
        return {};
    }
    std::sort(
        candidates.begin(),
        candidates.end(),
        [](auto const* left, auto const* right) {
            return left->splice_start_offset > right->splice_start_offset;
        }
    );

    auto composed = original_function_ir;
    auto const composition_parts =
        build_runtime_indexed_cleanup_function_ir_composition_parts(original_function_ir, candidates);
    if (composition_parts.empty()) {
        return {};
    }
    auto appended_cleanup_cfg = std::string {};
    for (auto const& part : composition_parts) {
        composed.replace(
            part.splice_start_offset,
            part.splice_end_offset - part.splice_start_offset,
            part.replacement_branch_text
        );
        appended_cleanup_cfg = part.cleanup_cfg_tail + appended_cleanup_cfg;
    }

    auto const closing_position = composed.rfind("\n}\n");
    if (closing_position == std::string::npos) {
        return {};
    }
    composed.insert(closing_position + 1, appended_cleanup_cfg);
    for (auto const& part : composition_parts) {
        composed = retarget_phi_incoming_predecessor(
            composed,
            part.predecessor_block_name,
            part.continuation_block_name
        );
        if (composed.empty()) {
            return {};
        }
    }
    return composed;
}

} // namespace orison::pipeline
