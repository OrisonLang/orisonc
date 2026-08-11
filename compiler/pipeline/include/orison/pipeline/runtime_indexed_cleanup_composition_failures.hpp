#pragma once

#include <string_view>

namespace orison::pipeline {

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

auto runtime_indexed_cleanup_ir_composition_failure_token(
    RuntimeIndexedCleanupIrCompositionFailure failure
) -> std::string_view;

} // namespace orison::pipeline
