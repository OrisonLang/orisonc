#pragma once

#include "orison/lowering/aggregate_projection_access_plan.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_call.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_handoff.hpp"
#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/consumed_descriptor_finalization.hpp"
#include "orison/lowering/dynamic_array_cleanup_capability.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>
#include <vector>

namespace orison::lowering {

struct FunctionEmissionResult {
    std::string function_symbol_name;
    std::string ir_text;
    std::vector<std::string> emitted_dynamic_array_cleanup_obligation_report;
    std::vector<std::string> emitted_dynamic_array_cleanup_sequence_plan_report;
    std::vector<std::string> emitted_dynamic_array_cleanup_sequence_verification_report;
    std::vector<std::string> emitted_dynamic_array_cleanup_emission_gate_report;
    std::vector<DynamicArrayCleanupEmissionCapability> emitted_dynamic_array_cleanup_emission_capabilities;
    std::vector<AggregateProjectionAccessPlan> aggregate_projection_access_plans;
    std::vector<ComputedDynamicArrayCleanupStateHandoff> computed_dynamic_array_inserted_cleanup_handoffs;
    std::vector<ComputedDynamicArrayCleanupCallOperands> computed_dynamic_array_cleanup_call_operands;
    std::vector<ConsumedDescriptorFinalizationPlan> consumed_descriptor_finalization_plans;
};

auto emit_function_with_metadata(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    semantics::SemanticAnalysisResult const& semantic_result,
    diagnostics::DiagnosticBag& diagnostics,
    LlvmIrEmissionOptions const& options = {}
) -> FunctionEmissionResult;

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

}  // namespace orison::lowering
