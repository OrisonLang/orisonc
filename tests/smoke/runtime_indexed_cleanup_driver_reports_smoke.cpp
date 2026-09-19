#include "orison/driver/runtime_indexed_cleanup_reports.hpp"
#include "orison/pipeline/compile_pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

namespace driver = orison::driver;
namespace pipeline = orison::pipeline;

void assert_mutation_report_with_composition_detail() {
    auto const report = driver::runtime_indexed_cleanup_function_module_mutation_report(
        pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .replacement_targets_unique = true,
            .mutation_applied = false,
            .module_matches_candidate = false,
            .llvm_verifier_passed = false,
            .composition_failure = pipeline::RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
            .composition_failure_part_available = true,
            .rewrite_apply_stage_available = true,
            .branch_replacements_applied = true,
            .cleanup_cfg_appended = false,
            .phi_predecessors_retargeted = false,
            .candidate_count = 3,
            .composition_failure_part_index = 2,
            .composition_failure_splice_range = pipeline::RuntimeIndexedCleanupTextSpliceRange {
                .start_offset = 144,
                .end_offset = 188,
            },
            .final_module_line_count = 91,
            .llvm_verifier_diagnostic_count = 0,
        }
    );

    assert(report.find("runtime-index cleanup function-module mutation requested true") != std::string::npos);
    assert(report.find("candidate-verified true replacement-targets unique") != std::string::npos);
    assert(report.find("mutation-applied false module-matches-candidate false") != std::string::npos);
    assert(
        report.find(
            "composition-failure invalid-candidate composition-part 2 splice-range 144..188 "
            "apply-stages available branch-replacements true cleanup-cfg-appended false phi-retargeted false "
            "llvm-passed false diagnostics 0 final-lines 91"
        ) != std::string::npos
    );
}

void assert_mutation_report_without_composition_detail() {
    auto const report = driver::runtime_indexed_cleanup_function_module_mutation_report(
        pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .replacement_targets_unique = true,
            .mutation_applied = true,
            .module_matches_candidate = true,
            .llvm_verifier_passed = true,
            .composition_failure = pipeline::RuntimeIndexedCleanupIrCompositionFailure::none,
            .composition_failure_part_available = false,
            .rewrite_apply_stage_available = false,
            .branch_replacements_applied = false,
            .cleanup_cfg_appended = false,
            .phi_predecessors_retargeted = false,
            .candidate_count = 1,
            .final_module_line_count = 42,
            .llvm_verifier_diagnostic_count = 0,
        }
    );

    assert(
        report.find(
            "composition-failure none apply-stages unavailable branch-replacements false "
            "cleanup-cfg-appended false phi-retargeted false llvm-passed true diagnostics 0 final-lines 42"
        ) != std::string::npos
    );
    assert(report.find("composition-part") == std::string::npos);
    assert(report.find("splice-range") == std::string::npos);
}

void assert_mutation_report_for_scalar_same_function_success() {
    auto const report = driver::runtime_indexed_cleanup_function_module_mutation_report(
        pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .replacement_targets_unique = true,
            .mutation_applied = true,
            .module_matches_candidate = true,
            .llvm_verifier_passed = true,
            .composition_failure = pipeline::RuntimeIndexedCleanupIrCompositionFailure::none,
            .composition_failure_part_available = false,
            .rewrite_apply_stage_available = true,
            .branch_replacements_applied = true,
            .cleanup_cfg_appended = true,
            .phi_predecessors_retargeted = true,
            .candidate_count = 2,
            .final_module_line_count = 176,
            .llvm_verifier_diagnostic_count = 0,
        }
    );

    assert(
        report ==
        "runtime-index cleanup function-module mutation requested true candidate-verified true "
        "replacement-targets unique mutation-applied true module-matches-candidate true "
        "composition-failure none apply-stages available branch-replacements true "
        "cleanup-cfg-appended true phi-retargeted true llvm-passed true diagnostics 0 final-lines 176"
    );
    assert(report.find("composition-part") == std::string::npos);
    assert(report.find("splice-range") == std::string::npos);
}

void assert_constructor_move_report_for_two_member_cleanup_ready() {
    auto options = pipeline::production_compile_pipeline_options();
    options.collect_runtime_indexed_cleanup_audit = true;
    auto const result = pipeline::CompilePipeline {}.emit_llvm(
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
            "runtime_indexed_dynamic_array_constructor_two_computed_member_transfers.or",
        options
    );
    auto const report = driver::runtime_indexed_constructor_move_production_readiness_report(result);

    assert(
        report.find(
            "runtime-index cleanup constructor-move production-readiness "
            "constructor-move enabled partial-ownership accepted cleanup-proof ready cleanup-production enabled "
            "capability-count 2 ordinary-emit accepted member-cleanup-promotion ready "
            "member-production-records 2 member-gate-records 2 member-mutation-records 2 "
            "member-rewrite-records 2 diagnostic none member-module-ir-shape ready"
        ) != std::string::npos
    );
    assert(report.find("diagnostic none member-module-ir-shape ready") != std::string::npos);
    assert(report.find("member-module-ir-shape-detail") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move plan owner left_items") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move plan owner right_items") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move ir-shape owner left_items") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move ir-shape owner right_items") == std::string::npos);
    assert(
        report.find(
            "runtime-index member cleanup helper-drop-bindings owner left_items "
            "index (left_index + left_zero) element Box moved Inner member-path item "
            "source-line 18 source-text "
            "var left_outer: Outer = Outer(left_items[left_index + left_zero].item) "
            "helper __orison_member_cleanup.Box.except.item sibling-bindings 0 "
            "drop-definitions ready nested-path false helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup helper-drop-bindings owner right_items "
            "index (right_index + right_zero) element Box moved Inner member-path item "
            "source-line 25 source-text "
            "var right_outer: Outer = Outer(right_items[right_index + right_zero].item) "
            "helper __orison_member_cleanup.Box.except.item sibling-bindings 0 "
            "drop-definitions ready nested-path false helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup production-readiness owner left_items "
            "index (left_index + left_zero) element Box moved Inner member-path item "
            "source-line 18 source-text "
            "var left_outer: Outer = Outer(left_items[left_index + left_zero].item) "
            "proof ready target-metadata ready helper-drop-bindings ready cfg-slice ready "
            "module-mutation ready production-member-cleanup ready production-gate ready "
            "production-enabled true production ready blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup production-readiness owner right_items "
            "index (right_index + right_zero) element Box moved Inner member-path item "
            "source-line 25 source-text "
            "var right_outer: Outer = Outer(right_items[right_index + right_zero].item) "
            "proof ready target-metadata ready helper-drop-bindings ready cfg-slice ready "
            "module-mutation ready production-member-cleanup ready production-gate ready "
            "production-enabled true production ready blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-production-readiness owner left_items "
            "index (left_index + left_zero) element Box moved Inner member-path item "
            "source-line 18 source-text "
            "var left_outer: Outer = Outer(left_items[left_index + left_zero].item) "
            "promotion ready post-apply-verification ready authorization ready ir-mutation requested "
            "production-gate enabled readiness ready report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-production-readiness owner right_items "
            "index (right_index + right_zero) element Box moved Inner member-path item "
            "source-line 25 source-text "
            "var right_outer: Outer = Outer(right_items[right_index + right_zero].item) "
            "promotion ready post-apply-verification ready authorization ready ir-mutation requested "
            "production-gate enabled readiness ready report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite authorization owner left_items "
            "index (left_index + left_zero) element Box moved Inner member-path item "
            "source-line 18 source-text "
            "var left_outer: Outer = Outer(left_items[left_index + left_zero].item) "
            "verdict ready guarded-rewrite ready authorization ready rewrite-requested true "
            "rewrite-authorized true report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite authorization owner right_items "
            "index (right_index + right_zero) element Box moved Inner member-path item "
            "source-line 25 source-text "
            "var right_outer: Outer = Outer(right_items[right_index + right_zero].item) "
            "verdict ready guarded-rewrite ready authorization ready rewrite-requested true "
            "rewrite-authorized true report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite execution-plan owner left_items "
            "index (left_index + left_zero) element Box moved Inner member-path item "
            "source-line 18 source-text "
            "var left_outer: Outer = Outer(left_items[left_index + left_zero].item) "
            "authorization ready rewrite-authorized true execution-plan ready execution-requested true "
            "execution enabled report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite execution-plan owner right_items "
            "index (right_index + right_zero) element Box moved Inner member-path item "
            "source-line 25 source-text "
            "var right_outer: Outer = Outer(right_items[right_index + right_zero].item) "
            "authorization ready rewrite-authorized true execution-plan ready execution-requested true "
            "execution enabled report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite execution verdict owner left_items "
            "index (left_index + left_zero) element Box moved Inner member-path item "
            "source-line 18 source-text "
            "var left_outer: Outer = Outer(left_items[left_index + left_zero].item) "
            "execution-plan ready execution enabled blockers 0 diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite execution verdict owner right_items "
            "index (right_index + right_zero) element Box moved Inner member-path item "
            "source-line 25 source-text "
            "var right_outer: Outer = Outer(right_items[right_index + right_zero].item) "
            "execution-plan ready execution enabled blockers 0 diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite promotion-status owner left_items "
            "index (left_index + left_zero) element Box moved Inner member-path item "
            "source-line 18 source-text "
            "var left_outer: Outer = Outer(left_items[left_index + left_zero].item) "
            "authorization ready execution-plan ready execution-verdict ready promotion ready blockers 0 "
            "diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite promotion-status owner right_items "
            "index (right_index + right_zero) element Box moved Inner member-path item "
            "source-line 25 source-text "
            "var right_outer: Outer = Outer(right_items[right_index + right_zero].item) "
            "authorization ready execution-plan ready execution-verdict ready promotion ready blockers 0 "
            "diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
}

void assert_constructor_move_report_for_branch_computed_cleanup_ready() {
    auto options = pipeline::production_compile_pipeline_options();
    options.collect_runtime_indexed_cleanup_audit = true;
    auto const result = pipeline::CompilePipeline {}.emit_llvm(
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
            "runtime_indexed_dynamic_array_constructor_branch_computed_member_transfer.or",
        options
    );
    auto const report = driver::runtime_indexed_constructor_move_production_readiness_report(result);

    assert(
        report.find(
            "runtime-index cleanup constructor-move production-readiness "
            "constructor-move enabled partial-ownership accepted cleanup-proof ready cleanup-production enabled "
            "capability-count 1 ordinary-emit accepted member-cleanup-promotion ready "
            "member-production-records 1 member-gate-records 1 member-mutation-records 1 "
            "member-rewrite-records 1 diagnostic none member-module-ir-shape ready"
        ) != std::string::npos
    );
    assert(report.find("runtime-index cleanup constructor-move plan owner items") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move ir-shape owner items") == std::string::npos);
    assert(
        report.find(
            "runtime-index member cleanup helper-drop-bindings owner items index choose_index(true) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(true)].item) "
            "helper __orison_member_cleanup.Box.except.item sibling-bindings 0 "
            "drop-definitions ready nested-path false helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup production-readiness owner items index choose_index(true) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(true)].item) "
            "proof ready target-metadata ready helper-drop-bindings ready cfg-slice ready "
            "module-mutation ready production-member-cleanup ready production-gate ready "
            "production-enabled true production ready blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-production-readiness owner items index choose_index(true) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(true)].item) "
            "promotion ready post-apply-verification ready authorization ready ir-mutation requested "
            "production-gate enabled readiness ready report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite promotion-status owner items index choose_index(true) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(true)].item) "
            "authorization ready execution-plan ready execution-verdict ready promotion ready blockers 0 "
            "diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
}

void assert_constructor_move_report_for_switch_computed_cleanup_ready() {
    auto options = pipeline::production_compile_pipeline_options();
    options.collect_runtime_indexed_cleanup_audit = true;
    auto const result = pipeline::CompilePipeline {}.emit_llvm(
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
            "runtime_indexed_dynamic_array_constructor_switch_computed_member_transfer.or",
        options
    );
    auto const report = driver::runtime_indexed_constructor_move_production_readiness_report(result);

    assert(
        report.find(
            "runtime-index cleanup constructor-move production-readiness "
            "constructor-move enabled partial-ownership accepted cleanup-proof ready cleanup-production enabled "
            "capability-count 1 ordinary-emit accepted member-cleanup-promotion ready "
            "member-production-records 1 member-gate-records 1 member-mutation-records 1 "
            "member-rewrite-records 1 diagnostic none member-module-ir-shape ready"
        ) != std::string::npos
    );
    assert(report.find("runtime-index cleanup constructor-move plan owner items") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move ir-shape owner items") == std::string::npos);
    assert(
        report.find(
            "runtime-index member cleanup helper-drop-bindings owner items index choose_index(1 as UInt32) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(1 as UInt32)].item) "
            "helper __orison_member_cleanup.Box.except.item sibling-bindings 0 "
            "drop-definitions ready nested-path false helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup production-readiness owner items index choose_index(1 as UInt32) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(1 as UInt32)].item) "
            "proof ready target-metadata ready helper-drop-bindings ready cfg-slice ready "
            "module-mutation ready production-member-cleanup ready production-gate ready "
            "production-enabled true production ready blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-production-readiness owner items index choose_index(1 as UInt32) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(1 as UInt32)].item) "
            "promotion ready post-apply-verification ready authorization ready ir-mutation requested "
            "production-gate enabled readiness ready report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite promotion-status owner items index choose_index(1 as UInt32) "
            "element Box moved Inner member-path item source-line 16 source-text "
            "var outer: Outer = Outer(items[choose_index(1 as UInt32)].item) "
            "authorization ready execution-plan ready execution-verdict ready promotion ready blockers 0 "
            "diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
}

void assert_constructor_move_report_for_choice_payload_computed_cleanup_ready() {
    auto options = pipeline::production_compile_pipeline_options();
    options.collect_runtime_indexed_cleanup_audit = true;
    auto const result = pipeline::CompilePipeline {}.emit_llvm(
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
            "runtime_indexed_dynamic_array_choice_payload_computed_member_transfer.or",
        options
    );
    auto const report = driver::runtime_indexed_constructor_move_production_readiness_report(result);

    assert(
        report.find(
            "runtime-index cleanup constructor-move production-readiness "
            "constructor-move enabled partial-ownership accepted cleanup-proof ready cleanup-production enabled "
            "capability-count 1 ordinary-emit accepted member-cleanup-promotion ready "
            "member-production-records 1 member-gate-records 1 member-mutation-records 1 "
            "member-rewrite-records 1 diagnostic none member-module-ir-shape ready"
        ) != std::string::npos
    );
    assert(report.find("runtime-index cleanup constructor-move plan owner items") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move ir-shape owner items") == std::string::npos);
    assert(
        report.find(
            "runtime-index member cleanup helper-drop-bindings owner items index (index + zero) "
            "element Box moved Inner member-path item source-line 26 source-text "
            "var outer: Outer = Outer(items[index + zero].item) "
            "helper __orison_member_cleanup.Box.except.item sibling-bindings 0 "
            "drop-definitions ready nested-path false helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup production-readiness owner items index (index + zero) "
            "element Box moved Inner member-path item source-line 26 source-text "
            "var outer: Outer = Outer(items[index + zero].item) "
            "proof ready target-metadata ready helper-drop-bindings ready cfg-slice ready "
            "module-mutation ready production-member-cleanup ready production-gate ready "
            "production-enabled true production ready blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-production-readiness owner items index (index + zero) "
            "element Box moved Inner member-path item source-line 26 source-text "
            "var outer: Outer = Outer(items[index + zero].item) "
            "promotion ready post-apply-verification ready authorization ready ir-mutation requested "
            "production-gate enabled readiness ready report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite promotion-status owner items index (index + zero) "
            "element Box moved Inner member-path item source-line 26 source-text "
            "var outer: Outer = Outer(items[index + zero].item) "
            "authorization ready execution-plan ready execution-verdict ready promotion ready blockers 0 "
            "diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
}

void assert_constructor_move_report_for_choice_payload_nested_computed_cleanup_ready() {
    auto options = pipeline::production_compile_pipeline_options();
    options.collect_runtime_indexed_cleanup_audit = true;
    auto const result = pipeline::CompilePipeline {}.emit_llvm(
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
            "runtime_indexed_dynamic_array_choice_payload_nested_computed_member_transfer.or",
        options
    );
    auto const report = driver::runtime_indexed_constructor_move_production_readiness_report(result);

    assert(
        report.find(
            "runtime-index cleanup constructor-move production-readiness "
            "constructor-move enabled partial-ownership accepted cleanup-proof ready cleanup-production enabled "
            "capability-count 1 ordinary-emit accepted member-cleanup-promotion ready "
            "member-production-records 1 member-gate-records 1 member-mutation-records 1 "
            "member-rewrite-records 1 diagnostic none member-module-ir-shape ready"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup helper-drop-bindings owner holder.items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 51 source-text "
            "var outer: Outer = Outer(holder.items[index + zero].box.item) "
            "helper __orison_member_cleanup.Wrap.except.box.item sibling-bindings 4 "
            "drop-definitions ready nested-path true helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-production-readiness owner holder.items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 51 source-text "
            "var outer: Outer = Outer(holder.items[index + zero].box.item) "
            "promotion ready post-apply-verification ready authorization ready ir-mutation requested "
            "production-gate enabled readiness ready report-only false production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite promotion-status owner holder.items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 51 source-text "
            "var outer: Outer = Outer(holder.items[index + zero].box.item) "
            "authorization ready execution-plan ready execution-verdict ready promotion ready blockers 0 "
            "diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
}

void assert_constructor_move_report_for_nested_member_sibling_cleanup_ready() {
    auto options = pipeline::production_compile_pipeline_options();
    options.collect_runtime_indexed_cleanup_audit = true;
    auto const result = pipeline::CompilePipeline {}.emit_llvm(
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
            "runtime_indexed_dynamic_array_constructor_computed_expression_nested_member_sibling_transfer.or",
        options
    );
    auto const report = driver::runtime_indexed_constructor_move_production_readiness_report(result);

    assert(
        report.find(
            "runtime-index cleanup constructor-move production-readiness "
            "constructor-move enabled partial-ownership accepted cleanup-proof ready cleanup-production enabled "
            "capability-count 1 ordinary-emit accepted member-cleanup-promotion ready "
            "member-production-records 1 member-gate-records 1 member-mutation-records 1 "
            "member-rewrite-records 1 diagnostic none member-module-ir-shape ready"
        ) != std::string::npos
    );
    assert(report.find("diagnostic none member-module-ir-shape ready") != std::string::npos);
    assert(report.find("member-module-ir-shape-detail") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move plan owner items") == std::string::npos);
    assert(report.find("runtime-index cleanup constructor-move ir-shape owner items") == std::string::npos);
    assert(
        report.find(
            "runtime-index member cleanup helper-drop-bindings owner items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 37 source-text "
            "var outer: Outer = Outer(items[index + zero].box.item) "
            "helper __orison_member_cleanup.Wrap.except.box.item "
            "sibling-bindings 4 drop-definitions ready nested-path true helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup helper-body owner items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 37 source-text "
            "var outer: Outer = Outer(items[index + zero].box.item) "
            "helper __orison_member_cleanup.Wrap.except.box.item operations 4 address-projections 4 "
            "cleanup-calls 4 zero-stores 4 nested-path true helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup production-readiness owner items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 37 source-text "
            "var outer: Outer = Outer(items[index + zero].box.item) proof ready target-metadata ready "
            "helper-drop-bindings ready cfg-slice ready module-mutation ready production-member-cleanup ready "
            "production-gate ready production-enabled true production ready blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-operation-validation owner items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 37 source-text "
            "var outer: Outer = Outer(items[index + zero].box.item) seam selected count valid order valid "
            "branch-replacement-fields valid cfg-append-fields valid phi-retarget-fields valid operations-ready ready "
            "no-operations-applied true validation ready report-only true production disabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation-production-readiness owner items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 37 source-text "
            "var outer: Outer = Outer(items[index + zero].box.item) promotion ready post-apply-verification ready "
            "authorization ready ir-mutation requested production-gate enabled readiness ready report-only false "
            "production enabled blockers 0"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup mutation rewrite promotion-status owner items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 37 source-text "
            "var outer: Outer = Outer(items[index + zero].box.item) authorization ready execution-plan ready "
            "execution-verdict ready promotion ready blockers 0 diagnostics 0 report-only false production enabled"
        ) != std::string::npos
    );
    assert(
        report.find(
            "runtime-index member cleanup execution-summary owner items index (index + zero) "
            "element Wrap moved Inner member-path box.item source-line 37 source-text "
            "var outer: Outer = Outer(items[index + zero].box.item) typed-gate ready apply authorized "
            "rewrite-authorization authorized rewrite-execution enabled rewrite-verdict enabled "
            "rewrite-promotion ready helper-bindings 1 helper-target "
            "__orison_member_cleanup.Wrap.except.box.item helper-sibling-bindings 4 "
            "helper-definition ready production enabled"
        ) != std::string::npos
    );
    assert(report.find("blocker member-cleanup-module-mutation") == std::string::npos);
    assert(report.find("blocker production-member-cleanup") == std::string::npos);
    assert(report.find("blocker member-cleanup-ir-mutation ") == std::string::npos);
    assert(report.find("blocker production-member-cleanup-ir-mutation ") == std::string::npos);
    assert(report.find("blocker member-cleanup-mutation-rewrite-not-authorized") == std::string::npos);
}

}  // namespace

auto main() -> int {
    assert_mutation_report_with_composition_detail();
    assert_mutation_report_without_composition_detail();
    assert_mutation_report_for_scalar_same_function_success();
    assert_constructor_move_report_for_two_member_cleanup_ready();
    assert_constructor_move_report_for_branch_computed_cleanup_ready();
    assert_constructor_move_report_for_switch_computed_cleanup_ready();
    assert_constructor_move_report_for_choice_payload_computed_cleanup_ready();
    assert_constructor_move_report_for_choice_payload_nested_computed_cleanup_ready();
    assert_constructor_move_report_for_nested_member_sibling_cleanup_ready();
    return 0;
}
