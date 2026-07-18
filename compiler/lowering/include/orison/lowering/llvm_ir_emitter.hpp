#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct LlvmIrEmissionResult {
    diagnostics::DiagnosticBag diagnostics;
    std::string ir_text;
    std::vector<ConcurrencyDropCleanupPlan> drop_cleanups;
    std::vector<PlannedDropAction> planned_drop_actions;
    std::vector<PlannedDropDeclaration> planned_drop_declarations;
    std::vector<DynamicArrayRuntimeOperation> dynamic_array_runtime_operations;
    std::vector<DynamicArrayConstructionPlan> dynamic_array_construction_plans;
    std::vector<std::string> test_only_dynamic_array_allocation_call_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_binding_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_projection_ir;
    std::vector<std::string> test_only_dynamic_array_bounds_check_ir;
    std::vector<std::string> test_only_dynamic_array_element_address_ir;
    std::vector<std::string> test_only_dynamic_array_element_load_ir;
    std::vector<std::string> test_only_dynamic_array_element_store_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_length_update_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_write_back_ir;
    std::vector<std::string> test_only_dynamic_array_append_sequence_ir;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;

    auto has_errors() const -> bool;
    auto render(std::string_view path) const -> std::string;
    auto planned_drop_report() const -> std::vector<std::string>;
    auto dynamic_array_construction_plan_report() const -> std::vector<std::string>;
    auto dynamic_array_runtime_request_report() const -> std::vector<std::string>;
    auto emitted_drop_declaration_report() const -> std::vector<std::string>;
    auto planned_drop_action_report() const -> std::vector<std::string>;
    auto drop_cleanup_authorization_report() const -> std::vector<std::string>;
    auto drop_readiness_snapshot() const -> DropReadinessSnapshot;
    auto drop_readiness_snapshot_report() const -> std::vector<std::string>;
    auto drop_readiness_summary() const -> DropReadinessSummary;
    auto drop_readiness_summary_report() const -> std::vector<std::string>;
    auto drop_readiness_relation_report() const -> std::vector<std::string>;
};

class LlvmIrEmitter {
public:
    auto emit(
        syntax::ModuleSyntax const& module,
        semantics::SemanticAnalysisResult const& semantic_result,
        LlvmIrEmissionOptions const& options = {}
    ) const -> LlvmIrEmissionResult;
};

}  // namespace orison::lowering
