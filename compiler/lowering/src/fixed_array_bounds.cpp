#include "orison/lowering/fixed_array_bounds.hpp"

#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"

#include <ostream>
#include <string>

namespace orison::lowering {

void emit_fixed_array_runtime_index_bounds_check(
    std::string_view prefix_stem,
    std::string_view index_value,
    std::size_t length,
    FunctionLoweringSession& session,
    std::ostream& output
) {
    auto prefix = std::string {"%"};
    prefix += prefix_stem;
    prefix += std::to_string(session.state.next_temporary_index++);
    output << emit_dynamic_array_bounds_check(
        prefix + ".in_bounds",
        index_value,
        std::to_string(length),
        DynamicArrayBoundsCheckKind::index_within_length
    );

    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto value_block = llvm_block_name("fixed_array.index.in_bounds", block_index);
    auto failure_block = llvm_block_name("fixed_array.index.out_of_bounds", block_index);
    emit_llvm_conditional_branch(output, prefix + ".in_bounds", value_block, failure_block);
    emit_llvm_block_label(output, failure_block);
    output << "  call void @__orison_dynamic_array_bounds_failed()\n";
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, value_block);
    session.state.current_block = value_block;
}

}  // namespace orison::lowering
