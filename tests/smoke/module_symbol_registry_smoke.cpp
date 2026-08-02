#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/module_symbol_registry.hpp"

#include "orison/diagnostics/diagnostic_bag.hpp"

#include <cassert>
#include <string>

namespace {

auto single_message(orison::diagnostics::DiagnosticBag const& diagnostics) -> std::string {
    auto const entries = diagnostics.entries();
    assert(entries.size() == 1);
    return entries.front().message;
}

void generated_concurrency_thunk_collisions_are_diagnosed() {
    auto registry = orison::lowering::ModuleSymbolRegistry {};
    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    assert(registry.register_symbol("worker", "source function symbol", 2, diagnostics));

    auto plan = orison::lowering::ConcurrencyExpressionPlan {
        .thunk_symbol_name = "worker",
    };

    assert(!registry.validate_symbol(
        plan.thunk_symbol_name,
        "generated concurrency thunk",
        12,
        diagnostics
    ));
    assert(single_message(diagnostics) ==
        "LLVM symbol 'worker' for generated concurrency thunk collides with source function symbol");
}

void generated_concurrency_cleanup_collisions_are_diagnosed() {
    auto registry = orison::lowering::ModuleSymbolRegistry {};
    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    assert(registry.register_symbol("host_cleanup", "foreign declaration", 3, diagnostics));

    auto plan = orison::lowering::ConcurrencyDropCleanupPlan {
        .cleanup_symbol_name = "host_cleanup",
    };

    assert(!registry.validate_symbol(
        plan.cleanup_symbol_name,
        "generated concurrency cleanup",
        14,
        diagnostics
    ));
    assert(single_message(diagnostics) ==
        "LLVM symbol 'host_cleanup' for generated concurrency cleanup collides with foreign declaration");
}

void planned_drop_declaration_collisions_are_diagnosed() {
    auto registry = orison::lowering::ModuleSymbolRegistry {};
    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    assert(registry.register_symbol("__orison_drop.Payload", "source function symbol", 4, diagnostics));

    auto declaration = orison::lowering::PlannedDropDeclaration {
        .symbol_name = "__orison_drop.Payload",
        .source_type_name = "Payload",
        .discovery_line = 18,
        .emit_declaration = true,
    };

    assert(!registry.validate_symbol(
        declaration.symbol_name,
        "planned drop declaration",
        declaration.discovery_line,
        diagnostics
    ));
    assert(single_message(diagnostics) ==
        "LLVM symbol '__orison_drop.Payload' for planned drop declaration collides with source function symbol");
}

void non_colliding_generated_symbols_pass() {
    auto registry = orison::lowering::ModuleSymbolRegistry {};
    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    assert(registry.register_symbol("worker", "source function symbol", 2, diagnostics));

    assert(registry.validate_symbol(
        "__orison_thread_thunk.worker.12.0",
        "generated concurrency thunk",
        12,
        diagnostics
    ));
    assert(registry.validate_symbol(
        "__orison_thread_cleanup.worker.12.0",
        "generated concurrency cleanup",
        12,
        diagnostics
    ));
    assert(registry.validate_symbol(
        "__orison_drop.Payload",
        "planned drop declaration",
        18,
        diagnostics
    ));
    assert(diagnostics.entries().empty());
}

}  // namespace

auto main() -> int {
    generated_concurrency_thunk_collisions_are_diagnosed();
    generated_concurrency_cleanup_collisions_are_diagnosed();
    planned_drop_declaration_collisions_are_diagnosed();
    non_colliding_generated_symbols_pass();
}
