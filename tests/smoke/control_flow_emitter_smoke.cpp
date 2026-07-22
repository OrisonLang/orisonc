#include "orison/lowering/control_flow_emitter.hpp"
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
                  "        false => consume_payload(box.payload)\n";
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
    auto seed_aggregate_state = [] {
        auto aggregate_state = orison::lowering::FunctionLoweringState {};
        aggregate_state.immutable_bindings.emplace("flag", orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "%flag",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        });
        aggregate_state.addressable_bindings.emplace("box", orison::lowering::AddressableBinding {
            .type = orison::lowering::LoweredType {
                .type = "%record.Box",
                .signedness = orison::lowering::IntegerSignedness::not_integer,
            },
            .storage = "%box.addr",
        });
        aggregate_state.source_type_names.emplace("box", "Box");
        return aggregate_state;
    };

    auto aggregate_if_state = seed_aggregate_state();
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
    assert(
        orison::lowering::render_control_flow_lowering_failure(aggregate_if_failures.control_flow) ==
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );

    aggregate_diagnostics = {};
    auto aggregate_switch_state = seed_aggregate_state();
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
    assert(
        orison::lowering::render_control_flow_lowering_failure(aggregate_switch_failures.control_flow) ==
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );

    aggregate_diagnostics = {};
    auto aggregate_if_balanced_state = seed_aggregate_state();
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

    aggregate_diagnostics = {};
    auto aggregate_switch_balanced_state = seed_aggregate_state();
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
    std::filesystem::remove(path);
    std::filesystem::remove(aggregate_mismatch_path);
    std::filesystem::remove(choice_path);
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
