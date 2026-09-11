#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/module_prelude.hpp"

#include <cassert>

namespace {

auto cleanup_plan() -> orison::lowering::ConcurrencyExpressionPlan {
    return orison::lowering::ConcurrencyExpressionPlan {
        .cleanup_symbol_name = "__orison_thread_cleanup.allowed.1.0",
        .environment_layout = orison::lowering::ConcurrencyEnvironmentLayout {
            .llvm_type = "{ %record.DropTestPayload }",
        },
        .cleanup = orison::lowering::ConcurrencyCleanupPlan {
            .owned_cleanup_candidates = {
                orison::lowering::ConcurrencyCleanupFieldPlan {
                    .name = "payload",
                    .source_type_name = "DropTestPayload",
                    .llvm_type = "%record.DropTestPayload",
                    .drop_symbol_name = "__orison_owned_cleanup.DropTestPayload",
                    .field_index = 0,
                },
            },
            .owned_cleanup = orison::lowering::ConcurrencyDropCleanupPlan {
                .cleanup_symbol_name = "__orison_thread_cleanup.allowed.1.0",
                .actions = {
                    orison::lowering::OwnedCleanupAction {
                        .capture_name = "payload",
                        .source_type_name = "DropTestPayload",
                        .symbol_name = "__orison_owned_cleanup.DropTestPayload",
                        .field_index = 0,
                        .discovery_line = 1,
                    },
                },
            },
        },
    };
}

}  // namespace

int main() {
    auto plan = cleanup_plan();

    auto declarations = orison::lowering::declared_owned_cleanup_declarations_for_allowed_source_types(
        plan.cleanup.owned_cleanup.actions,
        {"DropTestPayload"}
    );
    assert(declarations.size() == 1);
    assert(declarations.front().emit_declaration);
    assert(
        orison::lowering::emit_module_prelude({}, {}, {}, declarations) ==
        "declare void @__orison_owned_cleanup.DropTestPayload(ptr)\n\n"
    );

    assert(orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(
        plan.cleanup.owned_cleanup,
        declarations
    ));
    assert(
        orison::lowering::emit_concurrency_cleanup_thunk(plan) ==
        "define private void @__orison_thread_cleanup.allowed.1.0(ptr %environment) {\n"
        "entry:\n"
        "  %cleanup.field.0 = getelementptr { %record.DropTestPayload }, ptr %environment, i32 0, i32 0\n"
        "  ; cleanup candidate payload: DropTestPayload field 0 drop __orison_owned_cleanup.DropTestPayload\n"
        "  call void @__orison_owned_cleanup.DropTestPayload(ptr %cleanup.field.0)\n"
        "  ret void\n"
        "}\n"
    );

    auto denied = orison::lowering::declared_owned_cleanup_declarations_for_allowed_source_types(
        plan.cleanup.owned_cleanup.actions,
        {"OtherPayload"}
    );
    assert(denied.empty());
    assert(!orison::lowering::authorize_drop_cleanup_calls_for_declared_abi(
        plan.cleanup.owned_cleanup,
        denied
    ));

    auto allowlist_plan = cleanup_plan();
    assert(orison::lowering::apply_drop_cleanup_authorization_options(
        allowlist_plan.cleanup.owned_cleanup,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_declared_owned_cleanup_source_type_allowlist = {"DropTestPayload"},
        }
    ));
    assert(orison::lowering::drop_calls_enabled(allowlist_plan.cleanup.owned_cleanup));

    auto denied_allowlist_plan = cleanup_plan();
    assert(!orison::lowering::apply_drop_cleanup_authorization_options(
        denied_allowlist_plan.cleanup.owned_cleanup,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_declared_owned_cleanup_source_type_allowlist = {"OtherPayload"},
        }
    ));
    assert(!orison::lowering::drop_calls_enabled(denied_allowlist_plan.cleanup.owned_cleanup));

    auto semantic_plan = cleanup_plan();
    assert(orison::lowering::apply_drop_cleanup_authorization_options(
        semantic_plan.cleanup.owned_cleanup,
        orison::lowering::LlvmIrEmissionOptions {
            .semantic_owned_cleanup_lowering_authorizations = {
                orison::semantics::OwnedCleanupLoweringAuthorization {
                    .site = orison::semantics::OwnedCleanupSite {
                        .source_type_name = "DropTestPayload",
                        .abi_symbol_name = "__orison_owned_cleanup.DropTestPayload",
                        .owner_name = "payload",
                        .site_line = 1,
                    },
                    .semantic_resolved = true,
                    .semantic_owned_cleanup_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    ));
    assert(orison::lowering::drop_calls_enabled(semantic_plan.cleanup.owned_cleanup));

    auto default_plan = cleanup_plan();
    assert(!orison::lowering::apply_drop_cleanup_authorization_options(
        default_plan.cleanup.owned_cleanup,
        orison::lowering::LlvmIrEmissionOptions {}
    ));
    assert(!orison::lowering::drop_calls_enabled(default_plan.cleanup.owned_cleanup));
    return 0;
}
