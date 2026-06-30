#include "orison/lowering/module_prelude.hpp"
#include "orison/lowering/statement_emitter.hpp"

#include <cassert>

int main() {
    auto plan = orison::lowering::ConcurrencyExpressionPlan {
        .cleanup_symbol_name = "__orison_thread_cleanup.allowed.1.0",
        .environment_layout = orison::lowering::ConcurrencyEnvironmentLayout {
            .llvm_type = "{ %record.DropTestPayload }",
        },
        .cleanup = orison::lowering::ConcurrencyCleanupPlan {
            .drop_candidates = {
                orison::lowering::ConcurrencyCleanupFieldPlan {
                    .name = "payload",
                    .source_type_name = "DropTestPayload",
                    .llvm_type = "%record.DropTestPayload",
                    .drop_symbol_name = "__orison_drop.DropTestPayload",
                    .field_index = 0,
                },
            },
            .drop_cleanup = orison::lowering::ConcurrencyDropCleanupPlan {
                .cleanup_symbol_name = "__orison_thread_cleanup.allowed.1.0",
                .actions = {
                    orison::lowering::PlannedDropAction {
                        .capture_name = "payload",
                        .source_type_name = "DropTestPayload",
                        .symbol_name = "__orison_drop.DropTestPayload",
                        .field_index = 0,
                        .discovery_line = 1,
                    },
                },
            },
        },
    };

    auto declarations = orison::lowering::declared_drop_declarations_for_allowed_source_types(
        plan.cleanup.drop_cleanup.actions,
        {"DropTestPayload"}
    );
    assert(declarations.size() == 1);
    assert(declarations.front().emit_declaration);
    assert(
        orison::lowering::emit_module_prelude({}, {}, {}, declarations) ==
        "declare void @__orison_drop.DropTestPayload(ptr)\n\n"
    );

    assert(orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(
        plan.cleanup.drop_cleanup,
        declarations
    ));
    assert(
        orison::lowering::emit_concurrency_cleanup_thunk(plan) ==
        "define private void @__orison_thread_cleanup.allowed.1.0(ptr %environment) {\n"
        "entry:\n"
        "  %cleanup.field.0 = getelementptr { %record.DropTestPayload }, ptr %environment, i32 0, i32 0\n"
        "  ; cleanup candidate payload: DropTestPayload field 0 drop __orison_drop.DropTestPayload\n"
        "  call void @__orison_drop.DropTestPayload(ptr %cleanup.field.0)\n"
        "  ret void\n"
        "}\n"
    );

    auto denied = orison::lowering::declared_drop_declarations_for_allowed_source_types(
        plan.cleanup.drop_cleanup.actions,
        {"OtherPayload"}
    );
    assert(denied.empty());
    assert(!orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(
        plan.cleanup.drop_cleanup,
        denied
    ));
    return 0;
}
