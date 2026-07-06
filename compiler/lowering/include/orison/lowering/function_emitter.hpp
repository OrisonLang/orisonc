#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/deferred_cleanup.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>

namespace orison::lowering {

auto emit_function(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    semantics::SemanticAnalysisResult const& semantic_result,
    diagnostics::DiagnosticBag& diagnostics,
    LlvmIrEmissionOptions const& options = {}
) -> std::string;

auto emit_function(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    diagnostics::DiagnosticBag& diagnostics,
    LlvmIrEmissionOptions const& options = {}
) -> std::string;

auto lower_unit_deferred_cleanup_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

}  // namespace orison::lowering
