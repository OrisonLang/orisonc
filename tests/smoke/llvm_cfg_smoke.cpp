#include "orison/lowering/llvm_cfg.hpp"

#include <array>
#include <cassert>
#include <sstream>

int main() {
    auto output = std::ostringstream {};
    orison::lowering::emit_llvm_conditional_branch(output, "%flag", "if.then.0", "if.else.0");
    orison::lowering::emit_llvm_block_label(output, "if.then.0");
    orison::lowering::emit_llvm_branch(output, "if.merge.0");

    auto switch_targets = std::array {
        orison::lowering::LlvmSwitchTarget {
            .value = "1",
            .block = "switch.case.0.0",
        },
        orison::lowering::LlvmSwitchTarget {
            .value = "2",
            .block = "switch.case.0.1",
        },
    };
    orison::lowering::emit_llvm_switch(
        output,
        "i32",
        "%subject",
        "switch.default.0",
        switch_targets
    );
    orison::lowering::emit_llvm_block_label(output, "switch.unreachable.0");
    orison::lowering::emit_llvm_unreachable(output);

    auto incoming = std::array {
        orison::lowering::BranchMergeIncoming {
            .value = "%left",
            .block = "if.then.0",
        },
        orison::lowering::BranchMergeIncoming {
            .value = "%right",
            .block = "if.else.0",
        },
    };
    orison::lowering::emit_llvm_phi(output, "%tmp0", "i32", incoming);

    assert(
        output.str() ==
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br label %if.merge.0\n"
        "  switch i32 %subject, label %switch.default.0 [\n"
        "    i32 1, label %switch.case.0.0\n"
        "    i32 2, label %switch.case.0.1\n"
        "  ]\n"
        "switch.unreachable.0:\n"
        "  unreachable\n"
        "  %tmp0 = phi i32 [%left, %if.then.0], [%right, %if.else.0]\n"
    );
    return 0;
}
