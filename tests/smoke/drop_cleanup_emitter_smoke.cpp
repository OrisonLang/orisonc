#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/drop_metadata.hpp"

#include <cassert>
#include <string>

int main() {
    auto plan = orison::lowering::ConcurrencyExpressionPlan {
        .cleanup_symbol_name = "__orison_thread_cleanup.manual.1.0",
        .environment_layout = orison::lowering::ConcurrencyEnvironmentLayout {
            .llvm_type = "{ %record.Payload }",
        },
        .cleanup = orison::lowering::ConcurrencyCleanupPlan {
            .owned_cleanup_candidates = {
                orison::lowering::ConcurrencyCleanupFieldPlan {
                    .name = "payload",
                    .source_type_name = "Payload",
                    .llvm_type = "%record.Payload",
                    .owned_cleanup_symbol_name = "__orison_owned_cleanup.Payload",
                    .field_index = 0,
                },
            },
            .owned_cleanup = orison::lowering::ConcurrencyDropCleanupPlan {
                .cleanup_symbol_name = "__orison_thread_cleanup.manual.1.0",
                .actions = {
                    orison::lowering::OwnedCleanupAction {
                        .capture_name = "payload",
                        .source_type_name = "Payload",
                        .symbol_name = "__orison_owned_cleanup.Payload",
                        .field_index = 0,
                        .discovery_line = 1,
                    },
                },
            },
        },
    };

    assert(orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(
        plan.cleanup.owned_cleanup,
        {
            orison::lowering::OwnedCleanupDeclaration {
                .symbol_name = "__orison_owned_cleanup.Payload",
                .source_type_name = "Payload",
                .discovery_line = 1,
                .emit_declaration = true,
            },
        }
    ));
    auto enabled = orison::lowering::emit_concurrency_cleanup_thunk(plan);
    assert(
        enabled ==
        "define private void @__orison_thread_cleanup.manual.1.0(ptr %environment) {\n"
        "entry:\n"
        "  %cleanup.field.0 = getelementptr { %record.Payload }, ptr %environment, i32 0, i32 0\n"
        "  ; cleanup candidate payload: Payload field 0 drop __orison_owned_cleanup.Payload\n"
        "  call void @__orison_owned_cleanup.Payload(ptr %cleanup.field.0)\n"
        "  ret void\n"
        "}\n"
    );

    assert(!orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(plan.cleanup.owned_cleanup, {}));
    auto disabled = orison::lowering::emit_concurrency_cleanup_thunk(plan);
    assert(disabled.find("call void @__orison_owned_cleanup.Payload") == std::string::npos);
    assert(disabled.find("; cleanup candidate payload: Payload field 0 drop __orison_owned_cleanup.Payload") != std::string::npos);
    return 0;
}
