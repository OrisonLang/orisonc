#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/lowering_failure_lifecycle.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace orison::lowering {

struct DirectDynamicArrayReceiver {
    LoweredExpression argument;
    std::string cleanup_owner_name;
};

struct DirectDynamicArrayReceiverLowering {
    std::optional<DirectDynamicArrayReceiver> receiver;
    std::string diagnostic;
};

auto lower_direct_dynamic_array_receiver(
    syntax::ExpressionSyntax const& receiver_expression,
    LoweredFunctionSignature const& method_signature,
    std::string_view receiver_type_name,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output,
    bool record_expression_failures
) -> DirectDynamicArrayReceiverLowering;

}  // namespace orison::lowering
