#include "computed_dynamic_array_audit_expectations.hpp"

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

namespace smoke = orison::tests::smoke;

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
    constexpr auto backend_examples = std::array<std::string_view, 85> {
        "concurrency_task_main.or",
        "concurrency_thread_main.or",
        "dynamic_array_owned_parameter.or",
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
        "local_null_safe_generic_aggregate.or",
        "local_result_choice_switch.or",
        "local_result_distinct_choice_switch.or",
        "local_result_distinct_record_choice_switch.or",
        "local_result_array_payload_choice_switch.or",
        "local_result_multi_payload_choice_switch.or",
        "local_result_multi_payload_choice_function_flow.or",
        "local_result_multi_payload_choice_branch_flow.or",
        "local_result_multi_payload_choice_reassignment.or",
        "local_result_multi_payload_choice_record_field.or",
        "local_result_multi_payload_choice_array_element.or",
        "local_result_multi_payload_choice_record_array.or",
        "local_result_multi_payload_choice_array_record.or",
        "local_result_multi_payload_choice_nested_array_record.or",
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
    assert(
        view_descriptor_ir.ir_text.find(
            "define { ptr, i64 } @method.UInt32.forward_view(i32 %this, { ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        view_descriptor_ir.ir_text.find(
            "call { ptr, i64 } @method.UInt32.forward_view(i32 %seed, { ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        view_descriptor_ir.ir_text.find(
            "define { ptr, i64 } @method.ViewBucket.forward_view(%record.ViewBucket %this, { ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        view_descriptor_ir.ir_text.find(
            "call { ptr, i64 } @method.ViewBucket.forward_view(%record.ViewBucket"
        ) != std::string::npos
    );
    assert(
        view_descriptor_ir.ir_text.find("getelementptr %record.ViewWrapper, ptr %wrapper.addr") !=
        std::string::npos
    );
    assert(
        view_descriptor_ir.ir_text.find("getelementptr [2 x %record.ViewBucket], ptr") !=
        std::string::npos
    );
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

    auto computed_dynamic_array_iterable =
        pipeline.emit_llvm(
            fixtures / "dynamic_array_computed_iterable_rejected.or",
            orison::pipeline::CompilePipelineOptions {
                .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            }
        );
    assert(computed_dynamic_array_iterable.has_errors());
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "lowering DynamicArray for statements currently requires a named descriptor iterable"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray iterable of type 'DynamicArray<UInt32>' requires a proven single descriptor owner"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "cleanup owner proof missing"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<UInt32> "
            "element UInt32 owners left right [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray cleanup sequence plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [loop cleanup blocked] [function cleanup blocked] "
            "[cleanup sequence disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray descriptor render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor load blocked] [data projection blocked] "
            "[length projection blocked] [capacity projection blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray loop control render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [entry branch blocked] [index phi blocked] [bounds check blocked] "
            "[conditional branch blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray element address render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [data pointer blocked] [index blocked] [element address blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray element load render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [element address blocked] [item value blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray loop continue render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [continue block blocked] [next index blocked] [backedge branch blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray loop render sequence plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor render blocked] [loop control blocked] [body block blocked] "
            "[element address blocked] [element load blocked] [loop continue blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray loop exit cleanup plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [exit block blocked] [cleanup blocked] [cleanup sequence disabled] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_iterable.error_text.find(
            "computed DynamicArray production emission gate plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [ownership blocked] [loop render blocked] [loop cleanup ownership blocked] "
            "[function cleanup resumption blocked] [exit cleanup blocked] "
            "[production sequence blocked] [production emission disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_dynamic_array_local_same_owner_iterable =
        pipeline.emit_llvm(fixtures / "dynamic_array_computed_local_same_owner_iterable.or");
    assert(!computed_dynamic_array_local_same_owner_iterable.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_iterable.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_iterable.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_iterable.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );

    auto computed_same_owner_dynamic_array_iterable =
        pipeline.emit_llvm(
            fixtures / "dynamic_array_computed_same_owner_iterable.or"
        );
    assert(!computed_same_owner_dynamic_array_iterable.has_errors());
    assert(
        computed_same_owner_dynamic_array_iterable.ir_text.find(
            "define i32 @sum_words(i1 %flag, { ptr, i64, i64 } %items)"
        ) != std::string::npos
    );
    assert(
        computed_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.cleanup.resume.call"
        ) == std::string::npos
    );
    auto same_owner_first_deallocate = computed_same_owner_dynamic_array_iterable.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    assert(same_owner_first_deallocate != std::string::npos);
    assert(
        computed_same_owner_dynamic_array_iterable.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            same_owner_first_deallocate + 1
        ) == std::string::npos
    );
    assert(
        computed_same_owner_dynamic_array_iterable.ir_text.find(
            "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr"
        ) != std::string::npos
    );

    auto computed_nested_same_owner_dynamic_array_iterable =
        pipeline.emit_llvm(
            fixtures / "dynamic_array_computed_nested_same_owner_iterable.or"
        );
    assert(!computed_nested_same_owner_dynamic_array_iterable.has_errors());
    assert(
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "define i32 @sum_words(i1 %flag, i1 %other_flag, { ptr, i64, i64 } %items)"
        ) != std::string::npos
    );
    assert(
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.cleanup.resume.call"
        ) == std::string::npos
    );
    auto nested_same_owner_first_deallocate =
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(nested_same_owner_first_deallocate != std::string::npos);
    assert(
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            nested_same_owner_first_deallocate + 1
        ) == std::string::npos
    );
    assert(
        computed_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr"
        ) != std::string::npos
    );

    auto computed_nested_owner_mismatch_dynamic_array_iterable =
        pipeline.emit_llvm(
            fixtures / "dynamic_array_computed_nested_owner_mismatch_iterable_rejected.or"
        );
    assert(computed_nested_owner_mismatch_dynamic_array_iterable.has_errors());
    assert(
        computed_nested_owner_mismatch_dynamic_array_iterable.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<UInt32> "
            "element UInt32 owners items items other [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_nested_owner_mismatch_dynamic_array_iterable.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_local_nested_owner_mismatch_dynamic_array_iterable =
        pipeline.emit_llvm(
            fixtures / "dynamic_array_computed_local_nested_owner_mismatch_iterable_rejected.or",
            orison::pipeline::CompilePipelineOptions {
                .dynamic_array_production_construction_lowering_enabled = true,
                .dynamic_array_production_for_lowering_enabled = true,
                .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
            }
        );
    assert(computed_local_nested_owner_mismatch_dynamic_array_iterable.has_errors());
    assert(
        computed_local_nested_owner_mismatch_dynamic_array_iterable.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<UInt32> "
            "element UInt32 owners items items other [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_local_nested_owner_mismatch_dynamic_array_iterable.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_local_same_owner_dynamic_array_iterable =
        pipeline.emit_llvm(
            fixtures / "dynamic_array_computed_local_same_owner_iterable.or",
            orison::pipeline::CompilePipelineOptions {
                .dynamic_array_production_construction_lowering_enabled = true,
                .dynamic_array_production_for_lowering_enabled = true,
                .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
            }
        );
    assert(!computed_local_same_owner_dynamic_array_iterable.has_errors());
    assert(
        computed_local_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_local_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_local_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_local_same_owner_dynamic_array_iterable.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_local_same_owner_dynamic_array_iterable.ir_text.find(
            "  ; cleanup state handoff resume operation items.computed_for.0.cleanup.resume "
            "from items.loop.entry to items [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_local_same_owner_dynamic_array_iterable.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data, "
            "i64 4, i64 %items.computed_for.0.capacity)\n"
        ) != std::string::npos
    );
    assert(
        computed_local_same_owner_dynamic_array_iterable.ir_text.find(
            "  store { ptr, i64, i64 } zeroinitializer, ptr %items.addr\n"
        ) != std::string::npos
    );

    auto computed_local_nested_same_owner_dynamic_array_iterable =
        pipeline.emit_llvm(
            fixtures / "dynamic_array_computed_local_nested_same_owner_iterable.or",
            orison::pipeline::CompilePipelineOptions {
                .dynamic_array_production_construction_lowering_enabled = true,
                .dynamic_array_production_for_lowering_enabled = true,
                .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
            }
        );
    assert(!computed_local_nested_same_owner_dynamic_array_iterable.has_errors());
    assert(
        computed_local_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "define i32 @sum_words(i1 %flag, i1 %other_flag)"
        ) != std::string::npos
    );
    assert(
        computed_local_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_local_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_local_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_local_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_local_nested_same_owner_dynamic_array_iterable.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data, "
            "i64 4, i64 %items.computed_for.0.capacity)\n"
        ) != std::string::npos
    );

    auto choice_dynamic_array_payload =
        pipeline.emit_llvm(fixtures / "choice_dynamic_array_payload.or");
    assert(!choice_dynamic_array_payload.has_errors());
    assert(
        choice_dynamic_array_payload.ir_text.find(
            "define { i32, { ptr, i64, i64 } } @make_buffer({ ptr, i64, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        choice_dynamic_array_payload.ir_text.find("call void @__orison_dynamic_array_deallocate") ==
        std::string::npos
    );

    auto choice_dynamic_array_payload_parameter =
        pipeline.emit_llvm(fixtures / "choice_dynamic_array_payload_parameter.or");
    assert(!choice_dynamic_array_payload_parameter.has_errors());
    assert(
        choice_dynamic_array_payload_parameter.ir_text.find(
            "define i32 @consume({ i32, { ptr, i64, i64 } } %buffer)"
        ) != std::string::npos
    );

    auto choice_dynamic_array_payload_let =
        pipeline.emit_llvm(fixtures / "choice_dynamic_array_payload_let.or");
    assert(!choice_dynamic_array_payload_let.has_errors());
    assert(
        choice_dynamic_array_payload_let.ir_text.find(
            "%buffer.addr = alloca { i32, { ptr, i64, i64 } }"
        ) != std::string::npos
    );

    auto choice_dynamic_array_payload_var =
        pipeline.emit_llvm(fixtures / "choice_dynamic_array_payload_var.or");
    assert(!choice_dynamic_array_payload_var.has_errors());
    assert(
        choice_dynamic_array_payload_var.ir_text.find(
            "%buffer.addr = alloca { i32, { ptr, i64, i64 } }"
        ) != std::string::npos
    );

    auto choice_dynamic_array_owned_payload =
        pipeline.emit_llvm(fixtures / "choice_dynamic_array_owned_payload.or");
    assert(!choice_dynamic_array_owned_payload.has_errors());
    auto choice_owned_tag_check = choice_dynamic_array_owned_payload.ir_text.find(
        "%buffer.Ready.values.choice_dynamic_array_cleanup2.is_active = icmp eq i32"
    );
    auto choice_owned_drop = choice_dynamic_array_owned_payload.ir_text.find(
        "call void @__orison_drop.Payload(ptr %buffer.Ready.values.choice_dynamic_array_cleanup0.drop.element.addr)"
    );
    auto choice_owned_deallocate = choice_dynamic_array_owned_payload.ir_text.find(
        "call void @__orison_dynamic_array_deallocate(ptr "
        "%buffer.Ready.values.choice_dynamic_array_cleanup0.cleanup.data"
    );
    auto choice_owned_return = choice_dynamic_array_owned_payload.ir_text.find("ret i32 0");
    assert(choice_owned_tag_check != std::string::npos);
    assert(choice_owned_drop != std::string::npos);
    assert(choice_owned_deallocate != std::string::npos);
    assert(choice_owned_return != std::string::npos);
    assert(choice_owned_tag_check < choice_owned_drop);
    assert(choice_owned_drop < choice_owned_deallocate);
    assert(choice_owned_deallocate < choice_owned_return);

    auto owned_dynamic_array_parameter_source_drop = pipeline.emit_llvm(
        fixtures / "dynamic_array_owned_parameter_source_drop.or"
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
