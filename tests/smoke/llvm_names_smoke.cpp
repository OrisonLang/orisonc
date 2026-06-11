#include "orison/lowering/llvm_names.hpp"

#include <cassert>

int main() {
    auto counts = std::unordered_map<std::string, std::size_t> {};
    assert(orison::lowering::llvm_local_value_name("value") == "%value");
    assert(orison::lowering::next_llvm_local_value_name("value", counts) == "%value");
    assert(orison::lowering::next_llvm_local_value_name("value", counts) == "%value.1");

    auto temporary_index = std::size_t {0};
    assert(orison::lowering::next_llvm_temporary_name(temporary_index) == "%tmp0");
    assert(orison::lowering::next_llvm_temporary_name(temporary_index) == "%tmp1");

    auto block_index = std::size_t {0};
    auto first = orison::lowering::next_llvm_block_index(block_index);
    assert(first == 0);
    assert(orison::lowering::llvm_block_name("if.then", first) == "if.then.0");
    assert(orison::lowering::llvm_block_name("switch.case", first, 2) == "switch.case.0.2");
    return 0;
}
