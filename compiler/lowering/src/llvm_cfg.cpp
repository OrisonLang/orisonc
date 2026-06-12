#include "orison/lowering/llvm_cfg.hpp"

namespace orison::lowering {

void emit_llvm_block_label(std::ostream& output, std::string_view block) {
    output << block << ":\n";
}

void emit_llvm_branch(std::ostream& output, std::string_view target) {
    output << "  br label %" << target << "\n";
}

void emit_llvm_conditional_branch(
    std::ostream& output,
    std::string_view condition,
    std::string_view true_target,
    std::string_view false_target
) {
    output << "  br i1 " << condition << ", label %" << true_target;
    output << ", label %" << false_target << "\n";
}

void emit_llvm_switch(
    std::ostream& output,
    std::string_view type,
    std::string_view value,
    std::string_view default_target,
    std::span<LlvmSwitchTarget const> targets
) {
    output << "  switch " << type << " " << value << ", label %" << default_target << " [\n";
    for (auto const& target : targets) {
        output << "    " << type << " " << target.value << ", label %" << target.block << "\n";
    }
    output << "  ]\n";
}

void emit_llvm_unreachable(std::ostream& output) {
    output << "  unreachable\n";
}

void emit_llvm_phi(
    std::ostream& output,
    std::string_view result,
    std::string_view type,
    std::span<BranchMergeIncoming const> incoming
) {
    output << "  " << result << " = phi " << type << " ";
    for (auto index = std::size_t {0}; index < incoming.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << "[" << incoming[index].value << ", %" << incoming[index].block << "]";
    }
    output << "\n";
}

}  // namespace orison::lowering
