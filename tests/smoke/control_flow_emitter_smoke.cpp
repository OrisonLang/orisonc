#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
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
    std::filesystem::remove(path);
    return 0;
}
