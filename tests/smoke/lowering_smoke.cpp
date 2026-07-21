#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"
#include "orison/lowering/llvm_ir_verifier.hpp"
#include "orison/semantics/drop_model.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

enum class FinalControlFlowKind {
    if_expression,
    switch_expression,
};

auto lower_source(
    std::filesystem::path const& path,
    std::string_view source,
    orison::lowering::LlvmIrEmissionOptions const& options = {}
)
    -> orison::lowering::LlvmIrEmissionResult {
    {
        std::ofstream output(path);
        output << source;
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto semantic_result = analyzer.analyze(parse_result.module);
    assert(!semantic_result.has_errors());

    orison::lowering::LlvmIrEmitter emitter;
    return emitter.emit(parse_result.module, semantic_result, options);
}

auto lower_source_with_semantics(
    std::filesystem::path const& path,
    std::string_view source,
    orison::semantics::SemanticAnalysisResult semantic_result,
    orison::lowering::LlvmIrEmissionOptions const& options = {}
)
    -> orison::lowering::LlvmIrEmissionResult {
    {
        std::ofstream output(path);
        output << source;
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    assert(!semantic_result.has_errors());

    orison::lowering::LlvmIrEmitter emitter;
    return emitter.emit(parse_result.module, semantic_result, options);
}

void assert_ir_contains(orison::lowering::LlvmIrEmissionResult const& result, std::string_view needle) {
    if (result.ir_text.find(needle) == std::string::npos) {
        std::cerr << "missing expected IR fragment: " << needle << "\n";
        std::cerr << result.ir_text << "\n";
    }
    assert(result.ir_text.find(needle) != std::string::npos);
}

void assert_emits_negative_int32_value(orison::lowering::LlvmIrEmissionResult const& result) {
    assert(!result.has_errors());
    assert_ir_contains(result, " = sub i32 0, 27");
}

void assert_stores_lowered_int32_tmp(orison::lowering::LlvmIrEmissionResult const& result) {
    assert_ir_contains(result, "store i32 %tmp");
}

void assert_returns_lowered_tmp(orison::lowering::LlvmIrEmissionResult const& result) {
    assert_ir_contains(result, "ret i32 %tmp");
}

void assert_returns_lowered_aggregate_tmp(
    orison::lowering::LlvmIrEmissionResult const& result,
    std::string_view aggregate_type
) {
    assert_ir_contains(result, std::string {"ret "} + std::string {aggregate_type} + " %tmp");
}

void assert_defines_method(
    orison::lowering::LlvmIrEmissionResult const& result,
    std::string_view return_type,
    std::string_view symbol_name,
    std::string_view parameter_fragment
) {
    assert_ir_contains(
        result,
        std::string {"define "} + std::string {return_type} + " @" + std::string {symbol_name} + "(" +
            std::string {parameter_fragment} + ")"
    );
}

void assert_emits_negative_int32_record_receiver_cleanup_call(
    orison::lowering::LlvmIrEmissionResult const& result
) {
    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
}

void assert_inserts_lowered_int32_tmp_into_aggregate(
    orison::lowering::LlvmIrEmissionResult const& result,
    std::string_view aggregate_type
) {
    assert_ir_contains(
        result,
        std::string {" = insertvalue "} + std::string {aggregate_type} + " undef, i32 %tmp"
    );
}

void assert_emits_final_if_blocks(orison::lowering::LlvmIrEmissionResult const& result) {
    assert_ir_contains(result, "if.then.");
    assert_ir_contains(result, "if.else.");
    assert_ir_contains(result, "if.merge.");
}

void assert_emits_final_switch_blocks(orison::lowering::LlvmIrEmissionResult const& result) {
    assert_ir_contains(result, "switch.case.");
    assert_ir_contains(result, "switch.default.");
    assert_ir_contains(result, "switch.merge.");
}

void assert_emits_negative_int32_scalar_control_flow_method_return(
    orison::lowering::LlvmIrEmissionResult const& result,
    std::string_view method_symbol,
    FinalControlFlowKind control_flow_kind
) {
    assert_emits_negative_int32_value(result);
    assert_defines_method(result, "i32", method_symbol, "i32 %this, i1 %flag");
    if (control_flow_kind == FinalControlFlowKind::switch_expression) {
        assert_emits_final_switch_blocks(result);
    } else {
        assert_emits_final_if_blocks(result);
    }
    assert_returns_lowered_tmp(result);
}

void assert_emits_negative_int32_aggregate_control_flow_method_return(
    orison::lowering::LlvmIrEmissionResult const& result,
    std::string_view return_type,
    std::string_view method_symbol,
    FinalControlFlowKind control_flow_kind
) {
    assert_emits_negative_int32_value(result);
    assert_defines_method(result, return_type, method_symbol, "i32 %this, i1 %flag");
    assert_inserts_lowered_int32_tmp_into_aggregate(result, return_type);
    if (control_flow_kind == FinalControlFlowKind::switch_expression) {
        assert_emits_final_switch_blocks(result);
    } else {
        assert_emits_final_if_blocks(result);
    }
    assert_returns_lowered_aggregate_tmp(result, return_type);
}

void assert_calls_lowered_int32_tmp(
    orison::lowering::LlvmIrEmissionResult const& result,
    std::string_view call_prefix
) {
    assert_ir_contains(result, std::string {call_prefix} + "i32 %tmp");
}

void assert_emits_negative_int32_control_flow_call_argument_return(
    orison::lowering::LlvmIrEmissionResult const& result,
    std::string_view call_prefix,
    FinalControlFlowKind control_flow_kind
) {
    assert_emits_negative_int32_value(result);
    assert_calls_lowered_int32_tmp(result, call_prefix);
    if (control_flow_kind == FinalControlFlowKind::switch_expression) {
        assert_emits_final_switch_blocks(result);
    } else {
        assert_emits_final_if_blocks(result);
    }
    assert_returns_lowered_tmp(result);
}

void assert_emits_negative_int32_record_field_control_flow_call_argument_return(
    orison::lowering::LlvmIrEmissionResult const& result,
    FinalControlFlowKind control_flow_kind
) {
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_ir_contains(result, " = insertvalue %record.SignedBox undef, i32 %tmp");
    assert_ir_contains(result, "%box.addr = alloca %record.SignedBox");
    assert_ir_contains(result, " = getelementptr %record.SignedBox, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_emits_negative_int32_control_flow_call_argument_return(
        result,
        " = call i32 @identity(",
        control_flow_kind
    );
}

void assert_emits_negative_int32_array_element_control_flow_call_argument_return(
    orison::lowering::LlvmIrEmissionResult const& result,
    FinalControlFlowKind control_flow_kind
) {
    assert_ir_contains(result, " = insertvalue [2 x i32] undef, i32 %tmp");
    assert_ir_contains(result, "%values.addr = alloca [2 x i32]");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_emits_negative_int32_control_flow_call_argument_return(
        result,
        " = call i32 @identity(",
        control_flow_kind
    );
}

void assert_emits_negative_int32_record_field_control_flow_assignment_return(
    orison::lowering::LlvmIrEmissionResult const& result,
    FinalControlFlowKind control_flow_kind
) {
    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    if (control_flow_kind == FinalControlFlowKind::switch_expression) {
        assert_emits_final_switch_blocks(result);
    } else {
        assert_emits_final_if_blocks(result);
    }
    assert_ir_contains(result, " = getelementptr %record.SignedBox, ptr %box.addr, i32 0, i32 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void assert_emits_negative_int32_array_element_control_flow_assignment_return(
    orison::lowering::LlvmIrEmissionResult const& result,
    FinalControlFlowKind control_flow_kind
) {
    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%values.addr = alloca [2 x i32]");
    if (control_flow_kind == FinalControlFlowKind::switch_expression) {
        assert_emits_final_switch_blocks(result);
    } else {
        assert_emits_final_if_blocks(result);
    }
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void assert_rejects_negative_uint32_cast(orison::lowering::LlvmIrEmissionResult const& result) {
    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message.find("unsupported cast: negative value to UInt32") !=
        std::string::npos
    );
}

void test_emit_constant_uint32_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_constant_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n"
    );

    assert(!result.has_errors());
    assert(result.dynamic_array_runtime_operations.empty());
    assert(result.dynamic_array_runtime_request_report().empty());
    assert(result.ir_text.find("__orison_dynamic_array_") == std::string::npos);
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_collects_test_only_dynamic_array_construction_metadata() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_dynamic_array_construction_metadata.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<UInt32>",
                    .initial_capacity = 4,
                },
            },
            .test_only_render_dynamic_array_allocation_calls = true,
            .test_only_render_dynamic_array_grow_calls = true,
            .test_only_render_dynamic_array_deallocation_calls = true,
            .test_only_render_dynamic_array_descriptor_bindings = true,
            .test_only_render_dynamic_array_descriptor_projections = true,
            .test_only_render_dynamic_array_bounds_checks = true,
            .test_only_render_dynamic_array_element_addresses = true,
            .test_only_render_dynamic_array_element_loads = true,
            .test_only_render_dynamic_array_element_stores = true,
            .test_only_render_dynamic_array_descriptor_length_updates = true,
            .test_only_render_dynamic_array_descriptor_write_backs = true,
            .test_only_render_dynamic_array_append_sequences = true,
            .test_only_render_dynamic_array_grow_sequences = true,
            .test_only_render_dynamic_array_append_with_grow_sequences = true,
            .test_only_render_dynamic_array_cleanup_sequences = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );

    assert(!result.has_errors());
    assert(result.dynamic_array_construction_plans.size() == 1);
    assert(result.dynamic_array_runtime_operations.size() == 3);
    assert(result.dynamic_array_runtime_operations.front() == orison::lowering::DynamicArrayRuntimeOperation::allocate);
    assert(result.dynamic_array_runtime_operations[1] == orison::lowering::DynamicArrayRuntimeOperation::grow);
    assert(result.dynamic_array_runtime_operations[2] == orison::lowering::DynamicArrayRuntimeOperation::deallocate);
    auto plan_report = result.dynamic_array_construction_plan_report();
    assert(plan_report.size() == 1);
    assert(
        plan_report.front() ==
        "dynamic array construction DynamicArray<UInt32> element UInt32 lowers to i32 "
        "element_size 4 initial_capacity 4 requests __orison_dynamic_array_allocate (metadata only)"
    );
    auto runtime_report = result.dynamic_array_runtime_request_report();
    assert(runtime_report.size() == 3);
    assert(
        runtime_report[0] ==
        "dynamic array runtime __orison_dynamic_array_allocate returns void "
        "params ptr sret({ ptr, i64, i64 }) i64 i64"
    );
    assert(
        runtime_report[1] ==
        "dynamic array runtime __orison_dynamic_array_grow returns void "
        "params ptr sret({ ptr, i64, i64 }) ptr byval({ ptr, i64, i64 }) i64 i64"
    );
    assert(
        runtime_report[2] ==
        "dynamic array runtime __orison_dynamic_array_deallocate returns void params ptr i64 i64"
    );
    assert_ir_contains(
        result,
        "declare void @__orison_dynamic_array_allocate(ptr sret({ ptr, i64, i64 }), i64, i64)"
    );
    assert_ir_contains(
        result,
        "declare void @__orison_dynamic_array_grow(ptr sret({ ptr, i64, i64 }), "
        "ptr byval({ ptr, i64, i64 }), i64, i64)"
    );
    assert_ir_contains(result, "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)");
    assert(result.ir_text.find("call { ptr, i64, i64 } @__orison_dynamic_array_allocate") == std::string::npos);
    assert(result.ir_text.find("call { ptr, i64, i64 } @__orison_dynamic_array_grow") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_dynamic_array_deallocate") == std::string::npos);
    assert(result.test_only_dynamic_array_allocation_call_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_allocation_call_ir.front() ==
        "  %dynamic_array_alloc0.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array_alloc0.addr, i64 4, i64 4)\n"
        "  %dynamic_array_alloc0 = load { ptr, i64, i64 }, ptr %dynamic_array_alloc0.addr\n"
    );
    assert(result.test_only_dynamic_array_grow_call_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_grow_call_ir.front() ==
        "  %dynamic_array0.grown.input = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %dynamic_array_alloc0, ptr %dynamic_array0.grown.input\n"
        "  %dynamic_array0.grown.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_grow("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array0.grown.addr, "
        "ptr byval({ ptr, i64, i64 }) %dynamic_array0.grown.input, "
        "i64 4, i64 %dynamic_array0.grow.next.capacity)\n"
        "  %dynamic_array0.grown = load { ptr, i64, i64 }, ptr %dynamic_array0.grown.addr\n"
    );
    assert(result.test_only_dynamic_array_deallocation_call_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_deallocation_call_ir.front() ==
        "  call void @__orison_dynamic_array_deallocate(ptr %dynamic_array0.data, i64 4, i64 %dynamic_array0.capacity)\n"
    );
    assert(result.test_only_dynamic_array_descriptor_binding_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_descriptor_binding_ir.front() ==
        "  %dynamic_array0.addr = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %dynamic_array_alloc0, ptr %dynamic_array0.addr\n"
    );
    assert(result.ir_text.find("%dynamic_array0.addr = alloca { ptr, i64, i64 }") == std::string::npos);
    assert(result.test_only_dynamic_array_descriptor_projection_ir.size() == 3);
    assert(
        result.test_only_dynamic_array_descriptor_projection_ir[0] ==
        "  %dynamic_array0.data = extractvalue { ptr, i64, i64 } %dynamic_array_alloc0, 0\n"
    );
    assert(
        result.test_only_dynamic_array_descriptor_projection_ir[1] ==
        "  %dynamic_array0.length = extractvalue { ptr, i64, i64 } %dynamic_array_alloc0, 1\n"
    );
    assert(
        result.test_only_dynamic_array_descriptor_projection_ir[2] ==
        "  %dynamic_array0.capacity = extractvalue { ptr, i64, i64 } %dynamic_array_alloc0, 2\n"
    );
    assert(result.ir_text.find("%dynamic_array0.length = extractvalue") == std::string::npos);
    assert(result.test_only_dynamic_array_bounds_check_ir.size() == 3);
    assert(
        result.test_only_dynamic_array_bounds_check_ir[0] ==
        "  %dynamic_array0.index.in_bounds = icmp ult i64 %dynamic_array0.index, %dynamic_array0.length\n"
    );
    assert(
        result.test_only_dynamic_array_bounds_check_ir[1] ==
        "  %dynamic_array0.append.has_capacity = icmp ult i64 %dynamic_array0.length, %dynamic_array0.capacity\n"
    );
    assert(
        result.test_only_dynamic_array_bounds_check_ir[2] ==
        "  %dynamic_array0.length.within_capacity = icmp ule i64 %dynamic_array0.length, %dynamic_array0.capacity\n"
    );
    assert(result.ir_text.find("%dynamic_array0.index.in_bounds = icmp") == std::string::npos);
    assert(result.test_only_dynamic_array_element_address_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_element_address_ir.front() ==
        "  %dynamic_array0.element.addr = getelementptr i32, ptr %dynamic_array0.data, i64 %dynamic_array0.index\n"
    );
    assert(result.ir_text.find("%dynamic_array0.element.addr = getelementptr") == std::string::npos);
    assert(result.test_only_dynamic_array_element_load_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_element_load_ir.front() ==
        "  %dynamic_array0.element = load i32, ptr %dynamic_array0.element.addr\n"
    );
    assert(result.ir_text.find("%dynamic_array0.element = load") == std::string::npos);
    assert(result.test_only_dynamic_array_element_store_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_element_store_ir.front() ==
        "  store i32 %dynamic_array0.value, ptr %dynamic_array0.element.addr\n"
    );
    assert(result.ir_text.find("store i32 %dynamic_array0.value") == std::string::npos);
    assert(result.test_only_dynamic_array_descriptor_length_update_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_descriptor_length_update_ir.front() ==
        "  %dynamic_array0.next.length = add i64 %dynamic_array0.length, 1\n"
        "  %dynamic_array0.updated = insertvalue { ptr, i64, i64 } %dynamic_array_alloc0, "
        "i64 %dynamic_array0.next.length, 1\n"
    );
    assert(result.ir_text.find("%dynamic_array0.updated = insertvalue") == std::string::npos);
    assert(result.test_only_dynamic_array_descriptor_write_back_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_descriptor_write_back_ir.front() ==
        "  store { ptr, i64, i64 } %dynamic_array0.updated, ptr %dynamic_array0.addr\n"
    );
    assert(result.ir_text.find("store { ptr, i64, i64 } %dynamic_array0.updated") == std::string::npos);
    assert(result.test_only_dynamic_array_append_sequence_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_append_sequence_ir.front() ==
        "  %dynamic_array0.append.has_capacity = icmp ult i64 %dynamic_array0.length, %dynamic_array0.capacity\n"
        "  %dynamic_array0.append.element.addr = getelementptr i32, ptr %dynamic_array0.data, "
        "i64 %dynamic_array0.length\n"
        "  store i32 %dynamic_array0.value, ptr %dynamic_array0.append.element.addr\n"
        "  %dynamic_array0.append.next.length = add i64 %dynamic_array0.length, 1\n"
        "  %dynamic_array0.append.updated = insertvalue { ptr, i64, i64 } %dynamic_array_alloc0, "
        "i64 %dynamic_array0.append.next.length, 1\n"
        "  store { ptr, i64, i64 } %dynamic_array0.append.updated, ptr %dynamic_array0.addr\n"
    );
    assert(result.ir_text.find("%dynamic_array0.append.has_capacity = icmp") == std::string::npos);
    assert(result.test_only_dynamic_array_grow_sequence_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_grow_sequence_ir.front() ==
        "  %dynamic_array0.grow.next.capacity = mul i64 %dynamic_array0.capacity, 2\n"
        "  %dynamic_array0.grown.input = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %dynamic_array_alloc0, ptr %dynamic_array0.grown.input\n"
        "  %dynamic_array0.grown.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_grow("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array0.grown.addr, "
        "ptr byval({ ptr, i64, i64 }) %dynamic_array0.grown.input, "
        "i64 4, i64 %dynamic_array0.grow.next.capacity)\n"
        "  %dynamic_array0.grown = load { ptr, i64, i64 }, ptr %dynamic_array0.grown.addr\n"
        "  store { ptr, i64, i64 } %dynamic_array0.grown, ptr %dynamic_array0.addr\n"
    );
    assert(result.ir_text.find("%dynamic_array0.grown = call") == std::string::npos);
    assert(result.test_only_dynamic_array_append_with_grow_sequence_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_append_with_grow_sequence_ir.front() ==
        "dynamic_array0.append.entry:\n"
        "  %dynamic_array0.append.has_capacity = icmp ult i64 %dynamic_array0.length, %dynamic_array0.capacity\n"
        "  br i1 %dynamic_array0.append.has_capacity, label %dynamic_array0.append.ready, "
        "label %dynamic_array0.grow\n"
        "dynamic_array0.grow:\n"
        "  %dynamic_array0.grow.next.capacity = mul i64 %dynamic_array0.capacity, 2\n"
        "  %dynamic_array0.grown.input = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %dynamic_array_alloc0, ptr %dynamic_array0.grown.input\n"
        "  %dynamic_array0.grown.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_grow("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array0.grown.addr, "
        "ptr byval({ ptr, i64, i64 }) %dynamic_array0.grown.input, "
        "i64 4, i64 %dynamic_array0.grow.next.capacity)\n"
        "  %dynamic_array0.grown = load { ptr, i64, i64 }, ptr %dynamic_array0.grown.addr\n"
        "  store { ptr, i64, i64 } %dynamic_array0.grown, ptr %dynamic_array0.addr\n"
        "  br label %dynamic_array0.append.ready\n"
        "dynamic_array0.append.ready:\n"
        "  %dynamic_array0.active = phi { ptr, i64, i64 } "
        "[ %dynamic_array_alloc0, %dynamic_array0.append.entry ], [ %dynamic_array0.grown, %dynamic_array0.grow ]\n"
        "  %dynamic_array0.active.data = extractvalue { ptr, i64, i64 } %dynamic_array0.active, 0\n"
        "  %dynamic_array0.active.length = extractvalue { ptr, i64, i64 } %dynamic_array0.active, 1\n"
        "  %dynamic_array0.active.append.element.addr = getelementptr i32, ptr %dynamic_array0.active.data, "
        "i64 %dynamic_array0.active.length\n"
        "  store i32 %dynamic_array0.value, ptr %dynamic_array0.active.append.element.addr\n"
        "  %dynamic_array0.active.append.next.length = add i64 %dynamic_array0.active.length, 1\n"
        "  %dynamic_array0.active.append.updated = insertvalue { ptr, i64, i64 } %dynamic_array0.active, "
        "i64 %dynamic_array0.active.append.next.length, 1\n"
        "  store { ptr, i64, i64 } %dynamic_array0.active.append.updated, ptr %dynamic_array0.addr\n"
    );
    assert(result.ir_text.find("dynamic_array0.append.entry") == std::string::npos);
    assert(result.test_only_dynamic_array_cleanup_sequence_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_cleanup_sequence_ir.front() ==
        "  %dynamic_array0.cleanup.data = extractvalue { ptr, i64, i64 } %dynamic_array_alloc0, 0\n"
        "  %dynamic_array0.cleanup.length = extractvalue { ptr, i64, i64 } %dynamic_array_alloc0, 1\n"
        "  %dynamic_array0.cleanup.capacity = extractvalue { ptr, i64, i64 } %dynamic_array_alloc0, 2\n"
        "  br label %dynamic_array0.drop.walk\n"
        "dynamic_array0.drop.walk:\n"
        "  %dynamic_array0.drop.index = phi i64 [ 0, %dynamic_array0.cleanup.entry ], "
        "[ %dynamic_array0.drop.next, %dynamic_array0.drop.body ]\n"
        "  %dynamic_array0.drop.more = icmp ult i64 %dynamic_array0.drop.index, "
        "%dynamic_array0.cleanup.length\n"
        "  br i1 %dynamic_array0.drop.more, label %dynamic_array0.drop.body, label %dynamic_array0.drop.done\n"
        "dynamic_array0.drop.body:\n"
        "  %dynamic_array0.drop.element.addr = getelementptr i32, ptr %dynamic_array0.cleanup.data, "
        "i64 %dynamic_array0.drop.index\n"
        "  ; planned drop for UInt32 at %dynamic_array0.drop.element.addr remains disabled\n"
        "  %dynamic_array0.drop.next = add i64 %dynamic_array0.drop.index, 1\n"
        "  br label %dynamic_array0.drop.walk\n"
        "dynamic_array0.drop.done:\n"
        "  call void @__orison_dynamic_array_deallocate"
        "(ptr %dynamic_array0.cleanup.data, i64 4, i64 %dynamic_array0.cleanup.capacity)\n"
    );
    assert(result.ir_text.find("%dynamic_array0.cleanup.data = extractvalue") == std::string::npos);
    assert(result.test_only_dynamic_array_element_drop_walk_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_element_drop_walk_ir.front() ==
        "  br label %dynamic_array0.drop.walk\n"
        "dynamic_array0.drop.walk:\n"
        "  %dynamic_array0.drop.index = phi i64 [ 0, %dynamic_array0.cleanup.entry ], "
        "[ %dynamic_array0.drop.next, %dynamic_array0.drop.body ]\n"
        "  %dynamic_array0.drop.more = icmp ult i64 %dynamic_array0.drop.index, "
        "%dynamic_array0.cleanup.length\n"
        "  br i1 %dynamic_array0.drop.more, label %dynamic_array0.drop.body, label %dynamic_array0.drop.done\n"
        "dynamic_array0.drop.body:\n"
        "  %dynamic_array0.drop.element.addr = getelementptr i32, ptr %dynamic_array0.cleanup.data, "
        "i64 %dynamic_array0.drop.index\n"
        "  ; planned drop for UInt32 at %dynamic_array0.drop.element.addr remains disabled\n"
        "  %dynamic_array0.drop.next = add i64 %dynamic_array0.drop.index, 1\n"
        "  br label %dynamic_array0.drop.walk\n"
        "dynamic_array0.drop.done:\n"
    );
    assert(result.ir_text.find("planned drop for UInt32") == std::string::npos);
    assert(result.planned_drop_actions.empty());
    assert(result.planned_drop_declarations.empty());
    assert(result.drop_cleanups.empty());
    auto readiness_summary = result.drop_readiness_summary();
    assert(readiness_summary.cleanup_authorized == 0);
    assert(readiness_summary.cleanup_blocked == 0);
    assert(result.drop_readiness_snapshot_report().front().find("cleanup authorizations 0") != std::string::npos);

    auto production_construction = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<UInt32>",
                    .initial_capacity = 4,
                },
            },
            .enable_dynamic_array_construction_lowering = true,
        }
    );

    assert(!production_construction.has_errors());
    assert(production_construction.dynamic_array_construction_plans.size() == 1);
    assert(production_construction.dynamic_array_runtime_operations.size() == 1);
    assert(
        production_construction.dynamic_array_runtime_operations.front() ==
        orison::lowering::DynamicArrayRuntimeOperation::allocate
    );
    assert_ir_contains(
        production_construction,
        "declare void @__orison_dynamic_array_allocate(ptr sret({ ptr, i64, i64 }), i64, i64)"
    );
    assert(production_construction.dynamic_array_allocation_call_ir.size() == 1);
    assert(
        production_construction.dynamic_array_allocation_call_ir.front() ==
        "  %dynamic_array_alloc0.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array_alloc0.addr, i64 4, i64 4)\n"
        "  %dynamic_array_alloc0 = load { ptr, i64, i64 }, ptr %dynamic_array_alloc0.addr\n"
    );
    assert(production_construction.test_only_dynamic_array_allocation_call_ir.empty());
    assert(
        production_construction.ir_text.find(
            "call void @__orison_dynamic_array_allocate"
        ) == std::string::npos
    );

    auto source_construction = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function build_items<T>() -> UInt32\n"
        "    let items: DynamicArray<UInt32> = DynamicArray()\n"
        "    1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
        }
    );

    assert(!source_construction.has_errors());
    assert(source_construction.dynamic_array_construction_plans.size() == 1);
    assert(source_construction.dynamic_array_construction_plans.front().owner_name == "items");
    assert(source_construction.dynamic_array_construction_plans.front().source_type_name == "DynamicArray<UInt32>");
    assert(source_construction.dynamic_array_construction_plans.front().initial_capacity == 0);
    assert(source_construction.dynamic_array_runtime_operations.size() == 1);
    assert(
        source_construction.dynamic_array_runtime_operations.front() ==
        orison::lowering::DynamicArrayRuntimeOperation::allocate
    );
    auto source_plan_report = source_construction.dynamic_array_construction_plan_report();
    assert(source_plan_report.size() == 1);
    assert(
        source_plan_report.front() ==
        "dynamic array construction DynamicArray<UInt32> owner items element UInt32 lowers to i32 "
        "element_size 4 initial_capacity 0 requests __orison_dynamic_array_allocate (metadata only)"
    );
    assert(source_construction.dynamic_array_allocation_call_ir.size() == 1);
    assert(
        source_construction.dynamic_array_allocation_call_ir.front() ==
        "  %dynamic_array_alloc0.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array_alloc0.addr, i64 4, i64 0)\n"
        "  %dynamic_array_alloc0 = load { ptr, i64, i64 }, ptr %dynamic_array_alloc0.addr\n"
    );
    assert(source_construction.test_only_dynamic_array_allocation_call_ir.empty());
    assert_ir_contains(
        source_construction,
        "declare void @__orison_dynamic_array_allocate(ptr sret({ ptr, i64, i64 }), i64, i64)"
    );
    assert(
        source_construction.ir_text.find(
            "call void @__orison_dynamic_array_allocate"
        ) == std::string::npos
    );

    auto placed_source_construction = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let items: DynamicArray<UInt32> = DynamicArray()\n"
        "    var words: DynamicArray<UInt32> = DynamicArray()\n"
        "    0 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
        }
    );

    assert(!placed_source_construction.has_errors());
    assert(placed_source_construction.dynamic_array_construction_plans.size() == 2);
    assert(placed_source_construction.dynamic_array_construction_plans[0].owner_name == "items");
    assert(placed_source_construction.dynamic_array_construction_plans[1].owner_name == "words");
    assert(placed_source_construction.dynamic_array_runtime_operations.size() == 2);
    assert_ir_contains(
        placed_source_construction,
        "declare void @__orison_dynamic_array_allocate(ptr sret({ ptr, i64, i64 }), i64, i64)"
    );
    assert_ir_contains(
        placed_source_construction,
        "  %items.dynamic_array_alloc.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %items.dynamic_array_alloc.addr, i64 4, i64 0)\n"
        "  %items.dynamic_array_alloc = load { ptr, i64, i64 }, ptr %items.dynamic_array_alloc.addr\n"
    );
    assert_ir_contains(
        placed_source_construction,
        "  %items.addr = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %items.dynamic_array_alloc, ptr %items.addr\n"
    );
    assert_ir_contains(
        placed_source_construction,
        "  %words.dynamic_array_alloc.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %words.dynamic_array_alloc.addr, i64 4, i64 0)\n"
        "  %words.dynamic_array_alloc = load { ptr, i64, i64 }, ptr %words.dynamic_array_alloc.addr\n"
    );
    assert_ir_contains(
        placed_source_construction,
        "  %words.addr = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %words.dynamic_array_alloc, ptr %words.addr\n"
    );
    assert(placed_source_construction.ir_text.find("__orison_dynamic_array_grow") == std::string::npos);
    assert(placed_source_construction.ir_text.find("__orison_dynamic_array_deallocate") == std::string::npos);

    auto placed_source_cleanup = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let items: DynamicArray<UInt32> = DynamicArray()\n"
        "    var words: DynamicArray<UInt32> = DynamicArray()\n"
        "    0 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!placed_source_cleanup.has_errors());
    assert(placed_source_cleanup.dynamic_array_runtime_operations.size() == 3);
    assert(
        placed_source_cleanup.dynamic_array_runtime_operations[0] ==
        orison::lowering::DynamicArrayRuntimeOperation::allocate
    );
    assert(
        placed_source_cleanup.dynamic_array_runtime_operations[1] ==
        orison::lowering::DynamicArrayRuntimeOperation::allocate
    );
    assert(
        placed_source_cleanup.dynamic_array_runtime_operations[2] ==
        orison::lowering::DynamicArrayRuntimeOperation::deallocate
    );
    assert_ir_contains(
        placed_source_cleanup,
        "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)"
    );
    assert_ir_contains(
        placed_source_cleanup,
        "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        placed_source_cleanup,
        "  call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 4, "
        "i64 %items.dynamic_array_cleanup0.cleanup.capacity)\n"
    );
    assert_ir_contains(
        placed_source_cleanup,
        "  %words.dynamic_array_cleanup1.descriptor = load { ptr, i64, i64 }, ptr %words.addr\n"
    );
    assert_ir_contains(
        placed_source_cleanup,
        "  call void @__orison_dynamic_array_deallocate(ptr %words.dynamic_array_cleanup1.cleanup.data, i64 4, "
        "i64 %words.dynamic_array_cleanup1.cleanup.capacity)\n"
    );
    auto items_cleanup = placed_source_cleanup.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto words_cleanup = placed_source_cleanup.ir_text.find(
        "call void @__orison_dynamic_array_deallocate",
        items_cleanup + 1
    );
    auto return_instruction = placed_source_cleanup.ir_text.find("ret i32 0");
    assert(items_cleanup != std::string::npos);
    assert(words_cleanup != std::string::npos);
    assert(return_instruction != std::string::npos);
    assert(items_cleanup < words_cleanup);
    assert(words_cleanup < return_instruction);

    auto placed_source_index_read = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let items: DynamicArray<UInt32> = DynamicArray()\n"
        "    items[0]\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
            .enable_dynamic_array_index_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!placed_source_index_read.has_errors());
    assert_ir_contains(
        placed_source_index_read,
        "declare void @__orison_dynamic_array_bounds_failed()"
    );
    assert_ir_contains(
        placed_source_index_read,
        "  %items.dynamic_array_index0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        placed_source_index_read,
        "  %items.dynamic_array_index0.length = extractvalue { ptr, i64, i64 } "
        "%items.dynamic_array_index0.descriptor, 1\n"
    );
    assert_ir_contains(
        placed_source_index_read,
        "  %items.dynamic_array_index0.in_bounds = icmp ult i64 0, %items.dynamic_array_index0.length\n"
    );
    assert_ir_contains(
        placed_source_index_read,
        "  br i1 %items.dynamic_array_index0.in_bounds, label %dynamic_array.index.in_bounds.0, "
        "label %dynamic_array.index.out_of_bounds.0\n"
    );
    assert_ir_contains(
        placed_source_index_read,
        "dynamic_array.index.out_of_bounds.0:\n"
        "  call void @__orison_dynamic_array_bounds_failed()\n"
        "  unreachable\n"
        "dynamic_array.index.in_bounds.0:\n"
    );
    assert_ir_contains(
        placed_source_index_read,
        "  %items.dynamic_array_index0.data = extractvalue { ptr, i64, i64 } "
        "%items.dynamic_array_index0.descriptor, 0\n"
    );
    assert_ir_contains(
        placed_source_index_read,
        "  %items.dynamic_array_index0.element.addr = getelementptr i32, ptr "
        "%items.dynamic_array_index0.data, i64 0\n"
    );
    assert_ir_contains(
        placed_source_index_read,
        "  %items.dynamic_array_index0.value = load i32, ptr %items.dynamic_array_index0.element.addr\n"
    );
    auto index_load = placed_source_index_read.ir_text.find("%items.dynamic_array_index0.value = load i32");
    auto index_cleanup = placed_source_index_read.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto index_return = placed_source_index_read.ir_text.find("ret i32 %items.dynamic_array_index0.value");
    assert(index_load != std::string::npos);
    assert(index_cleanup != std::string::npos);
    assert(index_return != std::string::npos);
    assert(index_load < index_cleanup);
    assert(index_cleanup < index_return);

    auto placed_source_append = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var items: DynamicArray<UInt32> = DynamicArray()\n"
        "    items.push(7 as UInt32)\n"
        "    0 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
            .enable_dynamic_array_append_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!placed_source_append.has_errors());
    assert_ir_contains(
        placed_source_append,
        "declare void @__orison_dynamic_array_grow(ptr sret({ ptr, i64, i64 }), "
        "ptr byval({ ptr, i64, i64 }), i64, i64)"
    );
    assert_ir_contains(
        placed_source_append,
        "  %items.dynamic_array_append0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        placed_source_append,
        "  %items.dynamic_array_append0.has_capacity = icmp ult i64 %items.dynamic_array_append0.length, "
        "%items.dynamic_array_append0.capacity\n"
    );
    assert_ir_contains(
        placed_source_append,
        "dynamic_array.append.grow.0:\n"
        "  %items.dynamic_array_append0.capacity.is_zero = icmp eq i64 %items.dynamic_array_append0.capacity, 0\n"
        "  %items.dynamic_array_append0.doubled.capacity = mul i64 %items.dynamic_array_append0.capacity, 2\n"
        "  %items.dynamic_array_append0.next.capacity = select i1 %items.dynamic_array_append0.capacity.is_zero, "
        "i64 1, i64 %items.dynamic_array_append0.doubled.capacity\n"
    );
    assert_ir_contains(
        placed_source_append,
        "  call void @__orison_dynamic_array_grow("
        "ptr sret({ ptr, i64, i64 }) %items.dynamic_array_append0.grown.addr, "
        "ptr byval({ ptr, i64, i64 }) %items.dynamic_array_append0.grown.input, "
        "i64 4, i64 %items.dynamic_array_append0.next.capacity)\n"
    );
    assert_ir_contains(
        placed_source_append,
        "dynamic_array.append.ready.0:\n"
        "  %items.dynamic_array_append0.active = phi { ptr, i64, i64 } "
    );
    assert_ir_contains(
        placed_source_append,
        "  %items.dynamic_array_append0.active.data = extractvalue "
        "{ ptr, i64, i64 } %items.dynamic_array_append0.active, 0\n"
        "  %items.dynamic_array_append0.active.length = extractvalue "
        "{ ptr, i64, i64 } %items.dynamic_array_append0.active, 1\n"
    );
    assert_ir_contains(
        placed_source_append,
        "dynamic_array.append.ready.0:\n"
    );
    assert_ir_contains(
        placed_source_append,
        "  %items.dynamic_array_append0.element.addr = getelementptr i32, ptr "
        "%items.dynamic_array_append0.active.data, i64 %items.dynamic_array_append0.active.length\n"
    );
    assert_ir_contains(
        placed_source_append,
        "  store i32 7, ptr %items.dynamic_array_append0.element.addr\n"
    );
    assert_ir_contains(
        placed_source_append,
        "  %items.dynamic_array_append0.next.length = add i64 %items.dynamic_array_append0.active.length, 1\n"
    );
    assert_ir_contains(
        placed_source_append,
        "  store { ptr, i64, i64 } %items.dynamic_array_append0.updated, ptr %items.addr\n"
    );
    assert(placed_source_append.ir_text.find("__orison_dynamic_array_capacity_failed") == std::string::npos);

    auto placed_source_append_index = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var items: DynamicArray<UInt32> = DynamicArray()\n"
        "    items.push(7 as UInt32)\n"
        "    items[0]\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
            .enable_dynamic_array_index_lowering = true,
            .enable_dynamic_array_append_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!placed_source_append_index.has_errors());
    assert(placed_source_append_index.dynamic_array_runtime_operations.size() == 4);
    assert(
        placed_source_append_index.dynamic_array_runtime_operations[1] ==
        orison::lowering::DynamicArrayRuntimeOperation::bounds_failed
    );
    assert(
        placed_source_append_index.dynamic_array_runtime_operations[2] ==
        orison::lowering::DynamicArrayRuntimeOperation::grow
    );
    assert_ir_contains(
        placed_source_append_index,
        "  store i32 7, ptr %items.dynamic_array_append0.element.addr\n"
    );
    assert_ir_contains(
        placed_source_append_index,
        "  %items.dynamic_array_index1.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        placed_source_append_index,
        "  %items.dynamic_array_index1.value = load i32, ptr %items.dynamic_array_index1.element.addr\n"
    );
    auto append_index_store = placed_source_append_index.ir_text.find(
        "store i32 7, ptr %items.dynamic_array_append0.element.addr"
    );
    auto append_index_descriptor_reload = placed_source_append_index.ir_text.find(
        "%items.dynamic_array_index1.descriptor = load { ptr, i64, i64 }, ptr %items.addr"
    );
    auto append_index_load = placed_source_append_index.ir_text.find(
        "%items.dynamic_array_index1.value = load i32"
    );
    auto append_index_cleanup = placed_source_append_index.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto append_index_return = placed_source_append_index.ir_text.find(
        "ret i32 %items.dynamic_array_index1.value"
    );
    assert(append_index_store != std::string::npos);
    assert(append_index_descriptor_reload != std::string::npos);
    assert(append_index_load != std::string::npos);
    assert(append_index_cleanup != std::string::npos);
    assert(append_index_return != std::string::npos);
    assert(append_index_store < append_index_descriptor_reload);
    assert(append_index_descriptor_reload < append_index_load);
    assert(append_index_load < append_index_cleanup);
    assert(append_index_cleanup < append_index_return);

    auto placed_source_append_length = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> IntSize\n"
        "    var items: DynamicArray<UInt32> = DynamicArray()\n"
        "    items.push(7 as UInt32)\n"
        "    items.length()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
            .enable_dynamic_array_length_lowering = true,
            .enable_dynamic_array_append_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!placed_source_append_length.has_errors());
    assert(placed_source_append_length.dynamic_array_runtime_operations.size() == 3);
    assert_ir_contains(
        placed_source_append_length,
        "  %items.dynamic_array_length1.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        placed_source_append_length,
        "  %items.dynamic_array_length1.value = extractvalue { ptr, i64, i64 } "
        "%items.dynamic_array_length1.descriptor, 1\n"
    );
    auto append_length_store = placed_source_append_length.ir_text.find(
        "store i32 7, ptr %items.dynamic_array_append0.element.addr"
    );
    auto append_length_descriptor_reload = placed_source_append_length.ir_text.find(
        "%items.dynamic_array_length1.descriptor = load { ptr, i64, i64 }, ptr %items.addr"
    );
    auto append_length_value = placed_source_append_length.ir_text.find(
        "%items.dynamic_array_length1.value = extractvalue { ptr, i64, i64 }"
    );
    auto append_length_cleanup = placed_source_append_length.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto append_length_return = placed_source_append_length.ir_text.find(
        "ret i64 %items.dynamic_array_length1.value"
    );
    assert(append_length_store != std::string::npos);
    assert(append_length_descriptor_reload != std::string::npos);
    assert(append_length_value != std::string::npos);
    assert(append_length_cleanup != std::string::npos);
    assert(append_length_return != std::string::npos);
    assert(append_length_store < append_length_descriptor_reload);
    assert(append_length_descriptor_reload < append_length_value);
    assert(append_length_value < append_length_cleanup);
    assert(append_length_cleanup < append_length_return);

    auto placed_source_append_for = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var items: DynamicArray<UInt32> = DynamicArray()\n"
        "    var total = 0 as UInt32\n"
        "    items.push(7 as UInt32)\n"
        "    items.push(9 as UInt32)\n"
        "    for item in items\n"
        "        total = total + item\n"
        "    total\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
            .enable_dynamic_array_for_lowering = true,
            .enable_dynamic_array_append_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!placed_source_append_for.has_errors());
    assert(placed_source_append_for.dynamic_array_runtime_operations.size() == 3);
    assert_ir_contains(
        placed_source_append_for,
        "  %items.sequence_for2.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        placed_source_append_for,
        "for.condition.2:\n"
        "  %items.sequence_for2.index = phi i64 [ 0, %dynamic_array.append.ready.1 ], "
        "[ %items.sequence_for2.next.index, %for.continue.2 ]\n"
    );
    assert_ir_contains(
        placed_source_append_for,
        "  %items.sequence_for2.more = icmp ult i64 %items.sequence_for2.index, "
        "%items.sequence_for2.length\n"
    );
    assert_ir_contains(
        placed_source_append_for,
        "  %items.sequence_for2.element.addr = getelementptr i32, ptr %items.sequence_for2.data, "
        "i64 %items.sequence_for2.index\n"
    );
    assert_ir_contains(
        placed_source_append_for,
        "  %items.sequence_for2.value = load i32, ptr %items.sequence_for2.element.addr\n"
    );
    auto for_descriptor = placed_source_append_for.ir_text.find("%items.sequence_for2.descriptor");
    auto for_condition = placed_source_append_for.ir_text.find("for.condition.2:");
    auto for_load = placed_source_append_for.ir_text.find("%items.sequence_for2.value = load i32");
    auto for_continue = placed_source_append_for.ir_text.find("for.continue.2:");
    auto for_cleanup = placed_source_append_for.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto for_return = placed_source_append_for.ir_text.find("ret i32 %tmp");
    assert(for_descriptor != std::string::npos);
    assert(for_condition != std::string::npos);
    assert(for_load != std::string::npos);
    assert(for_continue != std::string::npos);
    assert(for_cleanup != std::string::npos);
    assert(for_return != std::string::npos);
    assert(for_descriptor < for_condition);
    assert(for_condition < for_load);
    assert(for_load < for_continue);
    assert(for_continue < for_cleanup);
    assert(for_cleanup < for_return);

    auto placed_source_while_append_for = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var items: DynamicArray<UInt32> = DynamicArray()\n"
        "    var total = 0 as UInt32\n"
        "    items.push(4 as UInt32)\n"
        "    while total < 1 as UInt32\n"
        "        for item in items\n"
        "            total = total + item\n"
        "    total\n",
        orison::lowering::LlvmIrEmissionOptions {
            .enable_dynamic_array_construction_lowering = true,
            .enable_dynamic_array_for_lowering = true,
            .enable_dynamic_array_append_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!placed_source_while_append_for.has_errors());
    assert(placed_source_while_append_for.ir_text.find("while.condition.") != std::string::npos);
    assert(placed_source_while_append_for.ir_text.find("for.condition.") != std::string::npos);
    assert(
        placed_source_while_append_for.ir_text.find(
            "getelementptr i32, ptr %items.sequence_for"
        ) != std::string::npos
    );
}

void test_collects_test_only_dynamic_array_element_drop_readiness_metadata() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_dynamic_array_drop_readiness_metadata.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .initial_capacity = 2,
                },
            },
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );

    assert(!result.has_errors());
    assert(result.dynamic_array_construction_plans.size() == 1);
    assert(result.planned_drop_actions.size() == 1);
    assert(result.planned_drop_actions.front().capture_name == "dynamic_array0.element");
    assert(result.planned_drop_actions.front().source_type_name == "Payload");
    assert(result.planned_drop_actions.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_actions.front().field_index == 0);
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(!result.planned_drop_declarations.front().emit_declaration);
    assert(result.drop_cleanups.size() == 1);
    assert(result.drop_cleanups.front().cleanup_symbol_name == "__orison_dynamic_array_cleanup.0");
    assert(!orison::lowering::drop_calls_enabled(result.drop_cleanups.front()));
    auto action_report = result.planned_drop_action_report();
    assert(action_report.size() == 1);
    assert(
        action_report.front() ==
        "planned drop action __orison_drop.Payload for capture dynamic_array0.element: Payload "
        "field 0 (metadata only)"
    );
    auto readiness_report = result.drop_readiness_snapshot_report();
    assert(readiness_report.size() == 2);
    assert(
        readiness_report[0] ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 0 cleanup authorizations 1"
    );
    assert(
        readiness_report[1] ==
        "cleanup readiness __orison_dynamic_array_cleanup.0 blocked semantic blockers 1 missing declarations 1"
    );
    auto readiness_summary = result.drop_readiness_summary();
    assert(readiness_summary.cleanup_authorized == 0);
    assert(readiness_summary.cleanup_blocked == 1);
    auto relation_report = result.drop_readiness_relation_report();
    assert(relation_report.size() == 3);
    assert(
        relation_report[0] ==
        "drop readiness relation __orison_dynamic_array_cleanup.0 blocked semantic blockers 1 "
        "emitted declarations 0 missing declarations 1"
    );
    assert(
        relation_report[1] ==
        "drop readiness relation semantic blocker __orison_drop.Payload for Payload "
        "capture dynamic_array0.element field 0"
    );
    assert(
        relation_report[2] ==
        "drop readiness relation missing declaration __orison_drop.Payload for Payload "
        "capture dynamic_array0.element field 0"
    );
    assert(result.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);
    assert(result.ir_text.find("declare void @__orison_drop.Payload") == std::string::npos);
}

void test_derives_dynamic_array_element_cleanup_from_semantic_descriptor_origin() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_descriptor_cleanup_metadata.or";
    auto source =
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n";
    auto semantic_result = orison::semantics::SemanticAnalysisResult {};
    semantic_result.dynamic_array_descriptor_origins.push_back(
        orison::semantics::DynamicArrayDescriptorOrigin {
            .owner_name = "items",
            .source_type_name = "DynamicArray<Payload>",
            .element_source_type_name = "Payload",
            .line = 8,
        }
    );
    semantic_result.planned_drop_sites.push_back(
        orison::semantics::PlannedDropSite {
            .source_type_name = "Payload",
            .abi_symbol_name = "__orison_drop.Payload",
            .owner_name = "items.element",
            .site_line = 8,
        }
    );

    auto blocked = lower_source_with_semantics(
        path,
        source,
        semantic_result,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_render_dynamic_array_descriptor_load_cleanup_sequences = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );

    assert(!blocked.has_errors());
    assert(blocked.dynamic_array_construction_plans.empty());
    assert(blocked.dynamic_array_runtime_operations.empty());
    assert(blocked.dynamic_array_descriptor_cleanup_plans.size() == 1);
    assert(blocked.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_name == "%items.addr");
    assert(
        blocked.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_status ==
        orison::lowering::DynamicArrayDescriptorStorageStatus::predicted_owner_local
    );
    auto cleanup_report = blocked.dynamic_array_descriptor_cleanup_plan_report();
    assert(cleanup_report.size() == 1);
    assert(
        cleanup_report.front() ==
        "dynamic array descriptor cleanup DynamicArray<Payload> owner items element Payload "
        "lowers to %record.Payload descriptor %items.addr predicted element_size 8 (metadata only)"
    );
    auto obligation_report = blocked.dynamic_array_cleanup_obligation_report();
    assert(obligation_report.size() == 1);
    assert(
        obligation_report.front() ==
        "dynamic array cleanup obligation __orison_dynamic_array_cleanup.0 owner items "
        "source DynamicArray<Payload> element Payload descriptor %items.addr origin line 8 actions 1 "
        "descriptor deallocation required (metadata only)"
    );
    auto sequence_report = blocked.dynamic_array_cleanup_sequence_plan_report();
    assert(sequence_report.size() == 1);
    assert(
        sequence_report.front() ==
        "dynamic array cleanup sequence __orison_dynamic_array_cleanup.0 owner items phases "
        "[load descriptor] [drop initialized elements] [deallocate descriptor storage] (metadata only)"
    );
    auto verification_report = blocked.dynamic_array_cleanup_sequence_verification_report();
    assert(verification_report.size() == 1);
    assert(
        verification_report.front() ==
        "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed (metadata only)"
    );
    assert(blocked.planned_drop_actions.size() == 1);
    assert(blocked.planned_drop_actions.front().capture_name == "items.element");
    assert(blocked.planned_drop_actions.front().source_type_name == "Payload");
    assert(blocked.drop_cleanups.size() == 1);
    assert(blocked.drop_cleanups.front().cleanup_symbol_name == "__orison_dynamic_array_cleanup.0");
    auto blocked_readiness = blocked.drop_readiness_snapshot_report();
    assert(blocked_readiness.size() == 2);
    assert(
        blocked_readiness[1] ==
        "cleanup readiness __orison_dynamic_array_cleanup.0 blocked semantic blockers 1 missing declarations 1"
    );
    assert(blocked.test_only_dynamic_array_element_drop_walk_ir.size() == 1);
    assert(blocked.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.size() == 1);
    assert(
        blocked.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.front().find(
            "  %dynamic_array0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        blocked.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.front().find(
            "  call void @__orison_dynamic_array_deallocate(ptr %dynamic_array0.cleanup.data, i64 8, "
            "i64 %dynamic_array0.cleanup.capacity)\n"
        ) != std::string::npos
    );
    assert(
        blocked.test_only_dynamic_array_element_drop_walk_ir.front().find(
            "planned drop for Payload at %dynamic_array0.drop.element.addr remains disabled"
        ) != std::string::npos
    );

    auto authorized = lower_source_with_semantics(
        path,
        source,
        semantic_result,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = semantic_result.planned_drop_sites.front(),
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!authorized.has_errors());
    assert(authorized.dynamic_array_construction_plans.empty());
    assert(authorized.dynamic_array_runtime_operations.empty());
    auto authorized_summary = authorized.drop_readiness_summary();
    assert(authorized_summary.semantic_authorized == 1);
    assert(authorized_summary.emitted_declarations == 1);
    assert(authorized_summary.cleanup_authorized == 1);
    assert(authorized_summary.cleanup_blocked == 0);
    auto authorized_action_report = authorized.planned_drop_action_report();
    assert(authorized_action_report.size() == 1);
    assert(
        authorized_action_report.front() ==
        "planned drop action __orison_drop.Payload for capture items.element: Payload field 0 "
        "discovered at line 8 (metadata only)"
    );
    assert(authorized.ir_text.find("declare void @__orison_drop.Payload") != std::string::npos);
    assert(authorized.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);
    assert(authorized.ir_text.find("__orison_dynamic_array_allocate") == std::string::npos);
    assert(authorized.ir_text.find("%items.addr = alloca { ptr, i64, i64 }") == std::string::npos);
    assert(authorized.ir_text.find("%dynamic_array0.descriptor = load { ptr, i64, i64 }") == std::string::npos);
    assert(authorized.ir_text.find("call void @__orison_dynamic_array_deallocate") == std::string::npos);
}

void test_derives_dynamic_array_deallocation_only_cleanup_from_scalar_descriptor_origin() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_scalar_descriptor_cleanup_metadata.or";
    auto source =
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n";
    auto semantic_result = orison::semantics::SemanticAnalysisResult {};
    semantic_result.dynamic_array_descriptor_origins.push_back(
        orison::semantics::DynamicArrayDescriptorOrigin {
            .owner_name = "items",
            .source_type_name = "DynamicArray<UInt32>",
            .element_source_type_name = "UInt32",
            .line = 4,
        }
    );

    auto result = lower_source_with_semantics(
        path,
        source,
        semantic_result,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_render_dynamic_array_descriptor_load_cleanup_sequences = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );

    assert(!result.has_errors());
    assert(result.dynamic_array_construction_plans.empty());
    assert(result.dynamic_array_runtime_operations.empty());
    assert(result.dynamic_array_descriptor_cleanup_plans.size() == 1);
    assert(result.dynamic_array_descriptor_cleanup_plans.front().element_source_type_name == "UInt32");
    assert(result.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_name == "%items.addr");
    assert(
        result.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_status ==
        orison::lowering::DynamicArrayDescriptorStorageStatus::predicted_owner_local
    );
    auto obligation_report = result.dynamic_array_cleanup_obligation_report();
    assert(obligation_report.size() == 1);
    assert(
        obligation_report.front() ==
        "dynamic array cleanup obligation __orison_dynamic_array_cleanup.0 owner items "
        "source DynamicArray<UInt32> element UInt32 descriptor %items.addr origin line 4 actions 0 "
        "descriptor deallocation required (metadata only)"
    );
    auto sequence_report = result.dynamic_array_cleanup_sequence_plan_report();
    assert(sequence_report.size() == 1);
    assert(
        sequence_report.front() ==
        "dynamic array cleanup sequence __orison_dynamic_array_cleanup.0 owner items phases "
        "[load descriptor] [deallocate descriptor storage] (metadata only)"
    );
    auto verification_report = result.dynamic_array_cleanup_sequence_verification_report();
    assert(verification_report.size() == 1);
    assert(
        verification_report.front() ==
        "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.0 passed (metadata only)"
    );
    assert(result.planned_drop_actions.empty());
    assert(result.planned_drop_declarations.empty());
    assert(result.drop_cleanups.size() == 1);
    assert(result.drop_cleanups.front().cleanup_symbol_name == "__orison_dynamic_array_cleanup.0");
    assert(result.drop_cleanups.front().actions.empty());
    assert(result.drop_cleanups.front().requires_descriptor_deallocation);
    auto cleanup_plan_report =
        orison::lowering::format_concurrency_drop_cleanup_plan(result.drop_cleanups.front());
    assert(cleanup_plan_report.size() == 1);
    assert(
        cleanup_plan_report.front() ==
        "drop cleanup plan __orison_dynamic_array_cleanup.0 actions 0 descriptor deallocation required "
        "drop calls disabled (metadata only)"
    );
    auto readiness = result.drop_readiness_snapshot_report();
    assert(readiness.size() == 2);
    assert(
        readiness[0] ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 0 cleanup authorizations 1"
    );
    assert(readiness[1] == "cleanup readiness __orison_dynamic_array_cleanup.0 authorized");
    auto summary = result.drop_readiness_summary();
    assert(summary.semantic_authorized == 0);
    assert(summary.emitted_declarations == 0);
    assert(summary.cleanup_authorized == 1);
    assert(summary.cleanup_blocked == 0);
    auto relation = result.drop_readiness_relation_report();
    assert(relation.size() == 1);
    assert(
        relation.front() ==
        "drop readiness relation __orison_dynamic_array_cleanup.0 authorized semantic blockers 0 "
        "emitted declarations 0 missing declarations 0"
    );
    assert(result.test_only_dynamic_array_element_drop_walk_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_element_drop_walk_ir.front().find(
            "planned drop for UInt32 at %dynamic_array0.drop.element.addr remains disabled"
        ) != std::string::npos
    );
    assert(result.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.size() == 1);
    assert(
        result.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.front() ==
        "  %dynamic_array0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        "  %dynamic_array0.cleanup.data = extractvalue { ptr, i64, i64 } %dynamic_array0.descriptor, 0\n"
        "  %dynamic_array0.cleanup.length = extractvalue { ptr, i64, i64 } %dynamic_array0.descriptor, 1\n"
        "  %dynamic_array0.cleanup.capacity = extractvalue { ptr, i64, i64 } %dynamic_array0.descriptor, 2\n"
        "  call void @__orison_dynamic_array_deallocate(ptr %dynamic_array0.cleanup.data, i64 4, "
        "i64 %dynamic_array0.cleanup.capacity)\n"
    );
    assert(result.ir_text.find("%dynamic_array0.descriptor = load { ptr, i64, i64 }") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_dynamic_array_deallocate") == std::string::npos);
}

void test_binds_test_only_dynamic_array_parameter_descriptor_origin() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_parameter_descriptor_binding.or";
    auto source = std::string {
        "package demo.dynamicarray\n"
        "\n"
        "function use_items(items: DynamicArray<UInt32>) -> UInt32\n"
        "    1 as UInt32\n"
    };

    auto rejected = lower_source(path, source);
    assert(rejected.has_errors());
    assert(
        rejected.render(path.string()).find(
            "lowering does not yet support this function parameter type"
        ) != std::string::npos
    );

    auto production_signature_bound = lower_source(
        path,
        source,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .enable_dynamic_array_parameter_descriptors = true,
        }
    );

    assert(!production_signature_bound.has_errors());
    assert_ir_contains(production_signature_bound, "define i32 @use_items({ ptr, i64, i64 } %items)");
    assert_ir_contains(production_signature_bound, "  %items.addr = alloca { ptr, i64, i64 }");
    assert_ir_contains(production_signature_bound, "  store { ptr, i64, i64 } %items, ptr %items.addr");
    assert(production_signature_bound.dynamic_array_descriptor_cleanup_plans.size() == 1);
    assert(
        production_signature_bound.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_status ==
        orison::lowering::DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor
    );
    assert(production_signature_bound.ir_text.find("call void @__orison_dynamic_array_deallocate") == std::string::npos);

    auto owned_parameter_source = std::string {
        "package demo.dynamicarray\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
        "    1 as UInt32\n"
    };
    auto production_owned_rejected = lower_source(
        path,
        owned_parameter_source,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .enable_dynamic_array_parameter_descriptors = true,
        }
    );
    assert(production_owned_rejected.has_errors());
    assert(
        production_owned_rejected.render(path.string()).find(
            "lowering DynamicArray parameter 'items' with owned element type Payload requires ownership/drop proof "
            "before production lowering"
        ) != std::string::npos
    );

    auto bound = lower_source(
        path,
        source,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_render_dynamic_array_descriptor_load_cleanup_sequences = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );

    assert(!bound.has_errors());
    assert_ir_contains(bound, "define i32 @use_items({ ptr, i64, i64 } %items)");
    assert_ir_contains(bound, "  %items.addr = alloca { ptr, i64, i64 }");
    assert_ir_contains(bound, "  store { ptr, i64, i64 } %items, ptr %items.addr");
    assert(bound.dynamic_array_descriptor_cleanup_plans.size() == 1);
    assert(bound.dynamic_array_descriptor_cleanup_plans.front().owner_name == "items");
    assert(bound.dynamic_array_descriptor_cleanup_plans.front().source_type_name == "DynamicArray<UInt32>");
    assert(bound.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_name == "%items.addr");
    assert(
        bound.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_status ==
        orison::lowering::DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor
    );
    auto cleanup_report = bound.dynamic_array_descriptor_cleanup_plan_report();
    assert(cleanup_report.size() == 1);
    assert(
        cleanup_report.front() ==
        "dynamic array descriptor cleanup DynamicArray<UInt32> owner items element UInt32 "
        "lowers to i32 descriptor %items.addr bound element_size 4 (metadata only)"
    );
    assert(bound.planned_drop_actions.empty());
    assert(bound.drop_cleanups.size() == 1);
    assert(bound.drop_cleanups.front().requires_descriptor_deallocation);
    auto summary = bound.drop_readiness_summary();
    assert(summary.cleanup_authorized == 1);
    assert(summary.cleanup_blocked == 0);
    assert(bound.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.size() == 1);
    assert(
        bound.test_only_dynamic_array_descriptor_load_cleanup_sequence_ir.front().find(
            "  %dynamic_array0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(bound.ir_text.find("call void @__orison_dynamic_array_deallocate") == std::string::npos);

    auto cleanup_emitted = lower_source(
        path,
        source,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .enable_dynamic_array_parameter_descriptors = true,
            .enable_dynamic_array_cleanup_emission = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );

    assert(!cleanup_emitted.has_errors());
    assert(cleanup_emitted.dynamic_array_runtime_operations.size() == 1);
    assert(
        cleanup_emitted.dynamic_array_runtime_operations.front() ==
        orison::lowering::DynamicArrayRuntimeOperation::deallocate
    );
    assert_ir_contains(
        cleanup_emitted,
        "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)"
    );
    assert_ir_contains(
        cleanup_emitted,
        "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        cleanup_emitted,
        "  call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 4, "
        "i64 %items.dynamic_array_cleanup0.cleanup.capacity)\n"
    );
    auto cleanup_call = cleanup_emitted.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto return_instruction = cleanup_emitted.ir_text.find("ret i32 1");
    assert(cleanup_call != std::string::npos);
    assert(return_instruction != std::string::npos);
    assert(cleanup_call < return_instruction);

    auto production_parameter_length = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function use_words(words: DynamicArray<UInt32>) -> IntSize\n"
        "    words.length()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .enable_dynamic_array_parameter_descriptors = true,
            .enable_dynamic_array_length_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!production_parameter_length.has_errors());
    assert_ir_contains(production_parameter_length, "define i64 @use_words({ ptr, i64, i64 } %words)");
    assert_ir_contains(
        production_parameter_length,
        "  %words.dynamic_array_length0.descriptor = load { ptr, i64, i64 }, ptr %words.addr\n"
    );
    assert_ir_contains(
        production_parameter_length,
        "  %words.dynamic_array_length0.value = extractvalue { ptr, i64, i64 } "
        "%words.dynamic_array_length0.descriptor, 1\n"
    );
    auto parameter_length_load =
        production_parameter_length.ir_text.find("%words.dynamic_array_length0.value = extractvalue");
    auto parameter_length_cleanup =
        production_parameter_length.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto parameter_length_return =
        production_parameter_length.ir_text.find("ret i64 %words.dynamic_array_length0.value");
    assert(parameter_length_load != std::string::npos);
    assert(parameter_length_cleanup != std::string::npos);
    assert(parameter_length_return != std::string::npos);
    assert(parameter_length_load < parameter_length_cleanup);
    assert(parameter_length_cleanup < parameter_length_return);

    auto production_parameter_index = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function use_words(words: DynamicArray<UInt32>) -> UInt32\n"
        "    words[0]\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .enable_dynamic_array_parameter_descriptors = true,
            .enable_dynamic_array_index_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!production_parameter_index.has_errors());
    assert_ir_contains(production_parameter_index, "define i32 @use_words({ ptr, i64, i64 } %words)");
    assert_ir_contains(
        production_parameter_index,
        "  %words.dynamic_array_index0.descriptor = load { ptr, i64, i64 }, ptr %words.addr\n"
    );
    assert_ir_contains(
        production_parameter_index,
        "  %words.dynamic_array_index0.in_bounds = icmp ult i64 0, %words.dynamic_array_index0.length\n"
    );
    assert_ir_contains(
        production_parameter_index,
        "  %words.dynamic_array_index0.value = load i32, ptr %words.dynamic_array_index0.element.addr\n"
    );
    auto parameter_index_load =
        production_parameter_index.ir_text.find("%words.dynamic_array_index0.value = load i32");
    auto parameter_index_cleanup =
        production_parameter_index.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto parameter_index_return =
        production_parameter_index.ir_text.find("ret i32 %words.dynamic_array_index0.value");
    assert(parameter_index_load != std::string::npos);
    assert(parameter_index_cleanup != std::string::npos);
    assert(parameter_index_return != std::string::npos);
    assert(parameter_index_load < parameter_index_cleanup);
    assert(parameter_index_cleanup < parameter_index_return);

    auto production_parameter_for = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function sum_words(words: DynamicArray<UInt32>) -> UInt32\n"
        "    var total = 0 as UInt32\n"
        "    for word in words\n"
        "        total = total + word\n"
        "    total\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .enable_dynamic_array_parameter_descriptors = true,
            .enable_dynamic_array_for_lowering = true,
            .enable_dynamic_array_cleanup_emission = true,
        }
    );

    assert(!production_parameter_for.has_errors());
    assert_ir_contains(production_parameter_for, "define i32 @sum_words({ ptr, i64, i64 } %words)");
    assert_ir_contains(
        production_parameter_for,
        "  %words.sequence_for0.descriptor = load { ptr, i64, i64 }, ptr %words.addr\n"
    );
    assert_ir_contains(
        production_parameter_for,
        "for.condition.0:\n"
        "  %words.sequence_for0.index = phi i64 [ 0, %entry ], "
        "[ %words.sequence_for0.next.index, %for.continue.0 ]\n"
    );
    assert_ir_contains(
        production_parameter_for,
        "  %words.sequence_for0.element.addr = getelementptr i32, ptr %words.sequence_for0.data, "
        "i64 %words.sequence_for0.index\n"
    );
    assert_ir_contains(
        production_parameter_for,
        "  %words.sequence_for0.value = load i32, ptr %words.sequence_for0.element.addr\n"
    );
    auto parameter_for_descriptor =
        production_parameter_for.ir_text.find("%words.sequence_for0.descriptor");
    auto parameter_for_condition = production_parameter_for.ir_text.find("for.condition.0:");
    auto parameter_for_load =
        production_parameter_for.ir_text.find("%words.sequence_for0.value = load i32");
    auto parameter_for_continue = production_parameter_for.ir_text.find("for.continue.0:");
    auto parameter_for_cleanup =
        production_parameter_for.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto parameter_for_return = production_parameter_for.ir_text.rfind("ret i32 %tmp");
    assert(parameter_for_descriptor != std::string::npos);
    assert(parameter_for_condition != std::string::npos);
    assert(parameter_for_load != std::string::npos);
    assert(parameter_for_continue != std::string::npos);
    assert(parameter_for_cleanup != std::string::npos);
    assert(parameter_for_return != std::string::npos);
    assert(parameter_for_descriptor < parameter_for_condition);
    assert(parameter_for_condition < parameter_for_load);
    assert(parameter_for_load < parameter_for_continue);
    assert(parameter_for_continue < parameter_for_cleanup);
    assert(parameter_for_cleanup < parameter_for_return);
}

void test_emits_authorized_owned_dynamic_array_parameter_cleanup() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_owned_dynamic_array_parameter_cleanup.or";
    auto source = std::string {
        "package demo.dynamicarray\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
        "    1 as UInt32\n"
    };
    auto options = orison::lowering::LlvmIrEmissionOptions {
        .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        .test_only_enable_dynamic_array_parameter_descriptors = true,
        .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
    };

    auto unauthorized = lower_source(path, source, options);
    assert(!unauthorized.has_errors());
    assert(unauthorized.dynamic_array_descriptor_cleanup_plans.size() == 1);
    assert(
        unauthorized.dynamic_array_descriptor_cleanup_plans.front().descriptor_storage_status ==
        orison::lowering::DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor
    );
    assert(unauthorized.drop_readiness_summary().cleanup_blocked == 1);
    assert(unauthorized.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);
    assert(unauthorized.ir_text.find("call void @__orison_dynamic_array_deallocate") == std::string::npos);

    options.semantic_drop_lowering_authorizations = {
        orison::semantics::DropLoweringAuthorization {
            .site = orison::semantics::PlannedDropSite {
                .source_type_name = "Payload",
                .abi_symbol_name = "__orison_drop.Payload",
                .owner_name = "items.element",
                .site_line = 6,
            },
            .semantic_resolved = true,
            .source_drop_lowering_enabled = true,
            .authorized = true,
        },
    };
    auto authorized = lower_source(path, source, options);
    assert(!authorized.has_errors());
    assert(authorized.planned_drop_declarations.size() == 1);
    assert(authorized.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(authorized.planned_drop_declarations.front().emit_declaration);
    assert(authorized.drop_readiness_summary().cleanup_authorized == 1);
    assert(authorized.drop_readiness_summary().cleanup_blocked == 0);
    assert_ir_contains(authorized, "declare void @__orison_drop.Payload(ptr)");
    assert_ir_contains(authorized, "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)");
    assert_ir_contains(
        authorized,
        "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        authorized,
        "  %items.dynamic_array_cleanup0.drop.element.addr = getelementptr %record.Payload, "
        "ptr %items.dynamic_array_cleanup0.cleanup.data, i64 %items.dynamic_array_cleanup0.drop.index\n"
    );
    assert_ir_contains(
        authorized,
        "  call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)\n"
    );
    assert_ir_contains(
        authorized,
        "  call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
        "i64 %items.dynamic_array_cleanup0.cleanup.capacity)\n"
    );
    auto drop_call = authorized.ir_text.find("call void @__orison_drop.Payload");
    auto deallocate_call = authorized.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto return_instruction = authorized.ir_text.find("ret i32 1");
    assert(drop_call != std::string::npos);
    assert(deallocate_call != std::string::npos);
    assert(return_instruction != std::string::npos);
    assert(drop_call < deallocate_call);
    assert(deallocate_call < return_instruction);

    auto production_options = orison::lowering::LlvmIrEmissionOptions {
        .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        .enable_dynamic_array_parameter_descriptors = true,
        .enable_dynamic_array_cleanup_emission = true,
        .semantic_drop_lowering_authorizations = options.semantic_drop_lowering_authorizations,
    };
    auto production_authorized = lower_source(path, source, production_options);
    assert(!production_authorized.has_errors());
    assert_ir_contains(production_authorized, "define i32 @use_items({ ptr, i64, i64 } %items)");
    assert_ir_contains(production_authorized, "declare void @__orison_drop.Payload(ptr)");
    assert_ir_contains(
        production_authorized,
        "  call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)\n"
    );
    assert_ir_contains(
        production_authorized,
        "  call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
        "i64 %items.dynamic_array_cleanup0.cleanup.capacity)\n"
    );
    assert(production_authorized.drop_readiness_summary().cleanup_authorized == 1);
    assert(production_authorized.drop_readiness_summary().cleanup_blocked == 0);
}

void test_emits_authorized_owned_local_dynamic_array_cleanup() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_owned_local_dynamic_array_cleanup.or";
    auto source = std::string {
        "package demo.dynamicarray\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function main() -> UInt32\n"
        "    let items: DynamicArray<Payload> = DynamicArray()\n"
        "    1 as UInt32\n"
    };
    auto options = orison::lowering::LlvmIrEmissionOptions {
        .enable_dynamic_array_construction_lowering = true,
        .enable_dynamic_array_cleanup_emission = true,
    };

    auto unauthorized = lower_source(path, source, options);
    assert(!unauthorized.has_errors());
    assert(unauthorized.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);
    assert(unauthorized.ir_text.find("call void @__orison_dynamic_array_deallocate") == std::string::npos);

    options.semantic_drop_lowering_authorizations = {
        orison::semantics::DropLoweringAuthorization {
            .site = orison::semantics::PlannedDropSite {
                .source_type_name = "Payload",
                .abi_symbol_name = "__orison_drop.Payload",
                .owner_name = "items.element",
                .site_line = 7,
            },
            .semantic_resolved = true,
            .source_drop_lowering_enabled = true,
            .authorized = true,
        },
    };
    auto authorized = lower_source(path, source, options);
    assert(!authorized.has_errors());
    assert(authorized.planned_drop_declarations.size() == 1);
    assert(authorized.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(authorized.planned_drop_declarations.front().emit_declaration);
    assert_ir_contains(authorized, "declare void @__orison_drop.Payload(ptr)");
    assert_ir_contains(authorized, "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)");
    assert_ir_contains(
        authorized,
        "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        authorized,
        "  %items.dynamic_array_cleanup0.drop.element.addr = getelementptr %record.Payload, "
        "ptr %items.dynamic_array_cleanup0.cleanup.data, i64 %items.dynamic_array_cleanup0.drop.index\n"
    );
    assert_ir_contains(
        authorized,
        "  call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)\n"
    );
    assert_ir_contains(
        authorized,
        "  call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
        "i64 %items.dynamic_array_cleanup0.cleanup.capacity)\n"
    );
    auto drop_call = authorized.ir_text.find("call void @__orison_drop.Payload");
    auto deallocate_call = authorized.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto return_instruction = authorized.ir_text.find("ret i32 1");
    assert(drop_call != std::string::npos);
    assert(deallocate_call != std::string::npos);
    assert(return_instruction != std::string::npos);
    assert(drop_call < deallocate_call);
    assert(deallocate_call < return_instruction);
}

void test_emits_authorized_owned_dynamic_array_parameter_cleanup_on_guard_failure_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_owned_dynamic_array_parameter_guard_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function use_items(items: DynamicArray<Payload>, flag: Bool) -> UInt32\n"
        "    guard flag else\n"
        "        return 7 as UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_drop = function_ir.find("call void @__orison_drop.Payload");
    auto const first_deallocate = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const guard_return = function_ir.find("ret i32 7");
    auto const second_drop = function_ir.find("call void @__orison_drop.Payload", first_drop + 1);
    auto const second_deallocate =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_deallocate + 1);
    auto const final_return = function_ir.find("ret i32 1");
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(guard_return != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(final_return != std::string::npos);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < guard_return);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < final_return);
}

void test_emits_authorized_owned_dynamic_array_parameter_cleanup_after_if_arm_defer_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_owned_dynamic_array_parameter_if_defer_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function use_items(items: DynamicArray<Payload>, flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        defer\n"
        "            observe(41 as UInt32)\n"
        "        return 7 as UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 9,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const defer_call = function_ir.find("call void @observe(i32 41)");
    auto const first_drop = function_ir.find("call void @__orison_drop.Payload");
    auto const first_deallocate = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const arm_return = function_ir.find("ret i32 7");
    auto const second_drop = function_ir.find("call void @__orison_drop.Payload", first_drop + 1);
    auto const second_deallocate =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_deallocate + 1);
    auto const final_return = function_ir.find("ret i32 1");
    assert(defer_call != std::string::npos);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(arm_return != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(final_return != std::string::npos);
    assert(defer_call < first_drop);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < arm_return);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < final_return);
}

void test_emits_authorized_owned_dynamic_array_parameter_cleanup_on_explicit_unit_returns() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_owned_dynamic_array_parameter_unit_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function use_items(items: DynamicArray<Payload>, flag: Bool) -> Unit\n"
        "    if flag\n"
        "        return\n"
        "    return\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_drop = function_ir.find("call void @__orison_drop.Payload");
    auto const first_deallocate = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const first_return = function_ir.find("ret void");
    auto const second_drop = function_ir.find("call void @__orison_drop.Payload", first_drop + 1);
    auto const second_deallocate =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_deallocate + 1);
    auto const second_return = function_ir.find("ret void", first_return + 1);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(first_return != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(second_return != std::string::npos);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < first_return);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < second_return);
}

void test_emits_authorized_owned_dynamic_array_parameter_cleanup_after_switch_case_defer_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_owned_dynamic_array_parameter_switch_defer_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function use_items(items: DynamicArray<Payload>, value: UInt32) -> UInt32\n"
        "    if value > 0 as UInt32\n"
        "        switch value\n"
        "            1 =>\n"
        "                defer\n"
        "                    observe(42 as UInt32)\n"
        "                return 7 as UInt32\n"
        "            default => return 9 as UInt32\n"
        "    return 1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 9,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const defer_call = function_ir.find("call void @observe(i32 42)");
    auto const first_drop = function_ir.find("call void @__orison_drop.Payload");
    auto const first_deallocate = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const first_return = function_ir.find("ret i32 7");
    auto const second_drop = function_ir.find("call void @__orison_drop.Payload", first_drop + 1);
    auto const second_deallocate =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_deallocate + 1);
    auto const second_return = function_ir.find("ret i32 9");
    auto const third_drop = function_ir.find("call void @__orison_drop.Payload", second_drop + 1);
    auto const third_deallocate =
        function_ir.find("call void @__orison_dynamic_array_deallocate", second_deallocate + 1);
    auto const third_return = function_ir.find("ret i32 1");
    assert(defer_call != std::string::npos);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(first_return != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(second_return != std::string::npos);
    assert(third_drop != std::string::npos);
    assert(third_deallocate != std::string::npos);
    assert(third_return != std::string::npos);
    assert(defer_call < first_drop);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < first_return);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < second_return);
    assert(third_drop < third_deallocate);
    assert(third_deallocate < third_return);
}

void test_emits_dynamic_array_parameter_cleanup_on_explicit_unit_returns() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_parameter_unit_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function use_items(items: DynamicArray<UInt32>, flag: Bool) -> Unit\n"
        "    if flag\n"
        "        return\n"
        "    return\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_cleanup = function_ir.find("call void @__orison_dynamic_array_deallocate");
    assert(first_cleanup != std::string::npos);
    auto const first_return = function_ir.find("ret void");
    assert(first_return != std::string::npos);
    auto const second_cleanup =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_cleanup + 1);
    assert(second_cleanup != std::string::npos);
    auto const second_return = function_ir.find("ret void", first_return + 1);
    assert(second_return != std::string::npos);
    assert(first_cleanup < first_return);
    assert(second_cleanup < second_return);
    assert_ir_contains(
        result,
        "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        result,
        "  %items.dynamic_array_cleanup1.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
}

void test_emits_dynamic_array_parameter_cleanup_on_guard_failure_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_parameter_guard_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function use_items(items: DynamicArray<UInt32>, flag: Bool) -> UInt32\n"
        "    guard flag else\n"
        "        return 7 as UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_cleanup = function_ir.find("call void @__orison_dynamic_array_deallocate");
    assert(first_cleanup != std::string::npos);
    auto const guard_return = function_ir.find("ret i32 7");
    assert(guard_return != std::string::npos);
    auto const second_cleanup =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_cleanup + 1);
    assert(second_cleanup != std::string::npos);
    auto const final_return = function_ir.find("ret i32 1");
    assert(final_return != std::string::npos);
    assert(first_cleanup < guard_return);
    assert(second_cleanup < final_return);
    assert_ir_contains(
        result,
        "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert_ir_contains(
        result,
        "  %items.dynamic_array_cleanup1.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
}

void test_emits_dynamic_array_parameter_cleanup_after_if_arm_defer_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_parameter_if_defer_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function use_items(items: DynamicArray<UInt32>, flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        defer\n"
        "            observe(11 as UInt32)\n"
        "        return 7 as UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const defer_call = function_ir.find("call void @observe(i32 11)");
    auto const first_cleanup = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const arm_return = function_ir.find("ret i32 7");
    auto const second_cleanup =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_cleanup + 1);
    auto const final_return = function_ir.find("ret i32 1");
    assert(defer_call != std::string::npos);
    assert(first_cleanup != std::string::npos);
    assert(arm_return != std::string::npos);
    assert(second_cleanup != std::string::npos);
    assert(final_return != std::string::npos);
    assert(defer_call < first_cleanup);
    assert(first_cleanup < arm_return);
    assert(second_cleanup < final_return);
}

void test_emits_dynamic_array_parameter_cleanup_after_switch_case_defer_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_parameter_switch_defer_return_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function use_items(items: DynamicArray<UInt32>, value: UInt32) -> UInt32\n"
        "    if value > 0 as UInt32\n"
        "        switch value\n"
        "            1 =>\n"
        "                defer\n"
        "                    observe(22 as UInt32)\n"
        "                return 7 as UInt32\n"
        "            default => return 9 as UInt32\n"
        "    return 1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const defer_call = function_ir.find("call void @observe(i32 22)");
    auto const first_cleanup = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const first_return = function_ir.find("ret i32 7");
    auto const second_cleanup =
        function_ir.find("call void @__orison_dynamic_array_deallocate", first_cleanup + 1);
    auto const second_return = function_ir.find("ret i32 9");
    auto const third_cleanup =
        function_ir.find("call void @__orison_dynamic_array_deallocate", second_cleanup + 1);
    auto const third_return = function_ir.find("ret i32 1");
    assert(defer_call != std::string::npos);
    assert(first_cleanup != std::string::npos);
    assert(first_return != std::string::npos);
    assert(second_cleanup != std::string::npos);
    assert(second_return != std::string::npos);
    assert(third_cleanup != std::string::npos);
    assert(third_return != std::string::npos);
    assert(defer_call < first_cleanup);
    assert(first_cleanup < first_return);
    assert(second_cleanup < second_return);
    assert(third_cleanup < third_return);
}

void test_defers_but_delays_dynamic_array_parameter_cleanup_on_loop_break() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_parameter_break_defer_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function use_items(items: DynamicArray<UInt32>, flag: Bool) -> UInt32\n"
        "    repeat\n"
        "        defer\n"
        "            observe(31 as UInt32)\n"
        "        break\n"
        "    while flag\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const defer_call = function_ir.find("call void @observe(i32 31)");
    auto const loop_exit = function_ir.find("repeat.exit.");
    auto const cleanup = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const second_cleanup =
        function_ir.find("call void @__orison_dynamic_array_deallocate", cleanup + 1);
    auto const return_instruction = function_ir.find("ret i32 1");
    assert(defer_call != std::string::npos);
    assert(loop_exit != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(second_cleanup == std::string::npos);
    assert(return_instruction != std::string::npos);
    assert(defer_call < loop_exit);
    assert(loop_exit < cleanup);
    assert(cleanup < return_instruction);
}

void test_defers_but_delays_dynamic_array_parameter_cleanup_on_loop_continue() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_parameter_continue_defer_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.dynamicarray\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function use_items(items: DynamicArray<UInt32>) -> UInt32\n"
        "    repeat\n"
        "        defer\n"
        "            observe(32 as UInt32)\n"
        "        continue\n"
        "    while false\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_derive_dynamic_array_cleanup_from_semantics = true,
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        }
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define i32 @use_items");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const defer_call = function_ir.find("call void @observe(i32 32)");
    auto const cleanup = function_ir.find("call void @__orison_dynamic_array_deallocate");
    auto const second_cleanup =
        function_ir.find("call void @__orison_dynamic_array_deallocate", cleanup + 1);
    auto const return_instruction = function_ir.find("ret i32 1");
    assert(defer_call != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(second_cleanup == std::string::npos);
    assert(return_instruction != std::string::npos);
    assert(defer_call < cleanup);
    assert(cleanup < return_instruction);
}

void test_dynamic_array_element_drop_readiness_requires_semantic_authorization() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_dynamic_array_drop_readiness_authorization.or";
    auto source =
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n";

    auto allowlist_only = lower_source(
        path,
        source,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_declared_drop_source_type_allowlist = {"Payload"},
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .initial_capacity = 2,
                },
            },
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );

    assert(!allowlist_only.has_errors());
    assert(allowlist_only.emitted_drop_declaration_report().size() == 1);
    auto allowlist_summary = allowlist_only.drop_readiness_summary();
    assert(allowlist_summary.semantic_authorized == 0);
    assert(allowlist_summary.cleanup_authorized == 0);
    assert(allowlist_summary.cleanup_blocked == 1);
    auto allowlist_readiness = allowlist_only.drop_readiness_snapshot_report();
    assert(allowlist_readiness.size() == 3);
    assert(allowlist_readiness[1] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(
        allowlist_readiness[2] ==
        "cleanup readiness __orison_dynamic_array_cleanup.0 blocked semantic blockers 1 missing declarations 0"
    );
    assert(allowlist_only.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto authorized = lower_source(
        path,
        source,
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_dynamic_array_construction_requests = {
                orison::lowering::TestOnlyDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .initial_capacity = 2,
                },
            },
            .test_only_render_dynamic_array_element_drop_walks = true,
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "dynamic_array0.element",
                        .site_line = 0,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!authorized.has_errors());
    auto authorized_summary = authorized.drop_readiness_summary();
    assert(authorized_summary.semantic_authorized == 1);
    assert(authorized_summary.emitted_declarations == 1);
    assert(authorized_summary.cleanup_authorized == 1);
    assert(authorized_summary.cleanup_blocked == 0);
    auto authorized_readiness = authorized.drop_readiness_snapshot_report();
    assert(authorized_readiness.size() == 4);
    assert(
        authorized_readiness[0] ==
        "drop readiness snapshot semantic authorizations 1 emitted declarations 1 cleanup authorizations 1"
    );
    assert(authorized_readiness[1] == "semantic readiness __orison_drop.Payload for Payload authorized");
    assert(authorized_readiness[2] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(authorized_readiness[3] == "cleanup readiness __orison_dynamic_array_cleanup.0 authorized");
    assert(authorized.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);
    assert(authorized.ir_text.find("declare void @__orison_drop.Payload") != std::string::npos);
}

void test_emit_carries_semantic_drop_lowering_authorization_metadata() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_semantic_drop_authorization_metadata.or";
    auto site = orison::semantics::PlannedDropSite {
        .source_type_name = "Payload",
        .abi_symbol_name = orison::semantics::drop_abi_symbol_name("Payload"),
        .owner_name = "payload",
        .site_line = 12,
    };
    auto authorization = orison::semantics::DropLoweringAuthorization {
        .site = site,
        .semantic_resolved = true,
        .source_drop_lowering_enabled = false,
        .authorized = false,
    };
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .semantic_drop_lowering_authorizations = {authorization},
        }
    );

    assert(!result.has_errors());
    assert(result.semantic_drop_lowering_authorizations.size() == 1);
    assert(result.semantic_drop_lowering_authorizations.front().site.abi_symbol_name == "__orison_drop.Payload");
    assert(result.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!result.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!result.semantic_drop_lowering_authorizations.front().authorized);
    assert(result.ir_text.find("__orison_drop.Payload") == std::string::npos);
}

void test_emit_let_bound_uint32_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_let_bound_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value = 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_mutable_uint32_assignment_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 1 as UInt32\n"
        "    value = value + 2 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 1, ptr %value.addr\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = add i32 %tmp0, 2\n"
        "  store i32 %tmp1, ptr %value.addr\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp2\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_mutable_uint32_compound_assignment_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_uint32_compound.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 6 as UInt32\n"
        "    value += 2 as UInt32\n"
        "    value *= 3 as UInt32\n"
        "    value -= 4 as UInt32\n"
        "    value /= 2 as UInt32\n"
        "    value %= 5 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 6, ptr %value.addr\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = add i32 %tmp0, 2\n"
        "  store i32 %tmp1, ptr %value.addr\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = mul i32 %tmp2, 3\n"
        "  store i32 %tmp3, ptr %value.addr\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = sub i32 %tmp4, 4\n"
        "  store i32 %tmp5, ptr %value.addr\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  %tmp7 = udiv i32 %tmp6, 2\n"
        "  store i32 %tmp7, ptr %value.addr\n"
        "  %tmp8 = load i32, ptr %value.addr\n"
        "  %tmp9 = urem i32 %tmp8, 5\n"
        "  store i32 %tmp9, ptr %value.addr\n"
        "  %tmp10 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp10\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_var_initializer_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_var_initializer.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    var value = -27 as Int32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %value.addr = alloca i32\n"
        "  store i32 %tmp0, ptr %value.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_let_initializer_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_let_initializer.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    let value = -27 as Int32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %value = add i32 0, %tmp0\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_negative_uint32_cast_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_cast.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    -1 as UInt32\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
    );
}

void test_emit_negative_int32_cast_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_cast_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    -27 as Int32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_ternary_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_ternary_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    flag ? -27 as Int32 : 4 as Int32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %ternary.then.0], [4, %ternary.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_negative_uint32_ternary_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_ternary_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    flag ? -1 as UInt32 : 4 as UInt32\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
    );
}

void test_emit_mutable_int32_compound_assignment_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_int32_compound.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    var value = -27 as Int32\n"
        "    value /= 4 as Int32\n"
        "    value %= 5 as Int32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %value.addr = alloca i32\n"
        "  store i32 %tmp0, ptr %value.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  %tmp2 = sdiv i32 %tmp1, 4\n"
        "  store i32 %tmp2, ptr %value.addr\n"
        "  %tmp3 = load i32, ptr %value.addr\n"
        "  %tmp4 = srem i32 %tmp3, 5\n"
        "  store i32 %tmp4, ptr %value.addr\n"
        "  %tmp5 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp5\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_bool_compound_assignment() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_bool_compound_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Bool\n"
        "    var value = true\n"
        "    value += false\n"
        "    value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering compound assignment operator '+=' requires an integer assignment target"
    );
}

void test_reject_bool_aggregate_compound_assignment() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_bool_aggregate_compound_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Flags\n"
        "    enabled: Bool\n"
        "\n"
        "function main() -> Unit\n"
        "    var flags = Flags(true)\n"
        "    flags.enabled += false\n"
        "    return\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering compound assignment operator '+=' requires an integer assignment target"
    );
}

void test_emit_mutable_uint32_while_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_uint32_while.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 3\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = add i32 %tmp2, 1\n"
        "  store i32 %tmp3, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp4\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_while_call_statement() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 2 as UInt32\n"
        "        observe(value)\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @observe(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = call i32 @observe(i32 %tmp2)\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = add i32 %tmp4, 1\n"
        "  store i32 %tmp5, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_while_unit_call_statement() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_unit_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 2 as UInt32\n"
        "        observe(value)\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @observe(i32 %value) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  call void @observe(i32 %tmp2)\n"
        "  %tmp3 = load i32, ptr %value.addr\n"
        "  %tmp4 = add i32 %tmp3, 1\n"
        "  store i32 %tmp4, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp5 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp5\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_while_local_bindings() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_local_bindings.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 2 as UInt32\n"
        "        let step = 1 as UInt32\n"
        "        var local: UInt32 = value + step\n"
        "        value = local + step\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %step = add i32 0, 1\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = add i32 %tmp2, %step\n"
        "  %local.addr = alloca i32\n"
        "  store i32 %tmp3, ptr %local.addr\n"
        "  %tmp4 = load i32, ptr %local.addr\n"
        "  %tmp5 = add i32 %tmp4, %step\n"
        "  store i32 %tmp5, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_restore_outer_binding_after_while_local_shadow() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_while_local_shadow.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let step = 5 as UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        let step = 1 as UInt32\n"
        "        value = step\n"
        "        break\n"
        "    step\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %step = add i32 0, 5\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 1\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %step.1 = add i32 0, 1\n"
        "  store i32 %step.1, ptr %value.addr\n"
        "  br label %while.exit.0\n"
        "while.exit.0:\n"
        "  ret i32 %step\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminal_while_break() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_break.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "        break\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 3\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = add i32 %tmp2, 1\n"
        "  store i32 %tmp3, ptr %value.addr\n"
        "  br label %while.exit.0\n"
        "while.exit.0:\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp4\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminal_while_continue() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_continue.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "        continue\n"
        "    value\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("  br label %while.condition.0\nwhile.exit.0:\n") != std::string::npos);
}

void test_emit_defer_cleanup_on_early_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_defer_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_then_return(value: UInt32, flag: Bool) -> Unit\n"
        "    defer\n"
        "        observe(value)\n"
        "    if flag\n"
        "        return\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @cleanup_then_return");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_call = function_ir.find("call void @observe(i32 %value)");
    assert(first_call != std::string::npos);
    auto const second_call = function_ir.find("call void @observe(i32 %value)", first_call + 1);
    assert(second_call != std::string::npos);
    auto const first_ret = function_ir.find("ret void");
    assert(first_ret != std::string::npos);
    auto const second_ret = function_ir.find("ret void", first_ret + 1);
    assert(second_ret != std::string::npos);
    assert(first_call < first_ret);
    assert(second_call < second_ret);
    std::filesystem::remove(path);
}

void test_emit_conditional_while_continue_and_break() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_conditional_while_control.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 5 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "        if value == 2 as UInt32\n"
        "            continue\n"
        "        if value == 4 as UInt32\n"
        "            break\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 5\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = add i32 %tmp2, 1\n"
        "  store i32 %tmp3, ptr %value.addr\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = icmp eq i32 %tmp4, 2\n"
        "  br i1 %tmp5, label %if.then.1, label %if.merge.1\n"
        "if.then.1:\n"
        "  br label %while.condition.0\n"
        "if.merge.1:\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  %tmp7 = icmp eq i32 %tmp6, 4\n"
        "  br i1 %tmp7, label %if.then.2, label %if.merge.2\n"
        "if.then.2:\n"
        "  br label %while.exit.0\n"
        "if.merge.2:\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp8 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp8\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_switch_in_while_body() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_switch_in_while_body.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(value: UInt32) -> UInt32\n"
        "    var current = value\n"
        "    while current < 2 as UInt32\n"
        "        switch current\n"
        "            0 => break\n"
        "            default => current = current + 1 as UInt32\n"
        "        current = current + 1 as UInt32\n"
        "    current\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp2, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  br label %while.exit.0\n"
        "switch.default.1:\n"
        "  %tmp3 = load i32, ptr %current.addr\n"
        "  %tmp4 = add i32 %tmp3, 1\n"
        "  store i32 %tmp4, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  %tmp5 = load i32, ptr %current.addr\n"
        "  %tmp6 = add i32 %tmp5, 1\n"
        "  store i32 %tmp6, ptr %current.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp7 = load i32, ptr %current.addr\n"
        "  ret i32 %tmp7\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminating_while_if_else() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_terminating_while_if_else.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        if flag\n"
        "            break\n"
        "        else\n"
        "            continue\n"
        "    value\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("if.merge.1:") == std::string::npos);
    assert(result.ir_text.find("if.then.1:\n  br label %while.exit.0\n") != std::string::npos);
    assert(
        result.ir_text.find("if.else.1:\n  br label %while.condition.0\n") != std::string::npos
    );
}

void test_emit_nested_while_if_control() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_while_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(first: Bool, second: Bool) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        if first\n"
        "            if second\n"
        "                break\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("if.then.1:\n  br i1 %second, label %if.then.2") != std::string::npos);
    assert(result.ir_text.find("if.then.2:\n  br label %while.exit.0\n") != std::string::npos);
    assert(result.ir_text.find("if.merge.2:\n  br label %if.merge.1\n") != std::string::npos);
}

void test_emit_nested_while_loop_body() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_while_loop.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var outer = 0 as UInt32\n"
        "    var inner = 0 as UInt32\n"
        "    while outer < 2 as UInt32\n"
        "        while inner < 3 as UInt32\n"
        "            inner = inner + 1 as UInt32\n"
        "        outer = outer + 1 as UInt32\n"
        "    outer + inner\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("while.condition.0:") != std::string::npos);
    assert(result.ir_text.find("while.condition.1:") != std::string::npos);
    assert(result.ir_text.find("br i1 %tmp1, label %while.body.0, label %while.exit.0") != std::string::npos);
    assert(result.ir_text.find("br i1 %tmp3, label %while.body.1, label %while.exit.1") != std::string::npos);
}

void test_emit_nested_repeat_in_while_body() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_repeat_in_while.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var outer = 0 as UInt32\n"
        "    var inner = 0 as UInt32\n"
        "    while outer < 2 as UInt32\n"
        "        repeat\n"
        "            inner = inner + 1 as UInt32\n"
        "        while inner < 3 as UInt32\n"
        "        outer = outer + 1 as UInt32\n"
        "    outer + inner\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("while.condition.0:") != std::string::npos);
    assert(result.ir_text.find("repeat.body.1:") != std::string::npos);
    assert(result.ir_text.find("repeat.condition.1:") != std::string::npos);
    assert(result.ir_text.find("label %repeat.body.1, label %repeat.exit.1") != std::string::npos);
}

void test_emit_nested_for_in_while_body() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_for_in_while.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(values: Array<UInt32, 2>) -> UInt32\n"
        "    var outer = 0 as UInt32\n"
        "    var total = 0 as UInt32\n"
        "    while outer < 1 as UInt32\n"
        "        for item in [1 as UInt32, 2 as UInt32]\n"
        "            total = total + item\n"
        "        for item in values\n"
        "            total = total + item\n"
        "        outer = outer + 1 as UInt32\n"
        "    total\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("while.condition.0:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.1.0:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.1.1:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.2.0:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.2.1:") != std::string::npos);
    assert(result.ir_text.find("extractvalue [2 x i32] %values, 0") != std::string::npos);
}

void test_emit_helper_and_method_returned_fixed_array_for_iterables() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_helper_method_array_for.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function make_values() -> Array<UInt32, 3>\n"
        "    [1 as UInt32, 2 as UInt32, 3 as UInt32]\n"
        "\n"
        "extend UInt32\n"
        "    function triple(this: shared This) -> Array<UInt32, 3>\n"
        "        [this, this, this]\n"
        "\n"
        "function main() -> UInt32\n"
        "    var total = 0 as UInt32\n"
        "    for item in make_values()\n"
        "        total = total + item\n"
        "    let base: UInt32 = 4 as UInt32\n"
        "    for item in base.triple()\n"
        "        total = total + item\n"
        "    total\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("call [3 x i32] @make_values()") != std::string::npos);
    assert(result.ir_text.find("call [3 x i32] @method.UInt32.triple") != std::string::npos);
    assert(result.ir_text.find("for.iteration.0.0:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.1.0:") != std::string::npos);
    assert(result.ir_text.find("extractvalue [3 x i32]") != std::string::npos);
    assert(result.ir_text.find("ret i32 %tmp") != std::string::npos);
}

void test_reject_nonterminal_while_loop_control() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nonterminal_while_control.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        continue\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not support statements after terminating loop control"
    );
}

void test_reject_unsupported_while_body_statement() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_while_body.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value\n"
        "    value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering while body only supports local bindings, mutable-local assignments, "
        "call statements, loop control, nested if/switch statements, nested while/repeat/for statements, and unsafe blocks"
    );
}

void test_emit_scalar_extension_method_definition() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_scalar_extension_method.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function identity(this: shared This) -> UInt32\n"
        "        this\n"
        "\n"
        "extend Device\n"
        "    function scale(value: UInt32) -> UInt32\n"
        "        value + 1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 0\n"
        "}\n"
        "\n"
        "define i32 @method.UInt32.identity(i32 %this) {\n"
        "entry:\n"
        "  ret i32 %this\n"
        "}\n"
        "\n"
        "define i32 @method.Device.scale(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_scalar_method_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_scalar_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function negative(this: shared This) -> Int32\n"
        "        -27 as Int32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_defines_method(result, "i32", "method.Int32.negative", "i32 %this");
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_scalar_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_scalar_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function invalid(this: shared This) -> UInt32\n"
        "        -1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_record_receiver_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_record_receiver_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function negative(this: shared This) -> Int32\n"
        "        -27 as Int32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "i32", "method.SignedBox.negative", "%record.SignedBox %this");
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_record_receiver_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_record_receiver_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function invalid(this: shared This) -> UInt32\n"
        "        -1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_record_constructor_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend Int32\n"
        "    function make_box(this: shared This) -> SignedBox\n"
        "        SignedBox(-27 as Int32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "%record.SignedBox", "method.Int32.make_box", "i32 %this");
    assert_inserts_lowered_int32_tmp_into_aggregate(result, "%record.SignedBox");
    assert_returns_lowered_aggregate_tmp(result, "%record.SignedBox");
}

void test_emit_negative_int32_ternary_record_constructor_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend Int32\n"
        "    function choose_box(this: shared This, flag: Bool) -> SignedBox\n"
        "        flag ? SignedBox(-27 as Int32) : SignedBox(4 as Int32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "%record.SignedBox", "method.Int32.choose_box", "i32 %this, i1 %flag");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_inserts_lowered_int32_tmp_into_aggregate(result, "%record.SignedBox");
    assert_ir_contains(result, " = phi %record.SignedBox [%tmp");
    assert_returns_lowered_aggregate_tmp(result, "%record.SignedBox");
}

void test_emit_generic_record_constructor_let_lowering() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_constructor_let.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: Box<UInt32> = Box(1 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 1, 0");
    assert_ir_contains(result, "%box.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, "store %record.Box_UInt32_ %tmp0, ptr %box.addr");
}

void test_emit_negative_int32_array_literal_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function pair(this: shared This) -> Array<Int32, 2>\n"
        "        [-27 as Int32, 4 as Int32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_defines_method(result, "[2 x i32]", "method.Int32.pair", "i32 %this");
    assert_inserts_lowered_int32_tmp_into_aggregate(result, "[2 x i32]");
    assert_ir_contains(result, " = insertvalue [2 x i32] %tmp");
    assert_returns_lowered_aggregate_tmp(result, "[2 x i32]");
}

void test_emit_negative_int32_ternary_array_literal_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_ternary_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function choose_pair(this: shared This, flag: Bool) -> Array<Int32, 2>\n"
        "        flag ? [-27 as Int32, 4 as Int32] : [4 as Int32, 5 as Int32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_defines_method(result, "[2 x i32]", "method.Int32.choose_pair", "i32 %this, i1 %flag");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_inserts_lowered_int32_tmp_into_aggregate(result, "[2 x i32]");
    assert_ir_contains(result, " = phi [2 x i32] [%tmp");
    assert_returns_lowered_aggregate_tmp(result, "[2 x i32]");
}

void test_reject_negative_uint32_record_constructor_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function make_box(this: shared This) -> UnsignedBox\n"
        "        UnsignedBox(-1 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_record_constructor_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function choose_box(this: shared This, flag: Bool) -> UnsignedBox\n"
        "        flag ? UnsignedBox(-1 as UInt32) : UnsignedBox(4 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_array_literal_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_ternary_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function choose_pair(this: shared This, flag: Bool) -> Array<UInt32, 2>\n"
        "        flag ? [-1 as UInt32, 4 as UInt32] : [4 as UInt32, 5 as UInt32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_array_literal_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function pair(this: shared This) -> Array<UInt32, 2>\n"
        "        [-1 as UInt32, 4 as UInt32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_final_if_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_final_if_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function choose(this: shared This, flag: Bool) -> Int32\n"
        "        if flag\n"
        "            -27 as Int32\n"
        "        else\n"
        "            4 as Int32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_scalar_control_flow_method_return(
        result,
        "method.Int32.choose",
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_final_switch_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_final_switch_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function choose(this: shared This, flag: Bool) -> Int32\n"
        "        switch flag\n"
        "            true => -27 as Int32\n"
        "            default => 4 as Int32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_scalar_control_flow_method_return(
        result,
        "method.Int32.choose",
        FinalControlFlowKind::switch_expression
    );
}

void test_reject_negative_uint32_final_if_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_final_if_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function choose(this: shared This, flag: Bool) -> UInt32\n"
        "        if flag\n"
        "            -1 as UInt32\n"
        "        else\n"
        "            4 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_method_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_final_switch_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function choose(this: shared This, flag: Bool) -> UInt32\n"
        "        switch flag\n"
        "            true => -1 as UInt32\n"
        "            default => 4 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_final_if_record_constructor_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_if_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend Int32\n"
        "    function choose_box(this: shared This, flag: Bool) -> SignedBox\n"
        "        if flag\n"
        "            SignedBox(-27 as Int32)\n"
        "        else\n"
        "            SignedBox(4 as Int32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_emits_negative_int32_aggregate_control_flow_method_return(
        result,
        "%record.SignedBox",
        "method.Int32.choose_box",
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_final_switch_record_constructor_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_switch_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend Int32\n"
        "    function choose_box(this: shared This, flag: Bool) -> SignedBox\n"
        "        switch flag\n"
        "            true => SignedBox(-27 as Int32)\n"
        "            default => SignedBox(4 as Int32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_emits_negative_int32_aggregate_control_flow_method_return(
        result,
        "%record.SignedBox",
        "method.Int32.choose_box",
        FinalControlFlowKind::switch_expression
    );
}

void test_emit_negative_int32_final_if_array_literal_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_if_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function choose_pair(this: shared This, flag: Bool) -> Array<Int32, 2>\n"
        "        if flag\n"
        "            [-27 as Int32, 4 as Int32]\n"
        "        else\n"
        "            [4 as Int32, 5 as Int32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_aggregate_control_flow_method_return(
        result,
        "[2 x i32]",
        "method.Int32.choose_pair",
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_final_switch_array_literal_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_switch_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function choose_pair(this: shared This, flag: Bool) -> Array<Int32, 2>\n"
        "        switch flag\n"
        "            true => [-27 as Int32, 4 as Int32]\n"
        "            default => [4 as Int32, 5 as Int32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_emits_negative_int32_aggregate_control_flow_method_return(
        result,
        "[2 x i32]",
        "method.Int32.choose_pair",
        FinalControlFlowKind::switch_expression
    );
}

void test_reject_negative_uint32_final_if_record_constructor_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_if_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function choose_box(this: shared This, flag: Bool) -> UnsignedBox\n"
        "        if flag\n"
        "            UnsignedBox(-1 as UInt32)\n"
        "        else\n"
        "            UnsignedBox(4 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_record_constructor_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_switch_record_constructor_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function choose_box(this: shared This, flag: Bool) -> UnsignedBox\n"
        "        switch flag\n"
        "            true => UnsignedBox(-1 as UInt32)\n"
        "            default => UnsignedBox(4 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_if_array_literal_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_if_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function choose_pair(this: shared This, flag: Bool) -> Array<UInt32, 2>\n"
        "        if flag\n"
        "            [-1 as UInt32, 4 as UInt32]\n"
        "        else\n"
        "            [4 as UInt32, 5 as UInt32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_array_literal_method_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_switch_array_literal_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function choose_pair(this: shared This, flag: Bool) -> Array<UInt32, 2>\n"
        "        switch flag\n"
        "            true => [-1 as UInt32, 4 as UInt32]\n"
        "            default => [4 as UInt32, 5 as UInt32]\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_scalar_member_call_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_scalar_member_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this + amount\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    value.scale(2 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  %tmp0 = call i32 @method.UInt32.scale(i32 %value, i32 2)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @method.UInt32.scale(i32 %this, i32 %amount) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %this, %amount\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_scalar_member_call_argument_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function shift(this: shared This, amount: Int32) -> Int32\n"
        "        this + amount\n"
        "\n"
        "function main() -> Int32\n"
        "    let value: Int32 = 1 as Int32\n"
        "    value.shift(-27 as Int32)\n"
    );

    assert_emits_negative_int32_value(result);
    assert_calls_lowered_int32_tmp(result, " = call i32 @method.Int32.shift(i32 %value, ");
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_scalar_member_call_argument() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this + amount\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    value.scale(-1 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_scalar_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function shift(this: shared This, amount: Int32) -> Int32\n"
        "        this + amount\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let value: Int32 = 1 as Int32\n"
        "    value.shift(flag ? -27 as Int32 : 4 as Int32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main(i1 %flag) {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %ternary.then.0], [4, %ternary.else.0]\n"
        "  %tmp2 = call i32 @method.Int32.shift(i32 %value, i32 %tmp1)\n"
        "  ret i32 %tmp2\n"
        "}\n"
        "\n"
        "define i32 @method.Int32.shift(i32 %this, i32 %amount) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %this, %amount\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_negative_uint32_ternary_scalar_member_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this + amount\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    value.scale(flag ? -1 as UInt32 : 4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_record_receiver_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_record_receiver_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function shift(this: shared This, amount: Int32) -> Int32\n"
        "        this.value + amount\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    box.shift(flag ? -27 as Int32 : 4 as Int32)\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "i32", "method.SignedBox.shift", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, " = call i32 @method.SignedBox.shift(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_ternary_record_receiver_member_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_record_receiver_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this.value + amount\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    box.scale(flag ? -1 as UInt32 : 4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_final_if_scalar_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_if_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function shift(this: shared This, amount: Int32) -> Int32\n"
        "        this + amount\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let value: Int32 = 1 as Int32\n"
        "    if flag\n"
        "        value.shift(-27 as Int32)\n"
        "    else\n"
        "        value.shift(4 as Int32)\n"
    );

    assert_emits_negative_int32_control_flow_call_argument_return(
        result,
        " = call i32 @method.Int32.shift(i32 %value, ",
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_final_switch_scalar_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_switch_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function shift(this: shared This, amount: Int32) -> Int32\n"
        "        this + amount\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let value: Int32 = 1 as Int32\n"
        "    switch flag\n"
        "        true => value.shift(-27 as Int32)\n"
        "        default => value.shift(4 as Int32)\n"
    );

    assert_emits_negative_int32_control_flow_call_argument_return(
        result,
        " = call i32 @method.Int32.shift(i32 %value, ",
        FinalControlFlowKind::switch_expression
    );
}

void test_emit_negative_int32_final_if_record_receiver_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_if_record_receiver_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function shift(this: shared This, amount: Int32) -> Int32\n"
        "        this.value + amount\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    if flag\n"
        "        box.shift(-27 as Int32)\n"
        "    else\n"
        "        box.shift(4 as Int32)\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "i32", "method.SignedBox.shift", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "if.then.");
    assert_ir_contains(result, "if.else.");
    assert_ir_contains(result, "if.merge.");
    assert_ir_contains(result, " = call i32 @method.SignedBox.shift(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_final_switch_record_receiver_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_switch_record_receiver_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function shift(this: shared This, amount: Int32) -> Int32\n"
        "        this.value + amount\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    switch flag\n"
        "        true => box.shift(-27 as Int32)\n"
        "        default => box.shift(4 as Int32)\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "i32", "method.SignedBox.shift", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "switch.case.");
    assert_ir_contains(result, "switch.default.");
    assert_ir_contains(result, "switch.merge.");
    assert_ir_contains(result, " = call i32 @method.SignedBox.shift(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_final_if_scalar_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_if_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this + amount\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    if flag\n"
        "        value.scale(-1 as UInt32)\n"
        "    else\n"
        "        value.scale(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_scalar_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_switch_scalar_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this + amount\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    switch flag\n"
        "        true => value.scale(-1 as UInt32)\n"
        "        default => value.scale(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_if_record_receiver_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_if_record_receiver_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this.value + amount\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    if flag\n"
        "        box.scale(-1 as UInt32)\n"
        "    else\n"
        "        box.scale(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_record_receiver_member_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_switch_record_receiver_member_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this.value + amount\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    switch flag\n"
        "        true => box.scale(-1 as UInt32)\n"
        "        default => box.scale(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_scalar_member_call_statement() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_scalar_member_call_statement.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function reset(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        value.reset()\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 1\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  call void @method.UInt32.reset(i32 %tmp2)\n"
        "  %tmp3 = load i32, ptr %value.addr\n"
        "  %tmp4 = add i32 %tmp3, 1\n"
        "  store i32 %tmp4, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp5 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp5\n"
        "}\n"
        "\n"
        "define void @method.UInt32.reset(i32 %this) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_scalar_member_call_statement_argument() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_scalar_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main() -> Int32\n"
        "    let value: Int32 = 1 as Int32\n"
        "    value.observe(-27 as Int32)\n"
        "    value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_calls_lowered_int32_tmp(result, "call void @method.Int32.observe(i32 %value, ");
    assert_ir_contains(result, "ret i32 %value");
}

void test_reject_negative_uint32_scalar_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_scalar_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    value.observe(-1 as UInt32)\n"
        "    value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main() -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    box.observe(-27 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    box.observe(-1 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_scalar_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_scalar_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend Int32\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let value: Int32 = 1 as Int32\n"
        "    value.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_calls_lowered_int32_tmp(result, "call void @method.Int32.observe(i32 %value, ");
    assert_ir_contains(result, "ret i32 %value");
}

void test_reject_negative_uint32_ternary_scalar_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_scalar_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    value.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_while_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_while_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    var index = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "        index = index + 1 as UInt32\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_while_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_while_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    var index = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "        index = index + 1 as UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_repeat_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_repeat_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    repeat\n"
        "        box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    while false\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "repeat.body.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_repeat_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_repeat_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    repeat\n"
        "        box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    while false\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_for_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_for_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    for item in [0 as UInt32]\n"
        "        box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_for_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_for_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    for item in [0 as UInt32]\n"
        "        box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_if_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_if_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    if flag\n"
        "        box.observe(-27 as Int32)\n"
        "    else\n"
        "        box.observe(4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "if.then.");
    assert_ir_contains(result, "if.else.");
    assert_ir_contains(result, "if.merge.");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_if_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_if_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    if flag\n"
        "        box.observe(-1 as UInt32)\n"
        "    else\n"
        "        box.observe(4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_switch_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_switch_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    switch flag\n"
        "        true => box.observe(-27 as Int32)\n"
        "        default => box.observe(4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "switch.case.");
    assert_ir_contains(result, "switch.default.");
    assert_ir_contains(result, "switch.merge.");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_switch_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_switch_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    switch flag\n"
        "        true => box.observe(-1 as UInt32)\n"
        "        default => box.observe(4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_if_defer_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_if_defer_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> Int32\n"
        "    if flag\n"
        "        let box: SignedBox = SignedBox(1 as Int32)\n"
        "        defer\n"
        "            box.observe(choose ? -27 as Int32 : 4 as Int32)\n"
        "    else\n"
        "        let box: SignedBox = SignedBox(2 as Int32)\n"
        "        defer\n"
        "            box.observe(4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "if.then.");
    assert_ir_contains(result, "if.else.");
    assert_ir_contains(result, "if.merge.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_if_defer_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_if_defer_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> UInt32\n"
        "    if flag\n"
        "        let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "        defer\n"
        "            box.observe(choose ? -1 as UInt32 : 4 as UInt32)\n"
        "    else\n"
        "        let box: UnsignedBox = UnsignedBox(2 as UInt32)\n"
        "        defer\n"
        "            box.observe(4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_switch_defer_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_switch_defer_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> Int32\n"
        "    switch flag\n"
        "        true =>\n"
        "            let box: SignedBox = SignedBox(1 as Int32)\n"
        "            defer\n"
        "                box.observe(choose ? -27 as Int32 : 4 as Int32)\n"
        "        default =>\n"
        "            let box: SignedBox = SignedBox(2 as Int32)\n"
        "            defer\n"
        "                box.observe(4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "switch.case.");
    assert_ir_contains(result, "switch.default.");
    assert_ir_contains(result, "switch.merge.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_switch_defer_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_switch_defer_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> UInt32\n"
        "    switch flag\n"
        "        true =>\n"
        "            let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "            defer\n"
        "                box.observe(choose ? -1 as UInt32 : 4 as UInt32)\n"
        "        default =>\n"
        "            let box: UnsignedBox = UnsignedBox(2 as UInt32)\n"
        "            defer\n"
        "                box.observe(4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_while_if_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_while_if_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> Int32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        if flag\n"
        "            let box: SignedBox = SignedBox(1 as Int32)\n"
        "            defer\n"
        "                box.observe(choose ? -27 as Int32 : 4 as Int32)\n"
        "            index = index + 1 as UInt32\n"
        "            continue\n"
        "        else\n"
        "            index = index + 1 as UInt32\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "if.then.");
    assert_ir_contains(result, "if.else.");
    assert_ir_contains(result, "while.condition.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_while_if_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_while_if_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> UInt32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        if flag\n"
        "            let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "            defer\n"
        "                box.observe(choose ? -1 as UInt32 : 4 as UInt32)\n"
        "            index = index + 1 as UInt32\n"
        "            continue\n"
        "        else\n"
        "            index = index + 1 as UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_while_switch_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_while_switch_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> Int32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        switch flag\n"
        "            true =>\n"
        "                let box: SignedBox = SignedBox(1 as Int32)\n"
        "                defer\n"
        "                    box.observe(choose ? -27 as Int32 : 4 as Int32)\n"
        "                break\n"
        "            default =>\n"
        "                index = index + 1 as UInt32\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "switch.case.");
    assert_ir_contains(result, "switch.default.");
    assert_ir_contains(result, "while.exit.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_while_switch_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_while_switch_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> UInt32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        switch flag\n"
        "            true =>\n"
        "                let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "                defer\n"
        "                    box.observe(choose ? -1 as UInt32 : 4 as UInt32)\n"
        "                break\n"
        "            default =>\n"
        "                index = index + 1 as UInt32\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_defer_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_defer_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    defer\n"
        "        box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_defer_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_defer_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    defer\n"
        "        box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_defer_return_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_defer_return_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    defer\n"
        "        box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    return 0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_defer_return_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_defer_return_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    defer\n"
        "        box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    return 0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    while flag\n"
        "        let box: SignedBox = SignedBox(1 as Int32)\n"
        "        defer\n"
        "            box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "        break\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "while.exit.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    while flag\n"
        "        let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "        defer\n"
        "            box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "        break\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        let box: SignedBox = SignedBox(1 as Int32)\n"
        "        defer\n"
        "            box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "        index = index + 1 as UInt32\n"
        "        continue\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "while.condition.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "        defer\n"
        "            box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "        index = index + 1 as UInt32\n"
        "        continue\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_unsafe_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_unsafe_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    while flag\n"
        "        unsafe\n"
        "            let box: SignedBox = SignedBox(1 as Int32)\n"
        "            defer\n"
        "                box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "            break\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "while.exit.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_unsafe_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_unsafe_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    while flag\n"
        "        unsafe\n"
        "            let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "            defer\n"
        "                box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "            break\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_unsafe_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_unsafe_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        unsafe\n"
        "            let box: SignedBox = SignedBox(1 as Int32)\n"
        "            defer\n"
        "                box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "            index = index + 1 as UInt32\n"
        "            continue\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "while.condition.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_unsafe_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_unsafe_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        unsafe\n"
        "            let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "            defer\n"
        "                box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "            index = index + 1 as UInt32\n"
        "            continue\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_for_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_for_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    for item in [0 as UInt32]\n"
        "        let box: SignedBox = SignedBox(1 as Int32)\n"
        "        defer\n"
        "            box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "        continue\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_for_defer_continue_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_for_defer_continue_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    for item in [0 as UInt32]\n"
        "        let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "        defer\n"
        "            box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "        continue\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_repeat_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_repeat_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    repeat\n"
        "        let box: SignedBox = SignedBox(1 as Int32)\n"
        "        defer\n"
        "            box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "        break\n"
        "    while flag\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_record_receiver_cleanup_call(result);
    assert_ir_contains(result, "repeat.body.");
    assert_ir_contains(result, "repeat.exit.");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_repeat_defer_break_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_repeat_defer_break_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    repeat\n"
        "        let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "        defer\n"
        "            box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "        break\n"
        "    while flag\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_guard_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_guard_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    guard flag else\n"
        "        box.observe(choose ? -27 as Int32 : 4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_guard_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_guard_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    guard flag else\n"
        "        box.observe(choose ? -1 as UInt32 : 4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_ternary_unsafe_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_unsafe_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "extend SignedBox\n"
        "    function observe(this: shared This, amount: Int32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(1 as Int32)\n"
        "    unsafe\n"
        "        box.observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_defines_method(result, "void", "method.SignedBox.observe", "%record.SignedBox %this, i32 %amount");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, "call void @method.SignedBox.observe(%record.SignedBox %tmp");
    assert_ir_contains(result, ", i32 %tmp");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_unsafe_record_receiver_member_call_statement_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_unsafe_record_receiver_member_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "extend UnsignedBox\n"
        "    function observe(this: shared This, amount: UInt32) -> Unit\n"
        "        return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(1 as UInt32)\n"
        "    unsafe\n"
        "        box.observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_scalar_call_statements() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_scalar_call_statements.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function reset(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    observe(value)\n"
        "    value.reset()\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @observe(i32 %value) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  call void @observe(i32 %value)\n"
        "  call void @method.UInt32.reset(i32 %value)\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
        "define void @method.UInt32.reset(i32 %this) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_scalar_unit_call_statements() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_scalar_unit_call_statements.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function reset(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function observe_then_return(value: UInt32) -> Unit\n"
        "    observe(value)\n"
        "    value.reset()\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @observe(i32 %value) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define void @observe_then_return(i32 %value) {\n"
        "entry:\n"
        "  call void @observe(i32 %value)\n"
        "  call void @method.UInt32.reset(i32 %value)\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define void @method.UInt32.reset(i32 %this) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_ternary_unit_call_statement_argument() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_ternary_unit_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: Int32) -> Unit\n"
        "    return\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    observe(flag ? -27 as Int32 : 4 as Int32)\n"
        "    0 as Int32\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_calls_lowered_int32_tmp(result, "call void @observe(");
    assert_ir_contains(result, "ret i32 0");
}

void test_reject_negative_uint32_ternary_unit_call_statement_argument() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_ternary_unit_call_statement_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    observe(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    0 as UInt32\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_uint32_add_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_add.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let left = 1 as UInt32\n"
        "    let right = 2 as UInt32\n"
        "    left + right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %left = add i32 0, 1\n"
        "  %right = add i32 0, 2\n"
        "  %tmp0 = add i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_uint32_arithmetic_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_arithmetic.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let left = 8 as UInt32\n"
        "    let right = 2 as UInt32\n"
        "    left - right * right / right + left % 3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %left = add i32 0, 8\n"
        "  %right = add i32 0, 2\n"
        "  %tmp0 = mul i32 %right, %right\n"
        "  %tmp1 = udiv i32 %tmp0, %right\n"
        "  %tmp2 = sub i32 %left, %tmp1\n"
        "  %tmp3 = urem i32 %left, 3\n"
        "  %tmp4 = add i32 %tmp2, %tmp3\n"
        "  ret i32 %tmp4\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_int32_division_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_int32_division.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function divide(left: Int32, right: Int32) -> Int32\n"
        "    left / right\n"
        "\n"
        "function remainder(left: Int32, right: Int32) -> Int32\n"
        "    left % right\n"
        "\n"
        "function negate(value: Int32) -> Int32\n"
        "    -value\n"
        "\n"
        "function main() -> Int32\n"
        "    divide(8 as Int32, 2 as Int32) + remainder(9 as Int32, 4 as Int32) + negate(5 as Int32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @divide(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = sdiv i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @remainder(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = srem i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @negate(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, %value\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @divide(i32 8, i32 2)\n"
        "  %tmp1 = call i32 @remainder(i32 9, i32 4)\n"
        "  %tmp2 = add i32 %tmp0, %tmp1\n"
        "  %tmp3 = call i32 @negate(i32 5)\n"
        "  %tmp4 = add i32 %tmp2, %tmp3\n"
        "  ret i32 %tmp4\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_uint32_comparison_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_comparisons.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function is_equal(left: UInt32, right: UInt32) -> Bool\n"
        "    left == right\n"
        "\n"
        "function is_different(left: UInt32, right: UInt32) -> Bool\n"
        "    left != right\n"
        "\n"
        "function is_less(left: UInt32, right: UInt32) -> Bool\n"
        "    left < right\n"
        "\n"
        "function is_at_most(left: UInt32, right: UInt32) -> Bool\n"
        "    left <= right\n"
        "\n"
        "function is_greater(left: UInt32, right: UInt32) -> Bool\n"
        "    left > right\n"
        "\n"
        "function is_at_least(left: UInt32, right: UInt32) -> Bool\n"
        "    left >= right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @is_equal(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp eq i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_different(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ne i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_less(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ult i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_most(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ule i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_greater(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ugt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_least(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp uge i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_int32_comparison_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_int32_comparisons.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function is_less(left: Int32, right: Int32) -> Bool\n"
        "    left < right\n"
        "\n"
        "function is_at_most(left: Int32, right: Int32) -> Bool\n"
        "    left <= right\n"
        "\n"
        "function is_greater(left: Int32, right: Int32) -> Bool\n"
        "    left > right\n"
        "\n"
        "function is_at_least(left: Int32, right: Int32) -> Bool\n"
        "    left >= right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @is_less(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp slt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_most(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sle i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_greater(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sgt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_least(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sge i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_boolean_expression_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_boolean_expressions.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function enabled() -> Bool\n"
        "    true\n"
        "\n"
        "function disabled() -> Bool\n"
        "    false\n"
        "\n"
        "function invert(flag: Bool) -> Bool\n"
        "    not flag\n"
        "\n"
        "function allowed(left: Bool, right: Bool) -> Bool\n"
        "    left or right\n"
        "\n"
        "function same(left: Bool, right: Bool) -> Bool\n"
        "    left == right\n"
        "\n"
        "function different(left: Bool, right: Bool) -> Bool\n"
        "    left != right\n"
        "\n"
        "function in_range(value: UInt32) -> Bool\n"
        "    value >= 1 as UInt32 and value <= 10 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @enabled() {\n"
        "entry:\n"
        "  ret i1 1\n"
        "}\n"
        "\n"
        "define i1 @disabled() {\n"
        "entry:\n"
        "  ret i1 0\n"
        "}\n"
        "\n"
        "define i1 @invert(i1 %flag) {\n"
        "entry:\n"
        "  %tmp0 = xor i1 %flag, true\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @allowed(i1 %left, i1 %right) {\n"
        "entry:\n"
        "  %tmp0 = or i1 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @same(i1 %left, i1 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp eq i1 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @different(i1 %left, i1 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ne i1 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @in_range(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = icmp uge i32 %value, 1\n"
        "  %tmp1 = icmp ule i32 %value, 10\n"
        "  %tmp2 = and i1 %tmp0, %tmp1\n"
        "  ret i1 %tmp2\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_ternary_expression_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_ternary_expressions.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    flag ? 1 as UInt32 : 2 as UInt32\n"
        "\n"
        "function choose_flag(flag: Bool) -> Bool\n"
        "    flag ? true : false\n"
        "\n"
        "function choose_nested(first: Bool, second: Bool) -> UInt32\n"
        "    first ? second ? 1 as UInt32 : 2 as UInt32 : 3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp0 = phi i32 [1, %ternary.then.0], [2, %ternary.else.0]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @choose_flag(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp0 = phi i1 [1, %ternary.then.0], [0, %ternary.else.0]\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @choose_nested(i1 %first, i1 %second) {\n"
        "entry:\n"
        "  br i1 %first, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  br i1 %second, label %ternary.then.1, label %ternary.else.1\n"
        "ternary.then.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.else.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.merge.1:\n"
        "  %tmp0 = phi i32 [1, %ternary.then.1], [2, %ternary.else.1]\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %ternary.merge.1], [3, %ternary.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_if_expression_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_if_expressions.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        1 as UInt32\n"
        "    else\n"
        "        2 as UInt32\n"
        "\n"
        "function choose_return(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        return 3 as UInt32\n"
        "    else\n"
        "        return 4 as UInt32\n"
        "\n"
        "function choose_nested(first: Bool, second: Bool) -> UInt32\n"
        "    if first\n"
        "        second ? 5 as UInt32 : 6 as UInt32\n"
        "    else\n"
        "        7 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp0 = phi i32 [1, %if.then.0], [2, %if.else.0]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @choose_return(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp0 = phi i32 [3, %if.then.0], [4, %if.else.0]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @choose_nested(i1 %first, i1 %second) {\n"
        "entry:\n"
        "  br i1 %first, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br i1 %second, label %ternary.then.1, label %ternary.else.1\n"
        "ternary.then.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.else.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.merge.1:\n"
        "  %tmp0 = phi i32 [5, %ternary.then.1], [6, %ternary.else.1]\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %ternary.merge.1], [7, %if.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_if_branch_local_bindings() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_if_branch_locals.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        let value = 1 as UInt32\n"
        "        let result = value + 1 as UInt32\n"
        "        result\n"
        "    else\n"
        "        let value = 3 as UInt32\n"
        "        return value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  %value = add i32 0, 1\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  %result = add i32 0, %tmp0\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  %value.1 = add i32 0, 3\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp1 = phi i32 [%result, %if.then.0], [%value.1, %if.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_if_mutable_branch_prefixes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_if_mutable_prefixes.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    if flag\n"
        "        value = 1 as UInt32\n"
        "        var local = 10 as UInt32\n"
        "        local + value\n"
        "    else\n"
        "        value = 2 as UInt32\n"
        "        var local = 20 as UInt32\n"
        "        local + value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  store i32 1, ptr %value.addr\n"
        "  %local.addr = alloca i32\n"
        "  store i32 10, ptr %local.addr\n"
        "  %tmp0 = load i32, ptr %local.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  %tmp2 = add i32 %tmp0, %tmp1\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  store i32 2, ptr %value.addr\n"
        "  %local.addr.1 = alloca i32\n"
        "  store i32 20, ptr %local.addr.1\n"
        "  %tmp3 = load i32, ptr %local.addr.1\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = add i32 %tmp3, %tmp4\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp6 = phi i32 [%tmp2, %if.then.0], [%tmp5, %if.else.0]\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_nested_final_if_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_final_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(first: Bool, second: Bool) -> UInt32\n"
        "    if first\n"
        "        let base = 1 as UInt32\n"
        "        if second\n"
        "            let value = base + 1 as UInt32\n"
        "            value\n"
        "        else\n"
        "            let value = base + 2 as UInt32\n"
        "            value\n"
        "    else\n"
        "        4 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %first, i1 %second) {\n"
        "entry:\n"
        "  br i1 %first, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  %base = add i32 0, 1\n"
        "  br i1 %second, label %if.then.1, label %if.else.1\n"
        "if.then.1:\n"
        "  %tmp0 = add i32 %base, 1\n"
        "  %value = add i32 0, %tmp0\n"
        "  br label %if.merge.1\n"
        "if.else.1:\n"
        "  %tmp1 = add i32 %base, 2\n"
        "  %value.1 = add i32 0, %tmp1\n"
        "  br label %if.merge.1\n"
        "if.merge.1:\n"
        "  %tmp2 = phi i32 [%value, %if.then.1], [%value.1, %if.else.1]\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp3 = phi i32 [%tmp2, %if.merge.1], [4, %if.else.0]\n"
        "  ret i32 %tmp3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_integer_switch_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_integer_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(value: UInt32) -> UInt32\n"
        "    switch value\n"
        "        0 => 10 as UInt32\n"
        "        1 =>\n"
        "            let base = 20 as UInt32\n"
        "            base + 1 as UInt32\n"
        "        default =>\n"
        "            let base = 30 as UInt32\n"
        "            return base\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i32 %value) {\n"
        "entry:\n"
        "  switch i32 %value, label %switch.default.0 [\n"
        "    i32 0, label %switch.case.0.0\n"
        "    i32 1, label %switch.case.0.1\n"
        "  ]\n"
        "switch.case.0.0:\n"
        "  br label %switch.merge.0\n"
        "switch.case.0.1:\n"
        "  %base = add i32 0, 20\n"
        "  %tmp0 = add i32 %base, 1\n"
        "  br label %switch.merge.0\n"
        "switch.default.0:\n"
        "  %base.1 = add i32 0, 30\n"
        "  br label %switch.merge.0\n"
        "switch.merge.0:\n"
        "  %tmp1 = phi i32 [10, %switch.case.0.0], [%tmp0, %switch.case.0.1], [%base.1, %switch.default.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_switch_mutable_case_prefixes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_switch_mutable_prefixes.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(selector: UInt32) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    switch selector\n"
        "        0 =>\n"
        "            value = 1 as UInt32\n"
        "            var local = 10 as UInt32\n"
        "            local + value\n"
        "        default =>\n"
        "            value = 2 as UInt32\n"
        "            var local = 20 as UInt32\n"
        "            local + value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i32 %selector) {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  switch i32 %selector, label %switch.default.0 [\n"
        "    i32 0, label %switch.case.0.0\n"
        "  ]\n"
        "switch.case.0.0:\n"
        "  store i32 1, ptr %value.addr\n"
        "  %local.addr = alloca i32\n"
        "  store i32 10, ptr %local.addr\n"
        "  %tmp0 = load i32, ptr %local.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  %tmp2 = add i32 %tmp0, %tmp1\n"
        "  br label %switch.merge.0\n"
        "switch.default.0:\n"
        "  store i32 2, ptr %value.addr\n"
        "  %local.addr.1 = alloca i32\n"
        "  store i32 20, ptr %local.addr.1\n"
        "  %tmp3 = load i32, ptr %local.addr.1\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = add i32 %tmp3, %tmp4\n"
        "  br label %switch.merge.0\n"
        "switch.merge.0:\n"
        "  %tmp6 = phi i32 [%tmp2, %switch.case.0.0], [%tmp5, %switch.default.0]\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_exhaustive_boolean_switch_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_exhaustive_boolean_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    switch flag\n"
        "        true => 1 as UInt32\n"
        "        false => 2 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  switch i1 %flag, label %switch.unreachable.0 [\n"
        "    i1 1, label %switch.case.0.0\n"
        "    i1 0, label %switch.case.0.1\n"
        "  ]\n"
        "switch.case.0.0:\n"
        "  br label %switch.merge.0\n"
        "switch.case.0.1:\n"
        "  br label %switch.merge.0\n"
        "switch.unreachable.0:\n"
        "  unreachable\n"
        "switch.merge.0:\n"
        "  %tmp0 = phi i32 [1, %switch.case.0.0], [2, %switch.case.0.1]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_switch_nested_in_final_if() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_switch_nested_in_final_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    if enabled\n"
        "        switch value\n"
        "            0 => 1 as UInt32\n"
        "            default => 2 as UInt32\n"
        "    else\n"
        "        3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  br i1 %enabled, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  switch i32 %value, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  br label %switch.merge.1\n"
        "switch.default.1:\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  %tmp0 = phi i32 [1, %switch.case.1.0], [2, %switch.default.1]\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %switch.merge.1], [3, %if.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminating_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_terminating_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    if enabled\n"
        "        switch value\n"
        "            0 => return 1 as UInt32\n"
        "            default => return 2 as UInt32\n"
        "    return 3 as UInt32\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("switch.merge") == std::string::npos);
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  switch i32 %value, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret i32 1\n"
        "switch.default.1:\n"
        "  ret i32 2\n"
        "if.merge.0:\n"
        "  ret i32 3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    var current = value\n"
        "    if enabled\n"
        "        switch current\n"
        "            0 => return 1 as UInt32\n"
        "            default => current = current + 1 as UInt32\n"
        "    return 3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp0, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret i32 1\n"
        "switch.default.1:\n"
        "  %tmp1 = load i32, ptr %current.addr\n"
        "  %tmp2 = add i32 %tmp1, 1\n"
        "  store i32 %tmp2, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  ret i32 3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_switch_in_guard_failure() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_switch_in_guard_failure.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    var current = value\n"
        "    guard enabled else\n"
        "        switch current\n"
        "            0 => return 1 as UInt32\n"
        "            default => current = current + 1 as UInt32\n"
        "    current\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br i1 %enabled, label %guard.merge.0, label %guard.failure.0\n"
        "guard.failure.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp0, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret i32 1\n"
        "switch.default.1:\n"
        "  %tmp1 = load i32, ptr %current.addr\n"
        "  %tmp2 = add i32 %tmp1, 1\n"
        "  store i32 %tmp2, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  br label %guard.merge.0\n"
        "guard.merge.0:\n"
        "  %tmp3 = load i32, ptr %current.addr\n"
        "  ret i32 %tmp3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminating_unit_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_terminating_unit_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> Unit\n"
        "    if enabled\n"
        "        switch value\n"
        "            0 => return\n"
        "            default => return\n"
        "    return\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("switch.merge") == std::string::npos);
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  switch i32 %value, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret void\n"
        "switch.default.1:\n"
        "  ret void\n"
        "if.merge.0:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_unit_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_unit_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> Unit\n"
        "    var current = value\n"
        "    if enabled\n"
        "        switch current\n"
        "            0 => return\n"
        "            default => current = current + 1 as UInt32\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp0, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret void\n"
        "switch.default.1:\n"
        "  %tmp1 = load i32, ptr %current.addr\n"
        "  %tmp2 = add i32 %tmp1, 1\n"
        "  store i32 %tmp2, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_zero_argument_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_zero_argument_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function one() -> UInt32\n"
        "    1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    one()\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @one() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @one()\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_zero_argument_function_call_add_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_zero_argument_call_add.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function one() -> UInt32\n"
        "    1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    one() + 2 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @one() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @one()\n"
        "  %tmp1 = add i32 %tmp0, 2\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_single_uint32_parameter_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_single_uint32_parameter_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function increment(value: UInt32) -> UInt32\n"
        "    value + 1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    increment(41 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @increment(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @increment(i32 41)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_generic_record_parameter_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_generic_record_parameter_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function read(box: Box<UInt32>) -> UInt32\n"
        "    box.value\n"
        "\n"
        "function main() -> UInt32\n"
        "    read(Box(7 as UInt32))\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define i32 @read(%record.Box_UInt32_ %box)");
    assert_ir_contains(result, "%box.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, "store %record.Box_UInt32_ %box, ptr %box.addr");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_ir_contains(result, " = call i32 @read(%record.Box_UInt32_ %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_parameter_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_array_parameter_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function read(boxes: Array<Box<UInt32>, 2>) -> UInt32\n"
        "    boxes[0].value\n"
        "\n"
        "function main() -> UInt32\n"
        "    read([Box(7 as UInt32), Box(9 as UInt32)])\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define i32 @read([2 x %record.Box_UInt32_] %boxes)");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Box_UInt32_]");
    assert_ir_contains(result, "store [2 x %record.Box_UInt32_] %boxes, ptr %boxes.addr");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %boxes.addr, i64 0, i64 0");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_ir_contains(result, " = call i32 @read([2 x %record.Box_UInt32_] %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_function_return_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_generic_record_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function make_box() -> Box<UInt32>\n"
        "    Box(7 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    make_box().value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define %record.Box_UInt32_ @make_box()");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = call %record.Box_UInt32_ @make_box()");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_function_return_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_array_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function make_boxes() -> Array<Box<UInt32>, 2>\n"
        "    [Box(7 as UInt32), Box(9 as UInt32)]\n"
        "\n"
        "function main() -> UInt32\n"
        "    make_boxes()[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define [2 x %record.Box_UInt32_] @make_boxes()");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 9, 0");
    assert_ir_contains(result, " = insertvalue [2 x %record.Box_UInt32_] undef, %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, " = call [2 x %record.Box_UInt32_] @make_boxes()");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_array_function_boundaries() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_generic_record_array_function_boundaries.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "function read_nested_array(boxes: Array<Array<Tag<UInt32>, 2>, 2>) -> UInt32\n"
        "    boxes[1][0].code\n"
        "\n"
        "function read_nested_record(boxes: Array<Tag<Tag<UInt32>>, 2>) -> UInt32\n"
        "    boxes[1].code.code\n"
        "\n"
        "function make_nested_array() -> Array<Array<Tag<UInt32>, 2>, 2>\n"
        "    [[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]\n"
        "\n"
        "function make_nested_record() -> Array<Tag<Tag<UInt32>>, 2>\n"
        "    [Tag(Tag(5 as UInt32)), Tag(Tag(8 as UInt32))]\n"
        "\n"
        "function main() -> UInt32\n"
        "    read_nested_array([[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]) + read_nested_record([Tag(Tag(5 as UInt32)), Tag(Tag(8 as UInt32))]) + make_nested_array()[1][0].code + make_nested_record()[1].code.code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "define i32 @read_nested_array([2 x [2 x %record.Tag_UInt32_]] %boxes)");
    assert_ir_contains(result, "define i32 @read_nested_record([2 x %record.Tag_Tag_UInt32__] %boxes)");
    assert_ir_contains(result, "define [2 x [2 x %record.Tag_UInt32_]] @make_nested_array()");
    assert_ir_contains(result, "define [2 x %record.Tag_Tag_UInt32__] @make_nested_record()");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x [2 x %record.Tag_UInt32_]]");
    assert_ir_contains(result, "store [2 x [2 x %record.Tag_UInt32_]] %boxes, ptr %boxes.addr");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Tag_Tag_UInt32__]");
    assert_ir_contains(result, "store [2 x %record.Tag_Tag_UInt32__] %boxes, ptr %boxes.addr");
    assert_ir_contains(result, " = call i32 @read_nested_array([2 x [2 x %record.Tag_UInt32_]] %tmp");
    assert_ir_contains(result, " = call i32 @read_nested_record([2 x %record.Tag_Tag_UInt32__] %tmp");
    assert_ir_contains(result, " = call [2 x [2 x %record.Tag_UInt32_]] @make_nested_array()");
    assert_ir_contains(result, " = call [2 x %record.Tag_Tag_UInt32__] @make_nested_record()");
    assert_ir_contains(
        result, " = insertvalue [2 x [2 x %record.Tag_UInt32_]] undef, [2 x %record.Tag_UInt32_] %tmp");
    assert_ir_contains(
        result, " = insertvalue [2 x %record.Tag_Tag_UInt32__] undef, %record.Tag_Tag_UInt32__ %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_record_array_function_boundaries() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_record_array_function_boundaries.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Counter\n"
        "    value: UInt32\n"
        "\n"
        "record CounterPair\n"
        "    left: Counter\n"
        "    right: Counter\n"
        "\n"
        "record CounterMatrix\n"
        "    rows: Array<Array<Counter, 2>, 2>\n"
        "\n"
        "function read_pair(pair: CounterPair) -> UInt32\n"
        "    pair.right.value\n"
        "\n"
        "function read_matrix(matrix: CounterMatrix) -> UInt32\n"
        "    matrix.rows[1][0].value\n"
        "\n"
        "function make_pair() -> CounterPair\n"
        "    CounterPair(Counter(5 as UInt32), Counter(8 as UInt32))\n"
        "\n"
        "function make_matrix() -> CounterMatrix\n"
        "    CounterMatrix([[Counter(1 as UInt32), Counter(2 as UInt32)], [Counter(3 as UInt32), Counter(4 as UInt32)]])\n"
        "\n"
        "function main() -> UInt32\n"
        "    read_pair(CounterPair(Counter(1 as UInt32), Counter(2 as UInt32))) + read_matrix(CounterMatrix([[Counter(1 as UInt32), Counter(2 as UInt32)], [Counter(3 as UInt32), Counter(4 as UInt32)]])) + make_pair().right.value + make_matrix().rows[1][0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Counter = type { i32 }");
    assert_ir_contains(result, "%record.CounterPair = type { %record.Counter, %record.Counter }");
    assert_ir_contains(result, "%record.CounterMatrix = type { [2 x [2 x %record.Counter]] }");
    assert_ir_contains(result, "define i32 @read_pair(%record.CounterPair %pair)");
    assert_ir_contains(result, "define i32 @read_matrix(%record.CounterMatrix %matrix)");
    assert_ir_contains(result, "define %record.CounterPair @make_pair()");
    assert_ir_contains(result, "define %record.CounterMatrix @make_matrix()");
    assert_ir_contains(result, "%pair.addr = alloca %record.CounterPair");
    assert_ir_contains(result, "store %record.CounterPair %pair, ptr %pair.addr");
    assert_ir_contains(result, "%matrix.addr = alloca %record.CounterMatrix");
    assert_ir_contains(result, "store %record.CounterMatrix %matrix, ptr %matrix.addr");
    assert_ir_contains(result, " = call i32 @read_pair(%record.CounterPair %tmp");
    assert_ir_contains(result, " = call i32 @read_matrix(%record.CounterMatrix %tmp");
    assert_ir_contains(result, " = call %record.CounterPair @make_pair()");
    assert_ir_contains(result, " = call %record.CounterMatrix @make_matrix()");
    assert_ir_contains(result, " = insertvalue %record.CounterPair undef, %record.Counter %tmp");
    assert_ir_contains(result, " = insertvalue %record.CounterMatrix undef, [2 x [2 x %record.Counter]] %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterPair, ptr %pair.addr, i32 0, i32 1");
    assert_ir_contains(result, "getelementptr %record.Counter, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterMatrix, ptr %matrix.addr, i32 0, i32 0");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Counter]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Counter], ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_final_if_function_return_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_final_if_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function choose_box(flag: Bool) -> Box<UInt32>\n"
        "    if flag\n"
        "        Box(7 as UInt32)\n"
        "    else\n"
        "        Box(9 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_box(true).value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define %record.Box_UInt32_ @choose_box(i1 %flag)");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 9, 0");
    assert_ir_contains(result, " = phi %record.Box_UInt32_ [%tmp");
    assert_ir_contains(result, " = call %record.Box_UInt32_ @choose_box(i1 1)");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_final_switch_function_return_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_final_switch_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function choose_box(flag: Bool) -> Box<UInt32>\n"
        "    switch flag\n"
        "        true => Box(7 as UInt32)\n"
        "        default => Box(9 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_box(true).value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define %record.Box_UInt32_ @choose_box(i1 %flag)");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 9, 0");
    assert_ir_contains(result, " = phi %record.Box_UInt32_ [%tmp");
    assert_ir_contains(result, " = call %record.Box_UInt32_ @choose_box(i1 1)");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_final_if_function_return_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_array_final_if_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function choose_boxes(flag: Bool) -> Array<Box<UInt32>, 2>\n"
        "    if flag\n"
        "        [Box(7 as UInt32), Box(9 as UInt32)]\n"
        "    else\n"
        "        [Box(11 as UInt32), Box(13 as UInt32)]\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_boxes(true)[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define [2 x %record.Box_UInt32_] @choose_boxes(i1 %flag)");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 13, 0");
    assert_ir_contains(result, " = phi [2 x %record.Box_UInt32_] [%tmp");
    assert_ir_contains(result, " = call [2 x %record.Box_UInt32_] @choose_boxes(i1 1)");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_final_switch_function_return_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_array_final_switch_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function choose_boxes(flag: Bool) -> Array<Box<UInt32>, 2>\n"
        "    switch flag\n"
        "        true => [Box(7 as UInt32), Box(9 as UInt32)]\n"
        "        default => [Box(11 as UInt32), Box(13 as UInt32)]\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_boxes(true)[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define [2 x %record.Box_UInt32_] @choose_boxes(i1 %flag)");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 13, 0");
    assert_ir_contains(result, " = phi [2 x %record.Box_UInt32_] [%tmp");
    assert_ir_contains(result, " = call [2 x %record.Box_UInt32_] @choose_boxes(i1 1)");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_array_function_control_early_returns() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_nested_generic_record_array_function_control_early_returns.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function choose_nested_array(flag: Bool) -> Array<Array<Tag<UInt32>, 2>, 2>\n"
        "    if flag\n"
        "        [[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]\n"
        "    else\n"
        "        [[Tag(5 as UInt32), Tag(6 as UInt32)], [Tag(7 as UInt32), Tag(8 as UInt32)]]\n"
        "\n"
        "function choose_nested_record(flag: Bool) -> Array<Tag<Tag<UInt32>>, 2>\n"
        "    if flag\n"
        "        [Tag(Tag(11 as UInt32)), Tag(Tag(13 as UInt32))]\n"
        "    else\n"
        "        [Tag(Tag(17 as UInt32)), Tag(Tag(19 as UInt32))]\n"
        "\n"
        "function early_nested_array(flag: Bool) -> Array<Array<Tag<UInt32>, 2>, 2>\n"
        "    guard flag else\n"
        "        return [[Tag(23 as UInt32), Tag(29 as UInt32)], [Tag(31 as UInt32), Tag(37 as UInt32)]]\n"
        "    defer\n"
        "        observe(1 as UInt32)\n"
        "    return [[Tag(41 as UInt32), Tag(43 as UInt32)], [Tag(47 as UInt32), Tag(53 as UInt32)]]\n"
        "\n"
        "function early_nested_record(flag: Bool) -> Array<Tag<Tag<UInt32>>, 2>\n"
        "    guard flag else\n"
        "        return [Tag(Tag(59 as UInt32)), Tag(Tag(61 as UInt32))]\n"
        "    defer\n"
        "        observe(2 as UInt32)\n"
        "    return [Tag(Tag(67 as UInt32)), Tag(Tag(71 as UInt32))]\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_nested_array(true)[1][0].code + choose_nested_record(false)[1].code.code + early_nested_array(false)[0][1].code + early_nested_record(true)[1].code.code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "define [2 x [2 x %record.Tag_UInt32_]] @choose_nested_array(i1 %flag)");
    assert_ir_contains(result, "define [2 x %record.Tag_Tag_UInt32__] @choose_nested_record(i1 %flag)");
    assert_ir_contains(result, "define [2 x [2 x %record.Tag_UInt32_]] @early_nested_array(i1 %flag)");
    assert_ir_contains(result, "define [2 x %record.Tag_Tag_UInt32__] @early_nested_record(i1 %flag)");
    assert_ir_contains(result, " = phi [2 x [2 x %record.Tag_UInt32_]] [%tmp");
    assert_ir_contains(result, " = phi [2 x %record.Tag_Tag_UInt32__] [%tmp");
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_ir_contains(result, "call void @observe(i32 1)");
    assert_ir_contains(result, "call void @observe(i32 2)");
    assert_ir_contains(result, " = call [2 x [2 x %record.Tag_UInt32_]] @choose_nested_array(i1 1)");
    assert_ir_contains(result, " = call [2 x %record.Tag_Tag_UInt32__] @choose_nested_record(i1 0)");
    assert_ir_contains(result, " = call [2 x [2 x %record.Tag_UInt32_]] @early_nested_array(i1 0)");
    assert_ir_contains(result, " = call [2 x %record.Tag_Tag_UInt32__] @early_nested_record(i1 1)");
    assert_ir_contains(
        result, " = insertvalue [2 x [2 x %record.Tag_UInt32_]] undef, [2 x %record.Tag_UInt32_] %tmp");
    assert_ir_contains(
        result, " = insertvalue [2 x %record.Tag_Tag_UInt32__] undef, %record.Tag_Tag_UInt32__ %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_record_array_function_control_early_returns() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_nested_record_array_function_control_early_returns.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Counter\n"
        "    value: UInt32\n"
        "\n"
        "record CounterPair\n"
        "    left: Counter\n"
        "    right: Counter\n"
        "\n"
        "record CounterMatrix\n"
        "    rows: Array<Array<Counter, 2>, 2>\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function choose_pair(flag: Bool) -> CounterPair\n"
        "    if flag\n"
        "        CounterPair(Counter(1 as UInt32), Counter(2 as UInt32))\n"
        "    else\n"
        "        CounterPair(Counter(3 as UInt32), Counter(4 as UInt32))\n"
        "\n"
        "function choose_matrix(flag: Bool) -> CounterMatrix\n"
        "    if flag\n"
        "        CounterMatrix([[Counter(5 as UInt32), Counter(6 as UInt32)], [Counter(7 as UInt32), Counter(8 as UInt32)]])\n"
        "    else\n"
        "        CounterMatrix([[Counter(9 as UInt32), Counter(10 as UInt32)], [Counter(11 as UInt32), Counter(12 as UInt32)]])\n"
        "\n"
        "function early_pair(flag: Bool) -> CounterPair\n"
        "    guard flag else\n"
        "        return CounterPair(Counter(13 as UInt32), Counter(14 as UInt32))\n"
        "    defer\n"
        "        observe(1 as UInt32)\n"
        "    return CounterPair(Counter(15 as UInt32), Counter(16 as UInt32))\n"
        "\n"
        "function early_matrix(flag: Bool) -> CounterMatrix\n"
        "    guard flag else\n"
        "        return CounterMatrix([[Counter(17 as UInt32), Counter(18 as UInt32)], [Counter(19 as UInt32), Counter(20 as UInt32)]])\n"
        "    defer\n"
        "        observe(2 as UInt32)\n"
        "    return CounterMatrix([[Counter(21 as UInt32), Counter(22 as UInt32)], [Counter(23 as UInt32), Counter(24 as UInt32)]])\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_pair(true).right.value + choose_matrix(false).rows[1][0].value + early_pair(false).left.value + early_matrix(true).rows[0][1].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Counter = type { i32 }");
    assert_ir_contains(result, "%record.CounterPair = type { %record.Counter, %record.Counter }");
    assert_ir_contains(result, "%record.CounterMatrix = type { [2 x [2 x %record.Counter]] }");
    assert_ir_contains(result, "define %record.CounterPair @choose_pair(i1 %flag)");
    assert_ir_contains(result, "define %record.CounterMatrix @choose_matrix(i1 %flag)");
    assert_ir_contains(result, "define %record.CounterPair @early_pair(i1 %flag)");
    assert_ir_contains(result, "define %record.CounterMatrix @early_matrix(i1 %flag)");
    assert_ir_contains(result, " = phi %record.CounterPair [%tmp");
    assert_ir_contains(result, " = phi %record.CounterMatrix [%tmp");
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_ir_contains(result, "call void @observe(i32 1)");
    assert_ir_contains(result, "call void @observe(i32 2)");
    assert_ir_contains(result, " = call %record.CounterPair @choose_pair(i1 1)");
    assert_ir_contains(result, " = call %record.CounterMatrix @choose_matrix(i1 0)");
    assert_ir_contains(result, " = call %record.CounterPair @early_pair(i1 0)");
    assert_ir_contains(result, " = call %record.CounterMatrix @early_matrix(i1 1)");
    assert_ir_contains(result, " = insertvalue %record.CounterPair undef, %record.Counter %tmp");
    assert_ir_contains(result, " = insertvalue %record.CounterMatrix undef, [2 x [2 x %record.Counter]] %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterPair, ptr %tmp");
    assert_ir_contains(result, "getelementptr %record.Counter, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterMatrix, ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Counter]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Counter], ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_array_function_final_switch_returns() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_nested_generic_record_array_function_final_switch_returns.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "function choose_nested_array(flag: Bool) -> Array<Array<Tag<UInt32>, 2>, 2>\n"
        "    switch flag\n"
        "        true => [[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]\n"
        "        default => [[Tag(5 as UInt32), Tag(6 as UInt32)], [Tag(7 as UInt32), Tag(8 as UInt32)]]\n"
        "\n"
        "function choose_nested_record(flag: Bool) -> Array<Tag<Tag<UInt32>>, 2>\n"
        "    switch flag\n"
        "        true => [Tag(Tag(11 as UInt32)), Tag(Tag(13 as UInt32))]\n"
        "        default => [Tag(Tag(17 as UInt32)), Tag(Tag(19 as UInt32))]\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_nested_array(true)[1][0].code + choose_nested_record(false)[1].code.code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "define [2 x [2 x %record.Tag_UInt32_]] @choose_nested_array(i1 %flag)");
    assert_ir_contains(result, "define [2 x %record.Tag_Tag_UInt32__] @choose_nested_record(i1 %flag)");
    assert_ir_contains(result, "switch.case.");
    assert_ir_contains(result, "switch.default.");
    assert_ir_contains(result, "switch.merge.");
    assert_ir_contains(result, " = phi [2 x [2 x %record.Tag_UInt32_]] [%tmp");
    assert_ir_contains(result, " = phi [2 x %record.Tag_Tag_UInt32__] [%tmp");
    assert_ir_contains(result, " = call [2 x [2 x %record.Tag_UInt32_]] @choose_nested_array(i1 1)");
    assert_ir_contains(result, " = call [2 x %record.Tag_Tag_UInt32__] @choose_nested_record(i1 0)");
    assert_ir_contains(
        result, " = insertvalue [2 x [2 x %record.Tag_UInt32_]] undef, [2 x %record.Tag_UInt32_] %tmp");
    assert_ir_contains(
        result, " = insertvalue [2 x %record.Tag_Tag_UInt32__] undef, %record.Tag_Tag_UInt32__ %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_record_array_function_final_switch_returns() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_nested_record_array_function_final_switch_returns.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Counter\n"
        "    value: UInt32\n"
        "\n"
        "record CounterPair\n"
        "    left: Counter\n"
        "    right: Counter\n"
        "\n"
        "record CounterMatrix\n"
        "    rows: Array<Array<Counter, 2>, 2>\n"
        "\n"
        "function choose_pair(flag: Bool) -> CounterPair\n"
        "    switch flag\n"
        "        true => CounterPair(Counter(1 as UInt32), Counter(2 as UInt32))\n"
        "        default => CounterPair(Counter(3 as UInt32), Counter(4 as UInt32))\n"
        "\n"
        "function choose_matrix(flag: Bool) -> CounterMatrix\n"
        "    switch flag\n"
        "        true => CounterMatrix([[Counter(5 as UInt32), Counter(6 as UInt32)], [Counter(7 as UInt32), Counter(8 as UInt32)]])\n"
        "        default => CounterMatrix([[Counter(9 as UInt32), Counter(10 as UInt32)], [Counter(11 as UInt32), Counter(12 as UInt32)]])\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_pair(true).right.value + choose_matrix(false).rows[1][0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Counter = type { i32 }");
    assert_ir_contains(result, "%record.CounterPair = type { %record.Counter, %record.Counter }");
    assert_ir_contains(result, "%record.CounterMatrix = type { [2 x [2 x %record.Counter]] }");
    assert_ir_contains(result, "define %record.CounterPair @choose_pair(i1 %flag)");
    assert_ir_contains(result, "define %record.CounterMatrix @choose_matrix(i1 %flag)");
    assert_ir_contains(result, "switch.case.");
    assert_ir_contains(result, "switch.default.");
    assert_ir_contains(result, "switch.merge.");
    assert_ir_contains(result, " = phi %record.CounterPair [%tmp");
    assert_ir_contains(result, " = phi %record.CounterMatrix [%tmp");
    assert_ir_contains(result, " = call %record.CounterPair @choose_pair(i1 1)");
    assert_ir_contains(result, " = call %record.CounterMatrix @choose_matrix(i1 0)");
    assert_ir_contains(result, " = insertvalue %record.CounterPair undef, %record.Counter %tmp");
    assert_ir_contains(result, " = insertvalue %record.CounterMatrix undef, [2 x [2 x %record.Counter]] %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterPair, ptr %tmp");
    assert_ir_contains(result, "getelementptr %record.Counter, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterMatrix, ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Counter]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Counter], ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_array_function_loop_built_returns() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_generic_record_array_function_loop_returns.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "function build_nested_array_while() -> Array<Array<Tag<UInt32>, 2>, 2>\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "    [[Tag(value), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]\n"
        "\n"
        "function build_nested_array_for() -> Array<Array<Tag<UInt32>, 2>, 2>\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    for item in [7 as UInt32, 9 as UInt32]\n"
        "        value = item\n"
        "    [[Tag(value), Tag(11 as UInt32)], [Tag(13 as UInt32), Tag(17 as UInt32)]]\n"
        "\n"
        "function build_nested_record_while() -> Array<Tag<Tag<UInt32>>, 2>\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "    [Tag(Tag(value)), Tag(Tag(19 as UInt32))]\n"
        "\n"
        "function build_nested_record_for() -> Array<Tag<Tag<UInt32>>, 2>\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    for item in [23 as UInt32, 29 as UInt32]\n"
        "        value = item\n"
        "    [Tag(Tag(value)), Tag(Tag(31 as UInt32))]\n"
        "\n"
        "function main() -> UInt32\n"
        "    build_nested_array_while()[0][0].code + build_nested_array_for()[0][0].code + build_nested_record_while()[0].code.code + build_nested_record_for()[1].code.code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "define [2 x [2 x %record.Tag_UInt32_]] @build_nested_array_while()");
    assert_ir_contains(result, "define [2 x [2 x %record.Tag_UInt32_]] @build_nested_array_for()");
    assert_ir_contains(result, "define [2 x %record.Tag_Tag_UInt32__] @build_nested_record_while()");
    assert_ir_contains(result, "define [2 x %record.Tag_Tag_UInt32__] @build_nested_record_for()");
    assert_ir_contains(result, "while.condition.");
    assert_ir_contains(result, "while.exit.");
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "for.exit.");
    assert_ir_contains(result, " = call [2 x [2 x %record.Tag_UInt32_]] @build_nested_array_while()");
    assert_ir_contains(result, " = call [2 x [2 x %record.Tag_UInt32_]] @build_nested_array_for()");
    assert_ir_contains(result, " = call [2 x %record.Tag_Tag_UInt32__] @build_nested_record_while()");
    assert_ir_contains(result, " = call [2 x %record.Tag_Tag_UInt32__] @build_nested_record_for()");
    assert_ir_contains(
        result, " = insertvalue [2 x [2 x %record.Tag_UInt32_]] undef, [2 x %record.Tag_UInt32_] %tmp");
    assert_ir_contains(
        result, " = insertvalue [2 x %record.Tag_Tag_UInt32__] undef, %record.Tag_Tag_UInt32__ %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_record_array_function_loop_built_returns() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_record_array_function_loop_returns.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Counter\n"
        "    value: UInt32\n"
        "\n"
        "record CounterPair\n"
        "    left: Counter\n"
        "    right: Counter\n"
        "\n"
        "record CounterMatrix\n"
        "    rows: Array<Array<Counter, 2>, 2>\n"
        "\n"
        "function build_pair_while() -> CounterPair\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "    CounterPair(Counter(value), Counter(2 as UInt32))\n"
        "\n"
        "function build_pair_for() -> CounterPair\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    for item in [7 as UInt32, 9 as UInt32]\n"
        "        value = item\n"
        "    CounterPair(Counter(value), Counter(11 as UInt32))\n"
        "\n"
        "function build_matrix_while() -> CounterMatrix\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "    CounterMatrix([[Counter(value), Counter(13 as UInt32)], [Counter(17 as UInt32), Counter(19 as UInt32)]])\n"
        "\n"
        "function build_matrix_for() -> CounterMatrix\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    for item in [23 as UInt32, 29 as UInt32]\n"
        "        value = item\n"
        "    CounterMatrix([[Counter(value), Counter(31 as UInt32)], [Counter(37 as UInt32), Counter(41 as UInt32)]])\n"
        "\n"
        "function main() -> UInt32\n"
        "    build_pair_while().left.value + build_pair_for().right.value + build_matrix_while().rows[0][0].value + build_matrix_for().rows[1][0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Counter = type { i32 }");
    assert_ir_contains(result, "%record.CounterPair = type { %record.Counter, %record.Counter }");
    assert_ir_contains(result, "%record.CounterMatrix = type { [2 x [2 x %record.Counter]] }");
    assert_ir_contains(result, "define %record.CounterPair @build_pair_while()");
    assert_ir_contains(result, "define %record.CounterPair @build_pair_for()");
    assert_ir_contains(result, "define %record.CounterMatrix @build_matrix_while()");
    assert_ir_contains(result, "define %record.CounterMatrix @build_matrix_for()");
    assert_ir_contains(result, "while.condition.");
    assert_ir_contains(result, "while.exit.");
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "for.exit.");
    assert_ir_contains(result, " = call %record.CounterPair @build_pair_while()");
    assert_ir_contains(result, " = call %record.CounterPair @build_pair_for()");
    assert_ir_contains(result, " = call %record.CounterMatrix @build_matrix_while()");
    assert_ir_contains(result, " = call %record.CounterMatrix @build_matrix_for()");
    assert_ir_contains(result, " = insertvalue %record.CounterPair undef, %record.Counter %tmp");
    assert_ir_contains(result, " = insertvalue %record.CounterMatrix undef, [2 x [2 x %record.Counter]] %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterPair, ptr %tmp");
    assert_ir_contains(result, "getelementptr %record.Counter, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterMatrix, ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Counter]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Counter], ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_while_built_function_return_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_while_built_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function build_box() -> Box<UInt32>\n"
        "    var box: Box<UInt32> = Box(0 as UInt32)\n"
        "    var index: UInt32 = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        box = Box(7 as UInt32)\n"
        "        index = index + 1 as UInt32\n"
        "    box\n"
        "\n"
        "function main() -> UInt32\n"
        "    build_box().value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define %record.Box_UInt32_ @build_box()");
    assert_ir_contains(result, "%box.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, "store %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, " = call %record.Box_UInt32_ @build_box()");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_for_built_function_return_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_array_for_built_function_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function build_boxes() -> Array<Box<UInt32>, 2>\n"
        "    var boxes: Array<Box<UInt32>, 2> = [Box(0 as UInt32), Box(1 as UInt32)]\n"
        "    for item in [7 as UInt32]\n"
        "        boxes = [Box(item), Box(9 as UInt32)]\n"
        "    boxes\n"
        "\n"
        "function main() -> UInt32\n"
        "    build_boxes()[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define [2 x %record.Box_UInt32_] @build_boxes()");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Box_UInt32_]");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 9, 0");
    assert_ir_contains(result, "store [2 x %record.Box_UInt32_] %tmp");
    assert_ir_contains(result, " = call [2 x %record.Box_UInt32_] @build_boxes()");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_guard_early_return_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_guard_early_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function choose_box(flag: Bool) -> Box<UInt32>\n"
        "    guard flag else\n"
        "        return Box(7 as UInt32)\n"
        "    Box(9 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_box(false).value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define %record.Box_UInt32_ @choose_box(i1 %flag)");
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 9, 0");
    assert_ir_contains(result, " = call %record.Box_UInt32_ @choose_box(i1 0)");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_defer_early_return_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_array_defer_early_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function choose_boxes(flag: Bool) -> Array<Box<UInt32>, 2>\n"
        "    defer\n"
        "        observe(1 as UInt32)\n"
        "    if flag\n"
        "        return [Box(7 as UInt32), Box(9 as UInt32)]\n"
        "    [Box(11 as UInt32), Box(13 as UInt32)]\n"
        "\n"
        "function main() -> UInt32\n"
        "    choose_boxes(true)[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define [2 x %record.Box_UInt32_] @choose_boxes(i1 %flag)");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 13, 0");
    assert_ir_contains(result, "call void @observe(i32 1)");
    assert_ir_contains(result, " = call [2 x %record.Box_UInt32_] @choose_boxes(i1 1)");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_for_item_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_generic_record_for_item_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    let boxes: Array<Box<UInt32>, 2> = [Box(7 as UInt32), Box(9 as UInt32)]\n"
        "    for item in boxes\n"
        "        total = item.value\n"
        "    total\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, " = insertvalue [2 x %record.Box_UInt32_] undef, %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, "%item.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, "store %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %item.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_for_item_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_generic_record_for_item_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "function sum_nested_array_items() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    let rows: Array<Array<Tag<UInt32>, 2>, 2> = [[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]\n"
        "    for row in rows\n"
        "        total = total + row[1].code\n"
        "    total\n"
        "\n"
        "function sum_nested_record_items() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    let boxes: Array<Tag<Tag<UInt32>>, 2> = [Tag(Tag(5 as UInt32)), Tag(Tag(8 as UInt32))]\n"
        "    for box in boxes\n"
        "        total = total + box.code.code\n"
        "    total\n"
        "\n"
        "function main() -> UInt32\n"
        "    sum_nested_array_items() + sum_nested_record_items()\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "%row.addr = alloca [2 x %record.Tag_UInt32_]");
    assert_ir_contains(result, "store [2 x %record.Tag_UInt32_] %tmp");
    assert_ir_contains(result, "%box.addr = alloca %record.Tag_Tag_UInt32__");
    assert_ir_contains(result, "store %record.Tag_Tag_UInt32__ %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %row.addr");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "for.exit.");
    assert_returns_lowered_tmp(result);
}

void test_emit_bare_generic_record_array_literal_for_item_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_bare_generic_record_array_literal_for_item_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    for item in [Box(7 as UInt32), Box(9 as UInt32)]\n"
        "        total = item.value\n"
        "    total\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 9, 0");
    assert_ir_contains(result, "%item.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, "store %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %item.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_bare_nested_generic_record_array_literal_for_item_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_bare_nested_generic_record_array_literal_for_item_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "function sum_bare_nested_array_items() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    for row in [[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]\n"
        "        total = total + row[1].code\n"
        "    total\n"
        "\n"
        "function sum_bare_nested_record_items() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    for box in [Tag(Tag(5 as UInt32)), Tag(Tag(8 as UInt32))]\n"
        "        total = total + box.code.code\n"
        "    total\n"
        "\n"
        "function main() -> UInt32\n"
        "    sum_bare_nested_array_items() + sum_bare_nested_record_items()\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "%row.addr = alloca [2 x %record.Tag_UInt32_]");
    assert_ir_contains(result, "store [2 x %record.Tag_UInt32_] %tmp");
    assert_ir_contains(result, "%box.addr = alloca %record.Tag_Tag_UInt32__");
    assert_ir_contains(result, "store %record.Tag_Tag_UInt32__ %tmp");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 1, 0");
    assert_ir_contains(result, " = insertvalue %record.Tag_Tag_UInt32__ undef, %record.Tag_UInt32_ %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %row.addr");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "for.exit.");
    assert_returns_lowered_tmp(result);
}

void test_emit_view_for_iterable_lowering() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_view_for_iterable.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function sum(values: View<UInt32>) -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    for item in values\n"
        "        total = total + item\n"
        "    total\n",
        orison::lowering::LlvmIrEmissionOptions {}
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "define i32 @sum({ ptr, i64 } %values)");
    assert_ir_contains(result, "  %values.sequence_for0.descriptor = load { ptr, i64 }, ptr %values.addr\n");
    assert_ir_contains(
        result,
        "  %values.sequence_for0.length = extractvalue { ptr, i64 } %values.sequence_for0.descriptor, 1\n"
    );
    assert_ir_contains(
        result,
        "  %values.sequence_for0.more = icmp ult i64 %values.sequence_for0.index, "
        "%values.sequence_for0.length\n"
    );
    assert_ir_contains(
        result,
        "  %values.sequence_for0.element.addr = getelementptr i32, ptr %values.sequence_for0.data, "
        "i64 %values.sequence_for0.index\n"
    );
}

void test_emit_view_parameter_index_lowering() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_view_parameter_index.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function first(values: View<UInt32>) -> UInt32\n"
        "    values[0]\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "define i32 @first({ ptr, i64 } %values)");
    assert_ir_contains(result, "declare void @__orison_dynamic_array_bounds_failed()");
    assert_ir_contains(result, "  %view_index1.data = extractvalue { ptr, i64 } %values, 0\n");
    assert_ir_contains(result, "  %view_index1.length = extractvalue { ptr, i64 } %values, 1\n");
    assert_ir_contains(result, "  %view_index1.in_bounds = icmp ult i64 0, %view_index1.length\n");
    assert_ir_contains(result, "view.index.out_of_bounds.0:\n");
    assert_ir_contains(result, "  call void @__orison_dynamic_array_bounds_failed()\n");
    assert_ir_contains(result, "view.index.in_bounds.0:\n");
    assert_ir_contains(result, "  %view_index1.element.addr = getelementptr i32, ptr %view_index1.data, i64 0\n");
    assert_ir_contains(result, "  %view_index1.value = load i32, ptr %view_index1.element.addr\n");
}

void test_emit_view_parameter_length_lowering() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_view_parameter_length.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function count(values: View<UInt32>) -> IntSize\n"
        "    values.length()\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "define i64 @count({ ptr, i64 } %values)");
    assert_ir_contains(result, "  %view_length0.value = extractvalue { ptr, i64 } %values, 1\n");
    assert_ir_contains(result, "ret i64 %view_length0.value");
}

void test_emit_access_qualified_view_parameter_lowering() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_access_qualified_view_parameter.or";
    auto shared_result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function count(values: shared.View<UInt32>) -> IntSize\n"
        "    values.length()\n"
    );

    assert(!shared_result.has_errors());
    assert_ir_contains(shared_result, "define i64 @count({ ptr, i64 } %values)");
    assert_ir_contains(shared_result, "  %view_length0.value = extractvalue { ptr, i64 } %values, 1\n");

    auto exclusive_index_result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function first(values: exclusive.View<UInt32>) -> UInt32\n"
        "    values[0]\n"
    );

    assert(!exclusive_index_result.has_errors());
    assert_ir_contains(exclusive_index_result, "define i32 @first({ ptr, i64 } %values)");
    assert_ir_contains(exclusive_index_result, "declare void @__orison_dynamic_array_bounds_failed()");
    assert_ir_contains(
        exclusive_index_result,
        "  %view_index1.in_bounds = icmp ult i64 0, %view_index1.length\n"
    );
    assert_ir_contains(
        exclusive_index_result,
        "  %view_index1.value = load i32, ptr %view_index1.element.addr\n"
    );

    auto shared_for_result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function sum(values: shared.View<UInt32>) -> UInt32\n"
        "    var total = 0 as UInt32\n"
        "    for item in values\n"
        "        total = total + item\n"
        "    total\n",
        orison::lowering::LlvmIrEmissionOptions {}
    );

    assert(!shared_for_result.has_errors());
    assert_ir_contains(shared_for_result, "define i32 @sum({ ptr, i64 } %values)");
    assert_ir_contains(
        shared_for_result,
        "  %values.sequence_for0.descriptor = load { ptr, i64 }, ptr %values.addr\n"
    );
    assert_ir_contains(
        shared_for_result,
        "  %values.sequence_for0.value = load i32, ptr %values.sequence_for0.element.addr\n"
    );
}

void test_reject_underconstrained_generic_record_array_literal_for_item_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_underconstrained_generic_record_array_literal_for_item_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    for item in [Tag(7 as UInt32), Tag(9 as UInt32)]\n"
        "        total = item.code\n"
        "    total\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message.find(
            "lowering array-literal for statements requires an explicit Array<T, N> source type"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find(
            "generic parameter 'T' cannot be inferred for record 'Tag'"
        ) != std::string::npos
    );
}

void test_reject_underconstrained_nested_generic_record_array_literal_for_item_field_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_underconstrained_nested_generic_record_array_literal_for_item_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    var total: UInt32 = 0 as UInt32\n"
        "    for box in [Tag(Tag(5 as UInt32)), Tag(Tag(8 as UInt32))]\n"
        "        total = total + box.code\n"
        "    total\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message.find(
            "lowering array-literal for statements requires an explicit Array<T, N> source type"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find(
            "generic parameter 'T' cannot be inferred for record 'Tag'"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find("unknown lowered function") ==
        std::string::npos
    );
}

void test_reject_underconstrained_generic_record_inferred_let_binding() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_underconstrained_generic_record_inferred_let.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    let tag = Tag(7 as UInt32)\n"
        "    tag.code\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message.find(
            "lowering does not yet support this let binding"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find(
            "generic parameter 'T' cannot be inferred for record 'Tag'"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find("unknown lowered function") ==
        std::string::npos
    );
}

void test_reject_underconstrained_generic_record_inferred_var_binding() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_underconstrained_generic_record_inferred_var.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    var tag = Tag(7 as UInt32)\n"
        "    tag.code\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message.find(
            "lowering does not yet support this var initializer"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find(
            "generic record constructor inference failed"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find(
            "generic parameter 'T' cannot be inferred for record 'Tag'"
        ) != std::string::npos
    );
    assert(
        result.diagnostics.entries().front().message.find("unknown lowered function") ==
        std::string::npos
    );
}

void test_emit_generic_record_constructor_call_argument_from_parameter_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_constructor_call_argument_context.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "function take(value: Tag<UInt32>) -> UInt32\n"
        "    value.code\n"
        "\n"
        "function main() -> UInt32\n"
        "    take(Tag(7 as UInt32))\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define i32 @take(%record.Tag_UInt32_ %value)");
    assert_ir_contains(result, "%value.addr = alloca %record.Tag_UInt32_");
    assert_ir_contains(result, "store %record.Tag_UInt32_ %value, ptr %value.addr");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %value.addr, i32 0, i32 0");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = call i32 @take(%record.Tag_UInt32_ %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_constructor_method_call_argument_from_parameter_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_constructor_method_argument_context.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function take(this: shared This, value: Tag<UInt32>) -> UInt32\n"
        "        value.code\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.take(Tag(7 as UInt32))\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_defines_method(result, "i32", "method.UInt32.take", "i32 %this, %record.Tag_UInt32_ %value");
    assert_ir_contains(result, "%value.addr = alloca %record.Tag_UInt32_");
    assert_ir_contains(result, "store %record.Tag_UInt32_ %value, ptr %value.addr");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %value.addr, i32 0, i32 0");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = call i32 @method.UInt32.take(i32 %receiver, %record.Tag_UInt32_ %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_constructor_method_return_control_flow_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_constructor_method_return_control_flow.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function make_if(this: shared This, flag: Bool) -> Tag<UInt32>\n"
        "        if flag\n"
        "            Tag(11 as UInt32)\n"
        "        else\n"
        "            Tag(13 as UInt32)\n"
        "\n"
        "    function make_switch(this: shared This, flag: Bool) -> Tag<UInt32>\n"
        "        switch flag\n"
        "            true => Tag(17 as UInt32)\n"
        "            default => Tag(19 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.make_if(true).code + receiver.make_switch(false).code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_defines_method(result, "%record.Tag_UInt32_", "method.UInt32.make_if", "i32 %this, i1 %flag");
    assert_defines_method(result, "%record.Tag_UInt32_", "method.UInt32.make_switch", "i32 %this, i1 %flag");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 11, 0");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 13, 0");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 17, 0");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 19, 0");
    assert_ir_contains(result, " = phi %record.Tag_UInt32_ [%tmp");
    assert_ir_contains(result, " = call %record.Tag_UInt32_ @method.UInt32.make_if(i32 %receiver, i1 1)");
    assert_ir_contains(result, " = call %record.Tag_UInt32_ @method.UInt32.make_switch(i32 %receiver, i1 0)");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_constructor_method_guard_defer_return_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_constructor_method_guard_defer_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "extend UInt32\n"
        "    function choose(this: shared This, flag: Bool) -> Tag<UInt32>\n"
        "        guard flag else\n"
        "            return Tag(11 as UInt32)\n"
        "        defer\n"
        "            observe(1 as UInt32)\n"
        "        return Tag(13 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.choose(false).code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_defines_method(result, "%record.Tag_UInt32_", "method.UInt32.choose", "i32 %this, i1 %flag");
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 11, 0");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 13, 0");
    assert_ir_contains(result, "call void @observe(i32 1)");
    assert_ir_contains(result, "ret %record.Tag_UInt32_ %tmp");
    assert_ir_contains(result, " = call %record.Tag_UInt32_ @method.UInt32.choose(i32 %receiver, i1 0)");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_constructor_method_loop_built_return_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_constructor_method_loop_built_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function make_while(this: shared This) -> Tag<UInt32>\n"
        "        var value: UInt32 = 0 as UInt32\n"
        "        while value < 1 as UInt32\n"
        "            value = value + 1 as UInt32\n"
        "        Tag(value)\n"
        "\n"
        "    function make_for(this: shared This) -> Tag<UInt32>\n"
        "        var value: UInt32 = 0 as UInt32\n"
        "        for item in [7 as UInt32, 9 as UInt32]\n"
        "            value = item\n"
        "        Tag(value)\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.make_while().code + receiver.make_for().code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_defines_method(result, "%record.Tag_UInt32_", "method.UInt32.make_while", "i32 %this");
    assert_defines_method(result, "%record.Tag_UInt32_", "method.UInt32.make_for", "i32 %this");
    assert_ir_contains(result, "while.condition.");
    assert_ir_contains(result, "while.exit.");
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "for.exit.");
    assert_ir_contains(result, " = insertvalue %record.Tag_UInt32_ undef, i32 %tmp");
    assert_ir_contains(result, " = call %record.Tag_UInt32_ @method.UInt32.make_while(i32 %receiver)");
    assert_ir_contains(result, " = call %record.Tag_UInt32_ @method.UInt32.make_for(i32 %receiver)");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_method_control_flow_early_return_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_array_method_control_flow_early_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "extend UInt32\n"
        "    function choose_if(this: shared This, flag: Bool) -> Array<Tag<UInt32>, 2>\n"
        "        if flag\n"
        "            [Tag(7 as UInt32), Tag(9 as UInt32)]\n"
        "        else\n"
        "            [Tag(11 as UInt32), Tag(13 as UInt32)]\n"
        "\n"
        "    function choose_early(this: shared This, flag: Bool) -> Array<Tag<UInt32>, 2>\n"
        "        guard flag else\n"
        "            return [Tag(17 as UInt32), Tag(19 as UInt32)]\n"
        "        defer\n"
        "            observe(1 as UInt32)\n"
        "        return [Tag(23 as UInt32), Tag(29 as UInt32)]\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.choose_if(true)[0].code + receiver.choose_early(false)[1].code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_defines_method(result, "[2 x %record.Tag_UInt32_]", "method.UInt32.choose_if", "i32 %this, i1 %flag");
    assert_defines_method(result, "[2 x %record.Tag_UInt32_]", "method.UInt32.choose_early", "i32 %this, i1 %flag");
    assert_ir_contains(result, " = insertvalue [2 x %record.Tag_UInt32_] undef, %record.Tag_UInt32_ %tmp");
    assert_ir_contains(result, " = phi [2 x %record.Tag_UInt32_] [%tmp");
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_ir_contains(result, "call void @observe(i32 1)");
    assert_ir_contains(result, " = call [2 x %record.Tag_UInt32_] @method.UInt32.choose_if(i32 %receiver, i1 1)");
    assert_ir_contains(result, " = call [2 x %record.Tag_UInt32_] @method.UInt32.choose_early(i32 %receiver, i1 0)");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_method_loop_built_return_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_array_method_loop_built_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: UInt32\n"
        "\n"
        "extend UInt32\n"
        "    function make_while(this: shared This) -> Array<Tag<UInt32>, 2>\n"
        "        var value: UInt32 = 0 as UInt32\n"
        "        while value < 1 as UInt32\n"
        "            value = value + 1 as UInt32\n"
        "        [Tag(value), Tag(7 as UInt32)]\n"
        "\n"
        "    function make_for(this: shared This) -> Array<Tag<UInt32>, 2>\n"
        "        var value: UInt32 = 0 as UInt32\n"
        "        for item in [7 as UInt32, 9 as UInt32]\n"
        "            value = item\n"
        "        [Tag(value), Tag(11 as UInt32)]\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.make_while()[0].code + receiver.make_for()[1].code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_defines_method(result, "[2 x %record.Tag_UInt32_]", "method.UInt32.make_while", "i32 %this");
    assert_defines_method(result, "[2 x %record.Tag_UInt32_]", "method.UInt32.make_for", "i32 %this");
    assert_ir_contains(result, "while.condition.");
    assert_ir_contains(result, "while.exit.");
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "for.exit.");
    assert_ir_contains(result, " = insertvalue [2 x %record.Tag_UInt32_] undef, %record.Tag_UInt32_ %tmp");
    assert_ir_contains(result, " = call [2 x %record.Tag_UInt32_] @method.UInt32.make_while(i32 %receiver)");
    assert_ir_contains(result, " = call [2 x %record.Tag_UInt32_] @method.UInt32.make_for(i32 %receiver)");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_array_method_return_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_nested_generic_record_array_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "extend UInt32\n"
        "    function nested_array(this: shared This) -> Array<Array<Tag<UInt32>, 2>, 2>\n"
        "        [[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]\n"
        "\n"
        "    function nested_record(this: shared This) -> Array<Tag<Tag<UInt32>>, 2>\n"
        "        [Tag(Tag(5 as UInt32)), Tag(Tag(8 as UInt32))]\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.nested_array()[1][0].code + receiver.nested_record()[1].code.code\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_defines_method(result, "[2 x [2 x %record.Tag_UInt32_]]", "method.UInt32.nested_array", "i32 %this");
    assert_defines_method(result, "[2 x %record.Tag_Tag_UInt32__]", "method.UInt32.nested_record", "i32 %this");
    assert_ir_contains(
        result, " = insertvalue [2 x [2 x %record.Tag_UInt32_]] undef, [2 x %record.Tag_UInt32_] %tmp");
    assert_ir_contains(
        result, " = insertvalue [2 x %record.Tag_Tag_UInt32__] undef, %record.Tag_Tag_UInt32__ %tmp");
    assert_ir_contains(result, " = call [2 x [2 x %record.Tag_UInt32_]] @method.UInt32.nested_array(i32 %receiver)");
    assert_ir_contains(result, " = call [2 x %record.Tag_Tag_UInt32__] @method.UInt32.nested_record(i32 %receiver)");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_record_array_method_return_context() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_nested_record_array_method_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Counter\n"
        "    value: UInt32\n"
        "\n"
        "record CounterPair\n"
        "    left: Counter\n"
        "    right: Counter\n"
        "\n"
        "record CounterMatrix\n"
        "    rows: Array<Array<Counter, 2>, 2>\n"
        "\n"
        "extend UInt32\n"
        "    function pair(this: shared This) -> CounterPair\n"
        "        CounterPair(Counter(5 as UInt32), Counter(8 as UInt32))\n"
        "\n"
        "    function matrix(this: shared This) -> CounterMatrix\n"
        "        CounterMatrix([[Counter(1 as UInt32), Counter(2 as UInt32)], [Counter(3 as UInt32), Counter(4 as UInt32)]])\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.pair().right.value + receiver.matrix().rows[1][0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Counter = type { i32 }");
    assert_ir_contains(result, "%record.CounterPair = type { %record.Counter, %record.Counter }");
    assert_ir_contains(result, "%record.CounterMatrix = type { [2 x [2 x %record.Counter]] }");
    assert_defines_method(result, "%record.CounterPair", "method.UInt32.pair", "i32 %this");
    assert_defines_method(result, "%record.CounterMatrix", "method.UInt32.matrix", "i32 %this");
    assert_ir_contains(result, " = insertvalue %record.CounterPair undef, %record.Counter %tmp");
    assert_ir_contains(result, " = insertvalue %record.CounterMatrix undef, [2 x [2 x %record.Counter]] %tmp");
    assert_ir_contains(result, " = call %record.CounterPair @method.UInt32.pair(i32 %receiver)");
    assert_ir_contains(result, " = call %record.CounterMatrix @method.UInt32.matrix(i32 %receiver)");
    assert_ir_contains(result, " = getelementptr %record.CounterPair, ptr %tmp");
    assert_ir_contains(result, "getelementptr %record.Counter, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterMatrix, ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Counter]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Counter], ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_receiver_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_generic_record_receiver_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "extend Box<UInt32>\n"
        "    function read(this: shared This) -> UInt32\n"
        "        this.value\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: Box<UInt32> = Box(7 as UInt32)\n"
        "    box.read()\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_defines_method(result, "i32", "method.Box_UInt32_.read", "%record.Box_UInt32_ %this");
    assert_ir_contains(result, "%this.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, "store %record.Box_UInt32_ %this, ptr %this.addr");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %this.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_ir_contains(result, " = call i32 @method.Box_UInt32_.read(%record.Box_UInt32_ %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_method_parameter_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_generic_record_method_parameter_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "extend UInt32\n"
        "    function read_box(this: shared This, box: Box<UInt32>) -> UInt32\n"
        "        box.value\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 0 as UInt32\n"
        "    value.read_box(Box(7 as UInt32))\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_defines_method(result, "i32", "method.UInt32.read_box", "i32 %this, %record.Box_UInt32_ %box");
    assert_ir_contains(result, "%box.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, "store %record.Box_UInt32_ %box, ptr %box.addr");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_ir_contains(result, " = call i32 @method.UInt32.read_box(i32 %value, %record.Box_UInt32_ %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_method_parameter_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_array_method_parameter_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "extend UInt32\n"
        "    function read_boxes(this: shared This, boxes: Array<Box<UInt32>, 2>) -> UInt32\n"
        "        boxes[0].value\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 0 as UInt32\n"
        "    value.read_boxes([Box(7 as UInt32), Box(9 as UInt32)])\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_defines_method(result, "i32", "method.UInt32.read_boxes", "i32 %this, [2 x %record.Box_UInt32_] %boxes");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Box_UInt32_]");
    assert_ir_contains(result, "store [2 x %record.Box_UInt32_] %boxes, ptr %boxes.addr");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %boxes.addr, i64 0, i64 0");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_ir_contains(result, " = call i32 @method.UInt32.read_boxes(i32 %value, [2 x %record.Box_UInt32_] %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_array_method_parameter_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_generic_record_array_method_parameter.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "extend UInt32\n"
        "    function read_nested_array(this: shared This, boxes: Array<Array<Tag<UInt32>, 2>, 2>) -> UInt32\n"
        "        boxes[1][0].code\n"
        "\n"
        "    function read_nested_record(this: shared This, boxes: Array<Tag<Tag<UInt32>>, 2>) -> UInt32\n"
        "        boxes[1].code.code\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.read_nested_array([[Tag(1 as UInt32), Tag(2 as UInt32)], [Tag(3 as UInt32), Tag(4 as UInt32)]]) + receiver.read_nested_record([Tag(Tag(5 as UInt32)), Tag(Tag(8 as UInt32))])\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_defines_method(
        result, "i32", "method.UInt32.read_nested_array", "i32 %this, [2 x [2 x %record.Tag_UInt32_]] %boxes");
    assert_defines_method(
        result, "i32", "method.UInt32.read_nested_record", "i32 %this, [2 x %record.Tag_Tag_UInt32__] %boxes");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x [2 x %record.Tag_UInt32_]]");
    assert_ir_contains(result, "store [2 x [2 x %record.Tag_UInt32_]] %boxes, ptr %boxes.addr");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Tag_Tag_UInt32__]");
    assert_ir_contains(result, "store [2 x %record.Tag_Tag_UInt32__] %boxes, ptr %boxes.addr");
    assert_ir_contains(
        result, " = call i32 @method.UInt32.read_nested_array(i32 %receiver, [2 x [2 x %record.Tag_UInt32_]] %tmp");
    assert_ir_contains(
        result, " = call i32 @method.UInt32.read_nested_record(i32 %receiver, [2 x %record.Tag_Tag_UInt32__] %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %boxes.addr");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %boxes.addr");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_record_array_method_parameter_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_record_array_method_parameter.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Counter\n"
        "    value: UInt32\n"
        "\n"
        "record CounterPair\n"
        "    left: Counter\n"
        "    right: Counter\n"
        "\n"
        "record CounterMatrix\n"
        "    rows: Array<Array<Counter, 2>, 2>\n"
        "\n"
        "extend UInt32\n"
        "    function read_pair(this: shared This, pair: CounterPair) -> UInt32\n"
        "        pair.right.value\n"
        "\n"
        "    function read_matrix(this: shared This, matrix: CounterMatrix) -> UInt32\n"
        "        matrix.rows[1][0].value\n"
        "\n"
        "function main() -> UInt32\n"
        "    let receiver: UInt32 = 0 as UInt32\n"
        "    receiver.read_pair(CounterPair(Counter(1 as UInt32), Counter(2 as UInt32))) + receiver.read_matrix(CounterMatrix([[Counter(1 as UInt32), Counter(2 as UInt32)], [Counter(3 as UInt32), Counter(4 as UInt32)]]))\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Counter = type { i32 }");
    assert_ir_contains(result, "%record.CounterPair = type { %record.Counter, %record.Counter }");
    assert_ir_contains(result, "%record.CounterMatrix = type { [2 x [2 x %record.Counter]] }");
    assert_defines_method(result, "i32", "method.UInt32.read_pair", "i32 %this, %record.CounterPair %pair");
    assert_defines_method(result, "i32", "method.UInt32.read_matrix", "i32 %this, %record.CounterMatrix %matrix");
    assert_ir_contains(result, "%pair.addr = alloca %record.CounterPair");
    assert_ir_contains(result, "store %record.CounterPair %pair, ptr %pair.addr");
    assert_ir_contains(result, "%matrix.addr = alloca %record.CounterMatrix");
    assert_ir_contains(result, "store %record.CounterMatrix %matrix, ptr %matrix.addr");
    assert_ir_contains(result, " = call i32 @method.UInt32.read_pair(i32 %receiver, %record.CounterPair %tmp");
    assert_ir_contains(result, " = call i32 @method.UInt32.read_matrix(i32 %receiver, %record.CounterMatrix %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterPair, ptr %pair.addr, i32 0, i32 1");
    assert_ir_contains(result, "getelementptr %record.Counter, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterMatrix, ptr %matrix.addr, i32 0, i32 0");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Counter]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Counter], ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_method_return_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_generic_record_method_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "extend UInt32\n"
        "    function make_box(this: shared This) -> Box<UInt32>\n"
        "        Box(7 as UInt32)\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 0 as UInt32\n"
        "    value.make_box().value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_defines_method(result, "%record.Box_UInt32_", "method.UInt32.make_box", "i32 %this");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = call %record.Box_UInt32_ @method.UInt32.make_box(i32 %value)");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_method_return_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_array_method_return_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "extend UInt32\n"
        "    function make_boxes(this: shared This) -> Array<Box<UInt32>, 2>\n"
        "        [Box(7 as UInt32), Box(9 as UInt32)]\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 0 as UInt32\n"
        "    value.make_boxes()[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_defines_method(result, "[2 x %record.Box_UInt32_]", "method.UInt32.make_boxes", "i32 %this");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 7, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 9, 0");
    assert_ir_contains(result, " = insertvalue [2 x %record.Box_UInt32_] undef, %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, " = call [2 x %record.Box_UInt32_] @method.UInt32.make_boxes(i32 %value)");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main() -> Int32\n"
        "    identity(-27 as Int32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @identity(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %tmp1 = call i32 @identity(i32 %tmp0)\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_final_if_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_final_if_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    if flag\n"
        "        identity(-27 as Int32)\n"
        "    else\n"
        "        identity(4 as Int32)\n"
    );

    assert_emits_negative_int32_control_flow_call_argument_return(
        result,
        " = call i32 @identity(",
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_final_switch_call_argument_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_final_switch_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    switch flag\n"
        "        true => identity(-27 as Int32)\n"
        "        default => identity(4 as Int32)\n"
    );

    assert_emits_negative_int32_control_flow_call_argument_return(
        result,
        " = call i32 @identity(",
        FinalControlFlowKind::switch_expression
    );
}

void test_emit_negative_int32_ternary_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_ternary_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    identity(flag ? -27 as Int32 : 4 as Int32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @identity(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
        "define i32 @main(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %ternary.then.0], [4, %ternary.else.0]\n"
        "  %tmp2 = call i32 @identity(i32 %tmp1)\n"
        "  ret i32 %tmp2\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_negative_uint32_ternary_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_ternary_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    identity(flag ? -1 as UInt32 : 4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_if_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_final_if_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        identity(-1 as UInt32)\n"
        "    else\n"
        "        identity(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_call_argument_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_final_switch_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    switch flag\n"
        "        true => identity(-1 as UInt32)\n"
        "        default => identity(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_record_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_record_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    let box: SignedBox = SignedBox(-27 as Int32)\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_ir_contains(result, " = insertvalue %record.SignedBox undef, i32 %tmp");
    assert_ir_contains(result, "%box.addr = alloca %record.SignedBox");
    assert_ir_contains(result, " = getelementptr %record.SignedBox, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_generic_record_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: Box<UInt32> = Box(1 as UInt32)\n"
        "    box.value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 1, 0");
    assert_ir_contains(result, "%box.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_generic_record_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    let outer: Box<Box<UInt32>> = Box(Box(1 as UInt32))\n"
        "    outer.value.value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Box_Box_UInt32__ = type { %record.Box_UInt32_ }");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 1, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_Box_UInt32__ undef, %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, "%outer.addr = alloca %record.Box_Box_UInt32__");
    assert_ir_contains(result, " = getelementptr %record.Box_Box_UInt32__, ptr %outer.addr, i32 0, i32 0");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_array_element_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_array_element.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    let values: Array<Int32, 2> = [-27 as Int32, 4 as Int32]\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, " = insertvalue [2 x i32] undef, i32 %tmp");
    assert_ir_contains(result, " = insertvalue [2 x i32] %tmp");
    assert_ir_contains(result, "%values.addr = alloca [2 x i32]");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_element_field_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_array_element_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    let boxes: Array<Box<UInt32>, 2> = [Box(1 as UInt32), Box(2 as UInt32)]\n"
        "    boxes[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 1, 0");
    assert_ir_contains(result, " = insertvalue %record.Box_UInt32_ undef, i32 2, 0");
    assert_ir_contains(result, " = insertvalue [2 x %record.Box_UInt32_] undef, %record.Box_UInt32_ %tmp");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Box_UInt32_]");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %boxes.addr, i64 0, i64 0");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_record_field_call_argument_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main() -> Int32\n"
        "    let box: SignedBox = SignedBox(-27 as Int32)\n"
        "    identity(box.value)\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_ir_contains(result, " = insertvalue %record.SignedBox undef, i32 %tmp");
    assert_ir_contains(result, "%box.addr = alloca %record.SignedBox");
    assert_ir_contains(result, " = getelementptr %record.SignedBox, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_calls_lowered_int32_tmp(result, " = call i32 @identity(");
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_record_field_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(flag ? -27 as Int32 : 4 as Int32)\n"
        "    identity(box.value)\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, " = insertvalue %record.SignedBox undef, i32 %tmp");
    assert_ir_contains(result, "%box.addr = alloca %record.SignedBox");
    assert_ir_contains(result, " = getelementptr %record.SignedBox, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_calls_lowered_int32_tmp(result, " = call i32 @identity(");
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_ternary_record_field_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(flag ? -1 as UInt32 : 4 as UInt32)\n"
        "    identity(box.value)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_array_element_call_argument_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main() -> Int32\n"
        "    let values: Array<Int32, 2> = [-27 as Int32, 4 as Int32]\n"
        "    identity(values[0])\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, " = insertvalue [2 x i32] undef, i32 %tmp");
    assert_ir_contains(result, "%values.addr = alloca [2 x i32]");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_calls_lowered_int32_tmp(result, " = call i32 @identity(");
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_array_element_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let values: Array<Int32, 2> = [flag ? -27 as Int32 : 4 as Int32, 5 as Int32]\n"
        "    identity(values[0])\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, " = insertvalue [2 x i32] undef, i32 %tmp");
    assert_ir_contains(result, "%values.addr = alloca [2 x i32]");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_calls_lowered_int32_tmp(result, " = call i32 @identity(");
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_ternary_array_element_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let values: Array<UInt32, 2> = [flag ? -1 as UInt32 : 4 as UInt32, 5 as UInt32]\n"
        "    identity(values[0])\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_final_if_record_field_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_if_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(-27 as Int32)\n"
        "    if flag\n"
        "        identity(box.value)\n"
        "    else\n"
        "        identity(4 as Int32)\n"
    );

    assert_emits_negative_int32_record_field_control_flow_call_argument_return(
        result,
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_final_switch_record_field_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_switch_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let box: SignedBox = SignedBox(-27 as Int32)\n"
        "    switch flag\n"
        "        true => identity(box.value)\n"
        "        default => identity(4 as Int32)\n"
    );

    assert_emits_negative_int32_record_field_control_flow_call_argument_return(
        result,
        FinalControlFlowKind::switch_expression
    );
}

void test_emit_negative_int32_final_if_array_element_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_if_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let values: Array<Int32, 2> = [-27 as Int32, 4 as Int32]\n"
        "    if flag\n"
        "        identity(values[0])\n"
        "    else\n"
        "        identity(4 as Int32)\n"
    );

    assert_emits_negative_int32_array_element_control_flow_call_argument_return(
        result,
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_final_switch_array_element_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_final_switch_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    let values: Array<Int32, 2> = [-27 as Int32, 4 as Int32]\n"
        "    switch flag\n"
        "        true => identity(values[0])\n"
        "        default => identity(4 as Int32)\n"
    );

    assert_emits_negative_int32_array_element_control_flow_call_argument_return(
        result,
        FinalControlFlowKind::switch_expression
    );
}

void test_reject_negative_uint32_record_field_initializer() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_record_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(-1 as UInt32)\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_array_element_initializer() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_array_element.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let values: Array<UInt32, 2> = [-1 as UInt32, 4 as UInt32]\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_record_field_call_argument() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(-1 as UInt32)\n"
        "    identity(box.value)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_array_element_call_argument() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main() -> UInt32\n"
        "    let values: Array<UInt32, 2> = [-1 as UInt32, 4 as UInt32]\n"
        "    identity(values[0])\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_if_record_field_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_if_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(-1 as UInt32)\n"
        "    if flag\n"
        "        identity(box.value)\n"
        "    else\n"
        "        identity(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_record_field_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_switch_record_field_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(-1 as UInt32)\n"
        "    switch flag\n"
        "        true => identity(box.value)\n"
        "        default => identity(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_if_array_element_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_if_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let values: Array<UInt32, 2> = [-1 as UInt32, 4 as UInt32]\n"
        "    if flag\n"
        "        identity(values[0])\n"
        "    else\n"
        "        identity(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_final_switch_array_element_call_argument() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_final_switch_array_element_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    let values: Array<UInt32, 2> = [-1 as UInt32, 4 as UInt32]\n"
        "    switch flag\n"
        "        true => identity(values[0])\n"
        "        default => identity(4 as UInt32)\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_record_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    box.value = -27 as Int32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_ir_contains(result, "%box.addr = alloca %record.SignedBox");
    assert_ir_contains(result, " = getelementptr %record.SignedBox, ptr %box.addr, i32 0, i32 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_generic_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    var box: Box<UInt32> = Box(0 as UInt32)\n"
        "    box.value = 7 as UInt32\n"
        "    box.value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%box.addr = alloca %record.Box_UInt32_");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %box.addr, i32 0, i32 0");
    assert_ir_contains(result, "store i32 7, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_generic_record_array_element_field_assignment_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_generic_record_array_element_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "function main() -> UInt32\n"
        "    var boxes: Array<Box<UInt32>, 2> = [Box(0 as UInt32), Box(1 as UInt32)]\n"
        "    boxes[0].value = 7 as UInt32\n"
        "    boxes[0].value\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Box_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Box_UInt32_]");
    assert_ir_contains(result, " = getelementptr [2 x %record.Box_UInt32_], ptr %boxes.addr, i64 0, i64 0");
    assert_ir_contains(result, " = getelementptr %record.Box_UInt32_, ptr %tmp");
    assert_ir_contains(result, "store i32 7, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_generic_record_array_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_generic_record_array_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "function assign_nested_array() -> UInt32\n"
        "    var boxes: Array<Array<Tag<UInt32>, 2>, 2> = [[Tag(0 as UInt32), Tag(1 as UInt32)], [Tag(2 as UInt32), Tag(3 as UInt32)]]\n"
        "    boxes[1][0].code = 7 as UInt32\n"
        "    boxes[1][0].code\n"
        "\n"
        "function assign_nested_record() -> UInt32\n"
        "    var boxes: Array<Tag<Tag<UInt32>>, 2> = [Tag(Tag(0 as UInt32)), Tag(Tag(1 as UInt32))]\n"
        "    boxes[1].code.code = 9 as UInt32\n"
        "    boxes[1].code.code\n"
        "\n"
        "function main() -> UInt32\n"
        "    assign_nested_array() + assign_nested_record()\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "define i32 @assign_nested_array()");
    assert_ir_contains(result, "define i32 @assign_nested_record()");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x [2 x %record.Tag_UInt32_]]");
    assert_ir_contains(result, "store [2 x [2 x %record.Tag_UInt32_]] %tmp");
    assert_ir_contains(result, "%boxes.addr = alloca [2 x %record.Tag_Tag_UInt32__]");
    assert_ir_contains(result, "store [2 x %record.Tag_Tag_UInt32__] %tmp");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %boxes.addr");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %boxes.addr");
    assert_ir_contains(result, " = getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_ir_contains(result, "store i32 7, ptr %tmp");
    assert_ir_contains(result, "store i32 9, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_nested_record_array_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_record_array_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Counter\n"
        "    value: UInt32\n"
        "\n"
        "record CounterPair\n"
        "    left: Counter\n"
        "    right: Counter\n"
        "\n"
        "record CounterMatrix\n"
        "    rows: Array<Array<Counter, 2>, 2>\n"
        "\n"
        "function assign_nested_record() -> UInt32\n"
        "    var pair: CounterPair = CounterPair(Counter(1 as UInt32), Counter(2 as UInt32))\n"
        "    pair.right.value = 7 as UInt32\n"
        "    pair.right.value\n"
        "\n"
        "function assign_nested_array() -> UInt32\n"
        "    var matrix: CounterMatrix = CounterMatrix([[Counter(1 as UInt32), Counter(2 as UInt32)], [Counter(3 as UInt32), Counter(4 as UInt32)]])\n"
        "    matrix.rows[1][0].value = 9 as UInt32\n"
        "    matrix.rows[1][0].value\n"
        "\n"
        "function main() -> UInt32\n"
        "    assign_nested_record() + assign_nested_array()\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.Counter = type { i32 }");
    assert_ir_contains(result, "%record.CounterPair = type { %record.Counter, %record.Counter }");
    assert_ir_contains(result, "%record.CounterMatrix = type { [2 x [2 x %record.Counter]] }");
    assert_ir_contains(result, "define i32 @assign_nested_record()");
    assert_ir_contains(result, "define i32 @assign_nested_array()");
    assert_ir_contains(result, "%pair.addr = alloca %record.CounterPair");
    assert_ir_contains(result, "%matrix.addr = alloca %record.CounterMatrix");
    assert_ir_contains(result, " = getelementptr %record.CounterPair, ptr %pair.addr, i32 0, i32 1");
    assert_ir_contains(result, "getelementptr %record.Counter, ptr %tmp");
    assert_ir_contains(result, " = getelementptr %record.CounterMatrix, ptr %matrix.addr, i32 0, i32 0");
    assert_ir_contains(result, " = getelementptr [2 x [2 x %record.Counter]], ptr %tmp");
    assert_ir_contains(result, " = getelementptr [2 x %record.Counter], ptr %tmp");
    assert_ir_contains(result, "store i32 7, ptr %tmp");
    assert_ir_contains(result, "store i32 9, ptr %tmp");
    assert_ir_contains(result, " = load i32, ptr %tmp");
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_record_field_assignment_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    box.value = flag ? -27 as Int32 : 4 as Int32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%record.SignedBox = type { i32 }");
    assert_ir_contains(result, "%box.addr = alloca %record.SignedBox");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, " = getelementptr %record.SignedBox, ptr %box.addr, i32 0, i32 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_array_element_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_array_element_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    values[0] = -27 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%values.addr = alloca [2 x i32]");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_array_element_assignment_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_array_element_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    values[0] = flag ? -27 as Int32 : 4 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "%values.addr = alloca [2 x i32]");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_record_field_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    box.value = -1 as UInt32\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_record_field_assignment() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    box.value = flag ? -1 as UInt32 : 4 as UInt32\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_array_element_assignment() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_array_element_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    values[0] = flag ? -1 as UInt32 : 4 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_array_element_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_array_element_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    values[0] = -1 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_if_record_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_if_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    if flag\n"
        "        box.value = -27 as Int32\n"
        "    else\n"
        "        box.value = 4 as Int32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_record_field_control_flow_assignment_return(
        result,
        FinalControlFlowKind::if_expression
    );
}

void test_emit_negative_int32_switch_array_element_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_switch_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(selector: UInt32) -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    switch selector\n"
        "        0 => values[0] = -27 as Int32\n"
        "        default => values[0] = 4 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_array_element_control_flow_assignment_return(
        result,
        FinalControlFlowKind::switch_expression
    );
}

void test_emit_negative_int32_switch_record_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_switch_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main(selector: UInt32) -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    switch selector\n"
        "        0 => box.value = -27 as Int32\n"
        "        default => box.value = 4 as Int32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_record_field_control_flow_assignment_return(
        result,
        FinalControlFlowKind::switch_expression
    );
}

void test_emit_negative_int32_if_array_element_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_if_array_element_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    if flag\n"
        "        values[0] = -27 as Int32\n"
        "    else\n"
        "        values[0] = 4 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_array_element_control_flow_assignment_return(
        result,
        FinalControlFlowKind::if_expression
    );
}

void test_reject_negative_uint32_if_record_field_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_if_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    if flag\n"
        "        box.value = -1 as UInt32\n"
        "    else\n"
        "        box.value = 4 as UInt32\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_switch_array_element_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_switch_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(selector: UInt32) -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    switch selector\n"
        "        0 => values[0] = -1 as UInt32\n"
        "        default => values[0] = 4 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_switch_record_field_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_switch_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main(selector: UInt32) -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    switch selector\n"
        "        0 => box.value = -1 as UInt32\n"
        "        default => box.value = 4 as UInt32\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_if_array_element_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_if_array_element_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    if flag\n"
        "        values[0] = -1 as UInt32\n"
        "    else\n"
        "        values[0] = 4 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_while_record_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_while_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    var index = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        box.value = -27 as Int32\n"
        "        index = index + 1 as UInt32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "while.body.");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_while_record_field_assignment_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_while_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    var index = 0 as UInt32\n"
        "    while index < 1 as UInt32\n"
        "        box.value = flag ? -27 as Int32 : 4 as Int32\n"
        "        index = index + 1 as UInt32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "while.body.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_for_array_element_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_for_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    for item in [0 as UInt32]\n"
        "        values[0] = -27 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_for_array_element_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_ternary_for_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    for item in [0 as UInt32]\n"
        "        values[0] = flag ? -27 as Int32 : 4 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "for.iteration.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_repeat_record_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_repeat_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    repeat\n"
        "        box.value = -27 as Int32\n"
        "    while false\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "repeat.body.");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_repeat_record_field_assignment_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_repeat_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    repeat\n"
        "        box.value = flag ? -27 as Int32 : 4 as Int32\n"
        "    while false\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "repeat.body.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_while_record_field_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_while_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    while true\n"
        "        box.value = -1 as UInt32\n"
        "        break\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_while_record_field_assignment() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_while_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    while true\n"
        "        box.value = flag ? -1 as UInt32 : 4 as UInt32\n"
        "        break\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_for_array_element_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_for_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    for item in [0 as UInt32]\n"
        "        values[0] = -1 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_for_array_element_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_ternary_for_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    for item in [0 as UInt32]\n"
        "        values[0] = flag ? -1 as UInt32 : 4 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_repeat_record_field_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_repeat_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    repeat\n"
        "        box.value = -1 as UInt32\n"
        "    while false\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_repeat_record_field_assignment() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_repeat_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    repeat\n"
        "        box.value = flag ? -1 as UInt32 : 4 as UInt32\n"
        "    while false\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_negative_int32_guard_record_field_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_guard_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    guard flag else\n"
        "        box.value = -27 as Int32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_guard_record_field_assignment_return() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_int32_ternary_guard_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> Int32\n"
        "    var box: SignedBox = SignedBox(0 as Int32)\n"
        "    guard flag else\n"
        "        box.value = choose ? -27 as Int32 : 4 as Int32\n"
        "    box.value\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "guard.failure.");
    assert_ir_contains(result, "guard.merge.");
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_unsafe_array_element_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_unsafe_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    unsafe\n"
        "        values[0] = -27 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_emit_negative_int32_ternary_unsafe_array_element_assignment_return() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_ternary_unsafe_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> Int32\n"
        "    var values: Array<Int32, 2> = [0 as Int32, 4 as Int32]\n"
        "    unsafe\n"
        "        values[0] = flag ? -27 as Int32 : 4 as Int32\n"
        "    values[0]\n"
    );

    assert_emits_negative_int32_value(result);
    assert_ir_contains(result, "ternary.then.");
    assert_ir_contains(result, "ternary.else.");
    assert_ir_contains(result, "ternary.merge.");
    assert_ir_contains(result, " = phi i32 [%tmp");
    assert_ir_contains(result, " = getelementptr [2 x i32], ptr %values.addr, i64 0, i64 0");
    assert_stores_lowered_int32_tmp(result);
    assert_returns_lowered_tmp(result);
}

void test_reject_negative_uint32_guard_record_field_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_guard_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    guard flag else\n"
        "        box.value = -1 as UInt32\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_guard_record_field_assignment() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_negative_uint32_ternary_guard_record_field_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main(flag: Bool, choose: Bool) -> UInt32\n"
        "    var box: UnsignedBox = UnsignedBox(0 as UInt32)\n"
        "    guard flag else\n"
        "        box.value = choose ? -1 as UInt32 : 4 as UInt32\n"
        "    box.value\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_unsafe_array_element_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_unsafe_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    unsafe\n"
        "        values[0] = -1 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_reject_negative_uint32_ternary_unsafe_array_element_assignment() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_ternary_unsafe_array_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var values: Array<UInt32, 2> = [0 as UInt32, 4 as UInt32]\n"
        "    unsafe\n"
        "        values[0] = flag ? -1 as UInt32 : 4 as UInt32\n"
        "    values[0]\n"
    );

    assert_rejects_negative_uint32_cast(result);
}

void test_emit_multi_uint32_parameter_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_multi_uint32_parameter_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function add(left: UInt32, right: UInt32) -> UInt32\n"
        "    left + right\n"
        "\n"
        "function main() -> UInt32\n"
        "    add(40 as UInt32, 2 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @add(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @add(i32 40, i32 2)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_c_foreign_call_with_string_literal() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_c_foreign_string_call.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function printf(format: Pointer<Byte>) -> Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    printf(\"Hello world from Orison!\\n\")\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [26 x i8] c\"Hello world from Orison!\\0A\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 (ptr, ...) @printf(ptr @.str.0)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_fixed_printf_adapter_with_integer_promotion() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_printf_adapter.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, value: Int16) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Hello world from Orison!\\n\", 42 as Int16)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [26 x i8] c\"Hello world from Orison!\\0A\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sext i16 42 to i32\n"
        "  %tmp1 = call i32 (ptr, ...) @printf(ptr @.str.0, i32 %tmp0)\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_fixed_printf_adapter_with_pointer_and_64_bit_arguments() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_printf_wide_arguments.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, text: Pointer<Byte>, signed_value: Int64, unsigned_value: UInt64) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Orison\\n\", \"safe\", 42 as Int64, 84 as UInt64)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [8 x i8] c\"Orison\\0A\\00\"\n"
        "@.str.1 = private unnamed_addr constant [5 x i8] c\"safe\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 (ptr, ...) @printf(ptr @.str.0, ptr @.str.1, i64 42, i64 84)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_fixed_printf_adapter_with_float_promotion() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_printf_float_arguments.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, narrow: Float32, wide: Float64) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Orison\\n\", 1.5 as Float32, 2.5 as Float64)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [8 x i8] c\"Orison\\0A\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = fpext float 1.5 to double\n"
        "  %tmp1 = call i32 (ptr, ...) @printf(ptr @.str.0, double %tmp0, double 2.5)\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_printf_adapter_with_invalid_fixed_prefix() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_invalid_printf_adapter.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(value: Int32) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(42 as Int32)\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "foreign symbol 'printf' does not match the required fixed C ABI prefix"
    );
}

void test_reject_printf_adapter_with_unsupported_trailing_type() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_printf_argument.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, value: Text) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Orison\\n\", \"unsafe representation\")\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "foreign symbol 'printf' parameter 'value' has no supported C variadic ABI representation"
    );
}

void test_emit_fixed_arity_c_foreign_call() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_arity_c_foreign_call.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function strcmp(left: Pointer<Byte>, right: Pointer<Byte>) -> Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    strcmp(\"Orison\", \"Orison\")\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [7 x i8] c\"Orison\\00\"\n"
        "\n"
        "declare i32 @strcmp(ptr, ptr)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @strcmp(ptr @.str.0, ptr @.str.0)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_unsafe_function_identity_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsafe_function_identity.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "unsafe function unsafe_identity(value: UInt32) -> UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @unsafe_identity(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_unsupported_return_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(left: Bool, right: Bool) -> Bool\n"
        "    left < right\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported operator: <"
    );
}

void test_reject_unsigned_unary_negation_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsigned_unary_negation.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function negate(value: UInt32) -> UInt32\n"
        "    -value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported operator: -"
    );
}

void test_reject_boolean_unary_negation_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_boolean_unary_negation.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function negate(value: Bool) -> Bool\n"
        "    -value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported operator: -"
    );
}

void test_emit_scalar_thread_join_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_thread_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let worker = thread\n"
        "        value + 1\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_spawn_failed()") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_thread_join(ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_handle_destroy(ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.4.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_thread_cleanup.on_thread.4.0)"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.4.0(ptr %environment) {\n"
            "entry:\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("icmp eq ptr %worker, null") != std::string::npos);
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "worker.thread.spawn_ok.0:"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_thread_join(ptr %worker)") != std::string::npos);
    assert(result.ir_text.find("load i64, ptr %worker.thread.result") != std::string::npos);
    assert(result.ir_text.find("call void @__orison_concurrency_handle_destroy(ptr %worker)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_abandoned_scalar_thread_cleanup() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_abandoned_thread_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let worker = thread\n"
        "        value + 1\n"
        "\n"
        "    0\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_spawn_failed()") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_handle_destroy(ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_thread_join(ptr)") == std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.4.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_thread_cleanup.on_thread.4.0)"
        ) != std::string::npos
    );
    assert(result.ir_text.find("icmp eq ptr %worker, null") != std::string::npos);
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "worker.thread.spawn_ok.0:"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_handle_destroy(ptr %worker)\n"
            "  ret i64 0"
        ) != std::string::npos
    );
}

void test_emit_scalar_task_await_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_task_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "async function on_task(value: Int64) -> Int64\n"
        "    let pending = task\n"
        "        value + 1\n"
        "\n"
        "    await pending\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_spawn_failed()") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_task_await(ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_handle_destroy(ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.on_task.4.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_task_cleanup.on_task.4.0)"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_task_cleanup.on_task.4.0(ptr %environment) {\n"
            "entry:\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("icmp eq ptr %pending, null") != std::string::npos);
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "pending.task.spawn_ok.0:"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_task_await(ptr %pending)") != std::string::npos);
    assert(result.ir_text.find("load i64, ptr %pending.task.result") != std::string::npos);
    assert(result.ir_text.find("call void @__orison_concurrency_handle_destroy(ptr %pending)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_no_capture_thread_uses_null_cleanup() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_no_capture_thread_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function on_thread() -> Int64\n"
        "    let worker = thread\n"
        "        41\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.4.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 8, ptr null)") != std::string::npos);
    assert(result.ir_text.find("__orison_thread_cleanup.on_thread.4.0") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_thread_join(ptr %worker)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_no_capture_task_uses_null_cleanup() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_no_capture_task_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "async function on_task() -> Int64\n"
        "    let pending = task\n"
        "        41\n"
        "\n"
        "    await pending\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.on_task.4.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 8, ptr null)") != std::string::npos);
    assert(result.ir_text.find("__orison_task_cleanup.on_task.4.0") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_task_await(ptr %pending)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_record_thread_result_storage_size() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_record_thread_result.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Pair\n"
        "    public first: Int64\n"
        "    public second: Int64\n"
        "\n"
        "implements Transferable for Pair\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread() -> Pair\n"
        "    let worker = thread\n"
        "        Pair(1, 2)\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("%record.Pair = type { i64, i64 }") != std::string::npos);
    assert(result.ir_text.find("%worker.thread.result = alloca %record.Pair") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.12.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 16, ptr null)") != std::string::npos);
    assert(result.ir_text.find("store %record.Pair %tmp1, ptr %result_storage") != std::string::npos);
    assert(result.ir_text.find("load %record.Pair, ptr %worker.thread.result") != std::string::npos);
    assert(result.ir_text.find("ret %record.Pair %tmp") != std::string::npos);
}

void test_emit_record_task_result_storage_size() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_record_task_result.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Pair\n"
        "    public first: Int64\n"
        "    public second: Int64\n"
        "\n"
        "implements Shareable for Pair\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "async function on_task() -> Pair\n"
        "    let pending = task\n"
        "        Pair(1, 2)\n"
        "\n"
        "    await pending\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("%record.Pair = type { i64, i64 }") != std::string::npos);
    assert(result.ir_text.find("%pending.task.result = alloca %record.Pair") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.on_task.12.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 16, ptr null)") != std::string::npos);
    assert(result.ir_text.find("store %record.Pair %tmp1, ptr %result_storage") != std::string::npos);
    assert(result.ir_text.find("load %record.Pair, ptr %pending.task.result") != std::string::npos);
    assert(result.ir_text.find("ret %record.Pair %tmp") != std::string::npos);
}

void test_emit_record_capture_cleanup_field_address() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "record OtherPayload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "implements Transferable for OtherPayload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let payload: Payload = Payload(value)\n"
        "    let other: OtherPayload = OtherPayload(value)\n"
        "    let worker = thread\n"
        "        payload.value + other.value\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.planned_drop_actions.size() == 2);
    assert(result.planned_drop_actions[0].capture_name == "payload");
    assert(result.planned_drop_actions[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_actions[0].source_type_name == "Payload");
    assert(result.planned_drop_actions[0].field_index == 0);
    assert(result.planned_drop_actions[0].discovery_line == 20);
    assert(result.planned_drop_actions[1].capture_name == "other");
    assert(result.planned_drop_actions[1].symbol_name == "__orison_drop.OtherPayload");
    assert(result.planned_drop_actions[1].source_type_name == "OtherPayload");
    assert(result.planned_drop_actions[1].field_index == 1);
    assert(result.planned_drop_actions[1].discovery_line == 20);
    auto action_report = result.planned_drop_action_report();
    assert(action_report.size() == 2);
    assert(
        action_report[0] ==
        "planned drop action __orison_drop.Payload for capture payload: Payload field 0 "
        "discovered at line 20 (metadata only)"
    );
    assert(
        action_report[1] ==
        "planned drop action __orison_drop.OtherPayload for capture other: OtherPayload field 1 "
        "discovered at line 20 (metadata only)"
    );
    assert(result.planned_drop_declarations.size() == 2);
    assert(result.planned_drop_declarations[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations[0].source_type_name == "Payload");
    assert(result.planned_drop_declarations[0].discovery_line == 20);
    assert(!result.planned_drop_declarations[0].emit_declaration);
    assert(result.planned_drop_declarations[1].symbol_name == "__orison_drop.OtherPayload");
    assert(result.planned_drop_declarations[1].source_type_name == "OtherPayload");
    assert(result.planned_drop_declarations[1].discovery_line == 20);
    assert(!result.planned_drop_declarations[1].emit_declaration);
    auto report = result.planned_drop_report();
    assert(report.size() == 2);
    assert(
        report[0] ==
        "planned drop __orison_drop.Payload for Payload discovered at line 20 (metadata only)"
    );
    assert(
        report[1] ==
        "planned drop __orison_drop.OtherPayload for OtherPayload discovered at line 20 (metadata only)"
    );
    assert(result.ir_text.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(result.ir_text.find("%record.OtherPayload = type { i64 }") != std::string::npos);
    assert(
        result.ir_text.find(
            "%worker.thread.env = alloca { %record.Payload, %record.OtherPayload }"
        ) != std::string::npos
    );
    assert(result.ir_text.find("store %record.Payload %tmp0, ptr %tmp2") != std::string::npos);
    assert(result.ir_text.find("store %record.OtherPayload %tmp1, ptr %tmp3") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.20.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_thread_cleanup.on_thread.20.0)"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.20.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate payload: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate other: OtherPayload field 1 drop __orison_drop.OtherPayload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
    assert(result.ir_text.find("declare void @__orison_drop.OtherPayload(ptr)") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.OtherPayload(ptr") == std::string::npos);
}

void test_emit_same_type_record_capture_drop_metadata_dedupes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_same_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let left: Payload = Payload(value)\n"
        "    let right: Payload = Payload(value)\n"
        "    let worker = thread\n"
        "        left.value + right.value\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.planned_drop_actions.size() == 2);
    assert(result.planned_drop_actions[0].capture_name == "left");
    assert(result.planned_drop_actions[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_actions[0].field_index == 0);
    assert(result.planned_drop_actions[0].discovery_line == 13);
    assert(result.planned_drop_actions[1].capture_name == "right");
    assert(result.planned_drop_actions[1].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_actions[1].field_index == 1);
    assert(result.planned_drop_actions[1].discovery_line == 13);
    auto action_report = result.planned_drop_action_report();
    assert(action_report.size() == 2);
    assert(
        action_report[0] ==
        "planned drop action __orison_drop.Payload for capture left: Payload field 0 "
        "discovered at line 13 (metadata only)"
    );
    assert(
        action_report[1] ==
        "planned drop action __orison_drop.Payload for capture right: Payload field 1 "
        "discovered at line 13 (metadata only)"
    );
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 13);
    assert(!result.planned_drop_declarations.front().emit_declaration);
    assert(result.emitted_drop_declaration_report().empty());
    assert(result.ir_text.find("%worker.thread.env = alloca { %record.Payload, %record.Payload }") != std::string::npos);
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.13.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate left: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate right: Payload field 1 drop __orison_drop.Payload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
}

void test_emit_allowed_record_capture_drop_abi_calls() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_allowed_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let left: Payload = Payload(value)\n"
        "    let right: Payload = Payload(value)\n"
        "    let worker = thread\n"
        "        left.value + right.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_declared_drop_source_type_allowlist = {"Payload"},
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 13);
    assert(result.planned_drop_declarations.front().emit_declaration);
    auto emitted_report = result.emitted_drop_declaration_report();
    assert(emitted_report.size() == 1);
    assert(emitted_report.front() == "planned drop __orison_drop.Payload for Payload discovered at line 13");
    auto readiness_summary_report = result.drop_readiness_summary_report();
    assert(readiness_summary_report.size() == 1);
    assert(
        readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 0 blocked 0 emitted declarations 1 cleanup authorized 1 blocked 0"
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    assert(result.ir_text.find("define void @__orison_drop.Payload(ptr %value)") == std::string::npos);
    auto readiness_report = result.drop_readiness_snapshot_report();
    assert(readiness_report.size() == 3);
    assert(
        readiness_report[0] ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 1 cleanup authorizations 1"
    );
    assert(readiness_report[1] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(readiness_report[2] == "cleanup readiness __orison_thread_cleanup.on_thread.13.0 authorized");
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.13.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate left: Payload field 0 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.0)\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate right: Payload field 1 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.1)\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
}

void test_emit_semantic_authorized_record_capture_drop_abi_calls() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_semantic_allowed_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let left: Payload = Payload(value)\n"
        "    let right: Payload = Payload(value)\n"
        "    let worker = thread\n"
        "        left.value + right.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "left",
                        .site_line = 11,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 13);
    assert(result.planned_drop_declarations.front().emit_declaration);
    auto emitted_report = result.emitted_drop_declaration_report();
    assert(emitted_report.size() == 1);
    assert(emitted_report.front() == "planned drop __orison_drop.Payload for Payload discovered at line 13");
    auto readiness_summary_report = result.drop_readiness_summary_report();
    assert(readiness_summary_report.size() == 1);
    assert(
        readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 1 blocked 0 emitted declarations 1 cleanup authorized 1 blocked 0"
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.13.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate left: Payload field 0 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.0)\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate right: Payload field 1 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.1)\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
}

void test_reject_partial_record_capture_drop_abi_calls() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_partial_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "record OtherPayload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "implements Transferable for OtherPayload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let payload: Payload = Payload(value)\n"
        "    let other: OtherPayload = OtherPayload(value)\n"
        "    let worker = thread\n"
        "        payload.value + other.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_declared_drop_source_type_allowlist = {"Payload"},
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 20);
    assert(result.planned_drop_declarations.front().emit_declaration);
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_drop.OtherPayload(ptr)") == std::string::npos);
    auto readiness_report = result.drop_readiness_snapshot_report();
    assert(readiness_report.size() == 3);
    assert(
        readiness_report[0] ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 1 cleanup authorizations 1"
    );
    assert(readiness_report[1] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(
        readiness_report[2] ==
        "cleanup readiness __orison_thread_cleanup.on_thread.20.0 blocked semantic blockers 0 missing declarations 1"
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.20.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate payload: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate other: OtherPayload field 1 drop __orison_drop.OtherPayload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.OtherPayload(ptr") == std::string::npos);
}

void test_reject_partial_semantic_authorized_record_capture_drop_abi_calls() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_partial_semantic_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "record OtherPayload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "implements Transferable for OtherPayload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let payload: Payload = Payload(value)\n"
        "    let other: OtherPayload = OtherPayload(value)\n"
        "    let worker = thread\n"
        "        payload.value + other.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "payload",
                        .site_line = 18,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 2);
    assert(result.planned_drop_declarations[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations[0].source_type_name == "Payload");
    assert(result.planned_drop_declarations[0].discovery_line == 20);
    assert(result.planned_drop_declarations[0].emit_declaration);
    assert(result.planned_drop_declarations[1].symbol_name == "__orison_drop.OtherPayload");
    assert(result.planned_drop_declarations[1].source_type_name == "OtherPayload");
    assert(result.planned_drop_declarations[1].discovery_line == 20);
    assert(!result.planned_drop_declarations[1].emit_declaration);
    auto emitted_report = result.emitted_drop_declaration_report();
    assert(emitted_report.size() == 1);
    assert(emitted_report.front() == "planned drop __orison_drop.Payload for Payload discovered at line 20");
    auto readiness_summary_report = result.drop_readiness_summary_report();
    assert(readiness_summary_report.size() == 1);
    assert(
        readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 1 blocked 0 emitted declarations 1 cleanup authorized 0 blocked 1"
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_drop.OtherPayload(ptr)") == std::string::npos);
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.20.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate payload: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate other: OtherPayload field 1 drop __orison_drop.OtherPayload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.OtherPayload(ptr") == std::string::npos);
}

void test_reject_unsupported_final_if_arm_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_final_if_arm.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n"
        "    if flag\n"
        "        left < right\n"
        "    else\n"
        "        false\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this final control-flow statement: "
        "if then arm lowering failed: unsupported operator: <"
    );
}

void test_reject_unsupported_final_switch_case_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_final_switch_case.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n"
        "    switch flag\n"
        "        true => left < right\n"
        "        false => false\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this final control-flow statement: "
        "switch case lowering failed: unsupported operator: <"
    );
}

void test_emit_nested_defer_cleanup_defers() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_then_cleanup(value: UInt32) -> Unit\n"
        "    defer\n"
        "        observe(1 as UInt32)\n"
        "    defer\n"
        "        observe(2 as UInt32)\n"
        "        defer\n"
        "            observe(3 as UInt32)\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @cleanup_then_cleanup");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_call = function_ir.find("call void @observe(i32 2)");
    assert(first_call != std::string::npos);
    auto const second_call = function_ir.find("call void @observe(i32 3)", first_call + 1);
    assert(second_call != std::string::npos);
    auto const third_call = function_ir.find("call void @observe(i32 1)", second_call + 1);
    assert(third_call != std::string::npos);
    auto const ret_void = function_ir.find("ret void");
    assert(ret_void != std::string::npos);
    assert(first_call < second_call);
    assert(second_call < third_call);
    assert(third_call < ret_void);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_multiple_defers() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_cleanup_multi.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_then_cleanup(value: UInt32) -> Unit\n"
        "    defer\n"
        "        observe(1 as UInt32)\n"
        "    defer\n"
        "        observe(2 as UInt32)\n"
        "        defer\n"
        "            observe(3 as UInt32)\n"
        "        defer\n"
        "            observe(4 as UInt32)\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @cleanup_then_cleanup");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_call = function_ir.find("call void @observe(i32 2)");
    assert(first_call != std::string::npos);
    auto const second_call = function_ir.find("call void @observe(i32 4)", first_call + 1);
    assert(second_call != std::string::npos);
    auto const third_call = function_ir.find("call void @observe(i32 3)", second_call + 1);
    assert(third_call != std::string::npos);
    auto const fourth_call = function_ir.find("call void @observe(i32 1)", third_call + 1);
    assert(fourth_call != std::string::npos);
    auto const ret_void = function_ir.find("ret void");
    assert(ret_void != std::string::npos);
    assert(first_call < second_call);
    assert(second_call < third_call);
    assert(third_call < fourth_call);
    assert(fourth_call < ret_void);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_on_loop_control() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_loop_control.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_on_break(value: UInt32) -> Unit\n"
        "    while value < 3 as UInt32\n"
        "        defer\n"
        "            observe(1 as UInt32)\n"
        "        defer\n"
        "            observe(2 as UInt32)\n"
        "            defer\n"
        "                observe(3 as UInt32)\n"
        "        break\n"
        "    return\n"
        "\n"
        "function cleanup_on_continue(value: UInt32) -> Unit\n"
        "    while value < 3 as UInt32\n"
        "        defer\n"
        "            observe(1 as UInt32)\n"
        "        defer\n"
        "            observe(2 as UInt32)\n"
        "            defer\n"
        "                observe(3 as UInt32)\n"
        "        continue\n"
        "    return\n"
    );

    assert(!result.has_errors());

    auto const break_function_pos = result.ir_text.find("define void @cleanup_on_break");
    assert(break_function_pos != std::string::npos);
    auto const break_function_ir = result.ir_text.substr(break_function_pos);
    auto const break_first_call = break_function_ir.find("call void @observe(i32 2)");
    assert(break_first_call != std::string::npos);
    auto const break_second_call = break_function_ir.find(
        "call void @observe(i32 3)",
        break_first_call + 1
    );
    assert(break_second_call != std::string::npos);
    auto const break_third_call = break_function_ir.find(
        "call void @observe(i32 1)",
        break_second_call + 1
    );
    assert(break_third_call != std::string::npos);
    auto const break_exit = break_function_ir.find("br label %while.exit.0");
    assert(break_exit != std::string::npos);
    assert(break_first_call < break_second_call);
    assert(break_second_call < break_third_call);
    assert(break_third_call < break_exit);

    auto const continue_function_pos = result.ir_text.find("define void @cleanup_on_continue");
    assert(continue_function_pos != std::string::npos);
    auto const continue_function_ir = result.ir_text.substr(continue_function_pos);
    auto const continue_first_call = continue_function_ir.find("call void @observe(i32 2)");
    assert(continue_first_call != std::string::npos);
    auto const continue_second_call = continue_function_ir.find(
        "call void @observe(i32 3)",
        continue_first_call + 1
    );
    assert(continue_second_call != std::string::npos);
    auto const continue_third_call = continue_function_ir.find(
        "call void @observe(i32 1)",
        continue_second_call + 1
    );
    assert(continue_third_call != std::string::npos);
    auto const continue_exit = continue_function_ir.find(
        "br label %while.condition.0",
        continue_third_call + 1
    );
    assert(continue_exit != std::string::npos);
    assert(continue_first_call < continue_second_call);
    assert(continue_second_call < continue_third_call);
    assert(continue_third_call < continue_exit);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_on_for_and_repeat() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_for_repeat.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_on_for_continue(value: UInt32) -> Unit\n"
        "    for item in [0 as UInt32, 1 as UInt32, 2 as UInt32]\n"
        "        defer\n"
        "            observe(1 as UInt32)\n"
        "        defer\n"
        "            observe(2 as UInt32)\n"
        "            defer\n"
        "                observe(3 as UInt32)\n"
        "        continue\n"
        "    return\n"
        "\n"
        "function cleanup_on_repeat_break(value: UInt32) -> Unit\n"
        "    repeat\n"
        "        defer\n"
        "            observe(1 as UInt32)\n"
        "        defer\n"
        "            observe(2 as UInt32)\n"
        "            defer\n"
        "                observe(3 as UInt32)\n"
        "        break\n"
        "    while value < 3 as UInt32\n"
        "    return\n"
    );

    assert(!result.has_errors());

    auto const for_function_pos = result.ir_text.find("define void @cleanup_on_for_continue");
    assert(for_function_pos != std::string::npos);
    auto const for_function_ir = result.ir_text.substr(for_function_pos);
    auto const for_first_call = for_function_ir.find("call void @observe(i32 2)");
    assert(for_first_call != std::string::npos);
    auto const for_second_call = for_function_ir.find("call void @observe(i32 3)", for_first_call + 1);
    assert(for_second_call != std::string::npos);
    auto const for_third_call = for_function_ir.find("call void @observe(i32 1)", for_second_call + 1);
    assert(for_third_call != std::string::npos);
    auto const for_continue_exit = for_function_ir.find("br label %for.iteration.0.1");
    assert(for_continue_exit != std::string::npos);
    assert(for_first_call < for_second_call);
    assert(for_second_call < for_third_call);
    assert(for_third_call < for_continue_exit);

    auto const repeat_function_pos = result.ir_text.find("define void @cleanup_on_repeat_break");
    assert(repeat_function_pos != std::string::npos);
    auto const repeat_function_ir = result.ir_text.substr(repeat_function_pos);
    auto const repeat_first_call = repeat_function_ir.find("call void @observe(i32 2)");
    assert(repeat_first_call != std::string::npos);
    auto const repeat_second_call = repeat_function_ir.find(
        "call void @observe(i32 3)",
        repeat_first_call + 1
    );
    assert(repeat_second_call != std::string::npos);
    auto const repeat_third_call = repeat_function_ir.find(
        "call void @observe(i32 1)",
        repeat_second_call + 1
    );
    assert(repeat_third_call != std::string::npos);
    auto const repeat_exit = repeat_function_ir.find("br label %repeat.exit.0");
    assert(repeat_exit != std::string::npos);
    assert(repeat_first_call < repeat_second_call);
    assert(repeat_second_call < repeat_third_call);
    assert(repeat_third_call < repeat_exit);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_in_unsafe_block() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_unsafe.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_on_unsafe_break(value: UInt32) -> Unit\n"
        "    while value < 3 as UInt32\n"
        "        unsafe\n"
        "            defer\n"
        "                observe(1 as UInt32)\n"
        "            defer\n"
        "                observe(2 as UInt32)\n"
        "                defer\n"
        "                    observe(3 as UInt32)\n"
        "            break\n"
        "    return\n"
        "\n"
        "function cleanup_on_unsafe_continue(value: UInt32) -> Unit\n"
        "    while value < 3 as UInt32\n"
        "        unsafe\n"
        "            defer\n"
        "                observe(1 as UInt32)\n"
        "            defer\n"
        "                observe(2 as UInt32)\n"
        "                defer\n"
        "                    observe(3 as UInt32)\n"
        "            continue\n"
        "    return\n"
    );

    assert(!result.has_errors());

    auto const break_function_pos = result.ir_text.find("define void @cleanup_on_unsafe_break");
    assert(break_function_pos != std::string::npos);
    auto const break_function_ir = result.ir_text.substr(break_function_pos);
    auto const break_first_call = break_function_ir.find("call void @observe(i32 2)");
    assert(break_first_call != std::string::npos);
    auto const break_second_call = break_function_ir.find("call void @observe(i32 3)", break_first_call + 1);
    assert(break_second_call != std::string::npos);
    auto const break_third_call = break_function_ir.find("call void @observe(i32 1)", break_second_call + 1);
    assert(break_third_call != std::string::npos);
    auto const break_exit = break_function_ir.find("br label %while.exit.0");
    assert(break_exit != std::string::npos);
    assert(break_first_call < break_second_call);
    assert(break_second_call < break_third_call);
    assert(break_third_call < break_exit);

    auto const continue_function_pos = result.ir_text.find("define void @cleanup_on_unsafe_continue");
    assert(continue_function_pos != std::string::npos);
    auto const continue_function_ir = result.ir_text.substr(continue_function_pos);
    auto const continue_first_call = continue_function_ir.find("call void @observe(i32 2)");
    assert(continue_first_call != std::string::npos);
    auto const continue_second_call = continue_function_ir.find(
        "call void @observe(i32 3)",
        continue_first_call + 1
    );
    assert(continue_second_call != std::string::npos);
    auto const continue_third_call = continue_function_ir.find(
        "call void @observe(i32 1)",
        continue_second_call + 1
    );
    assert(continue_third_call != std::string::npos);
    auto const continue_exit = continue_function_ir.find(
        "br label %while.condition.0",
        continue_third_call + 1
    );
    assert(continue_exit != std::string::npos);
    assert(continue_first_call < continue_second_call);
    assert(continue_second_call < continue_third_call);
    assert(continue_third_call < continue_exit);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_in_for_and_repeat_unsafe_blocks() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_for_repeat_unsafe.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_on_for_unsafe_continue(value: UInt32) -> Unit\n"
        "    for item in [0 as UInt32, 1 as UInt32, 2 as UInt32]\n"
        "        unsafe\n"
        "            defer\n"
        "                observe(1 as UInt32)\n"
        "            defer\n"
        "                observe(2 as UInt32)\n"
        "                defer\n"
        "                    observe(3 as UInt32)\n"
        "            continue\n"
        "    return\n"
        "\n"
        "function cleanup_on_repeat_unsafe_break(value: UInt32) -> Unit\n"
        "    repeat\n"
        "        unsafe\n"
        "            defer\n"
        "                observe(1 as UInt32)\n"
        "            defer\n"
        "                observe(2 as UInt32)\n"
        "                defer\n"
        "                    observe(3 as UInt32)\n"
        "            break\n"
        "    while value < 3 as UInt32\n"
        "    return\n"
    );

    assert(!result.has_errors());

    auto const for_function_pos = result.ir_text.find("define void @cleanup_on_for_unsafe_continue");
    assert(for_function_pos != std::string::npos);
    auto const for_function_ir = result.ir_text.substr(for_function_pos);
    auto const for_first_call = for_function_ir.find("call void @observe(i32 2)");
    assert(for_first_call != std::string::npos);
    auto const for_second_call = for_function_ir.find("call void @observe(i32 3)", for_first_call + 1);
    assert(for_second_call != std::string::npos);
    auto const for_third_call = for_function_ir.find("call void @observe(i32 1)", for_second_call + 1);
    assert(for_third_call != std::string::npos);
    auto const for_exit = for_function_ir.find("br label %for.iteration.0.1");
    assert(for_exit != std::string::npos);
    assert(for_first_call < for_second_call);
    assert(for_second_call < for_third_call);
    assert(for_third_call < for_exit);

    auto const repeat_function_pos = result.ir_text.find("define void @cleanup_on_repeat_unsafe_break");
    assert(repeat_function_pos != std::string::npos);
    auto const repeat_function_ir = result.ir_text.substr(repeat_function_pos);
    auto const repeat_first_call = repeat_function_ir.find("call void @observe(i32 2)");
    assert(repeat_first_call != std::string::npos);
    auto const repeat_second_call = repeat_function_ir.find(
        "call void @observe(i32 3)",
        repeat_first_call + 1
    );
    assert(repeat_second_call != std::string::npos);
    auto const repeat_third_call = repeat_function_ir.find(
        "call void @observe(i32 1)",
        repeat_second_call + 1
    );
    assert(repeat_third_call != std::string::npos);
    auto const repeat_exit = repeat_function_ir.find("br label %repeat.exit.0");
    assert(repeat_exit != std::string::npos);
    assert(repeat_first_call < repeat_second_call);
    assert(repeat_second_call < repeat_third_call);
    assert(repeat_third_call < repeat_exit);
    std::filesystem::remove(path);
}

void test_emit_nonvoid_repeat_for_unsafe_prefixes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nonvoid_repeat_for_unsafe.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> UInt32\n"
        "    repeat\n"
        "        observe(1 as UInt32)\n"
        "    while false\n"
        "    for item in [2 as UInt32, 3 as UInt32]\n"
        "        observe(item)\n"
        "    unsafe\n"
        "        observe(4 as UInt32)\n"
        "    5 as UInt32\n"
    );

    assert(!result.has_errors());
    auto const main_pos = result.ir_text.find("define i32 @main()");
    assert(main_pos != std::string::npos);
    auto const main_ir = result.ir_text.substr(main_pos);
    assert(main_ir.find("repeat.body") != std::string::npos);
    assert(main_ir.find("for.iteration") != std::string::npos);
    assert(main_ir.find("call void @observe(i32 4)") != std::string::npos);
    assert(main_ir.find("ret i32 5") != std::string::npos);
    std::filesystem::remove(path);
}

void test_emit_repeat_body_partial_if_and_switch_flow() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_repeat_body_partial_if_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(value: UInt32) -> Unit\n"
        "    var current = value\n"
        "    repeat\n"
        "        if current == 0 as UInt32\n"
        "            break\n"
        "        switch current\n"
        "            1 => continue\n"
        "            default => current = current + 1 as UInt32\n"
        "    while current < 3 as UInt32\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const main_pos = result.ir_text.find("define void @main");
    assert(main_pos != std::string::npos);
    auto const main_ir = result.ir_text.substr(main_pos);
    assert(main_ir.find("repeat.body.0:") != std::string::npos);
    assert(main_ir.find("if.then.1:") != std::string::npos);
    assert(main_ir.find("if.merge.1:") != std::string::npos);
    assert(main_ir.find("switch.case.2.0:") != std::string::npos);
    assert(main_ir.find("switch.merge.2:") != std::string::npos);
    assert(main_ir.find("br label %repeat.exit.0") != std::string::npos);
    assert(main_ir.find("br label %repeat.condition.0") != std::string::npos);
    std::filesystem::remove(path);
}

void test_emit_for_body_partial_if_and_switch_flow() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_for_body_partial_if_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> Unit\n"
        "    for item in [0 as UInt32, 1 as UInt32]\n"
        "        if item == 0 as UInt32\n"
        "            continue\n"
        "        switch item\n"
        "            1 => break\n"
        "            default => observe(item)\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const main_pos = result.ir_text.find("define void @main");
    assert(main_pos != std::string::npos);
    auto const main_ir = result.ir_text.substr(main_pos);
    assert(main_ir.find("for.iteration.0.0:") != std::string::npos);
    assert(main_ir.find("for.iteration.0.1:") != std::string::npos);
    assert(main_ir.find("if.then.1:") != std::string::npos);
    assert(main_ir.find("if.merge.1:") != std::string::npos);
    assert(main_ir.find("switch.case.2.0:") != std::string::npos);
    assert(main_ir.find("switch.merge.2:") != std::string::npos);
    assert(main_ir.find("br label %for.iteration.0.1") != std::string::npos);
    assert(main_ir.find("br label %for.exit.0") != std::string::npos);
    std::filesystem::remove(path);
}

void test_reject_statements_after_terminating_nonvoid_statement() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nonvoid_terminating_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        return 4 as UInt32\n"
        "    else\n"
        "        return 5 as UInt32\n"
        "    6 as UInt32\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support statements after a terminating non-Unit statement"
    );
    std::filesystem::remove(path);
}

void test_emit_raw_mmio_intrinsics() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_raw_mmio.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UartRegisters\n"
        "    data: UInt32\n"
        "    status: UInt32\n"
        "\n"
        "record Buffer\n"
        "    bytes: Array<Byte, 4>\n"
        "\n"
        "record Log\n"
        "    entries: Array<UartRegisters, 2>\n"
        "\n"
        "record Matrix\n"
        "    rows: Array<Array<Byte, 4>, 2>\n"
        "\n"
        "record Box<T>\n"
        "    value: T\n"
        "\n"
        "record GenericLog\n"
        "    entries: Array<Box<UInt32>, 2>\n"
        "\n"
        "record Device\n"
        "    registers: UartRegisters\n"
        "    buffer: Buffer\n"
        "\n"
        "extend Device\n"
        "    function consume_word_pointer(this: shared This, pointer: Pointer<UInt32>) -> UInt32\n"
        "        0 as UInt32\n"
        "\n"
        "unsafe function read_word(address: Address) -> UInt32\n"
        "    let pointer: Pointer<UInt32> = Pointer(address)\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_next_word(base: Pointer<UInt32>) -> UInt32\n"
        "    let next = raw_offset(base, 1)\n"
        "    raw_read(next)\n"
        "\n"
        "unsafe function read_local_status_pointer() -> UInt32\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    let pointer = Pointer(address_of(regs.status))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_local_status_offset() -> UInt32\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    let pointer = raw_offset(Pointer(address_of(regs.status)), 1 as UInt64)\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function consume_word_pointer(pointer: Pointer<UInt32>) -> UInt32\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_local_status_call_argument() -> UInt32\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    consume_word_pointer(raw_offset(Pointer(address_of(regs.status)), 1 as UInt64))\n"
        "\n"
        "unsafe function read_local_status_member_argument() -> UInt32\n"
        "    let device = Device(UartRegisters(0 as UInt32, 1 as UInt32), Buffer([0 as Byte, 0 as Byte, 0 as Byte, 0 as Byte]))\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    device.consume_word_pointer(raw_offset(Pointer(address_of(regs.status)), 1 as UInt64))\n"
        "\n"
        "unsafe function write_word(address: Address, value: UInt32) -> Unit\n"
        "    let pointer: Pointer<UInt32> = Pointer(address)\n"
        "    raw_write(pointer, value)\n"
        "\n"
        "public function clear(address: Address) -> Unit\n"
        "    unsafe\n"
        "        let pointer: Pointer<Byte> = Pointer(address)\n"
        "        volatile_write(pointer, 0 as Byte)\n"
        "\n"
        "unsafe function status_address(regs: Pointer<UartRegisters>) -> Address\n"
        "    address_of(regs.status)\n"
        "\n"
        "unsafe function nested_status_address(device: Pointer<Device>) -> Address\n"
        "    address_of(device.registers.status)\n"
        "\n"
        "unsafe function byte_address(buffer: Pointer<Buffer>, index: UInt64) -> Address\n"
        "    address_of(buffer.bytes[index])\n"
        "\n"
        "unsafe function nested_byte_address(device: Pointer<Device>, index: UInt64) -> Address\n"
        "    address_of(device.buffer.bytes[index])\n"
        "\n"
        "unsafe function entry_status_address(log: Pointer<Log>, index: UInt64) -> Address\n"
        "    address_of(log.entries[index].status)\n"
        "\n"
        "unsafe function read_entry_status_pointer(log: Pointer<Log>, index: UInt64) -> UInt32\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_box_value_pointer(box: Pointer<Box<UInt32>>) -> UInt32\n"
        "    let pointer = Pointer(address_of(box.value))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_generic_entry_value_pointer(log: Pointer<GenericLog>, index: UInt64) -> UInt32\n"
        "    let pointer = Pointer(address_of(log.entries[index].value))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function write_entry_status_pointer(log: Pointer<Log>, index: UInt64, value: UInt32) -> Unit\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    raw_write(pointer, value)\n"
        "\n"
        "unsafe function assign_box_value_pointer(box: Pointer<Box<UInt32>>, value: UInt32) -> Unit\n"
        "    box.value = value\n"
        "    return\n"
        "\n"
        "unsafe function assign_generic_entry_value_pointer(log: Pointer<GenericLog>, index: UInt64, value: UInt32) -> Unit\n"
        "    log.entries[index].value = value\n"
        "    return\n"
        "\n"
        "unsafe function volatile_read_entry_status_pointer(log: Pointer<Log>, index: UInt64) -> UInt32\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    volatile_read(pointer)\n"
        "\n"
        "unsafe function volatile_write_entry_status_pointer(log: Pointer<Log>, index: UInt64, value: UInt32) -> Unit\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    volatile_write(pointer, value)\n"
        "\n"
        "unsafe function read_entry_status_offset(log: Pointer<Log>, index: UInt64) -> UInt32\n"
        "    let pointer = raw_offset(Pointer(address_of(log.entries[index].status)), 1 as UInt64)\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function volatile_write_entry_status_offset(log: Pointer<Log>, index: UInt64, value: UInt32) -> Unit\n"
        "    let pointer = raw_offset(Pointer(address_of(log.entries[index].status)), 1 as UInt64)\n"
        "    volatile_write(pointer, value)\n"
        "\n"
        "unsafe function matrix_byte_address(matrix: Pointer<Matrix>, index: UInt64, inner: UInt64) -> Address\n"
        "    address_of(matrix.rows[index][inner])\n"
        "\n"
        "unsafe function local_status_address() -> Address\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    address_of(regs.status)\n"
        "\n"
        "unsafe function local_byte_address(index: UInt64) -> Address\n"
        "    var buffer = Buffer([1, 2, 3, 4])\n"
        "    address_of(buffer.bytes[index])\n"
        "\n"
        "unsafe function local_entry_status_address(index: UInt64) -> Address\n"
        "    var log = Log([UartRegisters(1 as UInt32, 2 as UInt32), UartRegisters(3 as UInt32, 4 as UInt32)])\n"
        "    address_of(log.entries[index].status)\n"
        "\n"
        "unsafe function local_matrix_byte_address(index: UInt64, inner: UInt64) -> Address\n"
        "    var matrix = Matrix([[1, 2, 3, 4], [5, 6, 7, 8]])\n"
        "    address_of(matrix.rows[index][inner])\n"
        "\n"
        "unsafe function local_array_byte_address(index: UInt64) -> Address\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    address_of(bytes[index])\n"
        "\n"
        "unsafe function reassign_local_status() -> Address\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    regs = UartRegisters(2 as UInt32, 3 as UInt32)\n"
        "    address_of(regs.status)\n"
        "\n"
        "unsafe function reassign_local_array_byte(index: UInt64) -> Address\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    bytes = [5, 6, 7, 8]\n"
        "    address_of(bytes[index])\n"
        "\n"
        "unsafe function assign_local_status() -> Unit\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    regs.status = 4 as UInt32\n"
        "    return\n"
        "\n"
        "unsafe function compound_assign_local_status() -> Unit\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    regs.status += 4 as UInt32\n"
        "    return\n"
        "\n"
        "unsafe function assign_local_array_byte(index: UInt64) -> Unit\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    bytes[index] = 9\n"
        "    return\n"
        "\n"
        "unsafe function compound_assign_local_array_byte(index: UInt64) -> Unit\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    bytes[index] += 9\n"
        "    return\n"
        "\n"
        "unsafe function assign_pointer_aggregate(regs: Pointer<UartRegisters>, buffer: Pointer<Buffer>, index: UInt64) -> Unit\n"
        "    regs.status = 4 as UInt32\n"
        "    buffer.bytes[index] = 7\n"
        "    return\n"
        "\n"
        "unsafe function compound_assign_pointer_aggregate(regs: Pointer<UartRegisters>, buffer: Pointer<Buffer>, index: UInt64) -> Unit\n"
        "    regs.status += 4 as UInt32\n"
        "    buffer.bytes[index] += 7\n"
        "    return\n"
        "\n"
        "unsafe function assign_nested_pointer_aggregate(log: Pointer<Log>, matrix: Pointer<Matrix>, index: UInt64, inner: UInt64) -> Unit\n"
        "    log.entries[index].status = 8 as UInt32\n"
        "    matrix.rows[index][inner] = 1 as Byte\n"
        "    return\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("%record.UartRegisters = type { i32, i32 }") != std::string::npos);
    assert(result.ir_text.find("%record.Buffer = type { [4 x i8] }") != std::string::npos);
    assert(result.ir_text.find("%record.Log = type { [2 x %record.UartRegisters] }") != std::string::npos);
    assert(result.ir_text.find("%record.Matrix = type { [2 x [4 x i8]] }") != std::string::npos);
    assert(result.ir_text.find("%record.GenericLog = type { [2 x %record.Box_UInt32_] }") != std::string::npos);
    assert(result.ir_text.find("%record.Box_UInt32_ = type { i32 }") != std::string::npos);
    assert(
        result.ir_text.find("%record.Device = type { %record.UartRegisters, %record.Buffer }") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i32 @read_word(i64 %address)") != std::string::npos);
    assert(result.ir_text.find("load i32, ptr") != std::string::npos);
    assert(result.ir_text.find("define void @write_word(i64 %address, i32 %value)") != std::string::npos);
    assert(result.ir_text.find("store i32 %value, ptr") != std::string::npos);
    assert(result.ir_text.find("define void @clear(i64 %address)") != std::string::npos);
    assert(result.ir_text.find("store volatile i8 0, ptr") != std::string::npos);
    assert(result.ir_text.find("define i64 @status_address(ptr %regs)") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %regs, i32 0, i32 1") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @nested_status_address(ptr %device)") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.Device, ptr %device, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %tmp") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @byte_address(ptr %buffer, i64 %index)") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.Buffer, ptr %buffer, i32 0, i32 0") !=
        std::string::npos
    );
    assert(result.ir_text.find("getelementptr [4 x i8], ptr %tmp") != std::string::npos);
    assert(
        result.ir_text.find("define i64 @nested_byte_address(ptr %device, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Device, ptr %device, i32 0, i32 1") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i64 @entry_status_address(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Log, ptr %log, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x %record.UartRegisters], ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i32 @read_entry_status_pointer(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(result.ir_text.find("load i32, ptr %pointer") != std::string::npos);
    assert(
        result.ir_text.find("define i32 @read_box_value_pointer(ptr %box)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Box_UInt32_, ptr %box, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i32 @read_generic_entry_value_pointer(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.GenericLog, ptr %log, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x %record.Box_UInt32_], ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Box_UInt32_, ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define void @write_entry_status_pointer(ptr %log, i64 %index, i32 %value)") !=
        std::string::npos
    );
    assert(result.ir_text.find("store i32 %value, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define void @assign_box_value_pointer(ptr %box, i32 %value)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Box_UInt32_, ptr %box, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define void @assign_generic_entry_value_pointer(ptr %log, i64 %index, i32 %value)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.GenericLog, ptr %log, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x %record.Box_UInt32_], ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Box_UInt32_, ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i32 @volatile_read_entry_status_pointer(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(result.ir_text.find("load volatile i32, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define void @volatile_write_entry_status_pointer(ptr %log, i64 %index, i32 %value)") !=
        std::string::npos
    );
    assert(result.ir_text.find("store volatile i32 %value, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define i32 @read_entry_status_offset(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(result.ir_text.find("getelementptr i32, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define void @volatile_write_entry_status_offset(ptr %log, i64 %index, i32 %value)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i64 @matrix_byte_address(ptr %matrix, i64 %index, i64 %inner)") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @local_status_address()") != std::string::npos);
    assert(
        result.ir_text.find("%tmp0 = insertvalue %record.UartRegisters undef, i32 0, 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("%tmp1 = insertvalue %record.UartRegisters %tmp0, i32 1, 1") !=
        std::string::npos
    );
    assert(result.ir_text.find("%regs.addr = alloca %record.UartRegisters") != std::string::npos);
    assert(
        result.ir_text.find("store %record.UartRegisters %tmp1, ptr %regs.addr") != std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %regs.addr, i32 0, i32 1") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @local_byte_address(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store %record.Buffer %tmp") != std::string::npos);
    assert(result.ir_text.find("define i64 @local_entry_status_address(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store %record.Log %tmp") != std::string::npos);
    assert(
        result.ir_text.find("define i64 @local_matrix_byte_address(i64 %index, i64 %inner)") !=
        std::string::npos
    );
    assert(result.ir_text.find("store %record.Matrix %tmp") != std::string::npos);
    assert(result.ir_text.find("define i64 @local_array_byte_address(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("%bytes.addr = alloca [4 x i8]") != std::string::npos);
    assert(result.ir_text.find("store [4 x i8] %tmp") != std::string::npos);
    assert(result.ir_text.find("getelementptr [4 x i8], ptr %bytes.addr, i64 0, i64 %index") != std::string::npos);
    assert(result.ir_text.find("define i64 @reassign_local_status()") != std::string::npos);
    assert(result.ir_text.find("store %record.UartRegisters %tmp") != std::string::npos);
    assert(result.ir_text.find("define i64 @reassign_local_array_byte(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store [4 x i8] %tmp") != std::string::npos);
    assert(result.ir_text.find("define void @assign_local_status()") != std::string::npos);
    assert(result.ir_text.find("store i32 4, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("define void @compound_assign_local_status()") != std::string::npos);
    assert(result.ir_text.find(" = load i32, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find(" = add i32 ") != std::string::npos);
    assert(result.ir_text.find("define void @assign_local_array_byte(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store i8 9, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("define void @compound_assign_local_array_byte(i64 %index)") != std::string::npos);
    assert(result.ir_text.find(" = load i8, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find(" = add i8 ") != std::string::npos);
    assert(
        result.ir_text.find("define void @assign_pointer_aggregate(ptr %regs, ptr %buffer, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %regs, i32 0, i32 1") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Buffer, ptr %buffer, i32 0, i32 0") !=
        std::string::npos
    );
    assert(result.ir_text.find("store i32 4, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("store i8 7, ptr %tmp") != std::string::npos);
    assert(
        result.ir_text.find("define void @compound_assign_pointer_aggregate(ptr %regs, ptr %buffer, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define void @assign_nested_pointer_aggregate(ptr %log, ptr %matrix, i64 %index, i64 %inner)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Log, ptr %log, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x %record.UartRegisters], ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Matrix, ptr %matrix, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x [4 x i8]], ptr %tmp") != std::string::npos
    );
    assert(result.ir_text.find("store i32 8, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("store i8 1, ptr %tmp") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.Matrix, ptr %matrix, i32 0, i32 0") !=
        std::string::npos
    );
    assert(result.ir_text.find("getelementptr [2 x [4 x i8]], ptr %tmp") != std::string::npos);
    std::filesystem::remove(path);
}

void test_emit_nested_generic_pointer_paths() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_generic_pointer_paths.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Tag<T>\n"
        "    code: T\n"
        "\n"
        "record NestedArrayLog\n"
        "    entries: Array<Array<Tag<UInt32>, 2>, 2>\n"
        "\n"
        "record NestedRecordLog\n"
        "    entries: Array<Tag<Tag<UInt32>>, 2>\n"
        "\n"
        "unsafe function read_nested_array_entry(log: Pointer<NestedArrayLog>, index: UInt64, inner: UInt64) -> UInt32\n"
        "    let pointer = Pointer(address_of(log.entries[index][inner].code))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_nested_record_entry(log: Pointer<NestedRecordLog>, index: UInt64) -> UInt32\n"
        "    let pointer = Pointer(address_of(log.entries[index].code.code))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function assign_nested_array_entry(log: Pointer<NestedArrayLog>, index: UInt64, inner: UInt64, value: UInt32) -> Unit\n"
        "    log.entries[index][inner].code = value\n"
        "    return\n"
        "\n"
        "unsafe function assign_nested_record_entry(log: Pointer<NestedRecordLog>, index: UInt64, value: UInt32) -> Unit\n"
        "    log.entries[index].code.code = value\n"
        "    return\n"
    );

    assert(!result.has_errors());
    assert_ir_contains(result, "%record.NestedArrayLog = type { [2 x [2 x %record.Tag_UInt32_]] }");
    assert_ir_contains(result, "%record.NestedRecordLog = type { [2 x %record.Tag_Tag_UInt32__] }");
    assert_ir_contains(result, "%record.Tag_Tag_UInt32__ = type { %record.Tag_UInt32_ }");
    assert_ir_contains(result, "%record.Tag_UInt32_ = type { i32 }");
    assert_ir_contains(result, "define i32 @read_nested_array_entry(ptr %log, i64 %index, i64 %inner)");
    assert_ir_contains(result, "define i32 @read_nested_record_entry(ptr %log, i64 %index)");
    assert_ir_contains(result, "define void @assign_nested_array_entry(ptr %log, i64 %index, i64 %inner, i32 %value)");
    assert_ir_contains(result, "define void @assign_nested_record_entry(ptr %log, i64 %index, i32 %value)");
    assert_ir_contains(result, "getelementptr %record.NestedArrayLog, ptr %log, i32 0, i32 0");
    assert_ir_contains(result, "getelementptr [2 x [2 x %record.Tag_UInt32_]], ptr %tmp");
    assert_ir_contains(result, "getelementptr [2 x %record.Tag_UInt32_], ptr %tmp");
    assert_ir_contains(result, "getelementptr %record.NestedRecordLog, ptr %log, i32 0, i32 0");
    assert_ir_contains(result, "getelementptr [2 x %record.Tag_Tag_UInt32__], ptr %tmp");
    assert_ir_contains(result, "getelementptr %record.Tag_Tag_UInt32__, ptr %tmp");
    assert_ir_contains(result, "getelementptr %record.Tag_UInt32_, ptr %tmp");
    assert_ir_contains(result, "load i32, ptr %tmp");
    assert_ir_contains(result, "store i32 %value, ptr %tmp");
    std::filesystem::remove(path);
}

void test_reject_malformed_generated_llvm_ir() {
    orison::lowering::LlvmIrVerifier verifier;
    auto diagnostics = verifier.verify(
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i1 1\n"
        "}\n"
    );

    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(
        diagnostics.entries().front().message.find("generated LLVM IR failed to parse") != std::string::npos
    );
}

void test_reject_invalid_generated_llvm_module() {
    orison::lowering::LlvmIrVerifier verifier;
    auto diagnostics = verifier.verify(
        "define i32 @main(i1 %condition) {\n"
        "entry:\n"
        "  br i1 %condition, label %left, label %right\n"
        "left:\n"
        "  br label %merge\n"
        "right:\n"
        "  br label %merge\n"
        "merge:\n"
        "  %value = phi i32 [1, %left]\n"
        "  ret i32 %value\n"
        "}\n"
    );

    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(
        diagnostics.entries().front().message.find("generated LLVM IR failed verification") != std::string::npos
    );
}

void test_emit_native_object_file() {
    orison::lowering::LlvmObjectEmitter emitter;
    auto result = emitter.emit(
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 42\n"
        "}\n"
    );

    assert(!result.has_errors());
    assert(!result.object_bytes.empty());
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_lowering_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    test_emit_constant_uint32_return();
    test_collects_test_only_dynamic_array_construction_metadata();
    test_collects_test_only_dynamic_array_element_drop_readiness_metadata();
    test_derives_dynamic_array_element_cleanup_from_semantic_descriptor_origin();
    test_derives_dynamic_array_deallocation_only_cleanup_from_scalar_descriptor_origin();
    test_binds_test_only_dynamic_array_parameter_descriptor_origin();
    test_emits_authorized_owned_dynamic_array_parameter_cleanup();
    test_emits_authorized_owned_local_dynamic_array_cleanup();
    test_emits_authorized_owned_dynamic_array_parameter_cleanup_on_guard_failure_return();
    test_emits_authorized_owned_dynamic_array_parameter_cleanup_after_if_arm_defer_return();
    test_emits_authorized_owned_dynamic_array_parameter_cleanup_on_explicit_unit_returns();
    test_emits_authorized_owned_dynamic_array_parameter_cleanup_after_switch_case_defer_return();
    test_emits_dynamic_array_parameter_cleanup_on_explicit_unit_returns();
    test_emits_dynamic_array_parameter_cleanup_on_guard_failure_return();
    test_emits_dynamic_array_parameter_cleanup_after_if_arm_defer_return();
    test_emits_dynamic_array_parameter_cleanup_after_switch_case_defer_return();
    test_defers_but_delays_dynamic_array_parameter_cleanup_on_loop_break();
    test_defers_but_delays_dynamic_array_parameter_cleanup_on_loop_continue();
    test_dynamic_array_element_drop_readiness_requires_semantic_authorization();
    test_emit_carries_semantic_drop_lowering_authorization_metadata();
    test_emit_let_bound_uint32_return();
    test_emit_mutable_uint32_assignment_return();
    test_emit_mutable_uint32_compound_assignment_return();
    test_emit_negative_int32_var_initializer_return();
    test_emit_negative_int32_let_initializer_return();
    test_reject_negative_uint32_cast_return();
    test_emit_negative_int32_cast_return();
    test_emit_negative_int32_ternary_return();
    test_reject_negative_uint32_ternary_return();
    test_emit_mutable_int32_compound_assignment_return();
    test_reject_bool_compound_assignment();
    test_reject_bool_aggregate_compound_assignment();
    test_emit_mutable_uint32_while_return();
    test_emit_while_call_statement();
    test_emit_while_unit_call_statement();
    test_emit_while_local_bindings();
    test_restore_outer_binding_after_while_local_shadow();
    test_emit_terminal_while_break();
    test_emit_terminal_while_continue();
    test_emit_defer_cleanup_on_early_return();
    test_emit_conditional_while_continue_and_break();
    test_emit_partial_switch_in_while_body();
    test_emit_terminating_while_if_else();
    test_emit_nested_while_if_control();
    test_emit_nested_while_loop_body();
    test_emit_nested_repeat_in_while_body();
    test_emit_nested_for_in_while_body();
    test_emit_helper_and_method_returned_fixed_array_for_iterables();
    test_reject_nonterminal_while_loop_control();
    test_reject_unsupported_while_body_statement();
    test_emit_scalar_extension_method_definition();
    test_emit_negative_int32_scalar_method_return();
    test_reject_negative_uint32_scalar_method_return();
    test_emit_negative_int32_record_receiver_method_return();
    test_reject_negative_uint32_record_receiver_method_return();
    test_emit_negative_int32_record_constructor_method_return();
    test_emit_negative_int32_ternary_record_constructor_method_return();
    test_emit_generic_record_constructor_let_lowering();
    test_emit_negative_int32_array_literal_method_return();
    test_emit_negative_int32_ternary_array_literal_method_return();
    test_reject_negative_uint32_record_constructor_method_return();
    test_reject_negative_uint32_ternary_record_constructor_method_return();
    test_reject_negative_uint32_array_literal_method_return();
    test_reject_negative_uint32_ternary_array_literal_method_return();
    test_emit_negative_int32_final_if_method_return();
    test_emit_negative_int32_final_switch_method_return();
    test_reject_negative_uint32_final_if_method_return();
    test_reject_negative_uint32_final_switch_method_return();
    test_emit_negative_int32_final_if_record_constructor_method_return();
    test_emit_negative_int32_final_switch_record_constructor_method_return();
    test_emit_negative_int32_final_if_array_literal_method_return();
    test_emit_negative_int32_final_switch_array_literal_method_return();
    test_reject_negative_uint32_final_if_record_constructor_method_return();
    test_reject_negative_uint32_final_switch_record_constructor_method_return();
    test_reject_negative_uint32_final_if_array_literal_method_return();
    test_reject_negative_uint32_final_switch_array_literal_method_return();
    test_emit_scalar_member_call_expression();
    test_emit_negative_int32_scalar_member_call_argument_return();
    test_reject_negative_uint32_scalar_member_call_argument();
    test_emit_negative_int32_ternary_scalar_member_call_argument_return();
    test_reject_negative_uint32_ternary_scalar_member_call_argument();
    test_emit_negative_int32_ternary_record_receiver_member_call_argument_return();
    test_reject_negative_uint32_ternary_record_receiver_member_call_argument();
    test_emit_negative_int32_final_if_scalar_member_call_argument_return();
    test_emit_negative_int32_final_switch_scalar_member_call_argument_return();
    test_emit_negative_int32_final_if_record_receiver_member_call_argument_return();
    test_emit_negative_int32_final_switch_record_receiver_member_call_argument_return();
    test_reject_negative_uint32_final_if_scalar_member_call_argument_return();
    test_reject_negative_uint32_final_switch_scalar_member_call_argument_return();
    test_reject_negative_uint32_final_if_record_receiver_member_call_argument_return();
    test_reject_negative_uint32_final_switch_record_receiver_member_call_argument_return();
    test_emit_scalar_member_call_statement();
    test_emit_negative_int32_scalar_member_call_statement_argument();
    test_reject_negative_uint32_scalar_member_call_statement_argument();
    test_emit_negative_int32_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_scalar_member_call_statement_argument();
    test_reject_negative_uint32_ternary_scalar_member_call_statement_argument();
    test_emit_negative_int32_ternary_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_while_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_while_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_repeat_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_repeat_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_for_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_for_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_if_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_if_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_switch_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_switch_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_if_defer_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_if_defer_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_switch_defer_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_switch_defer_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_while_if_defer_continue_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_while_if_defer_continue_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_while_switch_defer_break_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_while_switch_defer_break_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_defer_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_defer_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_defer_return_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_defer_return_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_defer_break_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_defer_break_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_defer_continue_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_defer_continue_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_unsafe_defer_break_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_unsafe_defer_break_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_unsafe_defer_continue_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_unsafe_defer_continue_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_for_defer_continue_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_for_defer_continue_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_repeat_defer_break_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_repeat_defer_break_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_guard_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_guard_record_receiver_member_call_statement_argument();
    test_emit_negative_int32_ternary_unsafe_record_receiver_member_call_statement_argument();
    test_reject_negative_uint32_ternary_unsafe_record_receiver_member_call_statement_argument();
    test_emit_scalar_call_statements();
    test_emit_scalar_unit_call_statements();
    test_emit_negative_int32_ternary_unit_call_statement_argument();
    test_reject_negative_uint32_ternary_unit_call_statement_argument();
    test_emit_uint32_add_return();
    test_emit_uint32_arithmetic_return();
    test_emit_int32_division_return();
    test_emit_uint32_comparison_returns();
    test_emit_int32_comparison_returns();
    test_emit_boolean_expression_returns();
    test_reject_unsigned_unary_negation_return();
    test_reject_boolean_unary_negation_return();
    test_emit_ternary_expression_returns();
    test_emit_final_if_expression_returns();
    test_emit_final_if_branch_local_bindings();
    test_emit_final_if_mutable_branch_prefixes();
    test_emit_nested_final_if_returns();
    test_emit_final_integer_switch_returns();
    test_emit_final_switch_mutable_case_prefixes();
    test_emit_exhaustive_boolean_switch_returns();
    test_emit_switch_nested_in_final_if();
    test_emit_terminating_switch_nested_in_nonfinal_if();
    test_emit_partial_switch_nested_in_nonfinal_if();
    test_emit_partial_switch_in_guard_failure();
    test_emit_terminating_unit_switch_nested_in_nonfinal_if();
    test_emit_partial_unit_switch_nested_in_nonfinal_if();
    test_emit_zero_argument_function_call_return();
    test_emit_zero_argument_function_call_add_return();
    test_emit_single_uint32_parameter_function_call_return();
    test_emit_generic_record_parameter_field_return();
    test_emit_generic_record_array_parameter_field_return();
    test_emit_generic_record_function_return_field_return();
    test_emit_generic_record_array_function_return_field_return();
    test_emit_nested_generic_record_array_function_boundaries();
    test_emit_nested_record_array_function_boundaries();
    test_emit_generic_record_final_if_function_return_field_return();
    test_emit_generic_record_final_switch_function_return_field_return();
    test_emit_generic_record_array_final_if_function_return_field_return();
    test_emit_generic_record_array_final_switch_function_return_field_return();
    test_emit_nested_generic_record_array_function_control_early_returns();
    test_emit_nested_record_array_function_control_early_returns();
    test_emit_nested_generic_record_array_function_final_switch_returns();
    test_emit_nested_record_array_function_final_switch_returns();
    test_emit_nested_generic_record_array_function_loop_built_returns();
    test_emit_nested_record_array_function_loop_built_returns();
    test_emit_generic_record_while_built_function_return_field_return();
    test_emit_generic_record_array_for_built_function_return_field_return();
    test_emit_generic_record_guard_early_return_field_return();
    test_emit_generic_record_array_defer_early_return_field_return();
    test_emit_generic_record_for_item_field_return();
    test_emit_nested_generic_record_for_item_field_return();
    test_emit_bare_generic_record_array_literal_for_item_field_return();
    test_emit_bare_nested_generic_record_array_literal_for_item_field_return();
    test_emit_view_for_iterable_lowering();
    test_emit_view_parameter_index_lowering();
    test_emit_view_parameter_length_lowering();
    test_emit_access_qualified_view_parameter_lowering();
    test_reject_underconstrained_generic_record_array_literal_for_item_field_return();
    test_reject_underconstrained_nested_generic_record_array_literal_for_item_field_return();
    test_reject_underconstrained_generic_record_inferred_let_binding();
    test_reject_underconstrained_generic_record_inferred_var_binding();
    test_emit_generic_record_constructor_call_argument_from_parameter_context();
    test_emit_generic_record_constructor_method_call_argument_from_parameter_context();
    test_emit_generic_record_constructor_method_return_control_flow_context();
    test_emit_generic_record_constructor_method_guard_defer_return_context();
    test_emit_generic_record_constructor_method_loop_built_return_context();
    test_emit_generic_record_array_method_control_flow_early_return_context();
    test_emit_generic_record_array_method_loop_built_return_context();
    test_emit_nested_generic_record_array_method_return_context();
    test_emit_nested_record_array_method_return_context();
    test_emit_generic_record_receiver_field_return();
    test_emit_generic_record_method_parameter_field_return();
    test_emit_generic_record_array_method_parameter_field_return();
    test_emit_nested_generic_record_array_method_parameter_field_return();
    test_emit_nested_record_array_method_parameter_field_return();
    test_emit_generic_record_method_return_field_return();
    test_emit_generic_record_array_method_return_field_return();
    test_emit_negative_int32_call_argument_return();
    test_emit_negative_int32_final_if_call_argument_return();
    test_emit_negative_int32_final_switch_call_argument_return();
    test_emit_negative_int32_ternary_call_argument_return();
    test_reject_negative_uint32_ternary_call_argument_return();
    test_reject_negative_uint32_final_if_call_argument_return();
    test_reject_negative_uint32_final_switch_call_argument_return();
    test_emit_negative_int32_record_field_return();
    test_emit_generic_record_field_return();
    test_emit_nested_generic_record_field_return();
    test_emit_negative_int32_array_element_return();
    test_emit_generic_record_array_element_field_return();
    test_emit_negative_int32_record_field_call_argument_return();
    test_emit_negative_int32_ternary_record_field_call_argument_return();
    test_emit_negative_int32_array_element_call_argument_return();
    test_emit_negative_int32_ternary_array_element_call_argument_return();
    test_emit_negative_int32_final_if_record_field_call_argument_return();
    test_emit_negative_int32_final_switch_record_field_call_argument_return();
    test_emit_negative_int32_final_if_array_element_call_argument_return();
    test_emit_negative_int32_final_switch_array_element_call_argument_return();
    test_reject_negative_uint32_record_field_initializer();
    test_reject_negative_uint32_array_element_initializer();
    test_reject_negative_uint32_record_field_call_argument();
    test_reject_negative_uint32_ternary_record_field_call_argument();
    test_reject_negative_uint32_array_element_call_argument();
    test_reject_negative_uint32_ternary_array_element_call_argument();
    test_reject_negative_uint32_final_if_record_field_call_argument();
    test_reject_negative_uint32_final_switch_record_field_call_argument();
    test_reject_negative_uint32_final_if_array_element_call_argument();
    test_reject_negative_uint32_final_switch_array_element_call_argument();
    test_emit_negative_int32_record_field_assignment_return();
    test_emit_generic_record_field_assignment_return();
    test_emit_generic_record_array_element_field_assignment_return();
    test_emit_nested_generic_record_array_field_assignment_return();
    test_emit_nested_record_array_field_assignment_return();
    test_emit_negative_int32_ternary_record_field_assignment_return();
    test_emit_negative_int32_array_element_assignment_return();
    test_emit_negative_int32_ternary_array_element_assignment_return();
    test_reject_negative_uint32_record_field_assignment();
    test_reject_negative_uint32_ternary_record_field_assignment();
    test_reject_negative_uint32_array_element_assignment();
    test_reject_negative_uint32_ternary_array_element_assignment();
    test_emit_negative_int32_if_record_field_assignment_return();
    test_emit_negative_int32_switch_array_element_assignment_return();
    test_emit_negative_int32_switch_record_field_assignment_return();
    test_emit_negative_int32_if_array_element_assignment_return();
    test_reject_negative_uint32_if_record_field_assignment();
    test_reject_negative_uint32_switch_array_element_assignment();
    test_reject_negative_uint32_switch_record_field_assignment();
    test_reject_negative_uint32_if_array_element_assignment();
    test_emit_negative_int32_while_record_field_assignment_return();
    test_emit_negative_int32_ternary_while_record_field_assignment_return();
    test_emit_negative_int32_for_array_element_assignment_return();
    test_emit_negative_int32_ternary_for_array_element_assignment_return();
    test_emit_negative_int32_repeat_record_field_assignment_return();
    test_emit_negative_int32_ternary_repeat_record_field_assignment_return();
    test_reject_negative_uint32_while_record_field_assignment();
    test_reject_negative_uint32_ternary_while_record_field_assignment();
    test_reject_negative_uint32_for_array_element_assignment();
    test_reject_negative_uint32_ternary_for_array_element_assignment();
    test_reject_negative_uint32_repeat_record_field_assignment();
    test_reject_negative_uint32_ternary_repeat_record_field_assignment();
    test_emit_negative_int32_guard_record_field_assignment_return();
    test_emit_negative_int32_ternary_guard_record_field_assignment_return();
    test_emit_negative_int32_unsafe_array_element_assignment_return();
    test_emit_negative_int32_ternary_unsafe_array_element_assignment_return();
    test_reject_negative_uint32_guard_record_field_assignment();
    test_reject_negative_uint32_ternary_guard_record_field_assignment();
    test_reject_negative_uint32_unsafe_array_element_assignment();
    test_reject_negative_uint32_ternary_unsafe_array_element_assignment();
    test_emit_multi_uint32_parameter_function_call_return();
    test_emit_c_foreign_call_with_string_literal();
    test_emit_fixed_printf_adapter_with_integer_promotion();
    test_emit_fixed_printf_adapter_with_pointer_and_64_bit_arguments();
    test_emit_fixed_printf_adapter_with_float_promotion();
    test_reject_printf_adapter_with_invalid_fixed_prefix();
    test_reject_printf_adapter_with_unsupported_trailing_type();
    test_emit_fixed_arity_c_foreign_call();
    test_emit_unsafe_function_identity_return();
    test_reject_unsupported_return_expression();
    test_emit_scalar_thread_join_expression();
    test_emit_abandoned_scalar_thread_cleanup();
    test_emit_scalar_task_await_expression();
    test_emit_no_capture_thread_uses_null_cleanup();
    test_emit_no_capture_task_uses_null_cleanup();
    test_emit_record_thread_result_storage_size();
    test_emit_record_task_result_storage_size();
    test_emit_record_capture_cleanup_field_address();
    test_emit_same_type_record_capture_drop_metadata_dedupes();
    test_emit_allowed_record_capture_drop_abi_calls();
    test_emit_semantic_authorized_record_capture_drop_abi_calls();
    test_reject_partial_record_capture_drop_abi_calls();
    test_reject_partial_semantic_authorized_record_capture_drop_abi_calls();
    test_reject_unsupported_final_if_arm_expression();
    test_reject_unsupported_final_switch_case_expression();
    test_emit_nested_defer_cleanup_defers();
    test_emit_nested_defer_cleanup_multiple_defers();
    test_emit_nested_defer_cleanup_on_loop_control();
    test_emit_nested_defer_cleanup_on_for_and_repeat();
    test_emit_nested_defer_cleanup_in_unsafe_block();
    test_emit_nested_defer_cleanup_in_for_and_repeat_unsafe_blocks();
    test_emit_nonvoid_repeat_for_unsafe_prefixes();
    test_emit_repeat_body_partial_if_and_switch_flow();
    test_emit_for_body_partial_if_and_switch_flow();
    test_reject_statements_after_terminating_nonvoid_statement();
    test_emit_raw_mmio_intrinsics();
    test_emit_nested_generic_pointer_paths();
    test_reject_malformed_generated_llvm_ir();
    test_reject_invalid_generated_llvm_module();
    test_emit_native_object_file();
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
