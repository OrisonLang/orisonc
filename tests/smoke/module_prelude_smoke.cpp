#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/module_prelude.hpp"

#include <cassert>

int main() {
    auto strings = orison::lowering::StringConstantTable {};
    strings.constants.push_back(orison::lowering::StringConstant {
        .name = ".str.0",
        .bytes = std::string("ready\0", 6),
        .llvm_bytes = "ready\\00",
    });

    auto declarations = std::vector<orison::lowering::LoweredFunctionSignature> {
        orison::lowering::LoweredFunctionSignature {
            .return_type = "i32",
            .parameter_types = {"ptr", "i16"},
            .symbol_name = "printf",
            .adapter = orison::lowering::CAbiAdapterKind::variadic,
            .fixed_abi_parameter_count = 1,
        },
        orison::lowering::LoweredFunctionSignature {
            .return_type = "double",
            .parameter_types = {"double"},
            .symbol_name = "sin",
        },
    };

    auto prelude = orison::lowering::emit_module_prelude(strings, declarations);
    assert(
        prelude ==
        "@.str.0 = private unnamed_addr constant [6 x i8] c\"ready\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "declare double @sin(double)\n"
        "\n"
    );
    assert(orison::lowering::emit_module_prelude({}, {}).empty());

    using orison::lowering::ConcurrencyRuntimeOperation;
    auto concurrency_prelude = orison::lowering::emit_module_prelude(
        {},
        {},
        {
            ConcurrencyRuntimeOperation::spawn_thread,
            ConcurrencyRuntimeOperation::spawn_failed,
            ConcurrencyRuntimeOperation::join_thread,
            ConcurrencyRuntimeOperation::spawn_thread,
            ConcurrencyRuntimeOperation::spawn_task,
            ConcurrencyRuntimeOperation::await_task,
            ConcurrencyRuntimeOperation::destroy_handle,
        }
    );
    assert(
        concurrency_prelude ==
        "declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)\n"
        "declare void @__orison_concurrency_spawn_failed()\n"
        "declare void @__orison_thread_join(ptr)\n"
        "declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)\n"
        "declare void @__orison_task_await(ptr)\n"
        "declare void @__orison_concurrency_handle_destroy(ptr)\n"
        "\n"
    );

    using orison::lowering::DynamicArrayRuntimeOperation;
    auto dynamic_array_prelude = orison::lowering::emit_module_prelude(
        {},
        {},
        {},
        {},
        {
            DynamicArrayRuntimeOperation::allocate,
            DynamicArrayRuntimeOperation::grow,
            DynamicArrayRuntimeOperation::allocate,
            DynamicArrayRuntimeOperation::deallocate,
            DynamicArrayRuntimeOperation::bounds_failed,
        }
    );
    assert(
        dynamic_array_prelude ==
        "declare { ptr, i64, i64 } @__orison_dynamic_array_allocate(i64, i64)\n"
        "declare { ptr, i64, i64 } @__orison_dynamic_array_grow({ ptr, i64, i64 }, i64, i64)\n"
        "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)\n"
        "declare void @__orison_dynamic_array_bounds_failed()\n"
        "\n"
    );

    auto disabled_drop_prelude = orison::lowering::emit_module_prelude(
        {},
        {},
        {},
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 12,
            },
        }
    );
    assert(disabled_drop_prelude.empty());

    auto enabled_drop_prelude = orison::lowering::emit_module_prelude(
        {},
        {},
        {},
        {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 12,
                .emit_declaration = true,
            },
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 12,
                .emit_declaration = true,
            },
        }
    );
    assert(enabled_drop_prelude == "declare void @__orison_drop.Payload(ptr)\n\n");
    return 0;
}
