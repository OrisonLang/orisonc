#include "runtime_indexed_cleanup_ir_composition.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

namespace {

void assert_runtime_indexed_cleanup_ir_text_helpers_are_shared() {
    auto const function_ir =
        std::string {
            "define i32 @helper() {\n"
            "entry:\n"
            "  br label %left\n"
            "\n"
            "left:\n"
            "  %x = add i32 1, 2\n"
            "  br label %join\n"
            "\n"
            "join:\n"
            "  ret i32 0\n"
            "}\n"
        };

    assert(orison::pipeline::occurrence_count(function_ir, "br label") == 2);
    assert(orison::pipeline::block_label_found(function_ir, "left"));
    assert(!orison::pipeline::block_label_found(function_ir, "missing"));
    assert(
        orison::pipeline::predecessor_branch_pattern(function_ir, "entry", "br label %left") ==
        "  br label %left\n"
    );
    auto const terminator = orison::pipeline::predecessor_terminator_pattern(function_ir, "left");
    assert(terminator == "  br label %join\n");
    auto const terminator_position =
        orison::pipeline::predecessor_terminator_position(function_ir, "left", terminator);
    assert(terminator_position != std::string::npos);
    assert(function_ir.substr(terminator_position, terminator.size()) == terminator);
}

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
    auto const part_result =
        orison::pipeline::build_runtime_indexed_cleanup_function_ir_composition_part_result(
            original_function_ir,
            candidates
        );
    assert(part_result.succeeded());

    auto const& parts = part_result.parts;
    assert(parts.size() == 2);
    assert(parts[0].predecessor_block_name == "left");
    assert(parts[0].continuation_block_name == "left.runtime_cleanup.exit");
    assert(parts[0].replacement_branch_text == "  br label %left.runtime_cleanup.entry\n");
    assert(parts[0].cleanup_cfg_tail.find("left.runtime_cleanup.entry:\n") != std::string::npos);
    assert(parts[1].predecessor_block_name == "right");
    assert(parts[1].continuation_block_name == "right.runtime_cleanup.exit");
    assert(parts[1].replacement_branch_text == "  br label %right.runtime_cleanup.entry\n");
    assert(parts[1].cleanup_cfg_tail.find("right.runtime_cleanup.entry:\n") != std::string::npos);

    auto const operation_result =
        orison::pipeline::build_runtime_indexed_cleanup_function_ir_rewrite_operation_result(
            original_function_ir,
            candidates
        );
    assert(operation_result.succeeded());
    assert(operation_result.operation.parts.size() == 2);
    assert(
        operation_result.operation.appended_cleanup_cfg.find("left.runtime_cleanup.entry:\n") !=
        std::string::npos
    );
    assert(
        operation_result.operation.appended_cleanup_cfg.find("right.runtime_cleanup.entry:\n") !=
        std::string::npos
    );
    assert(
        operation_result.operation.appended_cleanup_cfg.find("left.runtime_cleanup.entry:\n") <
        operation_result.operation.appended_cleanup_cfg.find("right.runtime_cleanup.entry:\n")
    );

    auto const compose_result =
        orison::pipeline::compose_non_overlapping_function_ir_rewrite_result(original_function_ir, candidates);
    assert(compose_result.succeeded());

    auto const& composed = compose_result.rewritten_function_ir;
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

    auto const rewrite_result =
        orison::pipeline::rewrite_predecessor_terminator_and_insert_cfg_result(original_function_ir, insertion);
    assert(rewrite_result.succeeded());

    auto const& rewritten = rewrite_result.rewritten_function_ir;
    assert(!rewritten.empty());
    assert(rewritten.find("entry:\n  br label %entry.runtime_cleanup.entry\n") != std::string::npos);
    assert(rewritten.find("entry.runtime_cleanup.entry:\n") != std::string::npos);
    assert(rewritten.find("entry.runtime_cleanup.exit:\n  br label %join\n") != std::string::npos);
    assert(rewritten.find("%x = phi i32 [ 1, %entry.runtime_cleanup.exit ]") != std::string::npos);
}

void assert_runtime_indexed_cleanup_ir_failures_are_structured() {
    auto const original_function_ir =
        std::string {
            "define i32 @failed() {\n"
            "entry:\n"
            "  br label %join\n"
            "\n"
            "join:\n"
            "  ret i32 0\n"
            "}\n"
        };
    auto const missing_predecessor = orison::pipeline::RuntimeIndexedCleanupFunctionIrInsertion {
        .predecessor_block_name = "missing",
        .inserted_branch_text = "br label %cleanup.entry",
        .cfg_lines = {
            "  br label %cleanup.entry\n",
            "cleanup.entry:\n",
        },
    };
    auto const insertion_result =
        orison::pipeline::rewrite_predecessor_terminator_and_insert_cfg_result(
            original_function_ir,
            missing_predecessor
        );
    assert(!insertion_result.succeeded());
    assert(insertion_result.rewritten_function_ir.empty());
    assert(
        insertion_result.failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::missing_predecessor_block
    );

    auto candidate = candidate_with_cleanup_tail("entry", "entry.runtime_cleanup.entry", "entry.runtime_cleanup.exit");
    candidate.splice_start_offset = 0;
    candidate.splice_end_offset = 2;
    auto const candidates =
        std::vector<orison::pipeline::RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> {
            &candidate,
        };
    auto const part_result =
        orison::pipeline::build_runtime_indexed_cleanup_function_ir_composition_part_result(
            original_function_ir,
            candidates
        );
    assert(!part_result.succeeded());
    assert(part_result.parts.empty());
    assert(
        part_result.failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::unexpected_splice_text
    );

    auto const compose_result =
        orison::pipeline::compose_non_overlapping_function_ir_rewrite_result({}, candidates);
    assert(!compose_result.succeeded());
    assert(compose_result.rewritten_function_ir.empty());
    assert(
        compose_result.failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::empty_input
    );
}

} // namespace

auto main() -> int {
    assert_runtime_indexed_cleanup_ir_text_helpers_are_shared();
    assert_runtime_indexed_cleanup_ir_composition_parts_are_structured();
    assert_runtime_indexed_cleanup_ir_single_candidate_insertion_is_structured();
    assert_runtime_indexed_cleanup_ir_failures_are_structured();
    return 0;
}
