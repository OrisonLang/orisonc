#pragma once

#include "orison/lowering/lowering_context.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

auto can_emit_interface_dispatch_table(LoweredInterfaceDispatchTable const& table) -> bool;

auto emit_interface_dispatch_definitions(LoweringContext const& context) -> std::string;

auto emit_borrowed_interface_existential(
    LoweredInterfaceExistentialAbi const& abi,
    LoweredInterfaceDispatchTable const& table,
    std::string_view concrete_storage_pointer,
    std::string_view result_name
) -> std::string;

auto emit_borrowed_interface_dispatch_call(
    LoweredInterfaceExistentialAbi const& abi,
    LoweredInterfaceDispatchTable const& table,
    std::size_t slot_index,
    std::string_view existential_value,
    std::vector<LoweredExpression> const& arguments,
    std::string_view instruction_stem
) -> std::string;

}  // namespace orison::lowering
