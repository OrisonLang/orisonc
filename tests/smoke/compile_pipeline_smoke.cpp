#include "orison/pipeline/compile_pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_pipeline_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::pipeline::CompilePipeline pipeline;
    auto source_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "minimal.or";

    auto analysis = pipeline.analyze(source_path);
    assert(!analysis.has_errors());
    assert(analysis.source_file.has_value());
    assert(analysis.parse_result.module.package_name == "demo.minimal");
    assert(analysis.parse_result.module.functions.size() == 1);
    assert(analysis.semantic_planned_drop_report.empty());
    assert(analysis.semantic_drop_implementation_report.empty());
    assert(analysis.semantic_drop_resolution_report.empty());
    assert(analysis.semantic_drop_diagnostic_report.empty());
    assert(analysis.semantic_drop_lowering_authorizations.empty());
    assert(analysis.semantic_drop_lowering_authorization_report.empty());
    assert(analysis.semantic_drop_resolution_summary_report.empty());

    auto ir = pipeline.emit_llvm(source_path);
    assert(!ir.has_errors());
    assert(ir.ir_text.find("define i32 @main()") != std::string::npos);
    assert(ir.ir_text.find("ret i32 0") != std::string::npos);
    assert(ir.semantic_drop_lowering_authorizations.empty());
    assert(ir.planned_drop_report.empty());
    assert(ir.emitted_drop_declaration_report.empty());
    assert(ir.drop_readiness_snapshot.semantic_authorizations.empty());
    assert(ir.drop_readiness_snapshot.emitted_declarations.empty());
    assert(ir.drop_readiness_snapshot.cleanup_authorizations.empty());
    assert(ir.drop_readiness_snapshot_report.size() == 1);
    assert(
        ir.drop_readiness_snapshot_report.front() ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 0 cleanup authorizations 0"
    );
    assert(ir.drop_readiness_summary.semantic_authorized == 0);
    assert(ir.drop_readiness_summary.semantic_blocked == 0);
    assert(ir.drop_readiness_summary.emitted_declarations == 0);
    assert(ir.drop_readiness_summary.cleanup_authorized == 0);
    assert(ir.drop_readiness_summary.cleanup_blocked == 0);
    assert(ir.drop_readiness_summary_report.size() == 1);
    assert(
        ir.drop_readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 0 blocked 0 emitted declarations 0 cleanup authorized 0 blocked 0"
    );
    assert(ir.drop_readiness_relation_report.empty());
    assert(ir.drop_readiness_blocker_summary.blocked_cleanups == 0);
    assert(ir.drop_readiness_blocker_summary.semantic_lowering_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.semantic_unresolved_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.missing_declarations.empty());
    assert(ir.drop_readiness_blocker_report.size() == 1);
    assert(
        ir.drop_readiness_blocker_report.front() ==
        "drop readiness blockers cleanups 0 semantic blockers 0 semantic unresolved 0 "
        "source lowering blocked 0 missing declarations 0"
    );
    assert(ir.drop_readiness_source_correlation_report.size() == 1);
    assert(
        ir.drop_readiness_source_correlation_report.front() ==
        "drop readiness source correlations actions 0 semantic sites 0"
    );

    auto drop_readiness_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness.or";
    auto drop_readiness = pipeline.emit_llvm(drop_readiness_path);
    assert(!drop_readiness.has_errors());
    assert(drop_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 1);
    assert(drop_readiness.drop_readiness_snapshot.emitted_declarations.empty());
    assert(drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(drop_readiness.drop_readiness_snapshot_report.size() == 3);
    assert(
        drop_readiness.drop_readiness_snapshot_report[0] ==
        "drop readiness snapshot semantic authorizations 1 emitted declarations 0 cleanup authorizations 1"
    );
    assert(
        drop_readiness.drop_readiness_snapshot_report[1] ==
        "semantic readiness __orison_drop.Payload for Payload blocked"
    );
    assert(
        drop_readiness.drop_readiness_snapshot_report[2] ==
        "cleanup readiness __orison_thread_cleanup.launch.12.0 blocked "
        "semantic blockers 1 missing declarations 1"
    );
    assert(drop_readiness.drop_readiness_summary.semantic_authorized == 0);
    assert(drop_readiness.drop_readiness_summary.semantic_blocked == 1);
    assert(drop_readiness.drop_readiness_summary.emitted_declarations == 0);
    assert(drop_readiness.drop_readiness_summary.cleanup_authorized == 0);
    assert(drop_readiness.drop_readiness_summary.cleanup_blocked == 1);
    assert(drop_readiness.drop_readiness_summary_report.size() == 1);
    assert(
        drop_readiness.drop_readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 0 blocked 1 emitted declarations 0 cleanup authorized 0 blocked 1"
    );
    assert(drop_readiness.drop_readiness_relation_report.size() == 3);
    assert(
        drop_readiness.drop_readiness_relation_report[0] ==
        "drop readiness relation __orison_thread_cleanup.launch.12.0 blocked "
        "semantic blockers 1 emitted declarations 0 missing declarations 1"
    );
    assert(
        drop_readiness.drop_readiness_relation_report[1] ==
        "drop readiness relation semantic blocker __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(
        drop_readiness.drop_readiness_relation_report[2] ==
        "drop readiness relation missing declaration __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_report.size() == 4);
    assert(
        drop_readiness.drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 1 "
        "source lowering blocked 0 missing declarations 1"
    );
    assert(
        drop_readiness.drop_readiness_blocker_report[1] ==
        "drop readiness blocker semantic __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(
        drop_readiness.drop_readiness_blocker_report[2] ==
        "drop readiness blocker semantic unresolved __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(
        drop_readiness.drop_readiness_blocker_report[3] ==
        "drop readiness blocker missing declaration __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(drop_readiness.drop_readiness_source_correlation_report.size() == 2);
    assert(
        drop_readiness.drop_readiness_source_correlation_report[0] ==
        "drop readiness source correlations actions 1 semantic sites 1"
    );
    assert(
        drop_readiness.drop_readiness_source_correlation_report[1] ==
        "drop readiness source correlation __orison_thread_cleanup.launch.12.0 __orison_drop.Payload for Payload "
        "capture payload field 0 action line 12 semantic owner payload site line 11 semantic unresolved "
        "source lowering not accepted declaration missing"
    );

    auto multi_drop_readiness_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness_multi.or";
    auto multi_drop_readiness = pipeline.emit_llvm(multi_drop_readiness_path);
    assert(!multi_drop_readiness.has_errors());
    assert(multi_drop_readiness.planned_drop_report.size() == 2);
    assert(
        multi_drop_readiness.planned_drop_report[0] ==
        "planned drop __orison_drop.Payload for Payload discovered at line 20 (metadata only)"
    );
    assert(
        multi_drop_readiness.planned_drop_report[1] ==
        "planned drop __orison_drop.OtherPayload for OtherPayload discovered at line 20 (metadata only)"
    );
    assert(multi_drop_readiness.planned_drop_action_report.size() == 2);
    assert(
        multi_drop_readiness.planned_drop_action_report[0] ==
        "planned drop action __orison_drop.Payload for capture payload: Payload field 0 discovered at line 20 "
        "(metadata only)"
    );
    assert(
        multi_drop_readiness.planned_drop_action_report[1] ==
        "planned drop action __orison_drop.OtherPayload for capture other: OtherPayload field 1 discovered at line 20 "
        "(metadata only)"
    );
    assert(multi_drop_readiness.drop_cleanup_authorization_report.size() == 7);
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[0] ==
        "drop cleanup authorization __orison_thread_cleanup.launch.20.0 blocked semantic blockers 2 "
        "missing declarations 2"
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[1] ==
        "semantic drop lowering blocked __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[2] ==
        "semantic drop lowering blocked __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[3] ==
        "semantic drop unresolved __orison_drop.Payload for Payload capture payload field 0 discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[4] ==
        "semantic drop unresolved __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[5] ==
        "missing drop declaration __orison_drop.Payload for Payload capture payload field 0 discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_cleanup_authorization_report[6] ==
        "missing drop declaration __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(multi_drop_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 2);
    assert(multi_drop_readiness.drop_readiness_snapshot.emitted_declarations.empty());
    assert(multi_drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(multi_drop_readiness.drop_readiness_snapshot_report.size() == 4);
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[0] ==
        "drop readiness snapshot semantic authorizations 2 emitted declarations 0 cleanup authorizations 1"
    );
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[1] ==
        "semantic readiness __orison_drop.Payload for Payload blocked"
    );
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[2] ==
        "semantic readiness __orison_drop.OtherPayload for OtherPayload blocked"
    );
    assert(
        multi_drop_readiness.drop_readiness_snapshot_report[3] ==
        "cleanup readiness __orison_thread_cleanup.launch.20.0 blocked "
        "semantic blockers 2 missing declarations 2"
    );
    assert(multi_drop_readiness.drop_readiness_summary.semantic_authorized == 0);
    assert(multi_drop_readiness.drop_readiness_summary.semantic_blocked == 2);
    assert(multi_drop_readiness.drop_readiness_summary.emitted_declarations == 0);
    assert(multi_drop_readiness.drop_readiness_summary.cleanup_authorized == 0);
    assert(multi_drop_readiness.drop_readiness_summary.cleanup_blocked == 1);
    assert(multi_drop_readiness.drop_readiness_summary_report.size() == 1);
    assert(
        multi_drop_readiness.drop_readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 0 blocked 2 emitted declarations 0 cleanup authorized 0 blocked 1"
    );
    assert(multi_drop_readiness.drop_readiness_relation_report.size() == 5);
    assert(
        multi_drop_readiness.drop_readiness_relation_report[0] ==
        "drop readiness relation __orison_thread_cleanup.launch.20.0 blocked "
        "semantic blockers 2 emitted declarations 0 missing declarations 2"
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[1] ==
        "drop readiness relation semantic blocker __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[2] ==
        "drop readiness relation semantic blocker __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[3] ==
        "drop readiness relation missing declaration __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_relation_report[4] ==
        "drop readiness relation missing declaration __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(multi_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(multi_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_report.size() == 7);
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 2 semantic unresolved 2 "
        "source lowering blocked 0 missing declarations 2"
    );
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[1] ==
        "drop readiness blocker semantic __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[2] ==
        "drop readiness blocker semantic __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[3] ==
        "drop readiness blocker semantic unresolved __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[4] ==
        "drop readiness blocker semantic unresolved __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[5] ==
        "drop readiness blocker missing declaration __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 20"
    );
    assert(
        multi_drop_readiness.drop_readiness_blocker_report[6] ==
        "drop readiness blocker missing declaration __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );

    auto failed_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_summary_failure.or";
    {
        std::ofstream source(failed_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function same(left: Bool, right: Bool) -> Bool\n";
        source << "    left == right\n";
    }
    auto failed_lowering = pipeline.emit_llvm(failed_lowering_path);
    assert(failed_lowering.has_errors());
    assert(
        failed_lowering.error_text.find("lowering does not yet support this return expression") != std::string::npos
    );
    assert(failed_lowering.drop_readiness_snapshot_report.empty());
    assert(failed_lowering.drop_readiness_summary_report.empty());
    assert(failed_lowering.drop_readiness_relation_report.empty());
    assert(failed_lowering.drop_readiness_blocker_report.empty());

    auto object = pipeline.emit_object(source_path);
    assert(!object.has_errors());
    assert(!object.object_bytes.empty());

    auto missing = pipeline.analyze(source_path.parent_path() / "missing.or");
    assert(missing.has_errors());
    assert(missing.error_text == "error: unable to read source file\n");

    auto library_path = std::filesystem::temp_directory_path() / "orison_pipeline_libraries.or";
    {
        std::ofstream source(library_path);
        source << "package demo.libraries\n";
        source << "package foreign \"c\" library \"m\"\n";
        source << "    function first(value: Int32) -> Int32\n";
        source << "package foreign \"c\" library \"m\"\n";
        source << "    function second(value: Int32) -> Int32\n";
        source << "function main() -> Int32\n";
        source << "    0 as Int32\n";
    }
    auto libraries = pipeline.analyze(library_path);
    assert(!libraries.has_errors());
    assert(libraries.link_libraries.size() == 1);
    assert(libraries.link_libraries.front() == "m");

    auto semantic_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "semantic_planned_drop.or";
    auto semantic_drops = pipeline.analyze(semantic_drop_path);
    assert(!semantic_drops.has_errors());
    assert(semantic_drops.semantic_planned_drop_report.size() == 2);
    assert(
        semantic_drops.semantic_planned_drop_report[0] ==
        "drop site __orison_drop.Payload for Payload owner input at line 6"
    );
    assert(
        semantic_drops.semantic_planned_drop_report[1] ==
        "drop site __orison_drop.Payload for Payload owner local at line 7"
    );
    assert(semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert(
        semantic_drops.semantic_drop_resolution_report[0] ==
        "missing drop site __orison_drop.Payload for Payload owner input at line 6"
    );
    assert(
        semantic_drops.semantic_drop_resolution_report[1] ==
        "missing drop site __orison_drop.Payload for Payload owner local at line 7"
    );
    assert(semantic_drops.semantic_drop_diagnostic_report.size() == 2);
    assert(
        semantic_drops.semantic_drop_diagnostic_report[0] ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner input at line 6 blocked "
        "no implementation discovered"
    );
    assert(
        semantic_drops.semantic_drop_diagnostic_report[1] ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner local at line 7 blocked "
        "no implementation discovered"
    );
    assert(semantic_drops.semantic_drop_lowering_authorization_report.size() == 2);
    assert(semantic_drops.semantic_drop_lowering_authorizations.size() == 2);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].semantic_resolved);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].source_drop_lowering_enabled);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].authorized);
    assert(
        semantic_drops.semantic_drop_lowering_authorization_report[0] ==
        "drop lowering authorization drop site __orison_drop.Payload for Payload owner input at line 6 "
        "semantic-unresolved lowering-blocked semantic drop unresolved"
    );
    assert(semantic_drops.semantic_drop_resolution_summary_report.size() == 1);
    assert(
        semantic_drops.semantic_drop_resolution_summary_report.front() ==
        "drop resolution summary __orison_drop.Payload for Payload resolved 0 missing 2"
    );

    auto parsed_drop_path = std::filesystem::temp_directory_path() / "orison_pipeline_parsed_drop_candidate.or";
    {
        std::ofstream source(parsed_drop_path);
        source << "package demo.parseddrop\n";
        source << "record Payload\n";
        source << "    public value: Int64\n";
        source << "interface Drop\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "implements Drop for Payload\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "        return\n";
        source << "function read(input: Payload) -> Int64\n";
        source << "    input.value\n";
    }
    auto parsed_drop = pipeline.analyze(parsed_drop_path);
    assert(!parsed_drop.has_errors());
    assert(parsed_drop.semantic_planned_drop_report.size() == 1);
    assert(
        parsed_drop.semantic_planned_drop_report.front() ==
        "drop site __orison_drop.Payload for Payload owner input at line 9"
    );
    assert(parsed_drop.semantic_drop_implementation_report.size() == 1);
    assert(
        parsed_drop.semantic_drop_implementation_report.front() ==
        "drop implementation __orison_drop.Payload for Payload declared at line 7 origin source-derived finite "
        "safe-boundary (proven) discovery parsed-candidate-collection"
    );
    assert(parsed_drop.semantic_drop_resolution_report.size() == 1);
    assert(
        parsed_drop.semantic_drop_resolution_report.front() ==
        "resolved drop site __orison_drop.Payload for Payload owner input at line 9"
    );
    assert(parsed_drop.semantic_drop_diagnostic_report.size() == 1);
    assert(
        parsed_drop.semantic_drop_diagnostic_report.front() ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner input at line 9 resolved"
    );
    assert(parsed_drop.semantic_drop_lowering_authorization_report.size() == 1);
    assert(parsed_drop.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop.semantic_drop_lowering_authorizations.front().authorized);
    assert(
        parsed_drop.semantic_drop_lowering_authorization_report.front() ==
        "drop lowering authorization drop site __orison_drop.Payload for Payload owner input at line 9 "
        "semantic-resolved lowering-blocked source drop lowering not accepted"
    );
    auto parsed_drop_ir = pipeline.emit_llvm(parsed_drop_path);
    assert(!parsed_drop_ir.has_errors());
    assert(parsed_drop_ir.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_ir.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop_ir.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop_ir.semantic_drop_lowering_authorizations.front().authorized);
    assert(parsed_drop_ir.ir_text.find("__orison_drop.Payload") == std::string::npos);

    auto parsed_drop_readiness_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_parsed_drop_readiness.or";
    {
        std::ofstream source(parsed_drop_readiness_path);
        source << "package demo.parseddropreadiness\n";
        source << "record Payload\n";
        source << "    public value: Int64\n";
        source << "interface Drop\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "implements Transferable for Payload\n";
        source << "    function placeholder(this: shared This) -> Unit\n";
        source << "        return\n";
        source << "implements Drop for Payload\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "        return\n";
        source << "function launch(value: Int64) -> Int64\n";
        source << "    let payload: Payload = Payload(value)\n";
        source << "    let worker = thread\n";
        source << "        payload.value\n";
        source << "\n";
        source << "    worker.join()\n";
    }
    auto parsed_drop_readiness = pipeline.emit_llvm(parsed_drop_readiness_path);
    assert(!parsed_drop_readiness.has_errors());
    assert(parsed_drop_readiness.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_readiness.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop_readiness.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop_readiness.semantic_drop_lowering_authorizations.front().authorized);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.empty());
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_report.size() == 4);
    assert(
        parsed_drop_readiness.drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 0 "
        "source lowering blocked 1 missing declarations 1"
    );
    assert(
        parsed_drop_readiness.drop_readiness_blocker_report[1] ==
        "drop readiness blocker semantic __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 14"
    );
    assert(
        parsed_drop_readiness.drop_readiness_blocker_report[2] ==
        "drop readiness blocker source lowering not accepted __orison_drop.Payload for Payload capture payload "
        "field 0 discovered at line 14"
    );
    assert(
        parsed_drop_readiness.drop_readiness_blocker_report[3] ==
        "drop readiness blocker missing declaration __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 14"
    );
    assert(parsed_drop_readiness.drop_readiness_source_correlation_report.size() == 2);
    assert(
        parsed_drop_readiness.drop_readiness_source_correlation_report[0] ==
        "drop readiness source correlations actions 1 semantic sites 1"
    );
    assert(
        parsed_drop_readiness.drop_readiness_source_correlation_report[1] ==
        "drop readiness source correlation __orison_thread_cleanup.launch.14.0 __orison_drop.Payload for Payload "
        "capture payload field 0 action line 14 semantic owner payload site line 13 semantic resolved "
        "source lowering not accepted declaration missing"
    );
    assert(parsed_drop_readiness.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto resolved_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    }
                ),
            },
        }
    );
    assert(!resolved_semantic_drops.has_errors());
    assert(resolved_semantic_drops.semantic_drop_implementation_report.size() == 1);
    assert(
        resolved_semantic_drops.semantic_drop_implementation_report.front() ==
        "drop implementation __orison_drop.Payload for Payload declared at line 3 origin source-derived finite "
        "safe-boundary (proven) discovery test-injection"
    );
    assert(resolved_semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert(
        resolved_semantic_drops.semantic_drop_resolution_report[0] ==
        "resolved drop site __orison_drop.Payload for Payload owner input at line 6"
    );
    assert(
        resolved_semantic_drops.semantic_drop_resolution_report[1] ==
        "resolved drop site __orison_drop.Payload for Payload owner local at line 7"
    );
    assert(resolved_semantic_drops.semantic_drop_diagnostic_report.size() == 2);
    assert(
        resolved_semantic_drops.semantic_drop_diagnostic_report[0] ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner input at line 6 resolved"
    );
    assert(
        resolved_semantic_drops.semantic_drop_diagnostic_report[1] ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner local at line 7 resolved"
    );
    assert(resolved_semantic_drops.semantic_drop_lowering_authorization_report.size() == 2);
    assert(resolved_semantic_drops.semantic_drop_lowering_authorizations.size() == 2);
    assert(resolved_semantic_drops.semantic_drop_lowering_authorizations[0].semantic_resolved);
    assert(!resolved_semantic_drops.semantic_drop_lowering_authorizations[0].source_drop_lowering_enabled);
    assert(!resolved_semantic_drops.semantic_drop_lowering_authorizations[0].authorized);
    assert(
        resolved_semantic_drops.semantic_drop_lowering_authorization_report[0] ==
        "drop lowering authorization drop site __orison_drop.Payload for Payload owner input at line 6 "
        "semantic-resolved lowering-blocked source drop lowering not accepted"
    );
    assert(resolved_semantic_drops.semantic_drop_resolution_summary_report.size() == 1);
    assert(
        resolved_semantic_drops.semantic_drop_resolution_summary_report.front() ==
        "drop resolution summary __orison_drop.Payload for Payload resolved 2 missing 0"
    );

    auto candidate_resolved_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementation_candidates = {
                orison::semantics::DropImplementationCandidate {
                    .source_type_name = "Payload",
                    .declaration_line = 3,
                    .body = orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    },
                },
                orison::semantics::DropImplementationCandidate {
                    .source_type_name = "Payload",
                    .declaration_line = 4,
                    .body = orison::semantics::DropImplementationBodySummary {},
                },
            },
        }
    );
    assert(!candidate_resolved_semantic_drops.has_errors());
    assert(candidate_resolved_semantic_drops.semantic_drop_implementation_report.size() == 1);
    assert(
        candidate_resolved_semantic_drops.semantic_drop_implementation_report.front() ==
        "drop implementation __orison_drop.Payload for Payload declared at line 3 origin source-derived finite "
        "safe-boundary (proven) discovery candidate-collection"
    );
    assert(candidate_resolved_semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert(
        candidate_resolved_semantic_drops.semantic_drop_resolution_report[0] ==
        "resolved drop site __orison_drop.Payload for Payload owner input at line 6"
    );
    assert(
        candidate_resolved_semantic_drops.semantic_drop_resolution_report[1] ==
        "resolved drop site __orison_drop.Payload for Payload owner local at line 7"
    );
    assert(candidate_resolved_semantic_drops.semantic_drop_resolution_summary_report.size() == 1);
    assert(
        candidate_resolved_semantic_drops.semantic_drop_resolution_summary_report.front() ==
        "drop resolution summary __orison_drop.Payload for Payload resolved 2 missing 0"
    );

    auto unproven_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {}
                ),
            },
        }
    );
    assert(!unproven_semantic_drops.has_errors());
    assert(unproven_semantic_drops.semantic_drop_resolution_report.size() == 2);
    assert(
        unproven_semantic_drops.semantic_drop_resolution_report[0] ==
        "missing drop site __orison_drop.Payload for Payload owner input at line 6"
    );
    assert(
        unproven_semantic_drops.semantic_drop_resolution_report[1] ==
        "missing drop site __orison_drop.Payload for Payload owner local at line 7"
    );
    assert(unproven_semantic_drops.semantic_drop_diagnostic_report.size() == 2);
    assert(
        unproven_semantic_drops.semantic_drop_diagnostic_report[0] ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner input at line 6 blocked "
        "implementation discovered but unproven"
    );
    assert(
        unproven_semantic_drops.semantic_drop_diagnostic_report[1] ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner local at line 7 blocked "
        "implementation discovered but unproven"
    );

    auto partial_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "semantic_partial_drop_resolution.or";
    auto partial_semantic_drops = pipeline.analyze(
        partial_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    }
                ),
            },
        }
    );
    assert(!partial_semantic_drops.has_errors());
    assert(partial_semantic_drops.semantic_drop_resolution_report.size() == 4);
    assert(
        partial_semantic_drops.semantic_drop_resolution_report[0] ==
        "resolved drop site __orison_drop.Payload for Payload owner payload at line 9"
    );
    assert(
        partial_semantic_drops.semantic_drop_resolution_report[1] ==
        "missing drop site __orison_drop.Resource for Resource owner resource at line 9"
    );
    assert(
        partial_semantic_drops.semantic_drop_resolution_report[2] ==
        "resolved drop site __orison_drop.Payload for Payload owner local_payload at line 10"
    );
    assert(
        partial_semantic_drops.semantic_drop_resolution_report[3] ==
        "missing drop site __orison_drop.Resource for Resource owner local_resource at line 11"
    );
    assert(partial_semantic_drops.semantic_drop_resolution_summary_report.size() == 2);
    assert(
        partial_semantic_drops.semantic_drop_resolution_summary_report[0] ==
        "drop resolution summary __orison_drop.Payload for Payload resolved 2 missing 0"
    );
    assert(
        partial_semantic_drops.semantic_drop_resolution_summary_report[1] ==
        "drop resolution summary __orison_drop.Resource for Resource resolved 0 missing 2"
    );
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
