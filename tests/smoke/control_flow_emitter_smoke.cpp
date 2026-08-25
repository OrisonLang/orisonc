#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/lowering_failure_rendering.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_control_flow_emitter_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto path = std::filesystem::temp_directory_path() / "orison_control_flow_emitter_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.control_flow_emitter\n"
                  "\n"
                  "function choose(flag: Bool, left: UInt32, right: UInt32) -> UInt32\n"
                  "    if flag\n"
                  "        let value = left + 1 as UInt32\n"
                  "        value\n"
                  "    else\n"
                  "        let value = right + 2 as UInt32\n"
                  "        value\n";
    }

    auto source = orison::source::SourceFile::read(path);
    assert(source.has_value());
    auto parser = orison::syntax::ModuleParser {};
    auto parse_result = parser.parse(*source);
    assert(!parse_result.diagnostics.has_errors());

    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    auto lowering = orison::lowering::build_lowering_context(parse_result.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto strings = orison::lowering::collect_string_constants(parse_result.module);
    auto context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = strings,
        .options = {},
    };
    auto state = orison::lowering::FunctionLoweringState {};
    auto failures = orison::lowering::LoweringFailures {};
    auto session = orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };
    state.immutable_bindings.emplace("flag", orison::lowering::LoweredExpression {
        .type = "i1",
        .value = "%flag",
    });
    state.immutable_bindings.emplace("left", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%left",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    state.immutable_bindings.emplace("right", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%right",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });

    auto output = std::ostringstream {};
    auto lowered = orison::lowering::lower_final_control_flow_statement(
        parse_result.module.functions.front().body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        diagnostics,
        output
    );
    assert(!diagnostics.has_errors());
    assert(lowered.has_value());
    assert(lowered->value == "%tmp2");
    assert(state.current_block == "if.merge.0");
    assert(state.immutable_bindings.size() == 3);
    assert(!state.immutable_bindings.contains("value"));
    assert(
        output.str() ==
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  %tmp0 = add i32 %left, 1\n"
        "  %value = add i32 0, %tmp0\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  %tmp1 = add i32 %right, 2\n"
        "  %value.1 = add i32 0, %tmp1\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp2 = phi i32 [%value, %if.then.0], [%value.1, %if.else.0]\n"
    );

    auto& malformed_if = parse_result.module.functions.front().body_statements.front();
    malformed_if.alternate_statements.clear();
    auto malformed_state = orison::lowering::FunctionLoweringState {};
    auto malformed_failures = orison::lowering::LoweringFailures {};
    auto malformed_session = orison::lowering::FunctionLoweringSession {
        .state = malformed_state,
        .failures = malformed_failures,
    };
    auto malformed_output = std::ostringstream {};
    auto malformed = orison::lowering::lower_final_control_flow_statement(
        malformed_if,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        malformed_session,
        diagnostics,
        malformed_output
    );
    assert(!malformed.has_value());
    assert(
        malformed_failures.control_flow.reason ==
        orison::lowering::ControlFlowLoweringFailureReason::invalid_if_shape
    );
    assert(
        orison::lowering::render_control_flow_lowering_failure(malformed_failures.control_flow) ==
        "invalid final if shape: a final if requires non-empty then and else arms"
    );

    auto aggregate_mismatch_path =
        std::filesystem::temp_directory_path() / "orison_control_flow_aggregate_mismatch_smoke.or";
    {
        auto output = std::ofstream(aggregate_mismatch_path);
        output << "package demo.control_flow_emitter\n"
                  "\n"
                  "record Payload\n"
                  "    value: UInt32\n"
                  "record Box\n"
                  "    payload: Payload\n"
                  "record Nested\n"
                  "    box: Box\n"
                  "function consume_payload(payload: Payload) -> UInt32\n"
                  "    payload.value\n"
                  "function choose_if(flag: Bool, box: Box) -> UInt32\n"
                  "    if flag\n"
                  "        consume_payload(box.payload)\n"
                  "    else\n"
                  "        0 as UInt32\n"
                  "function choose_switch(flag: Bool, box: Box) -> UInt32\n"
                  "    switch flag\n"
                  "        true => consume_payload(box.payload)\n"
                  "        false => 0 as UInt32\n"
                  "function choose_if_balanced(flag: Bool, box: Box) -> UInt32\n"
                  "    if flag\n"
                  "        consume_payload(box.payload)\n"
                  "    else\n"
                  "        consume_payload(box.payload)\n"
                  "function choose_switch_balanced(flag: Bool, box: Box) -> UInt32\n"
                  "    switch flag\n"
                  "        true => consume_payload(box.payload)\n"
                  "        false => consume_payload(box.payload)\n"
                  "function choose_nested_if(flag: Bool, nested: Nested) -> UInt32\n"
                  "    if flag\n"
                  "        consume_payload(nested.box.payload)\n"
                  "    else\n"
                  "        0 as UInt32\n"
                  "function choose_nested_switch(flag: Bool, nested: Nested) -> UInt32\n"
                  "    switch flag\n"
                  "        true => consume_payload(nested.box.payload)\n"
                  "        false => 0 as UInt32\n"
                  "function choose_nested_if_balanced(flag: Bool, nested: Nested) -> UInt32\n"
                  "    if flag\n"
                  "        consume_payload(nested.box.payload)\n"
                  "    else\n"
                  "        consume_payload(nested.box.payload)\n"
                  "function choose_nested_switch_balanced(flag: Bool, nested: Nested) -> UInt32\n"
                  "    switch flag\n"
                  "        true => consume_payload(nested.box.payload)\n"
                  "        false => consume_payload(nested.box.payload)\n";
    }

    auto aggregate_source = orison::source::SourceFile::read(aggregate_mismatch_path);
    assert(aggregate_source.has_value());
    auto aggregate_parse_result = parser.parse(*aggregate_source);
    assert(!aggregate_parse_result.diagnostics.has_errors());

    auto aggregate_diagnostics = orison::diagnostics::DiagnosticBag {};
    auto aggregate_lowering =
        orison::lowering::build_lowering_context(aggregate_parse_result.module, aggregate_diagnostics);
    assert(!aggregate_diagnostics.has_errors());
    auto aggregate_context = orison::lowering::LoweringEmissionContext {
        .lowering = aggregate_lowering,
        .string_constants = orison::lowering::collect_string_constants(aggregate_parse_result.module),
        .options = {},
    };
    auto seed_aggregate_state = [](
        std::string_view owner_name,
        std::string_view source_type_name,
        std::string_view llvm_type,
        std::string_view storage_name
    ) {
        auto aggregate_state = orison::lowering::FunctionLoweringState {};
        aggregate_state.immutable_bindings.emplace("flag", orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "%flag",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        });
        aggregate_state.addressable_bindings.emplace(std::string {owner_name}, orison::lowering::AddressableBinding {
            .type = orison::lowering::LoweredType {
                .type = std::string {llvm_type},
                .signedness = orison::lowering::IntegerSignedness::not_integer,
            },
            .storage = std::string {storage_name},
        });
        aggregate_state.source_type_names.emplace(std::string {owner_name}, std::string {source_type_name});
        return aggregate_state;
    };
    auto seed_box_state = [&seed_aggregate_state] {
        return seed_aggregate_state("box", "Box", "%record.Box", "%box.addr");
    };
    auto seed_nested_state = [&seed_aggregate_state] {
        return seed_aggregate_state("nested", "Nested", "%record.Nested", "%nested.addr");
    };
    auto assert_post_merge_reuse_failure = [&aggregate_context](
        orison::lowering::FunctionLoweringSession& session,
        orison::lowering::LoweringFailures const& failures,
        std::string_view owner_name,
        std::string_view expected_llvm_type,
        std::string_view expected_message
    ) {
        auto post_merge_output = std::ostringstream {};
        auto post_merge_value = orison::lowering::lower_expression(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = std::string {owner_name},
            },
            expected_llvm_type,
            orison::lowering::IntegerSignedness::not_integer,
            aggregate_context,
            session,
            post_merge_output
        );
        assert(!post_merge_value.has_value());
        assert(post_merge_output.str().empty());
        assert(orison::lowering::render_expression_lowering_failure(failures.expression) == expected_message);
    };

    auto aggregate_if_state = seed_box_state();
    auto aggregate_if_failures = orison::lowering::LoweringFailures {};
    auto aggregate_if_session = orison::lowering::FunctionLoweringSession {
        .state = aggregate_if_state,
        .failures = aggregate_if_failures,
    };
    auto aggregate_if_output = std::ostringstream {};
    auto aggregate_if_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[1].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        aggregate_if_session,
        aggregate_diagnostics,
        aggregate_if_output
    );
    assert(!aggregate_if_lowered.has_value());
    auto aggregate_if_failure =
        orison::lowering::render_control_flow_lowering_failure(aggregate_if_failures.control_flow);
    assert(aggregate_if_failure.find(
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    ) != std::string::npos);
    assert(aggregate_if_failure.find("branch-local cleanup plan: owner box.payload") != std::string::npos);

    aggregate_diagnostics = {};
    auto aggregate_switch_state = seed_box_state();
    auto aggregate_switch_failures = orison::lowering::LoweringFailures {};
    auto aggregate_switch_session = orison::lowering::FunctionLoweringSession {
        .state = aggregate_switch_state,
        .failures = aggregate_switch_failures,
    };
    auto aggregate_switch_output = std::ostringstream {};
    auto aggregate_switch_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[2].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        aggregate_switch_session,
        aggregate_diagnostics,
        aggregate_switch_output
    );
    assert(!aggregate_switch_lowered.has_value());
    auto aggregate_switch_failure =
        orison::lowering::render_control_flow_lowering_failure(aggregate_switch_failures.control_flow);
    assert(aggregate_switch_failure.find(
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    ) != std::string::npos);
    assert(aggregate_switch_failure.find("branch-local cleanup plan: owner box.payload") != std::string::npos);

    aggregate_diagnostics = {};
    auto aggregate_if_balanced_state = seed_box_state();
    auto aggregate_if_balanced_failures = orison::lowering::LoweringFailures {};
    auto aggregate_if_balanced_session = orison::lowering::FunctionLoweringSession {
        .state = aggregate_if_balanced_state,
        .failures = aggregate_if_balanced_failures,
    };
    auto aggregate_if_balanced_output = std::ostringstream {};
    auto aggregate_if_balanced_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[3].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        aggregate_if_balanced_session,
        aggregate_diagnostics,
        aggregate_if_balanced_output
    );
    assert(!aggregate_diagnostics.has_errors());
    assert(aggregate_if_balanced_lowered.has_value());
    assert(orison::lowering::is_owned_binding_consumed(
        aggregate_if_balanced_state.ownership_transfers,
        "box.payload"
    ));
    assert_post_merge_reuse_failure(
        aggregate_if_balanced_session,
        aggregate_if_balanced_failures,
        "box",
        "%record.Box",
        "use after move: box.payload"
    );

    aggregate_diagnostics = {};
    auto aggregate_switch_balanced_state = seed_box_state();
    auto aggregate_switch_balanced_failures = orison::lowering::LoweringFailures {};
    auto aggregate_switch_balanced_session = orison::lowering::FunctionLoweringSession {
        .state = aggregate_switch_balanced_state,
        .failures = aggregate_switch_balanced_failures,
    };
    auto aggregate_switch_balanced_output = std::ostringstream {};
    auto aggregate_switch_balanced_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[4].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        aggregate_switch_balanced_session,
        aggregate_diagnostics,
        aggregate_switch_balanced_output
    );
    assert(!aggregate_diagnostics.has_errors());
    assert(aggregate_switch_balanced_lowered.has_value());
    assert(orison::lowering::is_owned_binding_consumed(
        aggregate_switch_balanced_state.ownership_transfers,
        "box.payload"
    ));
    assert_post_merge_reuse_failure(
        aggregate_switch_balanced_session,
        aggregate_switch_balanced_failures,
        "box",
        "%record.Box",
        "use after move: box.payload"
    );

    aggregate_diagnostics = {};
    auto nested_if_state = seed_nested_state();
    auto nested_if_failures = orison::lowering::LoweringFailures {};
    auto nested_if_session = orison::lowering::FunctionLoweringSession {
        .state = nested_if_state,
        .failures = nested_if_failures,
    };
    auto nested_if_output = std::ostringstream {};
    auto nested_if_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[5].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        nested_if_session,
        aggregate_diagnostics,
        nested_if_output
    );
    assert(!nested_if_lowered.has_value());
    auto nested_if_failure =
        orison::lowering::render_control_flow_lowering_failure(nested_if_failures.control_flow);
    assert(nested_if_failure.find(
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    ) != std::string::npos);
    assert(nested_if_failure.find("branch-local cleanup plan: owner nested.box.payload") != std::string::npos);

    aggregate_diagnostics = {};
    auto nested_switch_state = seed_nested_state();
    auto nested_switch_failures = orison::lowering::LoweringFailures {};
    auto nested_switch_session = orison::lowering::FunctionLoweringSession {
        .state = nested_switch_state,
        .failures = nested_switch_failures,
    };
    auto nested_switch_output = std::ostringstream {};
    auto nested_switch_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[6].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        nested_switch_session,
        aggregate_diagnostics,
        nested_switch_output
    );
    assert(!nested_switch_lowered.has_value());
    auto nested_switch_failure =
        orison::lowering::render_control_flow_lowering_failure(nested_switch_failures.control_flow);
    assert(nested_switch_failure.find(
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    ) != std::string::npos);
    assert(nested_switch_failure.find("branch-local cleanup plan: owner nested.box.payload") != std::string::npos);

    aggregate_diagnostics = {};
    auto nested_if_balanced_state = seed_nested_state();
    auto nested_if_balanced_failures = orison::lowering::LoweringFailures {};
    auto nested_if_balanced_session = orison::lowering::FunctionLoweringSession {
        .state = nested_if_balanced_state,
        .failures = nested_if_balanced_failures,
    };
    auto nested_if_balanced_output = std::ostringstream {};
    auto nested_if_balanced_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[7].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        nested_if_balanced_session,
        aggregate_diagnostics,
        nested_if_balanced_output
    );
    assert(!aggregate_diagnostics.has_errors());
    assert(nested_if_balanced_lowered.has_value());
    assert(orison::lowering::is_owned_binding_consumed(
        nested_if_balanced_state.ownership_transfers,
        "nested.box.payload"
    ));
    assert_post_merge_reuse_failure(
        nested_if_balanced_session,
        nested_if_balanced_failures,
        "nested",
        "%record.Nested",
        "use after move: nested.box.payload"
    );

    aggregate_diagnostics = {};
    auto nested_switch_balanced_state = seed_nested_state();
    auto nested_switch_balanced_failures = orison::lowering::LoweringFailures {};
    auto nested_switch_balanced_session = orison::lowering::FunctionLoweringSession {
        .state = nested_switch_balanced_state,
        .failures = nested_switch_balanced_failures,
    };
    auto nested_switch_balanced_output = std::ostringstream {};
    auto nested_switch_balanced_lowered = orison::lowering::lower_final_control_flow_statement(
        aggregate_parse_result.module.functions[8].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        aggregate_context,
        nested_switch_balanced_session,
        aggregate_diagnostics,
        nested_switch_balanced_output
    );
    assert(!aggregate_diagnostics.has_errors());
    assert(nested_switch_balanced_lowered.has_value());
    assert(orison::lowering::is_owned_binding_consumed(
        nested_switch_balanced_state.ownership_transfers,
        "nested.box.payload"
    ));
    assert_post_merge_reuse_failure(
        nested_switch_balanced_session,
        nested_switch_balanced_failures,
        "nested",
        "%record.Nested",
        "use after move: nested.box.payload"
    );

    auto choice_path = std::filesystem::temp_directory_path() / "orison_control_flow_choice_payload_smoke.or";
    {
        auto output = std::ofstream(choice_path);
        output << "package demo.control_flow_emitter\n"
                  "\n"
                  "record Payload\n"
                  "    value: UInt32\n"
                  "choice Holder\n"
                  "    Loaded(payload: Payload)\n"
                  "    Empty\n"
                  "function classify(holder: Holder) -> UInt32\n"
                  "    switch holder\n"
                  "        Loaded(payload) => payload.value\n"
                  "        Empty => 2 as UInt32\n";
    }

    auto choice_source = orison::source::SourceFile::read(choice_path);
    assert(choice_source.has_value());
    auto choice_parse_result = parser.parse(*choice_source);
    assert(!choice_parse_result.diagnostics.has_errors());

    auto choice_diagnostics = orison::diagnostics::DiagnosticBag {};
    auto choice_lowering = orison::lowering::build_lowering_context(choice_parse_result.module, choice_diagnostics);
    assert(!choice_diagnostics.has_errors());
    auto choice_context = orison::lowering::LoweringEmissionContext {
        .lowering = choice_lowering,
        .string_constants = orison::lowering::collect_string_constants(choice_parse_result.module),
        .options = {},
    };
    auto choice_state = orison::lowering::FunctionLoweringState {};
    auto choice_failures = orison::lowering::LoweringFailures {};
    auto choice_session = orison::lowering::FunctionLoweringSession {
        .state = choice_state,
        .failures = choice_failures,
    };
    choice_state.immutable_bindings.emplace("holder", orison::lowering::LoweredExpression {
        .type = "{ i32, %record.Payload }",
        .value = "%holder",
        .signedness = orison::lowering::IntegerSignedness::not_integer,
    });
    choice_state.source_type_names.emplace("holder", "Holder");

    auto choice_output = std::ostringstream {};
    auto choice_lowered = orison::lowering::lower_final_control_flow_statement(
        choice_parse_result.module.functions.front().body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        choice_context,
        choice_session,
        choice_diagnostics,
        choice_output
    );
    assert(!choice_diagnostics.has_errors());
    assert(choice_lowered.has_value());
    assert(orison::lowering::is_owned_binding_consumed(
        choice_state.ownership_transfers,
        "holder.Loaded.payload"
    ));

    auto post_switch_output = std::ostringstream {};
    auto post_switch_holder = orison::lowering::lower_expression(
        orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::name,
            .text = "holder",
        },
        "{ i32, %record.Payload }",
        orison::lowering::IntegerSignedness::not_integer,
        choice_context,
        choice_session,
        post_switch_output
    );
    assert(!post_switch_holder.has_value());
    assert(post_switch_output.str().empty());
    assert(
        orison::lowering::render_expression_lowering_failure(choice_failures.expression) ==
        "use after move: holder.Loaded.payload"
    );

    auto local_dynamic_array_path =
        std::filesystem::temp_directory_path() / "orison_control_flow_local_dynamic_array_cleanup_smoke.or";
    {
        auto output = std::ofstream(local_dynamic_array_path);
        output << "package demo.control_flow_local_dynamic_array_cleanup\n"
                  "\n"
                  "record Payload\n"
                  "    public value: Int64\n"
                  "\n"
                  "interface Drop\n"
                  "    function drop(this: exclusive This) -> Unit\n"
                  "\n"
                  "implements Drop for Payload\n"
                  "    function drop(this: exclusive This) -> Unit\n"
                  "        return\n"
                  "\n"
                  "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
                  "    0 as UInt32\n"
                  "\n"
                  "function choose_if(flag: Bool) -> UInt32\n"
                  "    if flag\n"
                  "        use_items(items)\n"
                  "    else\n"
                  "        0 as UInt32\n"
                  "\n"
                  "function choose_switch(flag: Bool) -> UInt32\n"
                  "    switch flag\n"
                  "        true => use_items(items)\n"
                  "        false => 0 as UInt32\n";
    }

    auto local_dynamic_array_source = orison::source::SourceFile::read(local_dynamic_array_path);
    assert(local_dynamic_array_source.has_value());
    auto local_dynamic_array_parse_result = parser.parse(*local_dynamic_array_source);
    assert(!local_dynamic_array_parse_result.diagnostics.has_errors());

    auto local_dynamic_array_diagnostics = orison::diagnostics::DiagnosticBag {};
    auto local_dynamic_array_lowering = orison::lowering::build_lowering_context(
        local_dynamic_array_parse_result.module,
        local_dynamic_array_diagnostics
    );
    assert(!local_dynamic_array_diagnostics.has_errors());
    local_dynamic_array_lowering.functions.at("use_items").parameter_types.front() = "{ ptr, i64, i64 }";
    auto local_dynamic_array_options = orison::lowering::LlvmIrEmissionOptions {
        .enable_dynamic_array_parameter_descriptors = true,
        .enable_dynamic_array_construction_lowering = true,
        .enable_dynamic_array_cleanup_emission = true,
        .semantic_drop_lowering_authorizations = {
            orison::semantics::DropLoweringAuthorization {
                .site = orison::semantics::PlannedDropSite {
                    .source_type_name = "Payload",
                    .abi_symbol_name = "__orison_drop.Payload",
                    .owner_name = "items.element",
                },
                .semantic_resolved = true,
                .source_drop_lowering_enabled = true,
                .authorized = true,
            },
        },
    };
    auto local_dynamic_array_context = orison::lowering::LoweringEmissionContext {
        .lowering = local_dynamic_array_lowering,
        .string_constants = orison::lowering::collect_string_constants(local_dynamic_array_parse_result.module),
        .options = local_dynamic_array_options,
    };
    auto seed_local_dynamic_array_state = [] {
        auto state = orison::lowering::FunctionLoweringState {};
        state.immutable_bindings.emplace("flag", orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "%flag",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        });
        state.immutable_bindings.emplace("items", orison::lowering::LoweredExpression {
            .type = "{ ptr, i64, i64 }",
            .value = "%items",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        });
        state.addressable_bindings.emplace("items", orison::lowering::AddressableBinding {
            .type = orison::lowering::LoweredType {
                .type = "{ ptr, i64, i64 }",
                .signedness = orison::lowering::IntegerSignedness::not_integer,
            },
            .storage = "%items.addr",
        });
        state.source_type_names.emplace("items", "DynamicArray<Payload>");
        state.dynamic_array_local_cleanup_plans.push_back(orison::lowering::DynamicArrayDescriptorCleanupPlan {
            .owner_name = "items",
            .source_type_name = "DynamicArray<Payload>",
            .element_source_type_name = "Payload",
            .element_llvm_type = "%record.Payload",
            .descriptor_storage_name = "%items.addr",
            .descriptor_storage_status = orison::lowering::DynamicArrayDescriptorStorageStatus::lowered_local_descriptor,
            .element_size_bytes = 8,
        });
        return state;
    };
    auto assert_local_dynamic_array_post_merge_reuse_failure = [&local_dynamic_array_context](
        orison::lowering::FunctionLoweringSession& session,
        orison::lowering::LoweringFailures const& failures
    ) {
        auto post_merge_output = std::ostringstream {};
        auto post_merge_value = orison::lowering::lower_expression(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "items",
            },
            "{ ptr, i64, i64 }",
            orison::lowering::IntegerSignedness::not_integer,
            local_dynamic_array_context,
            session,
            post_merge_output
        );
        assert(!post_merge_value.has_value());
        assert(post_merge_output.str().empty());
        assert(orison::lowering::render_expression_lowering_failure(failures.expression) == "use after move: items");
    };

    local_dynamic_array_diagnostics = {};
    auto local_dynamic_array_if_state = seed_local_dynamic_array_state();
    auto local_dynamic_array_if_failures = orison::lowering::LoweringFailures {};
    auto local_dynamic_array_if_session = orison::lowering::FunctionLoweringSession {
        .state = local_dynamic_array_if_state,
        .failures = local_dynamic_array_if_failures,
        .enclosing_symbol_name = "choose_if",
    };
    auto local_dynamic_array_if_output = std::ostringstream {};
    auto local_dynamic_array_if_lowered = orison::lowering::lower_final_control_flow_statement(
        local_dynamic_array_parse_result.module.functions[1].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        local_dynamic_array_context,
        local_dynamic_array_if_session,
        local_dynamic_array_diagnostics,
        local_dynamic_array_if_output,
        std::string_view {"UInt32"}
    );
    assert(!local_dynamic_array_diagnostics.has_errors());
    assert(local_dynamic_array_if_lowered.has_value());
    assert(local_dynamic_array_if_output.str().find("items.dynamic_array_cleanup") != std::string::npos);
    assert(orison::lowering::is_owned_binding_consumed(
        local_dynamic_array_if_state.ownership_transfers,
        "items"
    ));
    assert_local_dynamic_array_post_merge_reuse_failure(
        local_dynamic_array_if_session,
        local_dynamic_array_if_failures
    );

    local_dynamic_array_diagnostics = {};
    auto local_dynamic_array_switch_state = seed_local_dynamic_array_state();
    auto local_dynamic_array_switch_failures = orison::lowering::LoweringFailures {};
    auto local_dynamic_array_switch_session = orison::lowering::FunctionLoweringSession {
        .state = local_dynamic_array_switch_state,
        .failures = local_dynamic_array_switch_failures,
        .enclosing_symbol_name = "choose_switch",
    };
    auto local_dynamic_array_switch_output = std::ostringstream {};
    auto local_dynamic_array_switch_lowered = orison::lowering::lower_final_control_flow_statement(
        local_dynamic_array_parse_result.module.functions[2].body_statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        local_dynamic_array_context,
        local_dynamic_array_switch_session,
        local_dynamic_array_diagnostics,
        local_dynamic_array_switch_output,
        std::string_view {"UInt32"}
    );
    assert(!local_dynamic_array_diagnostics.has_errors());
    assert(local_dynamic_array_switch_lowered.has_value());
    assert(local_dynamic_array_switch_output.str().find("items.dynamic_array_cleanup") != std::string::npos);
    assert(orison::lowering::is_owned_binding_consumed(
        local_dynamic_array_switch_state.ownership_transfers,
        "items"
    ));
    assert_local_dynamic_array_post_merge_reuse_failure(
        local_dynamic_array_switch_session,
        local_dynamic_array_switch_failures
    );

    std::filesystem::remove(path);
    std::filesystem::remove(aggregate_mismatch_path);
    std::filesystem::remove(choice_path);
    std::filesystem::remove(local_dynamic_array_path);
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
