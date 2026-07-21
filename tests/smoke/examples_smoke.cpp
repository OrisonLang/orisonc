#include "orison/pipeline/compile_pipeline.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_examples_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto examples = std::filesystem::path(ORISON_SOURCE_DIR) / "examples";
    orison::pipeline::CompilePipeline pipeline;
    constexpr auto backend_examples = std::array<std::string_view, 70> {
        "concurrency_task_main.or",
        "concurrency_thread_main.or",
        "dynamic_array_parameter_reads.or",
        "ffi_aggregate_scalar_parameters.or",
        "ffi_fixed_parameters.or",
        "local_helper_array_for.or",
        "local_ternary_array_for.or",
        "local_ternary_array_literal_for.or",
        "local_ternary_record_array_literal_for.or",
        "local_array_for.or",
        "local_aggregate_let.or",
        "local_inferred_record_let.or",
        "local_inferred_nested_record_let.or",
        "local_inferred_record_array_let.or",
        "local_inferred_array_let.or",
        "local_dynamic_array_append.or",
        "local_inferred_nested_array_let.or",
        "local_inferred_array_record_let.or",
        "local_inferred_nested_mixed_let.or",
        "local_inferred_aggregate_reassignment.or",
        "local_branch_aggregate_reassignment.or",
        "local_switch_aggregate_reassignment.or",
        "local_mutable_aggregate_path_read.or",
        "local_branch_aggregate_field_assignment.or",
        "local_switch_aggregate_field_assignment.or",
        "local_branch_nested_array_assignment.or",
        "local_switch_nested_array_assignment.or",
        "local_helper_aggregate_access.or",
        "local_aggregate_parameter_access.or",
        "local_call_argument_aggregate_scalar.or",
        "local_return_container_aggregate_scalar.or",
        "local_nested_return_container_aggregate_scalar.or",
        "local_branch_return_container_aggregate_scalar.or",
        "local_loop_return_container_aggregate_scalar.or",
        "local_control_flow_aggregate_scalar.or",
        "local_loop_aggregate_scalar.or",
        "local_guard_aggregate_scalar.or",
        "local_defer_aggregate_scalar.or",
        "local_unsafe_aggregate_scalar.or",
        "local_method_aggregate_access.or",
        "local_record_method_aggregate_access.or",
        "local_member_receiver_method_aggregate_access.or",
        "local_branch_inferred_aggregate_let.or",
        "local_nested_aggregate_let.or",
        "local_record_array_for.or",
        "local_record_aggregate_reassignment.or",
        "local_record_field_assignment.or",
        "local_record_index_for.or",
        "local_record_index_field_for.or",
        "local_member_receiver_method_array_for.or",
        "local_method_array_for.or",
        "local_record_method_array_for.or",
        "local_record_nested_addressing.or",
        "local_record_nested_record_addressing.or",
        "local_record_nested_record_assignment.or",
        "tour_01_packages_imports.or",
        "tour_02_records_choices.or",
        "tour_03_interfaces_methods.or",
        "tour_04_generics_ownership.or",
        "tour_05_bindings_operators.or",
        "tour_06_control_flow.or",
        "tour_07_recursion.or",
        "tour_08_collections.or",
        "tour_10_unsafe_memory.or",
        "nested_pointer_aggregate_assignment.or",
        "pointer_array_nested_assignment.or",
        "pointer_record_field_assignment.or",
        "pointer_record_nested_addressing.or",
        "tour_09_ffi_printf.or",
        "tour_11_concurrency.or",
    };
    for (auto name : backend_examples) {
        auto backend = pipeline.emit_object(examples / name);
        assert(!backend.has_errors());
        assert(!backend.object_bytes.empty());
    }

    auto view_descriptor_ir = pipeline.emit_llvm(examples / "view_descriptor_reads.or");
    assert(!view_descriptor_ir.has_errors());
    assert(view_descriptor_ir.ir_text.find("define i64 @count({ ptr, i64 } %values)") != std::string::npos);
    assert(view_descriptor_ir.ir_text.find("define i32 @first({ ptr, i64 } %values)") != std::string::npos);
    assert(view_descriptor_ir.ir_text.find("define i32 @sum({ ptr, i64 } %values)") != std::string::npos);
    auto view_descriptor_object =
        orison::lowering::LlvmObjectEmitter {}.emit(view_descriptor_ir.ir_text);
    assert(!view_descriptor_object.has_errors());
    assert(!view_descriptor_object.object_bytes.empty());

    auto dynamic_array_parameter_ir = pipeline.emit_llvm(examples / "dynamic_array_parameter_reads.or");
    assert(!dynamic_array_parameter_ir.has_errors());
    assert(
        dynamic_array_parameter_ir.ir_text.find("define i64 @count({ ptr, i64, i64 } %values)") !=
        std::string::npos
    );
    assert(
        dynamic_array_parameter_ir.ir_text.find(
            "%values.dynamic_array_index0.in_bounds = icmp ult i64 0, %values.dynamic_array_index0.length"
        ) != std::string::npos
    );
    assert(
        dynamic_array_parameter_ir.ir_text.find(
            "%values.sequence_for0.value = load i32, ptr %values.sequence_for0.element.addr"
        ) != std::string::npos
    );

    auto fixtures = std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures";
    auto owned_dynamic_array_parameter =
        pipeline.emit_llvm(fixtures / "dynamic_array_owned_parameter_rejected.or");
    assert(owned_dynamic_array_parameter.has_errors());
    assert(
        owned_dynamic_array_parameter.error_text.find(
            "lowering DynamicArray parameter 'items' with owned element type Payload requires ownership/drop proof before production lowering"
        ) != std::string::npos
    );

    auto owned_dynamic_array_parameter_source_drop = pipeline.emit_llvm(
        fixtures / "dynamic_array_owned_parameter_source_drop.or",
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!owned_dynamic_array_parameter_source_drop.has_errors());
    assert(
        owned_dynamic_array_parameter_source_drop.ir_text.find(
            "define i32 @use_items({ ptr, i64, i64 } %items)"
        ) != std::string::npos
    );
    assert(
        owned_dynamic_array_parameter_source_drop.ir_text.find(
            "define void @__orison_drop.Payload(ptr %value)"
        ) != std::string::npos
    );
    assert(
        owned_dynamic_array_parameter_source_drop.ir_text.find(
            "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)"
        ) != std::string::npos
    );
    assert(
        owned_dynamic_array_parameter_source_drop.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );
    auto owned_parameter_drop =
        owned_dynamic_array_parameter_source_drop.ir_text.find("call void @__orison_drop.Payload");
    auto owned_parameter_deallocate =
        owned_dynamic_array_parameter_source_drop.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto owned_parameter_return = owned_dynamic_array_parameter_source_drop.ir_text.find("ret i32 1");
    assert(owned_parameter_drop != std::string::npos);
    assert(owned_parameter_deallocate != std::string::npos);
    assert(owned_parameter_return != std::string::npos);
    assert(owned_parameter_drop < owned_parameter_deallocate);
    assert(owned_parameter_deallocate < owned_parameter_return);

    auto ordinary_pointer_path = std::filesystem::temp_directory_path() / "orison_string_pointer_boundary.or";
    {
        std::ofstream source(ordinary_pointer_path);
        source << "package examples.boundary\n";
        source << "function consume(value: Pointer<Byte>) -> Int32\n";
        source << "    0 as Int32\n";
        source << "function main() -> Int32\n";
        source << "    consume(\"not an ordinary pointer\")\n";
    }
    auto ordinary_pointer = pipeline.analyze(ordinary_pointer_path);
    assert(ordinary_pointer.has_errors());
    assert(
        ordinary_pointer.error_text.find("requires a structurally pointer-like expression") != std::string::npos
    );

    auto wrong_arity_path = std::filesystem::temp_directory_path() / "orison_fixed_ffi_wrong_arity.or";
    {
        std::ofstream source(wrong_arity_path);
        source << "package examples.boundary\n";
        source << "package foreign \"c\"\n";
        source << "    function strcmp(left: Pointer<Byte>, right: Pointer<Byte>) -> Int32\n";
        source << "function main() -> Int32\n";
        source << "    strcmp(\"Orison\")\n";
    }
    auto wrong_arity = pipeline.analyze(wrong_arity_path);
    assert(wrong_arity.has_errors());
    assert(
        wrong_arity.error_text.find("function 'strcmp' expects 2 arguments but received 1") != std::string::npos
    );
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
