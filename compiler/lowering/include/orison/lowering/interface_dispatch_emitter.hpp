#pragma once

#include "orison/lowering/lowering_context.hpp"

#include <string>

namespace orison::lowering {

auto can_emit_interface_dispatch_table(LoweredInterfaceDispatchTable const& table) -> bool;

auto emit_interface_dispatch_definitions(LoweringContext const& context) -> std::string;

}  // namespace orison::lowering
