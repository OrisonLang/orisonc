#pragma once

#include <ostream>
#include <span>
#include <string_view>

namespace orison::lowering {

struct LlvmSwitchTarget {
    std::string_view value;
    std::string_view block;
};

struct LlvmPhiIncoming {
    std::string_view value;
    std::string_view block;
};

void emit_llvm_block_label(std::ostream& output, std::string_view block);

void emit_llvm_branch(std::ostream& output, std::string_view target);

void emit_llvm_conditional_branch(
    std::ostream& output,
    std::string_view condition,
    std::string_view true_target,
    std::string_view false_target
);

void emit_llvm_switch(
    std::ostream& output,
    std::string_view type,
    std::string_view value,
    std::string_view default_target,
    std::span<LlvmSwitchTarget const> targets
);

void emit_llvm_unreachable(std::ostream& output);

void emit_llvm_phi(
    std::ostream& output,
    std::string_view result,
    std::string_view type,
    std::span<LlvmPhiIncoming const> incoming
);

}  // namespace orison::lowering
