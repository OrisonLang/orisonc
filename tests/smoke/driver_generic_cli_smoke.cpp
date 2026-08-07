#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

auto read_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status == 0);
    return output;
}

auto read_failing_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status != 0);
    return output;
}

template <typename SourceLines>
void write_lines(std::filesystem::path const& path, SourceLines const& lines) {
    std::ofstream output(path);
    for (auto line : lines) {
        output << line << '\n';
    }
}

template <typename SourceLines>
void assert_cli_parse_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    write_lines(path, lines);

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

template <typename SourceLines>
void assert_cli_parse_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines
) {
    write_lines(path, lines);

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);
    assert(output.find("parsed ") != std::string::npos);
}

template <typename SourceLines>
void assert_cli_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    write_lines(path, lines);

    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

void assert_cli_emit_llvm_existing_fixture_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    std::string_view expected_message
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

void assert_cli_runtime_indexed_cleanup_audit_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --runtime-indexed-cleanup-audit " + path.string();
    auto output = read_command_output(command);
    assert(output.find("runtime-index cleanup audit entries 1") != std::string::npos);
    assert(output.find(
        "runtime-index partial owner owner holder.items index index element Inner moved Inner "
        "cleanup skip-moved-element constructor-move enabled"
    ) != std::string::npos);
    assert(output.find(
        "runtime-index cleanup capability owner holder.items index index element Inner "
        "proof-ready true sketch-ready true prerequisites ready production enabled"
    ) != std::string::npos);
    assert(output.find(
        "runtime-index cleanup emission-plan owner holder.items index index element Inner "
        "operations 5 prerequisites ready production-gate requested production enabled "
        "length-load planned length-load-slice lowerable loop planned loop-block-slice lowerable "
        "skip planned skip-branch-slice lowerable live-drop planned live-drop-slice lowerable "
        "deallocate planned cleanup-tail-slice lowerable structured-ir-plan complete "
        "comment-ir-preview-lines 5 gated-ir-slice-lines 17"
    ) != std::string::npos);
    assert(output.find(
        "runtime-index cleanup module-ir production-readiness insertion-gate ready "
        "insertion-preview ready candidate ready candidate-verification verified "
        "module-mutation enabled production ready"
    ) != std::string::npos);
    assert(output.find("lowering does not yet support") == std::string::npos);
}

void assert_cli_run_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " run " + path.string();
    auto output = read_command_output(command);
    assert(output.empty());
}

void assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --test-only-runtime-indexed-constructor-move-run " + path.string();
    auto output = read_command_output(command);
    assert(output.empty());
}

void assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    std::string_view expected_message
) {
    auto command = executable.string() + " --test-only-runtime-indexed-constructor-move-run " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

void assert_cli_emit_llvm_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("define i64 @first__UInt64({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("call i32 @first__UInt32({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call i64 @first__UInt64({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("getelementptr i32, ptr %values.dynamic_array_index") != std::string::npos);
    assert(output.find("getelementptr i64, ptr %values.dynamic_array_index") != std::string::npos);
}

void assert_cli_emit_llvm_owned_dynamic_array_generic_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i64 @count_items__Payload({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("call i64 @count_items__Payload({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %values.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %values.dynamic_array_cleanup"
    ) != std::string::npos);
}

void assert_cli_emit_llvm_call_result_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define { ptr, i64, i64 } @make_values()") != std::string::npos);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("%tmp0 = call { ptr, i64, i64 } @make_values()") != std::string::npos);
    assert(output.find("%tmp1 = call i32 @first__UInt32({ ptr, i64, i64 } %tmp0)") != std::string::npos);
    assert(output.find("ret { ptr, i64, i64 } %tmp1") != std::string::npos);
}

void assert_cli_emit_llvm_nested_call_result_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("define i32 @consume__UInt32(i32 %value)") != std::string::npos);
    assert(output.find("%tmp1 = call i32 @first__UInt32({ ptr, i64, i64 } %tmp0)") != std::string::npos);
    assert(output.find("%tmp2 = call i32 @consume__UInt32(i32 %tmp1)") != std::string::npos);
}

void assert_cli_emit_llvm_local_call_result_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("define i32 @consume__UInt32(i32 %value)") != std::string::npos);
    assert(output.find("%value = add i32 0, %tmp1") != std::string::npos);
    assert(output.find("call i32 @consume__UInt32(i32 %value)") != std::string::npos);
}

void assert_cli_emit_llvm_generic_function_constructor_argument_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("%record.Box_UInt32_ = type { i32 }") != std::string::npos);
    assert(output.find("define i32 @value__UInt32(%record.Box_UInt32_ %box)") != std::string::npos);
    assert(output.find("call i32 @value__UInt32(%record.Box_UInt32_ %tmp") != std::string::npos);
}

void assert_cli_emit_llvm_generic_function_ternary_constructor_argument_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("%record.Box_UInt32_ = type { i32 }") != std::string::npos);
    assert(output.find("%tmp2 = phi %record.Box_UInt32_") != std::string::npos);
    assert(output.find("define i32 @value__UInt32(%record.Box_UInt32_ %box)") != std::string::npos);
    assert(output.find("call i32 @value__UInt32(%record.Box_UInt32_ %tmp") != std::string::npos);
}

void assert_cli_emit_llvm_generic_method_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @select_value__UInt64(i64 %value)") != std::string::npos);
    assert(output.find("define i32 @method.UInt32.select__UInt64(i32 %this, i64 %value)") != std::string::npos);
    assert(output.find("define i32 @method.Box_UInt32_.value__UInt32(%record.Box_UInt32_ %this)") != std::string::npos);
    assert(output.find("define i32 @method.Pair_UInt32__UInt64_.first__UInt32__UInt64(%record.Pair_UInt32__UInt64_ %this)") != std::string::npos);
    assert(output.find("define %record.Pair_UInt32__UInt64_ @method.Box_Pair_UInt32__UInt64__.value__Pair_UInt32__UInt64_(%record.Box_Pair_UInt32__UInt64__ %this)") != std::string::npos);
    assert(output.find("call i32 @select_value__UInt64(i64 11)") != std::string::npos);
    assert(output.find("call i32 @method.UInt32.select__UInt64(i32 %seed, i64 9)") != std::string::npos);
    assert(output.find("call i32 @method.Box_UInt32_.value__UInt32(%record.Box_UInt32_ %tmp") != std::string::npos);
    assert(output.find("call i32 @method.Pair_UInt32__UInt64_.first__UInt32__UInt64(%record.Pair_UInt32__UInt64_ %tmp") != std::string::npos);
    assert(output.find("call %record.Pair_UInt32__UInt64_ @method.Box_Pair_UInt32__UInt64__.value__Pair_UInt32__UInt64_(%record.Box_Pair_UInt32__UInt64__ %tmp") != std::string::npos);
}

void assert_cli_emit_llvm_generic_method_inferred_receiver_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("%record.Box_UInt32_ = type { i32 }") != std::string::npos);
    assert(output.find("define i32 @method.Box_UInt32_.value__UInt32(%record.Box_UInt32_ %this)") !=
        std::string::npos);
    assert(output.find("call i32 @method.Box_UInt32_.value__UInt32(%record.Box_UInt32_ %tmp") !=
        std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_generic_method_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find(
        "define i64 @method.DynamicArray_UInt32_.count_with__Payload__UInt32({ ptr, i64, i64 } %this, %record.Payload %value)"
    ) != std::string::npos);
    assert(output.find(
        "call i64 @method.DynamicArray_UInt32_.count_with__Payload__UInt32({ ptr, i64, i64 } %tmp"
    ) != std::string::npos);
    assert(output.find("ret i64 %this.dynamic_array_length0.value") != std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_receiver_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i64 @method.DynamicArray_UInt32_.count__UInt32({ ptr, i64, i64 } %this)") != std::string::npos);
    assert(output.find("call i64 @method.DynamicArray_UInt32_.count__UInt32({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("ret i64 %this.dynamic_array_length0.value") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %this.") == std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_receiver_append_scalar_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define void @method.DynamicArray_UInt32_.append_value__UInt32(ptr %this, i32 %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_UInt32_.append_value__UInt32(ptr %values.addr, i32 3)") !=
        std::string::npos);
    assert(output.find("%this.dynamic_array_append") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_grow") != std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_complete_contract_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define void @method.DynamicArray_Payload_.append_value__Payload(ptr %this, %record.Payload %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_Payload_.append_value__Payload(ptr %values.addr, %record.Payload %tmp") !=
        std::string::npos);
    assert(output.find("define void @method.DynamicArray_UInt32_.replace_first__UInt32(ptr %this, i32 %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_UInt32_.replace_first__UInt32(ptr %values.addr, i32 7)") !=
        std::string::npos);
    assert(output.find("define i64 @method.DynamicArray_UInt32_.count_each__UInt32({ ptr, i64, i64 } %this)") !=
        std::string::npos);
    assert(output.find("call i64 @method.DynamicArray_UInt32_.count_each__UInt32({ ptr, i64, i64 } %tmp") !=
        std::string::npos);
    assert(output.find("%this.sequence_for") != std::string::npos);
    assert(output.find("define void @method.DynamicArray_Payload_.replace_first__Payload(ptr %this, %record.Payload %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_Payload_.replace_first__Payload(ptr %values.addr, %record.Payload %tmp") !=
        std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %this.dynamic_array_assign") !=
        std::string::npos);
    assert(output.find("define i32 @method.DynamicArray_Payload_.first_value__Payload({ ptr, i64, i64 } %this)") !=
        std::string::npos);
    assert(output.find("call i32 @method.DynamicArray_Payload_.first_value__Payload({ ptr, i64, i64 } %tmp") !=
        std::string::npos);
    assert(output.find("%this.dynamic_array_element_path") != std::string::npos);
    assert(output.find("getelementptr %record.Payload, ptr %this.dynamic_array_element_path") !=
        std::string::npos);
    assert(output.find("getelementptr %record.Nested, ptr %tmp") !=
        std::string::npos);
    assert(output.find("define i32 @method.DynamicArray_Payload_.second_byte__Payload({ ptr, i64, i64 } %this)") !=
        std::string::npos);
    assert(output.find("call i32 @method.DynamicArray_Payload_.second_byte__Payload({ ptr, i64, i64 } %tmp") !=
        std::string::npos);
    assert(output.find("getelementptr [2 x i32], ptr %tmp") !=
        std::string::npos);
    assert(output.find("%values.dynamic_array_element_path") != std::string::npos);
    assert(output.find("getelementptr %record.Payload, ptr %values.dynamic_array_element_path") !=
        std::string::npos);
    assert(output.find("define i64 @method.DynamicArray_Payload_.count_each__Payload({ ptr, i64, i64 } %this)") !=
        std::string::npos);
    assert(output.find("call i64 @method.DynamicArray_Payload_.count_each__Payload({ ptr, i64, i64 } %tmp") !=
        std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %values.dynamic_array_cleanup") !=
        std::string::npos);
}

void assert_cli_emit_llvm_choice_dynamic_array_return_payload_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto function_start = output.find(
        "define { ptr, i64, i64 } @return_switch_payload({ i32, { ptr, i64, i64 } } %buffer)"
    );
    auto function_end = output.find("define { ptr, i64, i64 } @make_primary()", function_start);
    auto payload_phi = output.find("phi { ptr, i64, i64 }", function_start);
    auto case_cleanup = output.find("%values.dynamic_array_cleanup", function_start);
    auto primary_parent_cleanup = output.find(
        "%buffer.Primary.values.choice_dynamic_array_cleanup",
        function_start
    );
    auto secondary_parent_cleanup = output.find(
        "%buffer.Secondary.values.choice_dynamic_array_cleanup",
        function_start
    );
    assert(function_start != std::string::npos);
    assert(function_end != std::string::npos);
    assert(payload_phi != std::string::npos);
    assert(case_cleanup == std::string::npos || function_end < case_cleanup);
    assert(primary_parent_cleanup == std::string::npos || function_end < primary_parent_cleanup);
    assert(secondary_parent_cleanup == std::string::npos || function_end < secondary_parent_cleanup);
    assert(output.find("call { ptr, i64, i64 } @return_switch_payload({ i32, { ptr, i64, i64 } } %tmp6)") !=
        std::string::npos);
    assert(output.find("call { ptr, i64, i64 } @return_switch_payload({ i32, { ptr, i64, i64 } } %tmp8)") !=
        std::string::npos);
    assert(output.find("%returned.dynamic_array_length") !=
        std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %returned.dynamic_array_reassign_cleanup") !=
        std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %returned.dynamic_array_reassign_cleanup") !=
        std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %returned.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %returned.dynamic_array_cleanup") !=
        std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto cleanup = output.find("%holder.values.dynamic_array_reassign_cleanup");
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp");
    assert(cleanup != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(cleanup < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto cleanup = output.find("%holder.values.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.values.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp");
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_direct_indexed_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto values_address = output.find("%holder.values.addr");
    auto first_element_address = output.find("%holder.values.element0.reassign.addr");
    auto first_cleanup = output.find("%holder.values.element0.dynamic_array_reassign_cleanup");
    auto first_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.values.element0.dynamic_array_reassign_cleanup"
    );
    auto first_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.element0.dynamic_array_reassign_cleanup"
    );
    auto second_element_address = output.find("%holder.values.element1.reassign.addr");
    auto second_cleanup = output.find("%holder.values.element1.dynamic_array_reassign_cleanup");
    auto second_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.values.element1.dynamic_array_reassign_cleanup"
    );
    auto second_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.element1.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store [2 x { ptr, i64, i64 }] %tmp", second_deallocate);
    assert(values_address != std::string::npos);
    assert(first_element_address != std::string::npos);
    assert(first_cleanup != std::string::npos);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(second_element_address != std::string::npos);
    assert(second_cleanup != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(values_address < first_element_address);
    assert(first_element_address < first_cleanup);
    assert(first_cleanup < first_drop);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < second_element_address);
    assert(second_element_address < second_cleanup);
    assert(first_deallocate < second_cleanup);
    assert(second_cleanup < second_drop);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_direct_indexed_element_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto element_address = output.find("%holder.values.element0.addr");
    auto cleanup = output.find("%holder.values.element0.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.values.element0.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.element0.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(element_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(element_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_indexed_record_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto items_address = output.find("%holder.items.addr");
    auto first_item_address = output.find("%holder.items.element0.reassign.addr");
    auto first_field_address = output.find("%holder.items.element0.values.reassign.addr");
    auto first_cleanup = output.find("%holder.items.element0.values.dynamic_array_reassign_cleanup");
    auto first_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.values.dynamic_array_reassign_cleanup"
    );
    auto first_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.values.dynamic_array_reassign_cleanup"
    );
    auto second_item_address = output.find("%holder.items.element1.reassign.addr");
    auto second_field_address = output.find("%holder.items.element1.values.reassign.addr");
    auto second_cleanup = output.find("%holder.items.element1.values.dynamic_array_reassign_cleanup");
    auto second_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element1.values.dynamic_array_reassign_cleanup"
    );
    auto second_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element1.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store [2 x %record.Item] %tmp", second_deallocate);
    assert(items_address != std::string::npos);
    assert(first_item_address != std::string::npos);
    assert(first_field_address != std::string::npos);
    assert(first_cleanup != std::string::npos);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(second_item_address != std::string::npos);
    assert(second_field_address != std::string::npos);
    assert(second_cleanup != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(items_address < first_item_address);
    assert(first_item_address < first_field_address);
    assert(first_field_address < first_cleanup);
    assert(first_cleanup < first_drop);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < second_item_address);
    assert(second_item_address < second_field_address);
    assert(second_field_address < second_cleanup);
    assert(second_cleanup < second_drop);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_indexed_record_element_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto element_address = output.find("%holder.items.element0.addr");
    auto field_address = output.find("%holder.items.element0.values.addr");
    auto cleanup = output.find("%holder.items.element0.values.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.values.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(element_address != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(element_address < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_indexed_nested_record_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto element_address = output.find("%holder.items.element0.addr");
    auto inner_address = output.find("%holder.items.element0.inner.addr");
    auto field_address = output.find("%holder.items.element0.inner.values.addr");
    auto cleanup = output.find("%holder.items.element0.inner.values.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.inner.values.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.inner.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(element_address != std::string::npos);
    assert(inner_address != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(element_address < inner_address);
    assert(inner_address < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_indexed_nested_record_sibling_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto element_address = output.find("%holder.items.element0.addr");
    auto inner_address = output.find("%holder.items.element0.inner.addr");
    auto field_address = output.find("%holder.items.element0.inner.spare.addr");
    auto cleanup = output.find("%holder.items.element0.inner.spare.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(element_address != std::string::npos);
    assert(inner_address != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(element_address < inner_address);
    assert(inner_address < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_computed_index_nested_record_sibling_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto index_value = output.find("%index = add i64 0, 0");
    auto computed_element_address = output.find("getelementptr [2 x %record.Item], ptr %tmp");
    auto field_address = output.find("getelementptr %record.Inner, ptr %tmp");
    auto cleanup = output.find("%holder.items.element.inner.spare.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(index_value != std::string::npos);
    assert(computed_element_address != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(index_value < computed_element_address);
    assert(computed_element_address < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_dynamic_index_record_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto descriptor = output.find("%items.dynamic_array_index");
    auto bounds_check = output.find(".in_bounds = icmp ult i64 %index", descriptor);
    auto element_address = output.find(".element.addr = getelementptr %record.Item", bounds_check);
    auto field_address = output.find("getelementptr %record.Item, ptr %items.dynamic_array_index", element_address);
    auto cleanup = output.find("%items.element.values.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %items.element.values.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %items.element.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(descriptor != std::string::npos);
    assert(bounds_check != std::string::npos);
    assert(element_address != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(descriptor < bounds_check);
    assert(bounds_check < element_address);
    assert(element_address < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_nested_dynamic_index_record_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto outer_descriptor = output.find("%groups.dynamic_array_index");
    auto outer_bounds = output.find(".in_bounds = icmp ult i64 %group_index", outer_descriptor);
    auto outer_element = output.find(".element.addr = getelementptr %record.Group", outer_bounds);
    auto nested_descriptor = output.find("%groups.element.items.dynamic_array_index", outer_element);
    auto nested_bounds = output.find(".in_bounds = icmp ult i64 %item_index", nested_descriptor);
    auto nested_element = output.find(".element.addr = getelementptr %record.Item", nested_bounds);
    auto field_address = output.find("getelementptr %record.Item, ptr %groups.element.items.dynamic_array_index", nested_element);
    auto cleanup = output.find("%groups.element.items.element.values.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %groups.element.items.element.values.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %groups.element.items.element.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(outer_descriptor != std::string::npos);
    assert(outer_bounds != std::string::npos);
    assert(outer_element != std::string::npos);
    assert(nested_descriptor != std::string::npos);
    assert(nested_bounds != std::string::npos);
    assert(nested_element != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(outer_descriptor < outer_bounds);
    assert(outer_bounds < outer_element);
    assert(outer_element < nested_descriptor);
    assert(nested_descriptor < nested_bounds);
    assert(nested_bounds < nested_element);
    assert(nested_element < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_nested_dynamic_index_sibling_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto outer_descriptor = output.find("%groups.dynamic_array_index");
    auto outer_bounds = output.find(".in_bounds = icmp ult i64 %group_index", outer_descriptor);
    auto outer_element = output.find(".element.addr = getelementptr %record.Group", outer_bounds);
    auto nested_descriptor = output.find("%groups.element.items.dynamic_array_index", outer_element);
    auto nested_bounds = output.find(".in_bounds = icmp ult i64 %item_index", nested_descriptor);
    auto nested_element = output.find(".element.addr = getelementptr %record.Item", nested_bounds);
    auto field_address = output.find(
        "getelementptr %record.Item, ptr %groups.element.items.dynamic_array_index",
        nested_element
    );
    auto cleanup = output.find("%groups.element.items.element.spare.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %groups.element.items.element.spare.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %groups.element.items.element.spare.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(outer_descriptor != std::string::npos);
    assert(outer_bounds != std::string::npos);
    assert(outer_element != std::string::npos);
    assert(nested_descriptor != std::string::npos);
    assert(nested_bounds != std::string::npos);
    assert(nested_element != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(outer_descriptor < outer_bounds);
    assert(outer_bounds < outer_element);
    assert(outer_element < nested_descriptor);
    assert(nested_descriptor < nested_bounds);
    assert(nested_bounds < nested_element);
    assert(nested_element < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_nested_dynamic_index_multi_field_record_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto maker_start = output.find("define %record.Group @make_group");
    auto maker_end = output.find("define i32 @main", maker_start);
    auto stale_moved_local_cleanup = output.find("%items.dynamic_array_cleanup", maker_start);
    auto outer_descriptor = output.find("%groups.dynamic_array_index");
    auto outer_bounds = output.find(".in_bounds = icmp ult i64 %group_index", outer_descriptor);
    auto outer_element = output.find(".element.addr = getelementptr %record.Group", outer_bounds);
    auto nested_descriptor = output.find("%groups.element.items.dynamic_array_index", outer_element);
    auto nested_bounds = output.find(".in_bounds = icmp ult i64 %item_index", nested_descriptor);
    auto nested_element = output.find(".element.addr = getelementptr %record.Item", nested_bounds);
    auto values_address = output.find("%groups.element.items.element.values.reassign.addr", nested_element);
    auto values_cleanup = output.find("%groups.element.items.element.values.dynamic_array_reassign_cleanup");
    auto values_drop = output.find(
        "call void @__orison_drop.Payload(ptr %groups.element.items.element.values.dynamic_array_reassign_cleanup"
    );
    auto values_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %groups.element.items.element.values.dynamic_array_reassign_cleanup"
    );
    auto spare_address = output.find("%groups.element.items.element.spare.reassign.addr", values_deallocate);
    auto spare_cleanup = output.find("%groups.element.items.element.spare.dynamic_array_reassign_cleanup");
    auto spare_drop = output.find(
        "call void @__orison_drop.Payload(ptr %groups.element.items.element.spare.dynamic_array_reassign_cleanup"
    );
    auto spare_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %groups.element.items.element.spare.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store %record.Item %tmp", spare_deallocate);
    assert(maker_start != std::string::npos);
    assert(maker_end != std::string::npos);
    assert(stale_moved_local_cleanup == std::string::npos || maker_end < stale_moved_local_cleanup);
    assert(outer_descriptor != std::string::npos);
    assert(outer_bounds != std::string::npos);
    assert(outer_element != std::string::npos);
    assert(nested_descriptor != std::string::npos);
    assert(nested_bounds != std::string::npos);
    assert(nested_element != std::string::npos);
    assert(values_address != std::string::npos);
    assert(values_cleanup != std::string::npos);
    assert(values_drop != std::string::npos);
    assert(values_deallocate != std::string::npos);
    assert(spare_address != std::string::npos);
    assert(spare_cleanup != std::string::npos);
    assert(spare_drop != std::string::npos);
    assert(spare_deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(outer_descriptor < outer_bounds);
    assert(outer_bounds < outer_element);
    assert(outer_element < nested_descriptor);
    assert(nested_descriptor < nested_bounds);
    assert(nested_bounds < nested_element);
    assert(nested_element < values_address);
    assert(values_address < values_cleanup);
    assert(values_cleanup < values_drop);
    assert(values_drop < values_deallocate);
    assert(values_deallocate < spare_address);
    assert(spare_address < spare_cleanup);
    assert(spare_cleanup < spare_drop);
    assert(spare_drop < spare_deallocate);
    assert(spare_deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_returned_nested_record_field_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto maker_start = output.find("define %record.Outer @make_outer");
    auto maker_end = output.find("define i32 @main", maker_start);
    auto stale_values_cleanup = output.find("%inner.values.dynamic_array_cleanup", maker_start);
    auto stale_spare_cleanup = output.find("%inner.spare.dynamic_array_cleanup", maker_start);
    auto replacement_cleanup = output.find("%outer.inner.values.dynamic_array_reassign_cleanup", maker_end);
    auto replacement_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.inner.values.dynamic_array_reassign_cleanup",
        replacement_cleanup
    );
    auto replacement_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.inner.values.dynamic_array_reassign_cleanup",
        replacement_drop
    );
    auto final_values_cleanup = output.find("%outer.inner.values.dynamic_array_cleanup", replacement_deallocate);
    auto final_spare_cleanup = output.find("%outer.inner.spare.dynamic_array_cleanup", final_values_cleanup);
    assert(maker_start != std::string::npos);
    assert(maker_end != std::string::npos);
    assert(stale_values_cleanup == std::string::npos || maker_end < stale_values_cleanup);
    assert(stale_spare_cleanup == std::string::npos || maker_end < stale_spare_cleanup);
    assert(replacement_cleanup != std::string::npos);
    assert(replacement_drop != std::string::npos);
    assert(replacement_deallocate != std::string::npos);
    assert(final_values_cleanup != std::string::npos);
    assert(final_spare_cleanup != std::string::npos);
    assert(replacement_cleanup < replacement_drop);
    assert(replacement_drop < replacement_deallocate);
    assert(replacement_deallocate < final_values_cleanup);
    assert(final_values_cleanup < final_spare_cleanup);
}

void assert_cli_emit_llvm_dynamic_array_owned_returned_fixed_array_record_field_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto maker_start = output.find("define %record.Outer @make_outer");
    auto maker_end = output.find("define i32 @main", maker_start);
    auto stale_first_values_cleanup = output.find("%items.element0.values.dynamic_array_cleanup", maker_start);
    auto stale_first_spare_cleanup = output.find("%items.element0.spare.dynamic_array_cleanup", maker_start);
    auto stale_second_values_cleanup = output.find("%items.element1.values.dynamic_array_cleanup", maker_start);
    auto stale_second_spare_cleanup = output.find("%items.element1.spare.dynamic_array_cleanup", maker_start);
    auto replacement_cleanup = output.find("%outer.items.element0.values.dynamic_array_reassign_cleanup", maker_end);
    auto replacement_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_cleanup
    );
    auto replacement_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_drop
    );
    auto final_first_values_cleanup = output.find(
        "%outer.items.element0.values.dynamic_array_cleanup",
        replacement_deallocate
    );
    auto final_first_spare_cleanup = output.find(
        "%outer.items.element0.spare.dynamic_array_cleanup",
        final_first_values_cleanup
    );
    auto final_second_values_cleanup = output.find(
        "%outer.items.element1.values.dynamic_array_cleanup",
        final_first_spare_cleanup
    );
    auto final_second_spare_cleanup = output.find(
        "%outer.items.element1.spare.dynamic_array_cleanup",
        final_second_values_cleanup
    );
    assert(maker_start != std::string::npos);
    assert(maker_end != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos || maker_end < stale_first_values_cleanup);
    assert(stale_first_spare_cleanup == std::string::npos || maker_end < stale_first_spare_cleanup);
    assert(stale_second_values_cleanup == std::string::npos || maker_end < stale_second_values_cleanup);
    assert(stale_second_spare_cleanup == std::string::npos || maker_end < stale_second_spare_cleanup);
    assert(replacement_cleanup != std::string::npos);
    assert(replacement_drop != std::string::npos);
    assert(replacement_deallocate != std::string::npos);
    assert(final_first_values_cleanup != std::string::npos);
    assert(final_first_spare_cleanup != std::string::npos);
    assert(final_second_values_cleanup != std::string::npos);
    assert(final_second_spare_cleanup != std::string::npos);
    assert(replacement_cleanup < replacement_drop);
    assert(replacement_drop < replacement_deallocate);
    assert(replacement_deallocate < final_first_values_cleanup);
    assert(final_first_values_cleanup < final_first_spare_cleanup);
    assert(final_first_spare_cleanup < final_second_values_cleanup);
    assert(final_second_values_cleanup < final_second_spare_cleanup);
}

void assert_cli_emit_llvm_dynamic_array_owned_constructor_fixed_array_record_field_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_initial_first_values_cleanup = output.find("%items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_initial_first_spare_cleanup = output.find("%items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_initial_second_values_cleanup = output.find("%items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_initial_second_spare_cleanup = output.find("%items.element1.spare.dynamic_array_cleanup", main_start);
    auto stale_replacement_first_values_cleanup =
        output.find("%replacement_items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_replacement_first_spare_cleanup =
        output.find("%replacement_items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_replacement_second_values_cleanup =
        output.find("%replacement_items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_replacement_second_spare_cleanup =
        output.find("%replacement_items.element1.spare.dynamic_array_cleanup", main_start);
    auto replacement_cleanup = output.find("%outer.items.element0.values.dynamic_array_reassign_cleanup", main_start);
    auto replacement_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_cleanup
    );
    auto replacement_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_drop
    );
    auto final_first_values_cleanup = output.find(
        "%outer.items.element0.values.dynamic_array_cleanup",
        replacement_deallocate
    );
    auto final_first_spare_cleanup = output.find(
        "%outer.items.element0.spare.dynamic_array_cleanup",
        final_first_values_cleanup
    );
    auto final_second_values_cleanup = output.find(
        "%outer.items.element1.values.dynamic_array_cleanup",
        final_first_spare_cleanup
    );
    auto final_second_spare_cleanup = output.find(
        "%outer.items.element1.spare.dynamic_array_cleanup",
        final_second_values_cleanup
    );
    assert(main_start != std::string::npos);
    assert(stale_initial_first_values_cleanup == std::string::npos);
    assert(stale_initial_first_spare_cleanup == std::string::npos);
    assert(stale_initial_second_values_cleanup == std::string::npos);
    assert(stale_initial_second_spare_cleanup == std::string::npos);
    assert(stale_replacement_first_values_cleanup == std::string::npos);
    assert(stale_replacement_first_spare_cleanup == std::string::npos);
    assert(stale_replacement_second_values_cleanup == std::string::npos);
    assert(stale_replacement_second_spare_cleanup == std::string::npos);
    assert(replacement_cleanup != std::string::npos);
    assert(replacement_drop != std::string::npos);
    assert(replacement_deallocate != std::string::npos);
    assert(final_first_values_cleanup != std::string::npos);
    assert(final_first_spare_cleanup != std::string::npos);
    assert(final_second_values_cleanup != std::string::npos);
    assert(final_second_spare_cleanup != std::string::npos);
    assert(replacement_cleanup < replacement_drop);
    assert(replacement_drop < replacement_deallocate);
    assert(replacement_deallocate < final_first_values_cleanup);
    assert(final_first_values_cleanup < final_first_spare_cleanup);
    assert(final_first_spare_cleanup < final_second_values_cleanup);
    assert(final_second_values_cleanup < final_second_spare_cleanup);
}

void assert_cli_emit_llvm_dynamic_array_owned_constructor_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup = output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup = output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup = output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup = output.find("%holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto replacement_cleanup = output.find("%outer.items.element0.values.dynamic_array_reassign_cleanup", main_start);
    auto replacement_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_cleanup
    );
    auto replacement_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_drop
    );
    auto final_first_values_cleanup = output.find(
        "%outer.items.element0.values.dynamic_array_cleanup",
        replacement_deallocate
    );
    auto final_first_spare_cleanup = output.find(
        "%outer.items.element0.spare.dynamic_array_cleanup",
        final_first_values_cleanup
    );
    auto final_second_values_cleanup = output.find(
        "%outer.items.element1.values.dynamic_array_cleanup",
        final_first_spare_cleanup
    );
    auto final_second_spare_cleanup = output.find(
        "%outer.items.element1.spare.dynamic_array_cleanup",
        final_second_values_cleanup
    );
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(replacement_cleanup != std::string::npos);
    assert(replacement_drop != std::string::npos);
    assert(replacement_deallocate != std::string::npos);
    assert(final_first_values_cleanup != std::string::npos);
    assert(final_first_spare_cleanup != std::string::npos);
    assert(final_second_values_cleanup != std::string::npos);
    assert(final_second_spare_cleanup != std::string::npos);
    assert(replacement_cleanup < replacement_drop);
    assert(replacement_drop < replacement_deallocate);
    assert(replacement_deallocate < final_first_values_cleanup);
    assert(final_first_values_cleanup < final_first_spare_cleanup);
    assert(final_first_spare_cleanup < final_second_values_cleanup);
    assert(final_second_values_cleanup < final_second_spare_cleanup);
}

void assert_cli_emit_llvm_dynamic_array_owned_constructor_indexed_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto moved_source_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto moved_source_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto sibling_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto sibling_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", sibling_values_cleanup);
    auto outer_values_cleanup =
        output.find("%outer.item.values.dynamic_array_cleanup", sibling_spare_cleanup);
    auto outer_spare_cleanup =
        output.find("%outer.item.spare.dynamic_array_cleanup", outer_values_cleanup);
    auto final_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.item.spare.dynamic_array_cleanup",
        outer_spare_cleanup
    );
    assert(main_start != std::string::npos);
    assert(moved_source_values_cleanup == std::string::npos);
    assert(moved_source_spare_cleanup == std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(outer_values_cleanup != std::string::npos);
    assert(outer_spare_cleanup != std::string::npos);
    assert(final_deallocate != std::string::npos);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
    assert(sibling_spare_cleanup < outer_values_cleanup);
    assert(outer_values_cleanup < outer_spare_cleanup);
    assert(outer_spare_cleanup < final_deallocate);
}

void assert_cli_emit_llvm_dynamic_array_owned_constructor_indexed_member_path_sibling_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto outer_values_cleanup =
        output.find("%outer.item.values.dynamic_array_cleanup", main_start);
    auto outer_spare_cleanup =
        output.find("%outer.item.spare.dynamic_array_cleanup", outer_values_cleanup);
    auto sibling_values_cleanup =
        output.find("%sibling.item.values.dynamic_array_cleanup", outer_spare_cleanup);
    auto sibling_spare_cleanup =
        output.find("%sibling.item.spare.dynamic_array_cleanup", sibling_values_cleanup);
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(outer_values_cleanup != std::string::npos);
    assert(outer_spare_cleanup != std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(outer_values_cleanup < outer_spare_cleanup);
    assert(outer_spare_cleanup < sibling_values_cleanup);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_member_cleanup = output.find("%holder.values.dynamic_array_cleanup", main_start);
    auto selected_cleanup = output.find("%selected.Some.values.choice_dynamic_array_cleanup", main_start);
    auto selected_drop = output.find(
        "call void @__orison_drop.Payload(ptr %selected.Some.values.choice_dynamic_array_cleanup",
        selected_cleanup
    );
    auto selected_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %selected.Some.values.choice_dynamic_array_cleanup",
        selected_drop
    );
    assert(main_start != std::string::npos);
    assert(stale_member_cleanup == std::string::npos);
    assert(selected_cleanup != std::string::npos);
    assert(selected_drop != std::string::npos);
    assert(selected_deallocate != std::string::npos);
    assert(selected_cleanup < selected_drop);
    assert(selected_drop < selected_deallocate);
}

void assert_cli_emit_llvm_choice_constructor_nested_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto selected_first_values_cleanup =
        output.find("%selected.Some.items.element0.values.choice_dynamic_array_cleanup", main_start);
    auto selected_first_spare_cleanup =
        output.find("%selected.Some.items.element0.spare.choice_dynamic_array_cleanup", selected_first_values_cleanup);
    auto selected_second_values_cleanup =
        output.find("%selected.Some.items.element1.values.choice_dynamic_array_cleanup", selected_first_spare_cleanup);
    auto selected_second_spare_cleanup =
        output.find("%selected.Some.items.element1.spare.choice_dynamic_array_cleanup", selected_second_values_cleanup);
    auto selected_final_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr "
            "%selected.Some.items.element1.spare.choice_dynamic_array_cleanup",
        selected_second_spare_cleanup
    );
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(selected_first_values_cleanup != std::string::npos);
    assert(selected_first_spare_cleanup != std::string::npos);
    assert(selected_second_values_cleanup != std::string::npos);
    assert(selected_second_spare_cleanup != std::string::npos);
    assert(selected_final_deallocate != std::string::npos);
    assert(selected_first_values_cleanup < selected_first_spare_cleanup);
    assert(selected_first_spare_cleanup < selected_second_values_cleanup);
    assert(selected_second_values_cleanup < selected_second_spare_cleanup);
    assert(selected_second_spare_cleanup < selected_final_deallocate);
}

void assert_cli_emit_llvm_choice_constructor_indexed_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto moved_source_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto moved_source_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto sibling_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto sibling_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", sibling_values_cleanup);
    auto selected_values_cleanup =
        output.find("%selected.Some.item.values.choice_dynamic_array_cleanup", sibling_spare_cleanup);
    auto selected_spare_cleanup =
        output.find("%selected.Some.item.spare.choice_dynamic_array_cleanup", selected_values_cleanup);
    auto selected_final_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr "
            "%selected.Some.item.spare.choice_dynamic_array_cleanup1.cleanup.data",
        selected_spare_cleanup
    );
    assert(main_start != std::string::npos);
    assert(moved_source_values_cleanup == std::string::npos);
    assert(moved_source_spare_cleanup == std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(selected_values_cleanup != std::string::npos);
    assert(selected_spare_cleanup != std::string::npos);
    assert(selected_final_deallocate != std::string::npos);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
    assert(sibling_spare_cleanup < selected_values_cleanup);
    assert(selected_values_cleanup < selected_spare_cleanup);
    assert(selected_spare_cleanup < selected_final_deallocate);
}

void assert_cli_emit_llvm_choice_constructor_indexed_member_path_sibling_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto selected_values_cleanup =
        output.find("%selected.Some.item.values.choice_dynamic_array_cleanup", main_start);
    auto selected_spare_cleanup =
        output.find("%selected.Some.item.spare.choice_dynamic_array_cleanup", selected_values_cleanup);
    auto sibling_values_cleanup =
        output.find("%sibling.Some.item.values.choice_dynamic_array_cleanup", selected_spare_cleanup);
    auto sibling_spare_cleanup =
        output.find("%sibling.Some.item.spare.choice_dynamic_array_cleanup", sibling_values_cleanup);
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(selected_values_cleanup != std::string::npos);
    assert(selected_spare_cleanup != std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(selected_values_cleanup < selected_spare_cleanup);
    assert(selected_spare_cleanup < sibling_values_cleanup);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_multi_payload_nested_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto scalar_payload = output.find(
        "insertvalue { [2 x %record.Inner], i32 } %tmp",
        main_start
    );
    auto payload_tuple_extract = output.find(
        "%selected.Ready.items.element0.values.choice_dynamic_array_cleanup.payload.value = "
        "extractvalue { [2 x %record.Inner], i32 }",
        main_start
    );
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto selected_first_values_cleanup = output.find(
        "%selected.Ready.items.element0.values.choice_dynamic_array_cleanup.descriptor.extract",
        payload_tuple_extract
    );
    auto selected_first_spare_cleanup =
        output.find("%selected.Ready.items.element0.spare.choice_dynamic_array_cleanup", selected_first_values_cleanup);
    auto selected_second_values_cleanup =
        output.find("%selected.Ready.items.element1.values.choice_dynamic_array_cleanup", selected_first_spare_cleanup);
    auto selected_second_spare_cleanup =
        output.find("%selected.Ready.items.element1.spare.choice_dynamic_array_cleanup", selected_second_values_cleanup);
    assert(main_start != std::string::npos);
    assert(scalar_payload != std::string::npos);
    assert(payload_tuple_extract != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(selected_first_values_cleanup != std::string::npos);
    assert(selected_first_spare_cleanup != std::string::npos);
    assert(selected_second_values_cleanup != std::string::npos);
    assert(selected_second_spare_cleanup != std::string::npos);
    assert(payload_tuple_extract < selected_first_values_cleanup);
    assert(selected_first_values_cleanup < selected_first_spare_cleanup);
    assert(selected_first_spare_cleanup < selected_second_values_cleanup);
    assert(selected_second_values_cleanup < selected_second_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_multi_payload_second_nested_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto scalar_payload = output.find(
        "insertvalue { i32, [2 x %record.Inner] } undef, i32 7, 0",
        main_start
    );
    auto payload_tuple_extract = output.find(
        "%selected.Ready.items.element0.values.choice_dynamic_array_cleanup.payload.value = "
        "extractvalue { i32, [2 x %record.Inner] }",
        main_start
    );
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto selected_first_values_cleanup = output.find(
        "%selected.Ready.items.element0.values.choice_dynamic_array_cleanup.descriptor.extract",
        payload_tuple_extract
    );
    auto selected_first_spare_cleanup =
        output.find("%selected.Ready.items.element0.spare.choice_dynamic_array_cleanup", selected_first_values_cleanup);
    auto selected_second_values_cleanup =
        output.find("%selected.Ready.items.element1.values.choice_dynamic_array_cleanup", selected_first_spare_cleanup);
    auto selected_second_spare_cleanup =
        output.find("%selected.Ready.items.element1.spare.choice_dynamic_array_cleanup", selected_second_values_cleanup);
    assert(main_start != std::string::npos);
    assert(scalar_payload != std::string::npos);
    assert(payload_tuple_extract != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(selected_first_values_cleanup != std::string::npos);
    assert(selected_first_spare_cleanup != std::string::npos);
    assert(selected_second_values_cleanup != std::string::npos);
    assert(selected_second_spare_cleanup != std::string::npos);
    assert(payload_tuple_extract < selected_first_values_cleanup);
    assert(selected_first_values_cleanup < selected_first_spare_cleanup);
    assert(selected_first_spare_cleanup < selected_second_values_cleanup);
    assert(selected_second_values_cleanup < selected_second_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_multi_payload_indexed_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto scalar_payload = output.find(
        "insertvalue { %record.Inner, i32 } %tmp",
        main_start
    );
    auto moved_source_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto moved_source_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto sibling_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto sibling_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", sibling_values_cleanup);
    auto selected_payload_extract = output.find(
        "%selected.Ready.item.values.choice_dynamic_array_cleanup.payload.value = "
        "extractvalue { %record.Inner, i32 }",
        sibling_spare_cleanup
    );
    auto selected_values_cleanup = output.find(
        "%selected.Ready.item.values.choice_dynamic_array_cleanup.descriptor.extract",
        selected_payload_extract
    );
    auto selected_spare_cleanup =
        output.find("%selected.Ready.item.spare.choice_dynamic_array_cleanup", selected_values_cleanup);
    assert(main_start != std::string::npos);
    assert(scalar_payload != std::string::npos);
    assert(moved_source_values_cleanup == std::string::npos);
    assert(moved_source_spare_cleanup == std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(selected_payload_extract != std::string::npos);
    assert(selected_values_cleanup != std::string::npos);
    assert(selected_spare_cleanup != std::string::npos);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
    assert(sibling_spare_cleanup < selected_payload_extract);
    assert(selected_payload_extract < selected_values_cleanup);
    assert(selected_values_cleanup < selected_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_multi_payload_second_indexed_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto scalar_payload = output.find(
        "insertvalue { i32, %record.Inner } undef, i32 7, 0",
        main_start
    );
    auto moved_source_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto moved_source_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto sibling_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto sibling_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", sibling_values_cleanup);
    auto selected_payload_extract = output.find(
        "%selected.Ready.item.values.choice_dynamic_array_cleanup.payload.value = "
        "extractvalue { i32, %record.Inner }",
        sibling_spare_cleanup
    );
    auto selected_values_cleanup = output.find(
        "%selected.Ready.item.values.choice_dynamic_array_cleanup.descriptor.extract",
        selected_payload_extract
    );
    auto selected_spare_cleanup =
        output.find("%selected.Ready.item.spare.choice_dynamic_array_cleanup", selected_values_cleanup);
    assert(main_start != std::string::npos);
    assert(scalar_payload != std::string::npos);
    assert(moved_source_values_cleanup == std::string::npos);
    assert(moved_source_spare_cleanup == std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(selected_payload_extract != std::string::npos);
    assert(selected_values_cleanup != std::string::npos);
    assert(selected_spare_cleanup != std::string::npos);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
    assert(sibling_spare_cleanup < selected_payload_extract);
    assert(selected_payload_extract < selected_values_cleanup);
    assert(selected_values_cleanup < selected_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_multi_payload_indexed_member_path_sibling_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto selected_values_cleanup =
        output.find("%selected.Ready.item.values.choice_dynamic_array_cleanup", main_start);
    auto selected_spare_cleanup =
        output.find("%selected.Ready.item.spare.choice_dynamic_array_cleanup", selected_values_cleanup);
    auto sibling_values_cleanup =
        output.find("%sibling.Ready.item.values.choice_dynamic_array_cleanup", selected_spare_cleanup);
    auto sibling_spare_cleanup =
        output.find("%sibling.Ready.item.spare.choice_dynamic_array_cleanup", sibling_values_cleanup);
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(selected_values_cleanup != std::string::npos);
    assert(selected_spare_cleanup != std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(selected_values_cleanup < selected_spare_cleanup);
    assert(selected_spare_cleanup < sibling_values_cleanup);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_multi_payload_second_indexed_member_path_sibling_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto selected_values_cleanup =
        output.find("%selected.Ready.item.values.choice_dynamic_array_cleanup", main_start);
    auto selected_spare_cleanup =
        output.find("%selected.Ready.item.spare.choice_dynamic_array_cleanup", selected_values_cleanup);
    auto sibling_values_cleanup =
        output.find("%sibling.Ready.item.values.choice_dynamic_array_cleanup", selected_spare_cleanup);
    auto sibling_spare_cleanup =
        output.find("%sibling.Ready.item.spare.choice_dynamic_array_cleanup", sibling_values_cleanup);
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(selected_values_cleanup != std::string::npos);
    assert(selected_spare_cleanup != std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(selected_values_cleanup < selected_spare_cleanup);
    assert(selected_spare_cleanup < sibling_values_cleanup);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
}

void assert_cli_emit_llvm_choice_constructor_multi_variant_nested_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto secondary_constructor_tag = output.find(
        "insertvalue { i32, [2 x %record.Inner] } undef, i32 2, 0",
        main_start
    );
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto primary_tag_check = output.find(
        "%selected.Primary.items.element0.values.choice_dynamic_array_cleanup",
        main_start
    );
    auto primary_active_check = output.find("icmp eq i32 %selected.choice_dynamic_array_cleanup", primary_tag_check);
    auto primary_expected_tag = output.find(", 1", primary_active_check);
    auto primary_cleanup_entry = output.find(
        "selected.Primary.items.element0.values.choice_dynamic_array_cleanup0.cleanup.entry",
        primary_tag_check
    );
    auto secondary_tag_check = output.find(
        "%selected.Secondary.items.element0.values.choice_dynamic_array_cleanup",
        primary_cleanup_entry
    );
    auto secondary_active_check =
        output.find("icmp eq i32 %selected.choice_dynamic_array_cleanup", secondary_tag_check);
    auto secondary_expected_tag = output.find(", 2", secondary_active_check);
    auto secondary_first_values_cleanup = output.find(
        "%selected.Secondary.items.element0.values.choice_dynamic_array_cleanup.descriptor.extract",
        secondary_tag_check
    );
    auto secondary_first_spare_cleanup =
        output.find("%selected.Secondary.items.element0.spare.choice_dynamic_array_cleanup", secondary_first_values_cleanup);
    auto secondary_second_values_cleanup =
        output.find("%selected.Secondary.items.element1.values.choice_dynamic_array_cleanup", secondary_first_spare_cleanup);
    auto secondary_second_spare_cleanup =
        output.find("%selected.Secondary.items.element1.spare.choice_dynamic_array_cleanup", secondary_second_values_cleanup);
    auto secondary_final_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr "
            "%selected.Secondary.items.element1.spare.choice_dynamic_array_cleanup7.cleanup.data",
        secondary_second_spare_cleanup
    );
    auto main_return = output.find("ret i32 0", secondary_final_deallocate);
    assert(main_start != std::string::npos);
    assert(secondary_constructor_tag != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(primary_tag_check != std::string::npos);
    assert(primary_active_check != std::string::npos);
    assert(primary_expected_tag != std::string::npos);
    assert(primary_cleanup_entry != std::string::npos);
    assert(secondary_tag_check != std::string::npos);
    assert(secondary_active_check != std::string::npos);
    assert(secondary_expected_tag != std::string::npos);
    assert(secondary_first_values_cleanup != std::string::npos);
    assert(secondary_first_spare_cleanup != std::string::npos);
    assert(secondary_second_values_cleanup != std::string::npos);
    assert(secondary_second_spare_cleanup != std::string::npos);
    assert(secondary_final_deallocate != std::string::npos);
    assert(main_return != std::string::npos);
    assert(primary_tag_check < primary_cleanup_entry);
    assert(primary_cleanup_entry < secondary_tag_check);
    assert(secondary_tag_check < secondary_first_values_cleanup);
    assert(secondary_first_values_cleanup < secondary_first_spare_cleanup);
    assert(secondary_first_spare_cleanup < secondary_second_values_cleanup);
    assert(secondary_second_values_cleanup < secondary_second_spare_cleanup);
    assert(secondary_second_spare_cleanup < secondary_final_deallocate);
    assert(secondary_final_deallocate < main_return);
}

void assert_cli_emit_llvm_choice_constructor_multi_variant_indexed_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto secondary_constructor_tag = output.find(
        "insertvalue { i32, %record.Inner } undef, i32 2, 0",
        main_start
    );
    auto moved_source_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto moved_source_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto sibling_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto sibling_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", sibling_values_cleanup);
    auto primary_tag_check =
        output.find("%selected.Primary.item.values.choice_dynamic_array_cleanup", sibling_spare_cleanup);
    auto secondary_tag_check =
        output.find("%selected.Secondary.item.values.choice_dynamic_array_cleanup", primary_tag_check);
    auto secondary_spare_cleanup =
        output.find("%selected.Secondary.item.spare.choice_dynamic_array_cleanup", secondary_tag_check);
    auto secondary_final_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr "
            "%selected.Secondary.item.spare.choice_dynamic_array_cleanup3.cleanup.data",
        secondary_spare_cleanup
    );
    assert(main_start != std::string::npos);
    assert(secondary_constructor_tag != std::string::npos);
    assert(moved_source_values_cleanup == std::string::npos);
    assert(moved_source_spare_cleanup == std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(primary_tag_check != std::string::npos);
    assert(secondary_tag_check != std::string::npos);
    assert(secondary_spare_cleanup != std::string::npos);
    assert(secondary_final_deallocate != std::string::npos);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
    assert(sibling_spare_cleanup < primary_tag_check);
    assert(primary_tag_check < secondary_tag_check);
    assert(secondary_tag_check < secondary_spare_cleanup);
    assert(secondary_spare_cleanup < secondary_final_deallocate);
}

void assert_cli_emit_llvm_choice_constructor_multi_variant_indexed_member_path_sibling_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup =
        output.find("%holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup =
        output.find("%holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup =
        output.find("%holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup =
        output.find("%holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto selected_values_cleanup =
        output.find("%selected.Secondary.item.values.choice_dynamic_array_cleanup", main_start);
    auto selected_spare_cleanup =
        output.find("%selected.Secondary.item.spare.choice_dynamic_array_cleanup", selected_values_cleanup);
    auto sibling_values_cleanup =
        output.find("%sibling.Primary.item.values.choice_dynamic_array_cleanup", selected_spare_cleanup);
    auto sibling_spare_cleanup =
        output.find("%sibling.Primary.item.spare.choice_dynamic_array_cleanup", sibling_values_cleanup);
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(selected_values_cleanup != std::string::npos);
    assert(selected_spare_cleanup != std::string::npos);
    assert(sibling_values_cleanup != std::string::npos);
    assert(sibling_spare_cleanup != std::string::npos);
    assert(selected_values_cleanup < selected_spare_cleanup);
    assert(selected_spare_cleanup < sibling_values_cleanup);
    assert(sibling_values_cleanup < sibling_spare_cleanup);
}

void assert_cli_emit_llvm_dynamic_array_owned_constructor_nested_member_path_move_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto main_start = output.find("define i32 @main");
    auto stale_first_values_cleanup =
        output.find("%nested.holder.items.element0.values.dynamic_array_cleanup", main_start);
    auto stale_first_spare_cleanup =
        output.find("%nested.holder.items.element0.spare.dynamic_array_cleanup", main_start);
    auto stale_second_values_cleanup =
        output.find("%nested.holder.items.element1.values.dynamic_array_cleanup", main_start);
    auto stale_second_spare_cleanup =
        output.find("%nested.holder.items.element1.spare.dynamic_array_cleanup", main_start);
    auto replacement_cleanup = output.find("%outer.items.element0.values.dynamic_array_reassign_cleanup", main_start);
    auto replacement_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_cleanup
    );
    auto replacement_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.items.element0.values.dynamic_array_reassign_cleanup",
        replacement_drop
    );
    auto final_first_values_cleanup = output.find(
        "%outer.items.element0.values.dynamic_array_cleanup",
        replacement_deallocate
    );
    auto final_first_spare_cleanup = output.find(
        "%outer.items.element0.spare.dynamic_array_cleanup",
        final_first_values_cleanup
    );
    auto final_second_values_cleanup = output.find(
        "%outer.items.element1.values.dynamic_array_cleanup",
        final_first_spare_cleanup
    );
    auto final_second_spare_cleanup = output.find(
        "%outer.items.element1.spare.dynamic_array_cleanup",
        final_second_values_cleanup
    );
    assert(main_start != std::string::npos);
    assert(stale_first_values_cleanup == std::string::npos);
    assert(stale_first_spare_cleanup == std::string::npos);
    assert(stale_second_values_cleanup == std::string::npos);
    assert(stale_second_spare_cleanup == std::string::npos);
    assert(replacement_cleanup != std::string::npos);
    assert(replacement_drop != std::string::npos);
    assert(replacement_deallocate != std::string::npos);
    assert(final_first_values_cleanup != std::string::npos);
    assert(final_first_spare_cleanup != std::string::npos);
    assert(final_second_values_cleanup != std::string::npos);
    assert(final_second_spare_cleanup != std::string::npos);
    assert(replacement_cleanup < replacement_drop);
    assert(replacement_drop < replacement_deallocate);
    assert(replacement_deallocate < final_first_values_cleanup);
    assert(final_first_values_cleanup < final_first_spare_cleanup);
    assert(final_first_spare_cleanup < final_second_values_cleanup);
    assert(final_second_values_cleanup < final_second_spare_cleanup);
}

void assert_cli_emit_llvm_dynamic_array_owned_multi_field_indexed_record_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto items_address = output.find("%holder.items.addr");
    auto first_values_cleanup = output.find("%holder.items.element0.values.dynamic_array_reassign_cleanup");
    auto first_values_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.values.dynamic_array_reassign_cleanup"
    );
    auto first_values_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.values.dynamic_array_reassign_cleanup"
    );
    auto first_spare_cleanup = output.find("%holder.items.element0.spare.dynamic_array_reassign_cleanup");
    auto first_spare_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.spare.dynamic_array_reassign_cleanup"
    );
    auto first_spare_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.spare.dynamic_array_reassign_cleanup"
    );
    auto second_values_cleanup = output.find("%holder.items.element1.values.dynamic_array_reassign_cleanup");
    auto second_values_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element1.values.dynamic_array_reassign_cleanup"
    );
    auto second_values_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element1.values.dynamic_array_reassign_cleanup"
    );
    auto second_spare_cleanup = output.find("%holder.items.element1.spare.dynamic_array_reassign_cleanup");
    auto second_spare_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element1.spare.dynamic_array_reassign_cleanup"
    );
    auto second_spare_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element1.spare.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store [2 x %record.Item] %tmp", second_spare_deallocate);
    assert(items_address != std::string::npos);
    assert(first_values_cleanup != std::string::npos);
    assert(first_values_drop != std::string::npos);
    assert(first_values_deallocate != std::string::npos);
    assert(first_spare_cleanup != std::string::npos);
    assert(first_spare_drop != std::string::npos);
    assert(first_spare_deallocate != std::string::npos);
    assert(second_values_cleanup != std::string::npos);
    assert(second_values_drop != std::string::npos);
    assert(second_values_deallocate != std::string::npos);
    assert(second_spare_cleanup != std::string::npos);
    assert(second_spare_drop != std::string::npos);
    assert(second_spare_deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(items_address < first_values_cleanup);
    assert(first_values_cleanup < first_values_drop);
    assert(first_values_drop < first_values_deallocate);
    assert(first_values_deallocate < first_spare_cleanup);
    assert(first_spare_cleanup < first_spare_drop);
    assert(first_spare_drop < first_spare_deallocate);
    assert(first_spare_deallocate < second_values_cleanup);
    assert(second_values_cleanup < second_values_drop);
    assert(second_values_drop < second_values_deallocate);
    assert(second_values_deallocate < second_spare_cleanup);
    assert(second_spare_cleanup < second_spare_drop);
    assert(second_spare_drop < second_spare_deallocate);
    assert(second_spare_deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_multi_field_nested_record_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto inner_address = output.find("%outer.inner.addr");
    auto values_cleanup = output.find("%outer.inner.values.dynamic_array_reassign_cleanup");
    auto values_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.inner.values.dynamic_array_reassign_cleanup"
    );
    auto values_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.inner.values.dynamic_array_reassign_cleanup"
    );
    auto spare_cleanup = output.find("%outer.inner.spare.dynamic_array_reassign_cleanup");
    auto spare_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto spare_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store %record.Inner %tmp", spare_deallocate);
    assert(inner_address != std::string::npos);
    assert(values_cleanup != std::string::npos);
    assert(values_drop != std::string::npos);
    assert(values_deallocate != std::string::npos);
    assert(spare_cleanup != std::string::npos);
    assert(spare_drop != std::string::npos);
    assert(spare_deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(inner_address < values_cleanup);
    assert(values_cleanup < values_drop);
    assert(values_drop < values_deallocate);
    assert(values_deallocate < spare_cleanup);
    assert(spare_cleanup < spare_drop);
    assert(spare_drop < spare_deallocate);
    assert(spare_deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_indexed_nested_multi_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto items_address = output.find("%holder.items.addr");
    auto first_values_cleanup = output.find("%holder.items.element0.inner.values.dynamic_array_reassign_cleanup");
    auto first_values_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.inner.values.dynamic_array_reassign_cleanup"
    );
    auto first_values_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.inner.values.dynamic_array_reassign_cleanup"
    );
    auto first_spare_cleanup = output.find("%holder.items.element0.inner.spare.dynamic_array_reassign_cleanup");
    auto first_spare_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element0.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto first_spare_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element0.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto second_values_cleanup = output.find("%holder.items.element1.inner.values.dynamic_array_reassign_cleanup");
    auto second_values_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element1.inner.values.dynamic_array_reassign_cleanup"
    );
    auto second_values_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element1.inner.values.dynamic_array_reassign_cleanup"
    );
    auto second_spare_cleanup = output.find("%holder.items.element1.inner.spare.dynamic_array_reassign_cleanup");
    auto second_spare_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.items.element1.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto second_spare_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.items.element1.inner.spare.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store [2 x %record.Item] %tmp", second_spare_deallocate);
    assert(items_address != std::string::npos);
    assert(first_values_cleanup != std::string::npos);
    assert(first_values_drop != std::string::npos);
    assert(first_values_deallocate != std::string::npos);
    assert(first_spare_cleanup != std::string::npos);
    assert(first_spare_drop != std::string::npos);
    assert(first_spare_deallocate != std::string::npos);
    assert(second_values_cleanup != std::string::npos);
    assert(second_values_drop != std::string::npos);
    assert(second_values_deallocate != std::string::npos);
    assert(second_spare_cleanup != std::string::npos);
    assert(second_spare_drop != std::string::npos);
    assert(second_spare_deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(items_address < first_values_cleanup);
    assert(first_values_cleanup < first_values_drop);
    assert(first_values_drop < first_values_deallocate);
    assert(first_values_deallocate < first_spare_cleanup);
    assert(first_spare_cleanup < first_spare_drop);
    assert(first_spare_drop < first_spare_deallocate);
    assert(first_spare_deallocate < second_values_cleanup);
    assert(second_values_cleanup < second_values_drop);
    assert(second_values_drop < second_values_deallocate);
    assert(second_values_deallocate < second_spare_cleanup);
    assert(second_spare_cleanup < second_spare_drop);
    assert(second_spare_drop < second_spare_deallocate);
    assert(second_spare_deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_multidimensional_record_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto grid_address = output.find("%holder.grid.addr");
    auto first_cleanup = output.find("%holder.grid.element0.element0.values.dynamic_array_reassign_cleanup");
    auto first_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.grid.element0.element0.values.dynamic_array_reassign_cleanup"
    );
    auto first_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.grid.element0.element0.values.dynamic_array_reassign_cleanup"
    );
    auto second_cleanup = output.find("%holder.grid.element0.element1.values.dynamic_array_reassign_cleanup");
    auto second_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.grid.element0.element1.values.dynamic_array_reassign_cleanup"
    );
    auto second_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.grid.element0.element1.values.dynamic_array_reassign_cleanup"
    );
    auto third_cleanup = output.find("%holder.grid.element1.element0.values.dynamic_array_reassign_cleanup");
    auto third_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.grid.element1.element0.values.dynamic_array_reassign_cleanup"
    );
    auto third_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.grid.element1.element0.values.dynamic_array_reassign_cleanup"
    );
    auto fourth_cleanup = output.find("%holder.grid.element1.element1.values.dynamic_array_reassign_cleanup");
    auto fourth_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.grid.element1.element1.values.dynamic_array_reassign_cleanup"
    );
    auto fourth_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.grid.element1.element1.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store [2 x [2 x %record.Item]] %tmp", fourth_deallocate);
    assert(grid_address != std::string::npos);
    assert(first_cleanup != std::string::npos);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(second_cleanup != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(third_cleanup != std::string::npos);
    assert(third_drop != std::string::npos);
    assert(third_deallocate != std::string::npos);
    assert(fourth_cleanup != std::string::npos);
    assert(fourth_drop != std::string::npos);
    assert(fourth_deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(grid_address < first_cleanup);
    assert(first_cleanup < first_drop);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < second_cleanup);
    assert(second_cleanup < second_drop);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < third_cleanup);
    assert(third_cleanup < third_drop);
    assert(third_drop < third_deallocate);
    assert(third_deallocate < fourth_cleanup);
    assert(fourth_cleanup < fourth_drop);
    assert(fourth_drop < fourth_deallocate);
    assert(fourth_deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_computed_multidimensional_record_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto row_index = output.find("%row = add i64 0, 0");
    auto col_index = output.find("%col = add i64 0, 1");
    auto row_address = output.find("getelementptr [2 x [2 x %record.Item]], ptr %tmp");
    auto col_address = output.find("getelementptr [2 x %record.Item], ptr %tmp");
    auto field_address = output.find("getelementptr %record.Item, ptr %tmp");
    auto cleanup = output.find("%holder.grid.element.element.values.dynamic_array_reassign_cleanup");
    auto drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.grid.element.element.values.dynamic_array_reassign_cleanup"
    );
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.grid.element.element.values.dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(row_index != std::string::npos);
    assert(col_index != std::string::npos);
    assert(row_address != std::string::npos);
    assert(col_address != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(row_index < col_index);
    assert(col_index < row_address);
    assert(row_address < col_address);
    assert(col_address < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_mixed_multidimensional_record_field_reassignment_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    std::string const& owner_prefix
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto cleanup = output.find("%" + owner_prefix + ".dynamic_array_reassign_cleanup");
    auto drop = output.find("call void @__orison_drop.Payload(ptr %" + owner_prefix + ".dynamic_array_reassign_cleanup");
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %" + owner_prefix + ".dynamic_array_reassign_cleanup"
    );
    auto replacement_store = output.find("store { ptr, i64, i64 } %tmp", deallocate);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < replacement_store);
}

void assert_cli_emit_llvm_dynamic_array_owned_field_scope_cleanup_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto field_address = output.find("%holder.values.addr");
    auto cleanup = output.find("%holder.values.dynamic_array_cleanup");
    auto drop = output.find("call void @__orison_drop.Payload(ptr %holder.values.dynamic_array_cleanup");
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.dynamic_array_cleanup"
    );
    auto return_value = output.find("ret i32 0");
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(return_value != std::string::npos);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < return_value);
}

void assert_cli_emit_llvm_dynamic_array_owned_nested_field_scope_cleanup_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto inner_address = output.find("%outer.inner.addr");
    auto field_address = output.find("%outer.inner.values.addr");
    auto cleanup = output.find("%outer.inner.values.dynamic_array_cleanup");
    auto drop = output.find("call void @__orison_drop.Payload(ptr %outer.inner.values.dynamic_array_cleanup");
    auto deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.inner.values.dynamic_array_cleanup"
    );
    auto return_value = output.find("ret i32 0");
    assert(inner_address != std::string::npos);
    assert(field_address != std::string::npos);
    assert(cleanup != std::string::npos);
    assert(drop != std::string::npos);
    assert(deallocate != std::string::npos);
    assert(return_value != std::string::npos);
    assert(inner_address < field_address);
    assert(field_address < cleanup);
    assert(cleanup < drop);
    assert(drop < deallocate);
    assert(deallocate < return_value);
}

void assert_cli_emit_llvm_dynamic_array_owned_indexed_field_scope_cleanup_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto items_address = output.find("%outer.items.addr");
    auto first_item_address = output.find("%outer.items.element0.addr");
    auto first_field_address = output.find("%outer.items.element0.values.addr");
    auto first_cleanup = output.find("%outer.items.element0.values.dynamic_array_cleanup");
    auto first_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.items.element0.values.dynamic_array_cleanup"
    );
    auto first_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.items.element0.values.dynamic_array_cleanup"
    );
    auto second_item_address = output.find("%outer.items.element1.addr");
    auto second_field_address = output.find("%outer.items.element1.values.addr");
    auto second_cleanup = output.find("%outer.items.element1.values.dynamic_array_cleanup");
    auto second_drop = output.find(
        "call void @__orison_drop.Payload(ptr %outer.items.element1.values.dynamic_array_cleanup"
    );
    auto second_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %outer.items.element1.values.dynamic_array_cleanup"
    );
    auto return_value = output.find("ret i32 0");
    assert(items_address != std::string::npos);
    assert(first_item_address != std::string::npos);
    assert(first_field_address != std::string::npos);
    assert(first_cleanup != std::string::npos);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(second_item_address != std::string::npos);
    assert(second_field_address != std::string::npos);
    assert(second_cleanup != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(return_value != std::string::npos);
    assert(items_address < first_item_address);
    assert(first_item_address < first_field_address);
    assert(first_field_address < second_item_address);
    assert(second_item_address < second_field_address);
    assert(second_field_address < first_cleanup);
    assert(first_field_address < first_cleanup);
    assert(first_cleanup < first_drop);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < second_cleanup);
    assert(second_cleanup < second_drop);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < return_value);
}

void assert_cli_emit_llvm_dynamic_array_owned_direct_indexed_scope_cleanup_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    auto values_address = output.find("%holder.values.addr");
    auto first_element_address = output.find("%holder.values.element0.addr");
    auto first_cleanup = output.find("%holder.values.element0.dynamic_array_cleanup");
    auto first_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.values.element0.dynamic_array_cleanup"
    );
    auto first_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.element0.dynamic_array_cleanup"
    );
    auto second_element_address = output.find("%holder.values.element1.addr");
    auto second_cleanup = output.find("%holder.values.element1.dynamic_array_cleanup");
    auto second_drop = output.find(
        "call void @__orison_drop.Payload(ptr %holder.values.element1.dynamic_array_cleanup"
    );
    auto second_deallocate = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %holder.values.element1.dynamic_array_cleanup"
    );
    auto return_value = output.find("ret i32 0");
    assert(values_address != std::string::npos);
    assert(first_element_address != std::string::npos);
    assert(first_cleanup != std::string::npos);
    assert(first_drop != std::string::npos);
    assert(first_deallocate != std::string::npos);
    assert(second_element_address != std::string::npos);
    assert(second_cleanup != std::string::npos);
    assert(second_drop != std::string::npos);
    assert(second_deallocate != std::string::npos);
    assert(return_value != std::string::npos);
    assert(values_address < first_element_address);
    assert(first_element_address < second_element_address);
    assert(second_element_address < first_cleanup);
    assert(first_cleanup < first_drop);
    assert(first_drop < first_deallocate);
    assert(first_deallocate < second_cleanup);
    assert(second_cleanup < second_drop);
    assert(second_drop < second_deallocate);
    assert(second_deallocate < return_value);
}

auto generic_method_lines() -> std::vector<std::string> {
    return {
        "package demo.cli",
        "record Box<T>",
        "    value: T",
        "record Pair<A, B>",
        "    first: A",
        "    second: B",
        "function select_value<T>(value: T) -> UInt32",
        "    return 0 as UInt32",
        "extend UInt32",
        "    function select<T>(this: shared This, value: T) -> UInt32",
        "        return 0 as UInt32",
        "extend Box<T>",
        "    function value(this: shared This) -> T",
        "        return this.value",
        "extend Pair<A, B>",
        "    function first(this: shared This) -> A",
        "        return this.first",
        "function main() -> UInt32",
        "    let seed: UInt32 = 7 as UInt32",
        "    let box: Box<UInt32> = Box(13 as UInt32)",
        "    let pair: Pair<UInt32, UInt64> = Pair(17 as UInt32, 19 as UInt64)",
        "    let nested_box: Box<Pair<UInt32, UInt64>> = Box(pair)",
        "    let selected: UInt32 = select_value(11 as UInt64) + seed.select(9 as UInt64) + box.value()",
        "    let first_value: UInt32 = pair.first()",
        "    let nested_pair: Pair<UInt32, UInt64> = nested_box.value()",
        "    0 as UInt32",
    };
}

auto generic_pair_consumer_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record Pair<A, B>",
        "    first: A",
        "    second: B",
        "function consume_pair<T>(left: T, pair: Pair<T, UInt16>) -> UInt16",
        "    return pair.second",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_consumer_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "function same<T>(left: T, right: T) -> UInt16",
        "    return 1",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_record_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record Same<T>",
        "    first: T",
        "    second: T",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_record_for_lines() -> std::vector<std::string> {
    return {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record Same<T>",
        "    first: T",
        "    second: T",
        "function demo() -> UInt16",
        "    var total: UInt16 = 0 as UInt16",
        "    for item in [Same(Header([1, 2], 1), OtherHeader([1, 2], 1))]",
        "        total = total + item.first.version",
        "    total",
    };
}

auto underconstrained_generic_record_for_lines() -> std::vector<std::string> {
    return {
        "package demo.cli",
        "record Tag<T>",
        "    code: UInt32",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for item in [Tag(7 as UInt32), Tag(9 as UInt32)]",
        "        total = total + item.code",
        "    total",
    };
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_generic_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto fixtures = std::filesystem::current_path().parent_path().parent_path() / "tests" / "fixtures";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_function_dependent_argument_type.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(OtherHeader([1, 2], 1), 1 as UInt16))"}
        ),
        "function argument 'pair' type 'Pair<OtherHeader, UInt16>' does not match declared type 'Pair<Header, UInt16>'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_repeated_binding_argument_type.or",
        generic_same_consumer_lines({"    return same(Header([1, 2], 1), OtherHeader([1, 2], 1))"}),
        "function argument 'right' type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_record_repeated_field_type.or",
        generic_same_record_lines(
            {
                "    let same = Same(Header([1, 2], 1), OtherHeader([1, 2], 1))",
                "    return same.first.version",
            }
        ),
        "record constructor field 'second' type 'OtherHeader' does not match expected field type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_record_array_literal_for_repeated_field_type.or",
        generic_same_record_for_lines(),
        "record constructor field 'second' type 'OtherHeader' does not match expected field type 'Header'"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_underconstrained_generic_record_array_literal_for_emit.or",
        underconstrained_generic_record_for_lines(),
        "generic parameter 'T' cannot be inferred for record 'Tag'"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_parameter.or"
    );
    assert_cli_emit_llvm_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_parameter.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_owned_parameter.or"
    );
    assert_cli_emit_llvm_owned_dynamic_array_generic_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_owned_parameter.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_generic_owned_parameter_missing_drop.or",
        "lowering DynamicArray parameter 'values' with owned element type Payload requires ownership/drop proof before production lowering"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_call_result_parameter.or"
    );
    assert_cli_emit_llvm_call_result_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_call_result_parameter.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_nested_call_result_parameter.or"
    );
    assert_cli_emit_llvm_nested_call_result_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_nested_call_result_parameter.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_local_call_result_parameter.or"
    );
    assert_cli_emit_llvm_local_call_result_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_local_call_result_parameter.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "generic_function_constructor_argument.or"
    );
    assert_cli_emit_llvm_generic_function_constructor_argument_fixture_success(
        executable,
        fixtures / "generic_function_constructor_argument.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "generic_function_ternary_constructor_argument.or"
    );
    assert_cli_emit_llvm_generic_function_ternary_constructor_argument_fixture_success(
        executable,
        fixtures / "generic_function_ternary_constructor_argument.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "generic_function_ternary_constructor_argument_mismatch.or",
        "call argument lowering failed: value argument 1 has incompatible ternary arm source types: Box<UInt32> and Box<UInt64>"
    );
    auto generic_method_path = smoke_temp_root / "orison_cli_generic_method_specialization.or";
    write_lines(generic_method_path, generic_method_lines());
    assert_cli_run_fixture_success(
        executable,
        generic_method_path
    );
    assert_cli_emit_llvm_generic_method_fixture_success(
        executable,
        generic_method_path
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "generic_method_inferred_receiver.or"
    );
    assert_cli_emit_llvm_generic_method_inferred_receiver_fixture_success(
        executable,
        fixtures / "generic_method_inferred_receiver.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_method_parameter.or"
    );
    assert_cli_emit_llvm_dynamic_array_generic_method_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_method_parameter.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "generic_method_ambiguous_specialization.or",
        "extension method 'Box<T>.value' is duplicated"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_length.or"
    );
    assert_cli_emit_llvm_dynamic_array_receiver_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_length.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_append_scalar.or"
    );
    assert_cli_emit_llvm_dynamic_array_receiver_append_scalar_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_append_scalar.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_complete_contract.or"
    );
    assert_cli_emit_llvm_dynamic_array_complete_contract_fixture_success(
        executable,
        fixtures / "dynamic_array_complete_contract.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_dynamic_array_return_payload_run.or"
    );
    assert_cli_emit_llvm_choice_dynamic_array_return_payload_fixture_success(
        executable,
        fixtures / "choice_dynamic_array_return_payload_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_direct_indexed_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_direct_indexed_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_direct_indexed_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_direct_indexed_element_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_direct_indexed_element_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_direct_indexed_element_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_indexed_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_record_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_record_element_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_indexed_record_element_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_record_element_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_nested_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_indexed_nested_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_nested_record_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_nested_record_sibling_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_indexed_nested_record_sibling_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_nested_record_sibling_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_computed_index_nested_record_sibling_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_computed_index_nested_record_sibling_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_computed_index_nested_record_sibling_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_dynamic_index_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_dynamic_index_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_dynamic_index_record_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_dynamic_index_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_nested_dynamic_index_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_dynamic_index_record_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_dynamic_index_sibling_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_nested_dynamic_index_sibling_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_dynamic_index_sibling_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_dynamic_index_multi_field_record_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_nested_dynamic_index_multi_field_record_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_dynamic_index_multi_field_record_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_returned_nested_record_field_move_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_returned_nested_record_field_move_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_returned_nested_record_field_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_returned_fixed_array_record_field_move_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_returned_fixed_array_record_field_move_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_returned_fixed_array_record_field_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_fixed_array_record_field_move_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_constructor_fixed_array_record_field_move_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_fixed_array_record_field_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_member_path_move_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_constructor_member_path_move_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_nested_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_nested_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_nested_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_nested_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_payload_nested_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_nested_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_second_nested_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_payload_second_nested_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_second_nested_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_nested_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_variant_nested_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_nested_member_path_move_run.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_variant_nested_member_path_reuse_rejected.or",
        "use after move: holder.items"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_indexed_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_variant_indexed_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_indexed_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_variant_indexed_member_path_sibling_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_variant_indexed_member_path_reuse_rejected.or",
        "use after move: holder.items.element0"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_variant_computed_index_member_path_move_rejected.or",
        "runtime-index partial owner owner holder.items index index element Inner moved Inner "
        "cleanup skip-moved-element constructor-move disabled: "
        "runtime-index cleanup-skip plan owner holder.items index index element Inner moved Inner "
        "operation skip-moved-element production-cleanup disabled: "
        "runtime-index cleanup proof owner holder.items index index element Inner moved Inner "
        "operation skip-moved-element owner-known true index-known true type-match true "
        "operation-supported true prerequisites met lowering disabled: "
        "runtime-index cleanup emission-sketch owner holder.items index index element Inner snippets 5 "
        "report-only true production-emission disabled snippet load-length holder.items "
        "snippet loop-cleanup-index 0..<length snippet skip-cleanup-index index "
        "snippet drop-live-element holder.items[cleanup_index] as Inner "
        "snippet deallocate-owner holder.items: "
        "runtime-index cleanup capability owner holder.items index index element Inner "
        "proof-ready true sketch-ready true prerequisites ready production disabled"
    );
    assert_cli_runtime_indexed_cleanup_audit_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_computed_index_member_path_move_rejected.or"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_computed_index_member_path_move_run.or"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_variant_computed_index_member_path_sibling_run.or"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_variant_computed_index_member_path_reuse_rejected.or",
        "use after move: holder.items[index]"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_success(
        executable,
        fixtures / "runtime_indexed_record_constructor_computed_index_member_path_move_run.or"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_success(
        executable,
        fixtures / "runtime_indexed_record_constructor_computed_index_member_path_sibling_run.or"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_failure(
        executable,
        fixtures / "runtime_indexed_record_constructor_computed_index_member_path_reuse_rejected.or",
        "use after move: holder.items[index]"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_success(
        executable,
        fixtures / "runtime_indexed_choice_constructor_computed_index_member_path_move_run.or"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_success(
        executable,
        fixtures / "runtime_indexed_choice_constructor_computed_index_member_path_sibling_run.or"
    );
    assert_cli_test_only_runtime_indexed_constructor_move_run_fixture_failure(
        executable,
        fixtures / "runtime_indexed_choice_constructor_computed_index_member_path_reuse_rejected.or",
        "use after move: holder.items[index]"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_indexed_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_payload_indexed_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_indexed_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_second_indexed_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_payload_second_indexed_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_second_indexed_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_payload_indexed_member_path_sibling_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_second_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_multi_payload_second_indexed_member_path_sibling_move_fixture_success(
        executable,
        fixtures / "choice_constructor_multi_payload_second_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_payload_indexed_member_path_reuse_rejected.or",
        "use after move: holder.items.element0"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_payload_computed_index_member_path_move_rejected.or",
        "indexed constructor ownership move requires explicit partial ownership support"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_payload_second_computed_index_member_path_move_rejected.or",
        "indexed constructor ownership move requires explicit partial ownership support"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_payload_nested_member_path_reuse_rejected.or",
        "use after move: holder.items"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_multi_payload_second_nested_member_path_reuse_rejected.or",
        "use after move: holder.items"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_owned_constructor_member_path_reuse_rejected.or",
        "use after move: holder.items"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_member_path_reuse_rejected.or",
        "use after move: holder.values"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_nested_member_path_move_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_constructor_nested_member_path_move_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_nested_member_path_move_run.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_owned_constructor_nested_member_path_reuse_rejected.or",
        "use after move: nested.holder.items"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_indexed_member_path_move_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_constructor_indexed_member_path_move_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_indexed_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_constructor_indexed_member_path_sibling_move_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_constructor_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_owned_constructor_indexed_member_path_reuse_rejected.or",
        "use after move: holder.items.element0"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_owned_constructor_computed_index_member_path_move_rejected.or",
        "indexed constructor ownership move requires explicit partial ownership support"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_indexed_member_path_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_indexed_member_path_move_fixture_success(
        executable,
        fixtures / "choice_constructor_indexed_member_path_move_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "choice_constructor_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_choice_constructor_indexed_member_path_sibling_move_fixture_success(
        executable,
        fixtures / "choice_constructor_indexed_member_path_sibling_move_run.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_indexed_member_path_reuse_rejected.or",
        "use after move: holder.items.element0"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "choice_constructor_computed_index_member_path_move_rejected.or",
        "indexed constructor ownership move requires explicit partial ownership support"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_multi_field_indexed_record_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_multi_field_indexed_record_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_multi_field_indexed_record_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_multi_field_nested_record_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_multi_field_nested_record_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_multi_field_nested_record_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_nested_multi_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_indexed_nested_multi_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_nested_multi_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_multidimensional_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_multidimensional_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_multidimensional_record_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_computed_multidimensional_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_computed_multidimensional_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_computed_multidimensional_record_field_reassignment_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_literal_computed_multidimensional_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_mixed_multidimensional_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_literal_computed_multidimensional_record_field_reassignment_run.or",
        "holder.grid.element0.element.values"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_computed_literal_multidimensional_record_field_reassignment_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_mixed_multidimensional_record_field_reassignment_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_computed_literal_multidimensional_record_field_reassignment_run.or",
        "holder.grid.element.element0.values"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_field_scope_cleanup_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_field_scope_cleanup_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_field_scope_cleanup_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_field_scope_cleanup_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_nested_field_scope_cleanup_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_nested_field_scope_cleanup_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_field_scope_cleanup_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_indexed_field_scope_cleanup_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_indexed_field_scope_cleanup_run.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_direct_indexed_scope_cleanup_run.or"
    );
    assert_cli_emit_llvm_dynamic_array_owned_direct_indexed_scope_cleanup_fixture_success(
        executable,
        fixtures / "dynamic_array_owned_direct_indexed_scope_cleanup_run.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_receiver_owned_read_rejected.or",
        "DynamicArray index read of owned element requires a non-owning projection"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_local_owned_read_rejected.or",
        "DynamicArray index read of owned element requires a non-owning projection"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_owned_projection_rejected.or",
        "DynamicArray element path read of owned projection requires a non-owning scalar projection"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "aggregate_owned_projection_rejected.or",
        "aggregate path read of owned projection requires an explicit ownership transfer"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "aggregate_owned_projection_return_rejected.or",
        "aggregate path read of owned projection requires an explicit ownership transfer"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_receiver_append_missing_drop.or",
        "lowering DynamicArray push to owned element requires authorized element drop"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_generic_function_dependent_same_width_integer.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(Header([1, 2], 1), 1 as Int16))"}
        )
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
