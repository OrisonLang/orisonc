#include "runtime_indexed_cleanup_ir_composition.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

namespace {

auto branch_position_in_block(
    std::string const& function_ir,
    std::string const& block_name,
    std::string const& branch_text
) -> std::size_t {
    auto const block_label = "\n" + block_name + ":\n";
    auto const block_position = function_ir.find(block_label);
    assert(block_position != std::string::npos);
    auto const branch_position = function_ir.find(branch_text, block_position + block_label.size());
    assert(branch_position != std::string::npos);
    return branch_position;
}

auto candidate_with_cleanup_tail(
    std::string const& predecessor_name,
    std::string const& insertion_name,
    std::string const& continuation_name
) -> orison::pipeline::RuntimeIndexedCleanupFunctionIrRewriteCandidate {
    auto candidate = orison::pipeline::RuntimeIndexedCleanupFunctionIrRewriteCandidate {
        .function_symbol_name = "main",
        .predecessor_block_name = predecessor_name,
        .insertion_block_name = insertion_name,
        .continuation_block_name = continuation_name,
        .replaced_terminator_text = "br label %join",
        .inserted_branch_text = "br label %" + insertion_name,
        .candidate_function_ir_text = std::string {
            "define i32 @main() {\n"
            "entry:\n"
            "  br label %left\n"
            "\n"
        } +
            predecessor_name + ":\n"
            "  br label %" + insertion_name + "\n"
            "\n"
            "join:\n"
            "  ret i32 0\n"
            "\n" +
            insertion_name + ":\n"
            "  br label %" + continuation_name + "\n"
            "\n" +
            continuation_name + ":\n"
            "  br label %join\n"
            "}\n",
        .candidate_available = true,
        .splice_range_available = true,
    };
    return candidate;
}

void assert_runtime_indexed_cleanup_ir_composition_parts_are_structured() {
    auto const original_function_ir =
        std::string {
            "define i32 @main() {\n"
            "entry:\n"
            "  br label %left\n"
            "\n"
            "left:\n"
            "  br label %join\n"
            "\n"
            "right:\n"
            "  br label %join\n"
            "\n"
            "join:\n"
            "  %x = phi i32 [ 1, %left ], [ 2, %right ]\n"
            "  ret i32 %x\n"
            "}\n"
        };

    auto left_candidate =
        candidate_with_cleanup_tail("left", "left.runtime_cleanup.entry", "left.runtime_cleanup.exit");
    auto right_candidate =
        candidate_with_cleanup_tail("right", "right.runtime_cleanup.entry", "right.runtime_cleanup.exit");
    auto const branch_text = std::string {"  br label %join\n"};
    left_candidate.splice_start_offset = branch_position_in_block(original_function_ir, "left", branch_text);
    left_candidate.splice_end_offset = left_candidate.splice_start_offset + branch_text.size();
    right_candidate.splice_start_offset = branch_position_in_block(original_function_ir, "right", branch_text);
    right_candidate.splice_end_offset = right_candidate.splice_start_offset + branch_text.size();

    auto const candidates =
        std::vector<orison::pipeline::RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> {
            &left_candidate,
            &right_candidate,
        };
    auto const parts =
        orison::pipeline::build_runtime_indexed_cleanup_function_ir_composition_parts(
            original_function_ir,
            candidates
        );

    assert(parts.size() == 2);
    assert(parts[0].predecessor_block_name == "left");
    assert(parts[0].continuation_block_name == "left.runtime_cleanup.exit");
    assert(parts[0].replacement_branch_text == "  br label %left.runtime_cleanup.entry\n");
    assert(parts[0].cleanup_cfg_tail.find("left.runtime_cleanup.entry:\n") != std::string::npos);
    assert(parts[1].predecessor_block_name == "right");
    assert(parts[1].continuation_block_name == "right.runtime_cleanup.exit");
    assert(parts[1].replacement_branch_text == "  br label %right.runtime_cleanup.entry\n");
    assert(parts[1].cleanup_cfg_tail.find("right.runtime_cleanup.entry:\n") != std::string::npos);

    auto const composed =
        orison::pipeline::compose_non_overlapping_function_ir_rewrite(original_function_ir, candidates);
    assert(!composed.empty());
    assert(composed.find("left:\n  br label %left.runtime_cleanup.entry\n") != std::string::npos);
    assert(composed.find("right:\n  br label %right.runtime_cleanup.entry\n") != std::string::npos);
    assert(composed.find("left.runtime_cleanup.entry:\n") != std::string::npos);
    assert(composed.find("right.runtime_cleanup.entry:\n") != std::string::npos);
    assert(
        composed.find("%x = phi i32 [ 1, %left.runtime_cleanup.exit ], [ 2, %right.runtime_cleanup.exit ]") !=
        std::string::npos
    );
}

void assert_runtime_indexed_cleanup_ir_single_candidate_insertion_is_structured() {
    auto const original_function_ir =
        std::string {
            "define i32 @single() {\n"
            "entry:\n"
            "  br label %join\n"
            "\n"
            "join:\n"
            "  %x = phi i32 [ 1, %entry ]\n"
            "  ret i32 %x\n"
            "}\n"
        };
    auto const insertion = orison::pipeline::RuntimeIndexedCleanupFunctionIrInsertion {
        .predecessor_block_name = "entry",
        .inserted_branch_text = "br label %entry.runtime_cleanup.entry",
        .cfg_lines = {
            "  br label %entry.runtime_cleanup.entry\n",
            "entry.runtime_cleanup.entry:\n",
            "  br label %entry.runtime_cleanup.exit\n",
            "\n",
            "entry.runtime_cleanup.exit:\n",
        },
    };

    auto const rewritten =
        orison::pipeline::rewrite_predecessor_terminator_and_insert_cfg(original_function_ir, insertion);

    assert(!rewritten.empty());
    assert(rewritten.find("entry:\n  br label %entry.runtime_cleanup.entry\n") != std::string::npos);
    assert(rewritten.find("entry.runtime_cleanup.entry:\n") != std::string::npos);
    assert(rewritten.find("entry.runtime_cleanup.exit:\n  br label %join\n") != std::string::npos);
    assert(rewritten.find("%x = phi i32 [ 1, %entry.runtime_cleanup.exit ]") != std::string::npos);
}

} // namespace

auto main() -> int {
    assert_runtime_indexed_cleanup_ir_composition_parts_are_structured();
    assert_runtime_indexed_cleanup_ir_single_candidate_insertion_is_structured();
    return 0;
}
