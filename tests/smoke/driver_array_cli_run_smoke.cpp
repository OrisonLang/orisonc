#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace {

auto read_successful_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    if (status != 0) {
        std::fprintf(stderr, "command failed:\n%s\n\noutput:\n%s\n", command.c_str(), output.c_str());
    }
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
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) != 0);
    return output;
}

void assert_cli_emit_llvm_existing_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define ") != std::string::npos);
}

void assert_contains(std::string const& text, std::string_view expected_fragment) {
    if (text.find(expected_fragment) == std::string::npos) {
        std::fprintf(
            stderr,
            "missing expected fragment:\n%.*s\n\nactual output:\n%s\n",
            static_cast<int>(expected_fragment.size()),
            expected_fragment.data(),
            text.c_str()
        );
    }
    assert(text.find(expected_fragment) != std::string::npos);
}

void assert_excludes(std::string const& text, std::string_view unexpected_fragment) {
    assert(text.find(unexpected_fragment) == std::string::npos);
}

void assert_dynamic_array_payload_consumer_cleanup(std::string const& output) {
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup");
}

void assert_no_main_dynamic_array_deallocate(std::string const& output) {
    auto const main_start = output.find("define i32 @main");
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", main_start) == std::string::npos);
}

void assert_main_cleanup_fragments_before_return(
    std::string const& output,
    std::initializer_list<std::string_view> cleanup_fragments
) {
    auto const main_start = output.find("define i32 @main");
    auto const return_value = output.find("ret i32 %tmp", main_start);
    assert(main_start != std::string::npos);
    assert(return_value != std::string::npos);
    for (auto const cleanup_fragment : cleanup_fragments) {
        assert_contains(output, cleanup_fragment);
        auto const cleanup = output.find(cleanup_fragment, main_start);
        assert(cleanup != std::string::npos);
        assert(cleanup < return_value);
    }
}

void assert_returned_dynamic_array_field_forwarding_calls(std::string const& output) {
    assert_contains(output, "call { ptr, i64, i64 } @method.DynamicArray_UInt32_.forward__UInt32");
    assert_contains(output, "call i64 @method.DynamicArray_UInt32_.count__UInt32");
}

void assert_run_success(std::filesystem::path const& executable, std::filesystem::path const& source_path) {
    auto status = std::system((executable.string() + " run " + source_path.string()).c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

void assert_run_exit_code(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    int expected_exit_code
) {
    auto status = std::system((executable.string() + " run " + source_path.string()).c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == expected_exit_code);
}

void assert_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Box_UInt32_ = type { i32 }") != std::string::npos);
    assert(output.find("insertvalue %record.Box_UInt32_ undef, i32 7, 0") != std::string::npos);
    assert(output.find("insertvalue %record.Box_UInt32_ undef, i32 9, 0") != std::string::npos);
    assert(output.find("getelementptr %record.Box_UInt32_") != std::string::npos);
    assert(output.find("load i32, ptr") != std::string::npos);
}

void assert_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("declare void @__orison_dynamic_array_allocate") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_deallocate") != std::string::npos);
    assert(output.find("items.computed_for.") != std::string::npos);
    assert(output.find(".condition:") != std::string::npos);
    assert(output.find(".body:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation items.computed_for."
        ) != std::string::npos
    );
    assert(
        output.find(
            "from items to items.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation items.computed_for."
        ) != std::string::npos
    );
    assert(
        output.find(
            "from items.loop.entry to items [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for."
        ) != std::string::npos
    );
    assert(
        output.find(
            "i64 4, i64 %items.computed_for."
        ) != std::string::npos
    );
    auto const first_deallocation = output.find("call void @__orison_dynamic_array_deallocate");
    assert(first_deallocation != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate", first_deallocation + 1) == std::string::npos);
    auto const first_finalization = output.find("store { ptr, i64, i64 } zeroinitializer, ptr %items.addr");
    assert(first_finalization != std::string::npos);
    assert(
        output.find(
            "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr",
            first_finalization + 1
        ) == std::string::npos
    );
    assert(output.find("items.dynamic_array_cleanup") == std::string::npos);
}

void assert_owned_dynamic_array_replacement_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(output.find("define void @__orison_owned_cleanup.Payload(ptr %value)") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_allocate") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_deallocate") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_bounds_failed") != std::string::npos);

    auto const replacement_drop = output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const replacement_store_line_end = output.find('\n', replacement_store);
    assert(
        output.substr(replacement_store, replacement_store_line_end - replacement_store)
            .find("%items.dynamic_array_assign") != std::string::npos
    );
    auto const cleanup_drop = output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup");
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
}

void assert_dynamic_array_complete_contract_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @scalar_demo()");
    assert_contains(output, "define i32 @owned_demo()");
    assert_contains(output, "define void @method.DynamicArray_UInt32_.append_value__UInt32(ptr %this, i32 %value)");
    assert_contains(output, "define void @method.DynamicArray_Payload_.replace_first__Payload");
    assert_contains(output, "call i32 @scalar_demo()");
    assert_contains(output, "call i32 @owned_demo()");
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %this.dynamic_array_assign0.element.addr)");
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %values.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %values.dynamic_array_cleanup");
    assert_contains(output, "define i32 @method.DynamicArray_Payload_.first_value__Payload");
    assert_contains(output, "getelementptr [2 x i32]");
}

void assert_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(output.find("define void @__orison_owned_cleanup.Payload(ptr %value)") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_allocate") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_deallocate") != std::string::npos);
    assert(output.find("items.computed_for.") != std::string::npos);
    assert(output.find(".condition:") != std::string::npos);
    assert(output.find(".body:") != std::string::npos);

    auto const computed_drop = output.find(
        "call void @__orison_owned_cleanup.Payload(ptr %items.computed_dynamic_array_cleanup"
    );
    auto const computed_deallocation = output.find("call void @__orison_dynamic_array_deallocate", computed_drop);
    auto const finalization = output.find("store { ptr, i64, i64 } zeroinitializer, ptr %items.addr");
    auto const return_zero = output.find("ret i32 0");
    assert(computed_drop != std::string::npos);
    assert(computed_deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_zero != std::string::npos);
    assert(computed_drop < computed_deallocation);
    assert(computed_deallocation < finalization);
    assert(finalization < return_zero);
    assert(output.find("items.dynamic_array_cleanup") == std::string::npos);
}

void assert_owned_nested_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Payload = type { i32 }") != std::string::npos);
    assert(output.find("define void @__orison_owned_cleanup.Payload(ptr %value)") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_grow") != std::string::npos);
    assert(output.find("items.computed_for.2.condition:") != std::string::npos);
    assert(output.find("items.computed_for.2.body:") != std::string::npos);
    assert(output.find("items.computed_for.2.exit:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation items.computed_for.2.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation items.computed_for.2.cleanup.resume "
            "from items.loop.entry to items [cleanup calls enabled]"
        ) != std::string::npos
    );

    auto const computed_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.computed_dynamic_array_cleanup");
    auto const computed_deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.2.data", computed_drop);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %items.addr", computed_deallocation);
    auto const return_zero = output.find("ret i32 %tmp", finalization);
    assert(computed_drop != std::string::npos);
    assert(computed_deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_zero != std::string::npos);
    assert(computed_drop < computed_deallocation);
    assert(computed_deallocation < finalization);
    assert(finalization < return_zero);
    assert(output.find("items.dynamic_array_cleanup") == std::string::npos);
}

void assert_static_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view owner_name,
    std::string_view holder_type
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bucket = type { { ptr, i64, i64 } }");
    assert_contains(output, holder_type);
    assert_contains(output, "define void @__orison_owned_cleanup.Payload(ptr %value)");
    auto owner = std::string {owner_name};
    assert_contains(output, owner + ".computed_for.0.condition:");
    assert_contains(output, owner + ".computed_for.0.body:");
    assert_contains(output, owner + ".computed_for.0.exit:");
    assert_contains(
        output,
        "cleanup state handoff acquire operation " + owner + ".computed_for.0.cleanup.acquire "
        "from " + owner + " to " + owner + ".loop.entry [cleanup calls enabled]"
    );
    assert_contains(
        output,
        "cleanup state handoff resume operation " + owner + ".computed_for.0.cleanup.resume "
        "from " + owner + ".loop.entry to " + owner + " [cleanup calls enabled]"
    );

    auto const computed_drop = output.find(
        "call void @__orison_owned_cleanup.Payload(ptr %" + owner + ".computed_dynamic_array_cleanup"
    );
    auto const computed_deallocation = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %" + owner + ".computed_for.0.data",
        computed_drop
    );
    auto const computed_finalization = output.find(
        "store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr",
        computed_deallocation
    );
    auto const sibling_cleanup = output.find(
        "dynamic_array_cleanup",
        computed_finalization
    );
    auto const return_value = output.find("ret i32", sibling_cleanup);
    assert(computed_drop != std::string::npos);
    assert(computed_deallocation != std::string::npos);
    assert(computed_finalization != std::string::npos);
    assert(sibling_cleanup != std::string::npos);
    assert(return_value != std::string::npos);
    assert(computed_drop < computed_deallocation);
    assert(computed_deallocation < computed_finalization);
    assert(computed_finalization < sibling_cleanup);
    assert(sibling_cleanup < return_value);
}

void assert_runtime_index_aggregate_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view semantic_owner_name,
    std::string_view label_owner_name,
    std::string_view holder_type,
    std::initializer_list<std::string_view> dynamic_descriptor_fragments,
    std::string_view finalization_fragment,
    std::string_view sibling_cleanup_fragment
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bucket = type { { ptr, i64, i64 } }");
    assert_contains(output, holder_type);
    assert_contains(output, "define void @__orison_owned_cleanup.Payload(ptr %value)");
    assert_contains(output, "%computed_dynamic_array_runtime_aggregate_index");
    for (auto const dynamic_descriptor_fragment : dynamic_descriptor_fragments) {
        assert_contains(output, dynamic_descriptor_fragment);
    }
    auto semantic_owner = std::string {semantic_owner_name};
    auto label_owner = std::string {label_owner_name};
    assert_contains(output, label_owner + ".computed_for.0.condition:");
    assert_contains(output, label_owner + ".computed_for.0.body:");
    assert_contains(output, label_owner + ".computed_for.0.exit:");
    assert(output.find("call %record.Holder @forward_holder") == std::string::npos);
    assert_contains(
        output,
        "cleanup state handoff acquire operation " + label_owner + ".computed_for.0.cleanup.acquire "
        "from " + semantic_owner + " to " + semantic_owner + ".loop.entry [cleanup calls enabled]"
    );
    assert_contains(
        output,
        "cleanup state handoff resume operation " + label_owner + ".computed_for.0.cleanup.resume "
        "from " + semantic_owner + ".loop.entry to " + semantic_owner + " [cleanup calls enabled]"
    );

    auto const runtime_bounds = output.find("%computed_dynamic_array_runtime_aggregate_index");
    auto dynamic_descriptor = runtime_bounds;
    for (auto const dynamic_descriptor_fragment : dynamic_descriptor_fragments) {
        dynamic_descriptor = output.find(dynamic_descriptor_fragment, dynamic_descriptor);
        assert(dynamic_descriptor != std::string::npos);
    }
    auto const computed_drop = output.find(
        "call void @__orison_owned_cleanup.Payload(ptr %" + label_owner + ".computed_dynamic_array_cleanup"
    );
    auto const computed_deallocation = output.find(
        "call void @__orison_dynamic_array_deallocate(ptr %" + label_owner + ".computed_for.0.data",
        computed_drop
    );
    auto const computed_finalization = output.find(
        finalization_fragment,
        computed_deallocation
    );
    auto const sibling_cleanup = output.find(sibling_cleanup_fragment, computed_finalization);
    auto const return_value = output.find("ret i32", sibling_cleanup);
    assert(runtime_bounds != std::string::npos);
    assert(computed_drop != std::string::npos);
    assert(computed_deallocation != std::string::npos);
    assert(computed_finalization != std::string::npos);
    assert(sibling_cleanup != std::string::npos);
    assert(return_value != std::string::npos);
    assert(runtime_bounds < dynamic_descriptor);
    assert(dynamic_descriptor < computed_drop);
    assert(computed_drop < computed_deallocation);
    assert(computed_deallocation < computed_finalization);
    assert(computed_finalization < sibling_cleanup);
    assert(sibling_cleanup < return_value);
}

void assert_owned_dynamic_array_parameter_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(output.find("define void @__orison_owned_cleanup.Payload(ptr %value)") != std::string::npos);
    assert(
        output.find("define i64 @consume_items({ ptr, i64, i64 } %items)") !=
        std::string::npos
    );
    auto const length_read = output.find(".value = extractvalue { ptr, i64, i64 } %items.dynamic_array_length");
    assert(length_read != std::string::npos);
    auto const loop_descriptor =
        output.find("%items.sequence_for0.descriptor = load { ptr, i64, i64 }, ptr %items.addr");
    auto const loop_more =
        output.find("%items.sequence_for0.more = icmp ult i64 %items.sequence_for0.index, %items.sequence_for0.length");
    auto const loop_load =
        output.find("%items.sequence_for0.value = load %record.Payload, ptr %items.sequence_for0.element.addr");
    auto const loop_field_read =
        output.find("getelementptr %record.Payload, ptr %item.addr, i32 0, i32 0");
    assert(loop_descriptor != std::string::npos);
    assert(loop_more != std::string::npos);
    assert(loop_load != std::string::npos);
    assert(loop_field_read != std::string::npos);
    assert(
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") !=
        std::string::npos
    );
    auto const deallocation = output.find("call void @__orison_dynamic_array_deallocate");
    assert(deallocation != std::string::npos);
    assert(loop_descriptor < loop_load);
    assert(loop_load < loop_field_read);
    assert(length_read < deallocation);
    assert(loop_field_read < deallocation);
    assert(output.find("call void @__orison_dynamic_array_deallocate", deallocation + 1) == std::string::npos);
    assert(output.find("icmp eq i64") != std::string::npos);
    assert(output.find("ret i32 %") != std::string::npos);
}

void assert_owned_dynamic_array_parameter_missing_drop_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find(
            "lowering DynamicArray parameter 'items' with owned element type Payload requires ownership/drop proof "
            "before production lowering"
        ) != std::string::npos
    );
}

void assert_owned_dynamic_array_parameter_branch_join_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(output.find("define i32 @choose(i1 %flag, { ptr, i64, i64 } %items)") != std::string::npos);
    auto const first_transfer = output.find("call i32 @use_items({ ptr, i64, i64 } %items)");
    assert(first_transfer != std::string::npos);
    assert(output.find("call i32 @use_items({ ptr, i64, i64 } %items)", first_transfer + 1) != std::string::npos);
    assert(
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") !=
        std::string::npos
    );
    auto const deallocation = output.find("call void @__orison_dynamic_array_deallocate");
    assert(deallocation != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate", deallocation + 1) == std::string::npos);
}

void assert_owned_dynamic_array_parameter_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view control_flow_ir
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define i32 @choose(i1 %flag, { ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find(control_flow_ir) != std::string::npos);
    assert(output.find("call i32 @use_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") !=
        std::string::npos
    );
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
           std::string::npos);
    assert(output.find("if branch ownership mismatch") == std::string::npos);
    assert(output.find("switch case ownership mismatch") == std::string::npos);
}

void assert_owned_dynamic_array_parameter_statement_branch_mismatch_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find(
            "if branch ownership mismatch: owned transfers must match across all continuing branches"
        ) != std::string::npos
    );
}

void assert_returned_dynamic_array_distinct_choice_payload_branch_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(
        output,
        "right.Primary.values.choice_dynamic_array_cleanup"
    );
    assert_contains(
        output,
        "left.Primary.values.choice_dynamic_array_cleanup"
    );
    assert_contains(
        output,
        "phi i32 [%tmp2, %right.Primary.values.choice_dynamic_array_cleanup"
    );
    assert_contains(
        output,
        "[%tmp8, %left.Primary.values.choice_dynamic_array_cleanup"
    );
    assert_excludes(
        output,
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );
    assert_excludes(
        output,
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
}

void assert_branch_local_named_dynamic_array_cleanup(
    std::string const& output,
    std::string_view left_owner,
    std::string_view right_owner,
    std::string_view phi_type
) {
    auto left_cleanup = std::string {left_owner} + ".dynamic_array_cleanup";
    auto right_cleanup = std::string {right_owner} + ".dynamic_array_cleanup";
    assert_contains(output, left_cleanup);
    assert_contains(output, right_cleanup);
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %" + left_cleanup);
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %" + right_cleanup);
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %" + left_cleanup);
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %" + right_cleanup);
    assert_contains(output, "phi " + std::string {phi_type} + " [0, %" + left_cleanup);
    assert_contains(output, "[1, %" + right_cleanup);
}

void assert_branch_local_scratch_dynamic_array_cleanup(
    std::string const& output
) {
    assert_contains(output, "scratch.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %scratch.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %scratch.dynamic_array_cleanup");
    assert_contains(output, "phi { ptr, i64, i64 } [%tmp");
    assert_contains(output, "%scratch.dynamic_array_cleanup");
    assert_excludes(output, "returned.dynamic_array_cleanup");
}

void assert_branch_local_returned_dynamic_array_cleanup(
    std::string const& output
) {
    assert_contains(output, "left_values.dynamic_array_cleanup");
    assert_contains(output, "right_values.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %left_values.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %right_values.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %left_values.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %right_values.dynamic_array_cleanup");
    assert_contains(output, "phi { ptr, i64, i64 } [%tmp");
    assert_excludes(output, "if branch ownership mismatch");
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_branch_local_dynamic_array_cleanup_for_owners(
    std::string const& output,
    std::initializer_list<std::string_view> owner_names
) {
    for (auto const owner_name : owner_names) {
        auto cleanup_name = std::string {owner_name} + ".dynamic_array_cleanup";
        assert_contains(output, cleanup_name);
        assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %" + cleanup_name);
        assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %" + cleanup_name);
    }
}

void assert_no_branch_local_dynamic_array_cleanup_for_owners(
    std::string const& output,
    std::initializer_list<std::string_view> owner_names
) {
    for (auto const owner_name : owner_names) {
        assert_excludes(output, std::string {owner_name} + ".dynamic_array_cleanup");
    }
}

void assert_branch_local_three_case_returned_dynamic_array_cleanup(
    std::string const& output
) {
    assert_branch_local_dynamic_array_cleanup_for_owners(output, {"first_scratch", "second_scratch", "third_scratch"});
    assert_contains(output, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners(output, {"first_values", "second_values", "third_values"});
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup(
    std::string const& output
) {
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"first_scratch", "left_scratch", "right_scratch", "third_scratch"}
    );
    assert_contains(output, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"first_values", "left_values", "right_values", "third_values"}
    );
    assert_excludes(output, "if branch ownership mismatch");
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_branch_local_multi_nested_switch_dynamic_array_cleanup(
    std::string const& output
) {
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {
            "first_left_scratch",
            "first_right_scratch",
            "second_left_scratch",
            "second_right_scratch",
            "third_scratch",
        }
    );
    assert_contains(output, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {
            "first_left_values",
            "first_right_values",
            "second_left_values",
            "second_right_values",
            "third_values",
        }
    );
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_branch_local_if_two_switches_dynamic_array_cleanup(
    std::string const& output
) {
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {
            "first_left_scratch",
            "first_right_scratch",
            "second_left_scratch",
            "second_right_scratch",
        }
    );
    assert_contains(output, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {
            "first_left_values",
            "first_right_values",
            "second_left_values",
            "second_right_values",
        }
    );
    assert_excludes(output, "if branch ownership mismatch");
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_branch_local_switch_two_ifs_dynamic_array_cleanup(
    std::string const& output
) {
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {
            "first_left_scratch",
            "first_right_scratch",
            "second_left_scratch",
            "second_right_scratch",
            "third_scratch",
        }
    );
    assert_contains(output, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {
            "first_left_values",
            "first_right_values",
            "second_left_values",
            "second_right_values",
            "third_values",
        }
    );
    assert_excludes(output, "if branch ownership mismatch");
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_dynamic_array_local_final_if_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @choose(i1 %flag)");
    assert_branch_local_named_dynamic_array_cleanup(output, "left_values", "right_values", "i32");
}

void assert_dynamic_array_local_final_switch_case_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @choose(i1 %flag)");
    assert_contains(output, "switch i1 %flag");
    assert_branch_local_named_dynamic_array_cleanup(output, "left_values", "right_values", "i32");
}

void assert_dynamic_array_local_final_if_consumed_owner_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @choose(i1 %flag)");
    assert_contains(output, "br i1 %flag");
    assert_contains(output, "call i32 @use_items({ ptr, i64, i64 } %tmp");
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup");
    assert_excludes(output, "if branch ownership mismatch");
}

void assert_dynamic_array_local_final_switch_consumed_owner_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @choose(i1 %flag)");
    assert_contains(output, "switch i1 %flag");
    assert_contains(output, "call i32 @use_items({ ptr, i64, i64 } %tmp");
    assert_contains(output, "call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup");
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_dynamic_array_owned_result_final_if_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_branch_local_scratch_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_final_switch_case_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_contains(output, "switch i1 %flag");
    assert_branch_local_scratch_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_returned_local_final_if_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_returned_local_final_switch_case_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_contains(output, "switch i1 %flag");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_nested_final_if_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_nested_final_switch_case_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)");
    assert_contains(output, "switch i1 %outer");
    assert_contains(output, "switch i1 %inner");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_if_switch_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)");
    assert_contains(output, "switch i1 %inner");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_switch_if_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)");
    assert_contains(output, "switch i1 %outer");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_direct_nested_if_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_direct_nested_switch_branch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)");
    assert_contains(output, "switch i1 %inner");
    assert_branch_local_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_three_case_switch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector)");
    assert_contains(output, "switch i32 %selector");
    assert_branch_local_three_case_returned_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_three_case_mixed_switch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %flag)");
    assert_contains(output, "switch i32 %selector");
    assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_three_case_nested_switch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %inner)");
    assert_contains(output, "switch i32 %selector");
    assert_contains(output, "switch i1 %inner");
    assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_multi_nested_switch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "switch i32 %selector");
    assert_contains(output, "switch i1 %left_selector");
    assert_contains(output, "switch i1 %right_selector");
    assert_branch_local_multi_nested_switch_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "switch i32 %selector");
    assert_contains(output, "switch i1 %left_selector");
    assert_contains(output, "switch i1 %right_selector");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_branch_local_multi_nested_switch_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_if_two_switches_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "switch i1 %left_selector");
    assert_contains(output, "switch i1 %right_selector");
    assert_branch_local_if_two_switches_dynamic_array_cleanup(output);
}

void assert_dynamic_array_if_two_switches_consumed_scratch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "br i1 %outer");
    assert_contains(output, "switch i1 %left_selector");
    assert_contains(output, "switch i1 %right_selector");
    assert_contains(output, "call i32 @consume_items({ ptr, i64, i64 } %tmp");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"first_right_scratch", "second_left_scratch", "second_right_scratch"}
    );
    assert_no_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"first_left_scratch", "first_left_values", "first_right_values", "second_left_values", "second_right_values"}
    );
    assert_excludes(output, "if branch ownership mismatch");
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_dynamic_array_owned_result_if_two_switches_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %outer, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "br i1 %outer");
    assert_contains(output, "switch i1 %left_selector");
    assert_contains(output, "switch i1 %right_selector");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_branch_local_if_two_switches_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_switch_two_ifs_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "switch i32 %selector");
    assert_contains(output, "br i1 %left_selector");
    assert_contains(output, "br i1 %right_selector");
    assert_branch_local_switch_two_ifs_dynamic_array_cleanup(output);
}

void assert_dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "switch i32 %selector");
    assert_contains(output, "br i1 %left_selector");
    assert_contains(output, "br i1 %right_selector");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_branch_local_switch_two_ifs_dynamic_array_cleanup(output);
}

void assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: items") != std::string::npos);
}

void assert_dynamic_array_use_after_move_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view moved_owner
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: " + std::string {moved_owner}) != std::string::npos);
}

void assert_dynamic_array_switch_returned_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: first_left_values") != std::string::npos);
}

void assert_dynamic_array_helper_call_returned_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: first_left_values") != std::string::npos);
}

void assert_dynamic_array_ternary_named_helper_call_returned_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: left_values") != std::string::npos);
}

void assert_dynamic_array_ternary_local_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: selected") != std::string::npos);
}

void assert_dynamic_array_ternary_final_local_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: final_selected") != std::string::npos);
}

void assert_dynamic_array_ternary_scratch_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: scratch") != std::string::npos);
}

void assert_dynamic_array_ternary_parameter_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: values") != std::string::npos);
}

void assert_dynamic_array_ternary_alias_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: alias") != std::string::npos);
}

void assert_dynamic_array_multi_nested_switch_consumed_scratch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)");
    assert_contains(output, "call i32 @consume_items({ ptr, i64, i64 } %tmp");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"first_right_scratch", "second_left_scratch", "second_right_scratch", "third_scratch"}
    );
    assert_excludes(output, "%first_left_scratch.dynamic_array_cleanup");
    assert_excludes(output, "switch case ownership mismatch");
}

void assert_dynamic_array_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i32 %selector, i1 %inner)");
    assert_contains(output, "switch i32 %selector");
    assert_contains(output, "br i1 %inner");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup(output);
}

void assert_dynamic_array_ternary_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_contains(output, "br i1 %flag");
    assert_contains(output, "ternary.then.");
    assert_contains(output, "ternary.else.");
    assert_contains(output, "ternary.merge.");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_excludes(output, "%values.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_named_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_contains(output, "br i1 %flag");
    assert_contains(output, "ternary.then.");
    assert_contains(output, "ternary.else.");
    assert_contains(output, "ternary.merge.");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_named_chained_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_contains(output, "br i1 %flag");
    assert_contains(output, "ternary.then.");
    assert_contains(output, "ternary.else.");
    assert_contains(output, "ternary.merge.");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_named_helper_call_local_return_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %flag)");
    assert_contains(output, "br i1 %flag");
    assert_contains(output, "ternary.then.");
    assert_contains(output, "ternary.else.");
    assert_contains(output, "ternary.merge.");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_local_return_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "br i1 %select_left");
    assert_contains(output, "br i1 %finish_left_path");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_chained_branch_consumer_local_return_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "br i1 %select_left");
    assert_contains(output, "br i1 %finish_left_path");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_scratch_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "call { ptr, i64, i64 } @make_values(i32 31)");
    assert_contains(output, "call { ptr, i64, i64 } @make_values(i32 41)");
    assert_contains(output, "%forwarded_scratch.dynamic_array_cleanup");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%scratch.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_alias_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%alias.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_nested_alias_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%second_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%alias.dynamic_array_cleanup");
    assert_excludes(output, "%second_alias.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_asymmetric_alias_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%second_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%alias.dynamic_array_cleanup");
    assert_excludes(output, "%second_alias.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_alias_helper_call_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%second_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%alias.dynamic_array_cleanup");
    assert_excludes(output, "%second_alias.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_alias_helper_call_local_return_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%second_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%returned.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%alias.dynamic_array_cleanup");
    assert_excludes(output, "%second_alias.dynamic_array_cleanup");
    assert_excludes(output, "%returned.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_asymmetric_stored_helper_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%second_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%returned.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_return.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%alias.dynamic_array_cleanup");
    assert_excludes(output, "%second_alias.dynamic_array_cleanup");
    assert_excludes(output, "%returned.dynamic_array_cleanup");
    assert_excludes(output, "%final_return.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_three_local_helper_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%second_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%returned.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%middle_return.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_return.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%alias.dynamic_array_cleanup");
    assert_excludes(output, "%second_alias.dynamic_array_cleanup");
    assert_excludes(output, "%returned.dynamic_array_cleanup");
    assert_excludes(output, "%middle_return.dynamic_array_cleanup");
    assert_excludes(output, "%final_return.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_distinct_local_names_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%left_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%left_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%left_middle.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%left_final.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_second.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_middle.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_final.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%left_alias.dynamic_array_cleanup");
    assert_excludes(output, "%left_result.dynamic_array_cleanup");
    assert_excludes(output, "%left_middle.dynamic_array_cleanup");
    assert_excludes(output, "%left_final.dynamic_array_cleanup");
    assert_excludes(output, "%right_alias.dynamic_array_cleanup");
    assert_excludes(output, "%right_second.dynamic_array_cleanup");
    assert_excludes(output, "%right_result.dynamic_array_cleanup");
    assert_excludes(output, "%right_middle.dynamic_array_cleanup");
    assert_excludes(output, "%right_final.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_mixed_direct_distinct_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%left_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_second.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_middle.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_final.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%values.dynamic_array_cleanup");
    assert_excludes(output, "%left_alias.dynamic_array_cleanup");
    assert_excludes(output, "%right_alias.dynamic_array_cleanup");
    assert_excludes(output, "%right_second.dynamic_array_cleanup");
    assert_excludes(output, "%right_result.dynamic_array_cleanup");
    assert_excludes(output, "%right_middle.dynamic_array_cleanup");
    assert_excludes(output, "%right_final.dynamic_array_cleanup");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_nested_helper_argument_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_nested_argument_local_chain_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%left_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%left_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%left_final.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_middle.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%right_final.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
    assert_excludes(output, "%left_alias.dynamic_array_cleanup");
    assert_excludes(output, "%left_result.dynamic_array_cleanup");
    assert_excludes(output, "%left_final.dynamic_array_cleanup");
    assert_excludes(output, "%right_alias.dynamic_array_cleanup");
    assert_excludes(output, "%right_result.dynamic_array_cleanup");
    assert_excludes(output, "%right_middle.dynamic_array_cleanup");
    assert_excludes(output, "%right_final.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path, i1 %wrap_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%finished.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_left.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%finished.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_left.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path, i1 %wrap_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%finished.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_left.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_middle.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_final.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%finished.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_left.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_alias.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_result.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_middle.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_final.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path, i1 %wrap_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %");
    assert_contains(output, "%finished.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_middle.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_final.addr = alloca { ptr, i64, i64 }");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%wrapped_left.addr = alloca { ptr, i64, i64 }");
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%finished.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_alias.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_result.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_middle.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_final.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path, i1 %wrap_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %values)");
    assert_contains(output, "define { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %values)");
    assert_contains(output, "%finished.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_left.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_alias.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_result.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%wrapped_right_final.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%finished.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_left.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_alias.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_result.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_final.dynamic_array_cleanup");
}

void assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define { ptr, i64, i64 } @choose(i1 %select_left, i1 %finish_left_path, i1 %wrap_left_path)");
    assert_contains(output, "define { ptr, i64, i64 } @complete_values({ ptr, i64, i64 } %values)");
    assert_contains(output, "%finished.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%final_selected.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "%completed.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %");
    assert_contains(output, "call { ptr, i64, i64 } @complete_values({ ptr, i64, i64 } %");
    assert_branch_local_dynamic_array_cleanup_for_owners(
        output,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_excludes(output, "%selected.dynamic_array_cleanup");
    assert_excludes(output, "%finished.dynamic_array_cleanup");
    assert_excludes(output, "%final_selected.dynamic_array_cleanup");
    assert_excludes(output, "%completed.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_left.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_alias.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_result.dynamic_array_cleanup");
    assert_excludes(output, "%wrapped_right_final.dynamic_array_cleanup");
}

void assert_dynamic_array_parameter_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @overwrite({ ptr, i64, i64 } %items)");
    assert_contains(output, "items.dynamic_array_assign");
    assert_contains(output, "call void @__orison_dynamic_array_bounds_failed");
    assert_contains(output, "store i32 3, ptr %items.dynamic_array_assign");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate");
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Payload = type { i64 }");
    assert_contains(output, "define i64 @replace({ ptr, i64, i64 } %items)");
    assert_contains(output, "define void @__orison_owned_cleanup.Payload(ptr %value)");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup");
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_aggregate_parameter_field_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view holder_type,
    std::string_view replace_signature,
    std::string_view owner_prefix,
    std::string_view cleanup_callee
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Payload = type { i64 }");
    assert_contains(output, holder_type);
    assert_contains(output, replace_signature);
    assert_contains(output, "dynamic_array.aggregate_index.in_bounds");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %" + std::string {owner_prefix});
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const holder_cleanup = output.find(cleanup_callee);
    assert(holder_cleanup != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < holder_cleanup);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_out_of_bounds(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Payload = type { i64 }");
    assert_contains(output, "%record.Holder = type { [2 x %record.Group] }");
    assert_contains(output, "define i32 @replace(%record.Holder %holder, i64 %group_index, i64 %item_index)");
    auto const group_bounds = output.find(".in_bounds = icmp ult i64 %group_index, 2");
    auto const group_trap = output.find("call void @__orison_dynamic_array_bounds_failed()", group_bounds);
    auto const item_bounds = output.find(".in_bounds = icmp ult i64 %item_index, 2", group_trap);
    auto const dynamic_bounds = output.find("dynamic_array.aggregate_index.in_bounds", item_bounds);
    assert(group_bounds != std::string::npos);
    assert(group_trap != std::string::npos);
    assert(item_bounds != std::string::npos);
    assert(dynamic_bounds != std::string::npos);
    assert(group_bounds < group_trap);
    assert(group_trap < item_bounds);
    assert(item_bounds < dynamic_bounds);
    read_failing_command_output(executable.string() + " run " + source_path.string());
}

void assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_item_out_of_bounds(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Payload = type { i64 }");
    assert_contains(output, "%record.Holder = type { [2 x %record.Group] }");
    assert_contains(output, "define i32 @replace(%record.Holder %holder, i64 %group_index, i64 %item_index)");
    auto const group_bounds = output.find(".in_bounds = icmp ult i64 %group_index, 2");
    auto const item_bounds = output.find(".in_bounds = icmp ult i64 %item_index, 2", group_bounds);
    auto const item_trap = output.find("call void @__orison_dynamic_array_bounds_failed()", item_bounds);
    auto const dynamic_bounds = output.find("dynamic_array.aggregate_index.in_bounds", item_trap);
    assert(group_bounds != std::string::npos);
    assert(item_bounds != std::string::npos);
    assert(item_trap != std::string::npos);
    assert(dynamic_bounds != std::string::npos);
    assert(group_bounds < item_bounds);
    assert(item_bounds < item_trap);
    assert(item_trap < dynamic_bounds);
    read_failing_command_output(executable.string() + " run " + source_path.string());
}

void assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_out_of_bounds(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Payload = type { i64 }");
    assert_contains(output, "%record.Holder = type { [2 x %record.Group] }");
    assert_contains(
        output,
        "define i32 @replace(%record.Holder %holder, i64 %group_index, i64 %item_index, i64 %value_index)"
    );
    auto const group_bounds = output.find(".in_bounds = icmp ult i64 %group_index, 2");
    auto const item_bounds = output.find(".in_bounds = icmp ult i64 %item_index, 2", group_bounds);
    auto const dynamic_bounds = output.find("dynamic_array.aggregate_index.in_bounds", item_bounds);
    auto const dynamic_trap = output.find("call void @__orison_dynamic_array_bounds_failed()", dynamic_bounds);
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %current.groups.element.items.element.values");
    assert(group_bounds != std::string::npos);
    assert(item_bounds != std::string::npos);
    assert(dynamic_bounds != std::string::npos);
    assert(dynamic_trap != std::string::npos);
    assert(replacement_drop != std::string::npos);
    assert(group_bounds < item_bounds);
    assert(item_bounds < dynamic_bounds);
    assert(dynamic_bounds < dynamic_trap);
    assert(dynamic_trap < replacement_drop);
    read_failing_command_output(executable.string() + " run " + source_path.string());
}

void assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Payload = type { i64 }");
    assert_contains(output, "%record.Holder = type { [2 x %record.Group] }");
    assert_contains(
        output,
        "define i32 @replace(%record.Holder %holder, i64 %group_index, i64 %item_index, i64 %value_index)"
    );
    auto const group_bounds = output.find(".in_bounds = icmp ult i64 %group_index, 2");
    auto const item_bounds = output.find(".in_bounds = icmp ult i64 %item_index, 2", group_bounds);
    auto const dynamic_bounds = output.find("dynamic_array.aggregate_index.in_bounds", item_bounds);
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %current.groups.element.items.element.values");
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    auto const holder_cleanup = output.find("call void @__orison_owned_cleanup.Holder(ptr %current.addr)");
    assert(group_bounds != std::string::npos);
    assert(item_bounds != std::string::npos);
    assert(dynamic_bounds != std::string::npos);
    assert(replacement_drop != std::string::npos);
    assert(replacement_store != std::string::npos);
    assert(holder_cleanup != std::string::npos);
    assert(group_bounds < item_bounds);
    assert(item_bounds < dynamic_bounds);
    assert(dynamic_bounds < replacement_drop);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < holder_cleanup);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_branch_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace(i1 %flag, { ptr, i64, i64 } %items)");
    assert_contains(output, "br i1 %flag");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const branch_merge = output.find("if.merge", replacement_store);
    assert(branch_merge != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", branch_merge);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_switch_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace(i1 %flag, { ptr, i64, i64 } %items)");
    assert_contains(output, "switch i1 %flag");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const switch_merge = output.find("switch.merge", replacement_store);
    assert(switch_merge != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", switch_merge);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_loop_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace({ ptr, i64, i64 } %items)");
    assert_contains(output, "while.cond");
    assert_contains(output, "while.exit");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const loop_exit = output.find("while.exit", replacement_store);
    assert(loop_exit != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", loop_exit);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_repeat_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace({ ptr, i64, i64 } %items)");
    assert_contains(output, "repeat.body");
    assert_contains(output, "repeat.exit");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const repeat_exit = output.find("repeat.exit", replacement_store);
    assert(repeat_exit != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", repeat_exit);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_loop_break_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace({ ptr, i64, i64 } %items)");
    assert_contains(output, "while.cond");
    assert_contains(output, "while.exit");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const break_branch = output.find("br label %while.exit", replacement_store);
    assert(break_branch != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", break_branch);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_loop_continue_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace({ ptr, i64, i64 } %items)");
    assert_contains(output, "while.cond");
    assert_contains(output, "while.exit");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const continue_branch = output.find("br label %while.cond", replacement_store);
    assert(continue_branch != std::string::npos);
    auto const loop_exit = output.find("while.exit", continue_branch);
    assert(loop_exit != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", loop_exit);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_repeat_break_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace({ ptr, i64, i64 } %items)");
    assert_contains(output, "repeat.body");
    assert_contains(output, "repeat.exit");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const break_branch = output.find("br label %repeat.exit", replacement_store);
    assert(break_branch != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", break_branch);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_owned_dynamic_array_parameter_repeat_continue_index_assignment_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @replace({ ptr, i64, i64 } %items)");
    assert_contains(output, "repeat.condition");
    assert_contains(output, "repeat.exit");
    auto const replacement_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const continue_branch = output.find("br label %repeat.condition", replacement_store);
    assert(continue_branch != std::string::npos);
    auto const repeat_exit = output.find("repeat.exit", continue_branch);
    assert(repeat_exit != std::string::npos);
    auto const cleanup_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup", repeat_exit);
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    assert_run_success(executable, source_path);
}

void assert_dynamic_array_parameter_push_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "define i32 @append({ ptr, i64, i64 } %items)");
    assert_contains(output, "call void @__orison_dynamic_array_allocate");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate");
    assert_contains(output, "items.dynamic_array_append");
    assert_contains(output, "store { ptr, i64, i64 } %items.dynamic_array_append");
    assert_run_success(executable, source_path);
}

void assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find(
            "lowering computed DynamicArray cleanup for owned element type Payload requires authorized element drop"
        ) != std::string::npos
    );
    assert(
        output.find(
            "computed DynamicArray owned element cleanup authorization missing for Payload"
        ) != std::string::npos
    );
    assert(
        output.find(
            "lowering does not yet support this final control-flow statement"
        ) == std::string::npos
    );
}

void assert_choice_payload_final_switch_computed_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: values") != std::string::npos);
}

void assert_choice_payload_owned_computed_cleanup_run_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "call void @__orison_owned_cleanup.Payload");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate");
    assert_run_success(executable, source_path);
}

void assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view owner_name
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: " + std::string {owner_name}) != std::string::npos);
}

void assert_computed_dynamic_array_owner_mismatch_failure_matrix(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::filesystem::path const& object_path,
    std::filesystem::path const& output_path,
    std::string_view left_owner,
    std::string_view right_owner
) {
    auto const expected_summary_fragment =
        "computed DynamicArray owner mismatch: expected one cleanup owner; branches resolve to " +
        std::string {left_owner} + " " + std::string {right_owner} + " for DynamicArray<Payload>";
    auto const expected_owner_fragment =
        "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<Payload> "
        "element Payload owners " +
        std::string {left_owner} + " " + std::string {right_owner} + " "
        "[ownership join blocked] [cleanup owner blocked] (metadata only)";

    for (auto const& command : {
             executable.string() + " run " + source_path.string(),
             executable.string() + " --emit-llvm " + source_path.string(),
             executable.string() + " --emit-object " + source_path.string() + " -o " + object_path.string(),
             executable.string() + " --build " + source_path.string() + " -o " + output_path.string(),
         }) {
        auto output = read_failing_command_output(command);
        assert_contains(output, expected_summary_fragment);
        assert_contains(output, expected_owner_fragment);
    }
}

void assert_computed_dynamic_array_runtime_aggregate_owner_mismatch_failure_matrix(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::filesystem::path const& object_path,
    std::filesystem::path const& output_path,
    std::string_view left_owner,
    std::string_view right_owner
) {
    auto const left = std::string {left_owner};
    auto const right = std::string {right_owner};
    auto const expected_summary_fragment =
        "computed DynamicArray owner mismatch: expected one cleanup owner; branches resolve to " +
        left + " " + right + " for DynamicArray<Payload>";
    auto const expected_owner_fragment =
        "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<Payload> "
        "element Payload unsupported reason runtime aggregate index owners " +
        left + " " + right + " [ownership join blocked] [cleanup owner blocked] (metadata only)";
    auto const expected_gate_fragment =
        "computed DynamicArray cleanup authorization gate ownership join blocked source DynamicArray<Payload> "
        "element Payload [handoff inputs blocked] [production cleanup authorization blocked] "
        "[authorization contract blocked] [cleanup owner blocked] [runtime aggregate audit proof missing] "
        "[runtime aggregate single owner blocked] [runtime aggregate descriptor storage mismatch] "
        "[runtime aggregate cleanup authorization blocked] [production cleanup blocked] (metadata only)";

    for (auto const& command : {
             executable.string() + " run " + source_path.string(),
             executable.string() + " --emit-llvm " + source_path.string(),
             executable.string() + " --emit-object " + source_path.string() + " -o " + object_path.string(),
             executable.string() + " --build " + source_path.string() + " -o " + output_path.string(),
         }) {
        auto output = read_failing_command_output(command);
        assert_contains(output, expected_summary_fragment);
        assert_contains(output, expected_owner_fragment);
        assert_contains(output, expected_gate_fragment);
    }
}

void assert_computed_dynamic_array_unsupported_shape_failure_matrix(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::filesystem::path const& object_path,
    std::filesystem::path const& output_path
) {
    auto expected_summary_fragment =
        std::string {"computed DynamicArray unsupported shape: cannot prove a single owner for DynamicArray<Payload>; "
                     "forwarded owner resolution depth exhausted"};
    auto expected_shape_fragment = std::string {
        "computed DynamicArray ownership plan unsupported computed shape source DynamicArray<Payload> "
        "element Payload unsupported reason forwarded owner resolution depth exhausted"
    };
    expected_shape_fragment += " [ownership join blocked] [cleanup owner blocked] (metadata only)";

    for (auto const& command : {
             executable.string() + " run " + source_path.string(),
             executable.string() + " --emit-llvm " + source_path.string(),
             executable.string() + " --emit-object " + source_path.string() + " -o " + object_path.string(),
             executable.string() + " --build " + source_path.string() + " -o " + output_path.string(),
         }) {
        auto output = read_failing_command_output(command);
        assert_contains(output, expected_summary_fragment);
        assert_contains(output, expected_shape_fragment);
    }
}

void assert_returned_runtime_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Holder = type { [2 x %record.Group], { i1, { ptr, i64, i64 } }, { i32, { ptr, i64, i64 } } }");
    assert_contains(output, "define %record.Holder @make_holder()");
    assert_contains(output, "%returned_aggregate.make_holder...groups.group_index..items.item_index..values.aggregate.addr = alloca %record.Holder");
    assert_contains(output, "call %record.Holder @make_holder()");
    assert_contains(output, "computed_dynamic_array_runtime_aggregate_index");
    assert_contains(output, "store { ptr, i64, i64 } zeroinitializer, ptr %tmp");
    assert_contains(output, "returned_aggregate_members.returned_aggregate.make_holder...groups.group_index..items.item_index..values.groups.element0.items.element0.values.dynamic_array_cleanup");
    assert_contains(output, "returned_aggregate_members.returned_aggregate.make_holder...groups.group_index..items.item_index..values.groups.element1.items.element0.values.dynamic_array_cleanup");
    assert_contains(output, "returned_aggregate_members.returned_aggregate.make_holder...groups.group_index..items.item_index..values.maybe_values.maybe_dynamic_array_cleanup");
    assert_contains(output, "returned_aggregate_members.returned_aggregate.make_holder...groups.group_index..items.item_index..values.packet.Primary.values.choice_dynamic_array_cleanup");
    assert_contains(output, "returned_aggregate.make_holder...groups.group_index..items.item_index..values.computed_for.0.condition:");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %returned_aggregate.make_holder...groups.group_index..items.item_index..values.computed_for.0.data");
    assert_contains(output, "ret i32 %tmp");
}

void assert_diagnostic_failure_matrix(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::filesystem::path const& object_path,
    std::filesystem::path const& output_path,
    std::string_view expected_fragment
) {
    assert_contains(
        read_failing_command_output(executable.string() + " run " + source_path.string()),
        expected_fragment
    );
    assert_contains(
        read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string()),
        expected_fragment
    );
    assert_contains(
        read_failing_command_output(
            executable.string() + " --emit-object " + source_path.string() + " -o " + object_path.string()
        ),
        expected_fragment
    );
    assert_contains(
        read_failing_command_output(
            executable.string() + " --build " + source_path.string() + " -o " + output_path.string()
        ),
        expected_fragment
    );
}

void assert_returned_dynamic_array_parameter_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { ptr, i64, i64 } @make_items()") != std::string::npos);
    assert(output.find("define i32 @use_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("%returned.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("call i32 @use_items({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { ptr, i64, i64 } @make_items()") != std::string::npos);
    assert(output.find("define i32 @main()") != std::string::npos);
    assert(output.find("%returned.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("returned.computed_for.0.condition:") != std::string::npos);
    assert(output.find("returned.computed_for.0.body:") != std::string::npos);
    assert(output.find("returned.computed_for.0.exit:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation returned.computed_for.0.cleanup.acquire "
            "from returned to returned.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation returned.computed_for.0.cleanup.resume "
            "from returned.loop.entry to returned [cleanup calls enabled]"
        ) != std::string::npos
    );
    auto const drop_walk = output.find("returned.computed_dynamic_array_cleanup");
    auto const drop_call = output.find("call void @__orison_owned_cleanup.Payload(ptr %returned.computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %returned.computed_for.0.data", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %returned.addr", deallocation);
    auto const return_value = output.find("ret i32", finalization);
    assert(drop_walk != std::string::npos);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_value != std::string::npos);
    assert(drop_walk < drop_call);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(finalization < return_value);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_alias_chain_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { ptr, i64, i64 } @make_values()") != std::string::npos);
    assert(output.find("%alias.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("%second_alias.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("ret { ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("%returned.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("returned.computed_for.0.condition:") != std::string::npos);
    assert(output.find("returned.computed_for.0.body:") != std::string::npos);
    assert(output.find("returned.computed_for.0.exit:") != std::string::npos);
    auto const drop_walk = output.find("returned.computed_dynamic_array_cleanup");
    auto const drop_call = output.find("call void @__orison_owned_cleanup.Payload(ptr %returned.computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %returned.computed_for.0.data", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %returned.addr", deallocation);
    auto const return_value = output.find("ret i32", finalization);
    assert(drop_walk != std::string::npos);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_value != std::string::npos);
    assert(drop_walk < drop_call);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(finalization < return_value);
    assert(output.find("%values.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%alias.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%second_alias.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_helper_call_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { ptr, i64, i64 } @make_values()") != std::string::npos);
    assert(output.find("define { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %values)") !=
        std::string::npos);
    assert(output.find("define { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %values)") !=
        std::string::npos);
    assert(output.find("%returned.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("returned.computed_for.0.condition:") != std::string::npos);
    assert(output.find("returned.computed_for.0.body:") != std::string::npos);
    assert(output.find("returned.computed_for.0.exit:") != std::string::npos);
    auto const drop_walk = output.find("returned.computed_dynamic_array_cleanup");
    auto const drop_call = output.find("call void @__orison_owned_cleanup.Payload(ptr %returned.computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %returned.computed_for.0.data", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %returned.addr", deallocation);
    auto const return_value = output.find("ret i32 %tmp", finalization);
    assert(drop_walk != std::string::npos);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_value != std::string::npos);
    assert(drop_walk < drop_call);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(finalization < return_value);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
}

void assert_branch_returned_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { ptr, i64, i64 } @make_left()") != std::string::npos);
    assert(output.find("define { ptr, i64, i64 } @make_right()") != std::string::npos);
    assert(output.find("define { ptr, i64, i64 } @choose_items(i1 %flag)") != std::string::npos);
    assert(output.find("%tmp2 = phi { ptr, i64, i64 } [%tmp0, %if.then.0], [%tmp1, %if.else.0]") !=
        std::string::npos);
    assert(output.find("define i32 @main()") != std::string::npos);
    assert(output.find("%selected.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("selected.computed_for.0.condition:") != std::string::npos);
    assert(output.find("selected.computed_for.0.body:") != std::string::npos);
    assert(output.find("selected.computed_for.0.exit:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation selected.computed_for.0.cleanup.acquire "
            "from selected to selected.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation selected.computed_for.0.cleanup.resume "
            "from selected.loop.entry to selected [cleanup calls enabled]"
        ) != std::string::npos
    );
    auto const drop_walk = output.find("selected.computed_dynamic_array_cleanup");
    auto const drop_call = output.find("call void @__orison_owned_cleanup.Payload(ptr %selected.computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %selected.computed_for.0.data", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %selected.addr", deallocation);
    auto const return_value = output.find("ret i32 %tmp", finalization);
    assert(drop_walk != std::string::npos);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_value != std::string::npos);
    assert(drop_walk < drop_call);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(finalization < return_value);
    assert(output.find("%selected.dynamic_array_cleanup") == std::string::npos);
}

void assert_switch_returned_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { ptr, i64, i64 } @make_first()") != std::string::npos);
    assert(output.find("define { ptr, i64, i64 } @make_second()") != std::string::npos);
    assert(output.find("define { ptr, i64, i64 } @make_default()") != std::string::npos);
    assert(output.find("define { ptr, i64, i64 } @choose_items(i32 %selector)") != std::string::npos);
    assert(output.find("switch i32 %selector, label %switch.default.0") != std::string::npos);
    assert(
        output.find(
            "%tmp3 = phi { ptr, i64, i64 } [%tmp0, %switch.case.0.0], [%tmp1, %switch.case.0.1], "
            "[%tmp2, %switch.default.0]"
        ) != std::string::npos
    );
    assert(output.find("define i32 @main()") != std::string::npos);
    assert(output.find("%selected.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("selected.computed_for.0.condition:") != std::string::npos);
    assert(output.find("selected.computed_for.0.body:") != std::string::npos);
    assert(output.find("selected.computed_for.0.exit:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation selected.computed_for.0.cleanup.acquire "
            "from selected to selected.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation selected.computed_for.0.cleanup.resume "
            "from selected.loop.entry to selected [cleanup calls enabled]"
        ) != std::string::npos
    );
    auto const drop_walk = output.find("selected.computed_dynamic_array_cleanup");
    auto const drop_call = output.find("call void @__orison_owned_cleanup.Payload(ptr %selected.computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %selected.computed_for.0.data", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %selected.addr", deallocation);
    auto const return_value = output.find("ret i32 %tmp", finalization);
    assert(drop_walk != std::string::npos);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_value != std::string::npos);
    assert(drop_walk < drop_call);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(finalization < return_value);
    assert(output.find("%selected.dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view owner_name,
    std::string_view record_definition,
    std::string_view make_function
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    auto const owner = std::string {owner_name};
    assert(output.find(record_definition) != std::string::npos);
    assert(output.find(make_function) != std::string::npos);
    assert(output.find("%returned.addr = alloca") != std::string::npos);
    assert(output.find("%" + owner + ".addr") != std::string::npos);
    assert(output.find(owner + ".computed_for.0.condition:") != std::string::npos);
    assert(output.find(owner + ".computed_for.0.body:") != std::string::npos);
    assert(output.find(owner + ".computed_for.0.exit:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation " + owner + ".computed_for.0.cleanup.acquire "
            "from " + owner + " to " + owner + ".loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation " + owner + ".computed_for.0.cleanup.resume "
            "from " + owner + ".loop.entry to " + owner + " [cleanup calls enabled]"
        ) != std::string::npos
    );
    auto const drop_walk = output.find(owner + ".computed_dynamic_array_cleanup");
    auto const drop_call =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %" + owner + ".computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %" + owner + ".computed_for.0.data", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr", deallocation);
    auto const return_value = output.find("ret i32", finalization);
    assert(drop_walk != std::string::npos);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(return_value != std::string::npos);
    assert(drop_walk < drop_call);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(finalization < return_value);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%" + owner + ".dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view owner_name
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    auto const owner = std::string {owner_name};
    auto const returned_loop = output.find(owner + ".computed_for.1.condition:");
    auto const returned_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %" + owner + ".computed_dynamic_array_cleanup");
    auto const returned_deallocate =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %" + owner + ".computed_for.1.data");
    auto const returned_finalize =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr", returned_deallocate);
    auto const scratch_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %scratch.dynamic_array_cleanup", returned_finalize);
    auto const scratch_deallocate =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %scratch.dynamic_array_cleanup", scratch_drop);
    auto const scratch_finalize =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %scratch.addr", scratch_deallocate);
    auto const merge = output.find("switch.merge.0:", scratch_finalize);
    auto const return_value = output.find("ret i32 %tmp", merge);
    assert(output.find("switch i1 %flag") != std::string::npos);
    assert(returned_loop != std::string::npos);
    assert(returned_drop != std::string::npos);
    assert(returned_deallocate != std::string::npos);
    assert(returned_finalize != std::string::npos);
    assert(scratch_drop != std::string::npos);
    assert(scratch_deallocate != std::string::npos);
    assert(scratch_finalize != std::string::npos);
    assert(merge != std::string::npos);
    assert(return_value != std::string::npos);
    assert(returned_loop < returned_drop);
    assert(returned_drop < returned_deallocate);
    assert(returned_deallocate < returned_finalize);
    assert(returned_finalize < scratch_drop);
    assert(scratch_drop < scratch_deallocate);
    assert(scratch_deallocate < scratch_finalize);
    assert(scratch_finalize < merge);
    assert(merge < return_value);
    assert(output.find("switch case ownership mismatch") == std::string::npos);
    assert(output.find("%" + owner + ".dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view owner_name
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    auto const owner = std::string {owner_name};
    auto const returned_loop = output.find(owner + ".computed_for.1.condition:");
    auto const returned_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %" + owner + ".computed_dynamic_array_cleanup");
    auto const returned_deallocate =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %" + owner + ".computed_for.1.data");
    auto const returned_finalize =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr", returned_deallocate);
    auto const scratch_drop =
        output.find("call void @__orison_owned_cleanup.Payload(ptr %scratch.dynamic_array_cleanup", returned_finalize);
    auto const scratch_deallocate =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %scratch.dynamic_array_cleanup", scratch_drop);
    auto const scratch_finalize =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %scratch.addr", scratch_deallocate);
    auto const merge = output.find("if.merge.0:", scratch_finalize);
    auto const return_value = output.find("ret i32 %tmp", merge);
    assert(output.find("br i1 %flag, label %if.then.0, label %if.else.0") != std::string::npos);
    assert(returned_loop != std::string::npos);
    assert(returned_drop != std::string::npos);
    assert(returned_deallocate != std::string::npos);
    assert(returned_finalize != std::string::npos);
    assert(scratch_drop != std::string::npos);
    assert(scratch_deallocate != std::string::npos);
    assert(scratch_finalize != std::string::npos);
    assert(merge != std::string::npos);
    assert(return_value != std::string::npos);
    assert(returned_loop < returned_drop);
    assert(returned_drop < returned_deallocate);
    assert(returned_deallocate < returned_finalize);
    assert(returned_finalize < scratch_drop);
    assert(scratch_drop < scratch_deallocate);
    assert(scratch_deallocate < scratch_finalize);
    assert(scratch_finalize < merge);
    assert(merge < return_value);
    assert(output.find("if branch ownership mismatch") == std::string::npos);
    assert(output.find("%" + owner + ".dynamic_array_cleanup") == std::string::npos);
}

void assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define i32 @consume_packet({ i32, { ptr, i64, i64 } } %packet)") != std::string::npos);
    assert(output.find("switch i32 %tmp") != std::string::npos);
    assert(output.find("label %switch.unreachable.0") != std::string::npos);
    assert(output.find("%values.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("values.computed_for.") != std::string::npos);
    assert(output.find(".condition:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation values.computed_for."
        ) != std::string::npos
    );
    assert(
        output.find(
            "from values to values.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation values.computed_for."
        ) != std::string::npos
    );
    assert(
        output.find(
            "from values.loop.entry to values [cleanup calls enabled]"
        ) != std::string::npos
    );
    auto const drop_call = output.find("call void @__orison_owned_cleanup.Payload(ptr %values.computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %values.computed_for.", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %values.addr", deallocation);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(output.find("packet.Primary.values.choice_dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_choice_payload_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { i32, { ptr, i64, i64 } } @make_packet()") != std::string::npos);
    assert(output.find("call { i32, { ptr, i64, i64 } } @make_packet()") != std::string::npos);
    assert(output.find("%packet.addr = alloca { i32, { ptr, i64, i64 } }") != std::string::npos);
    assert(output.find("switch i32 %tmp") != std::string::npos);
    assert(output.find("%values.addr = alloca { ptr, i64, i64 }") != std::string::npos);
    assert(output.find("values.computed_for.") != std::string::npos);
    assert(output.find(".condition:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation values.computed_for."
        ) != std::string::npos
    );
    assert(
        output.find(
            "from values to values.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation values.computed_for."
        ) != std::string::npos
    );
    assert(
        output.find(
            "from values.loop.entry to values [cleanup calls enabled]"
        ) != std::string::npos
    );
    auto const drop_call = output.find("call void @__orison_owned_cleanup.Payload(ptr %values.computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %values.computed_for.", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %values.addr", deallocation);
    auto const packet_cleanup = output.find("%packet.choice_dynamic_array_cleanup", finalization);
    auto const return_value = output.find("ret i32 %tmp", packet_cleanup);
    assert(drop_call != std::string::npos);
    assert(deallocation != std::string::npos);
    assert(finalization != std::string::npos);
    assert(packet_cleanup != std::string::npos);
    assert(return_value != std::string::npos);
    assert(drop_call < deallocation);
    assert(deallocation < finalization);
    assert(finalization < packet_cleanup);
    assert(packet_cleanup < return_value);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %packet.choice_dynamic_array_cleanup") ==
        std::string::npos);
    assert(output.find("packet.Primary.values.choice_dynamic_array_cleanup") == std::string::npos);
}

void assert_returned_dynamic_array_multi_hop_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("define i32 @forward_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("call i32 @forward_items({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
    auto const forward_function_start = output.find("define i32 @forward_items");
    auto const forward_function_end = output.find("define i32 @main", forward_function_start);
    assert(forward_function_start != std::string::npos);
    assert(forward_function_end != std::string::npos);
    assert(
        output.find("__orison_dynamic_array_deallocate", forward_function_start) >
        forward_function_end
    );
}

void assert_returned_dynamic_array_branch_join_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("define i32 @choose_items(i1 %flag, { ptr, i64, i64 } %items)") != std::string::npos);
    auto const first_consume = output.find("call i32 @consume_items({ ptr, i64, i64 } %items)");
    assert(first_consume != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %items)", first_consume + 1) !=
        std::string::npos);
    assert(output.find("call i32 @choose_items(i1 0, { ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
    auto const choose_function_start = output.find("define i32 @choose_items");
    auto const choose_function_end = output.find("define i32 @main", choose_function_start);
    assert(choose_function_start != std::string::npos);
    assert(choose_function_end != std::string::npos);
    assert(
        output.find("__orison_dynamic_array_deallocate", choose_function_start) >
        choose_function_end
    );
}

void assert_returned_dynamic_array_choice_branch_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define { ptr, i64, i64 } @return_payload") != std::string::npos);
    assert(output.find("call { ptr, i64, i64 } @return_payload") != std::string::npos);
    assert(output.find("define i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("define i32 @choose_items(i1 %flag, { ptr, i64, i64 } %items)") != std::string::npos);
    auto const first_consume = output.find("call i32 @consume_items({ ptr, i64, i64 } %items)");
    assert(first_consume != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %items)", first_consume + 1) !=
        std::string::npos);
    assert(output.find("call i32 @choose_items(i1 0, { ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
    auto const choice_function_start = output.find("define { ptr, i64, i64 } @return_payload");
    auto const choice_function_end = output.find("define { ptr, i64, i64 } @make_payload", choice_function_start);
    assert(choice_function_start != std::string::npos);
    assert(choice_function_end != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", choice_function_start) > choice_function_end);
    auto const choose_function_start = output.find("define i32 @choose_items");
    auto const choose_function_end = output.find("define i32 @main", choose_function_start);
    assert(choose_function_start != std::string::npos);
    assert(choose_function_end != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", choose_function_start) > choose_function_end);
}

void assert_returned_dynamic_array_aggregate_field_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.PayloadBox = type { { ptr, i64, i64 } }") != std::string::npos);
    assert(output.find("define %record.PayloadBox @make_box()") != std::string::npos);
    assert(output.find("define i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("%returned.addr = alloca %record.PayloadBox") != std::string::npos);
    assert(output.find("%returned.values.addr") != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.values.dynamic_array_cleanup") == std::string::npos);
    auto const make_box_start = output.find("define %record.PayloadBox @make_box");
    auto const consume_start = output.find("define i32 @consume_items", make_box_start);
    assert(make_box_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", make_box_start) > consume_start);
    auto const main_start = output.find("define i32 @main");
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", main_start) == std::string::npos);
}

void assert_returned_dynamic_array_nested_aggregate_field_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.PayloadBox = type { { ptr, i64, i64 } }") != std::string::npos);
    assert(output.find("%record.OuterBox = type { %record.PayloadBox }") != std::string::npos);
    assert(output.find("define %record.OuterBox @make_outer_box()") != std::string::npos);
    assert(output.find("define i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("%returned.addr = alloca %record.OuterBox") != std::string::npos);
    assert(output.find("%returned.inner.values.addr") != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("%inner.values.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.inner.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.inner.values.dynamic_array_cleanup") == std::string::npos);
    auto const make_outer_start = output.find("define %record.OuterBox @make_outer_box");
    auto const consume_start = output.find("define i32 @consume_items", make_outer_start);
    assert(make_outer_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", make_outer_start) > consume_start);
    auto const main_start = output.find("define i32 @main");
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", main_start) == std::string::npos);
}

void assert_returned_dynamic_array_nested_aggregate_field_branch_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.PayloadBox = type { { ptr, i64, i64 } }") != std::string::npos);
    assert(output.find("%record.OuterBox = type { %record.PayloadBox }") != std::string::npos);
    assert(output.find("define %record.OuterBox @make_outer_box()") != std::string::npos);
    assert(output.find("define i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("define i32 @choose_items(i1 %flag, { ptr, i64, i64 } %items)") != std::string::npos);
    auto const first_consume = output.find("call i32 @consume_items({ ptr, i64, i64 } %items)");
    assert(first_consume != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %items)", first_consume + 1) !=
        std::string::npos);
    assert(output.find("%returned.addr = alloca %record.OuterBox") != std::string::npos);
    assert(output.find("%returned.inner.values.addr") != std::string::npos);
    assert(output.find("call i32 @choose_items(i1 0, { ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_owned_cleanup.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup") !=
        std::string::npos);
    assert(output.find("%inner.values.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.inner.dynamic_array_cleanup") == std::string::npos);
    assert(output.find("%returned.inner.values.dynamic_array_cleanup") == std::string::npos);
    auto const make_outer_start = output.find("define %record.OuterBox @make_outer_box");
    auto const consume_start = output.find("define i32 @consume_items", make_outer_start);
    assert(make_outer_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", make_outer_start) > consume_start);
    auto const choose_start = output.find("define i32 @choose_items");
    auto const main_start = output.find("define i32 @main", choose_start);
    assert(choose_start != std::string::npos);
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", choose_start) > main_start);
    assert(output.find("__orison_dynamic_array_deallocate", main_start) == std::string::npos);
}

void assert_returned_dynamic_array_aggregate_field_choice_payload_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PayloadBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "define %record.PayloadBox @make_box()");
    assert_contains(output, "define { ptr, i64, i64 } @unwrap_payload");
    assert_contains(output, "define i32 @consume_items({ ptr, i64, i64 } %items)");
    assert_contains(output, "%returned.addr = alloca %record.PayloadBox");
    assert_contains(output, "%returned.values.addr");
    assert_contains(output, "%unwrapped.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @unwrap_payload({ i32, { ptr, i64, i64 } } %tmp");
    assert_contains(output, "call i32 @consume_items({ ptr, i64, i64 } %tmp");
    assert_dynamic_array_payload_consumer_cleanup(output);
    assert_excludes(output, "%returned.values.dynamic_array_cleanup");
    assert_excludes(output, "%unwrapped.dynamic_array_cleanup");
    assert_excludes(output, ".choice_dynamic_array_cleanup0.cleanup.entry");
    auto const unwrap_start = output.find("define { ptr, i64, i64 } @unwrap_payload");
    auto const consume_start = output.find("define i32 @consume_items", unwrap_start);
    assert(unwrap_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", unwrap_start) > consume_start);
    assert_no_main_dynamic_array_deallocate(output);
}

void assert_returned_dynamic_array_aggregate_field_choice_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i32, { ptr, i64, i64 } } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_contains(output, ".packet.Primary.values.choice_dynamic_array_cleanup");
    assert_contains(output, "call { ptr, i64, i64 } @method.DynamicArray_UInt32_.forward__UInt32");
    assert_contains(output, "call i64 @method.DynamicArray_UInt32_.count__UInt32");
    assert_contains(output, "store { ptr, i64, i64 } zeroinitializer, ptr %tmp");
    auto const main_start = output.find("define i32 @main");
    auto const choice_cleanup =
        output.find(".packet.Primary.values.choice_dynamic_array_cleanup", main_start);
    auto const return_value = output.find("ret i32 %tmp", main_start);
    assert(main_start != std::string::npos);
    assert(choice_cleanup != std::string::npos);
    assert(return_value != std::string::npos);
    assert(choice_cleanup < return_value);
}

void assert_returned_dynamic_array_aggregate_field_choice_record_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PacketBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i32, %record.PacketBox } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(output, {".packet.Primary.box.values.choice_dynamic_array_cleanup"});
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_choice_fixed_array_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i32, [2 x { ptr, i64, i64 }] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".packet.Primary.values.element0.choice_dynamic_array_cleanup",
            ".packet.Primary.values.element1.choice_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i32, [2 x [2 x { ptr, i64, i64 }]] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".packet.Primary.values.element0.element0.choice_dynamic_array_cleanup",
            ".packet.Primary.values.element0.element1.choice_dynamic_array_cleanup",
            ".packet.Primary.values.element1.element0.choice_dynamic_array_cleanup",
            ".packet.Primary.values.element1.element1.choice_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PacketBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i32, [2 x %record.PacketBox] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".packet.Primary.boxes.element0.values.choice_dynamic_array_cleanup",
            ".packet.Primary.boxes.element1.values.choice_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PacketBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i32, [2 x [2 x %record.PacketBox]] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".packet.Primary.boxes.element0.element0.values.choice_dynamic_array_cleanup",
            ".packet.Primary.boxes.element0.element1.values.choice_dynamic_array_cleanup",
            ".packet.Primary.boxes.element1.element0.values.choice_dynamic_array_cleanup",
            ".packet.Primary.boxes.element1.element1.values.choice_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_fixed_array_choice_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(
        output,
        "%record.Bundle = type { { ptr, i64, i64 }, [2 x %record.PacketBox] }"
    );
    assert_contains(output, "%record.PacketBox = type { { i32, { ptr, i64, i64 } } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_contains(output, ".packets.element0.packet.Primary.values.choice_dynamic_array_cleanup");
    assert_contains(output, ".packets.element1.packet.Primary.values.choice_dynamic_array_cleanup");
    assert_contains(output, "call { ptr, i64, i64 } @method.DynamicArray_UInt32_.forward__UInt32");
    assert_contains(output, "call i64 @method.DynamicArray_UInt32_.count__UInt32");
    auto const main_start = output.find("define i32 @main");
    auto const element0_cleanup =
        output.find(".packets.element0.packet.Primary.values.choice_dynamic_array_cleanup", main_start);
    auto const element1_cleanup =
        output.find(".packets.element1.packet.Primary.values.choice_dynamic_array_cleanup", main_start);
    auto const return_value = output.find("ret i32 %tmp", main_start);
    assert(main_start != std::string::npos);
    assert(element0_cleanup != std::string::npos);
    assert(element1_cleanup != std::string::npos);
    assert(return_value != std::string::npos);
    assert(element0_cleanup < return_value);
    assert(element1_cleanup < return_value);
}

void assert_returned_dynamic_array_aggregate_field_nested_fixed_array_choice_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(
        output,
        "%record.Bundle = type { { ptr, i64, i64 }, [2 x [2 x %record.PacketBox]] }"
    );
    assert_contains(output, "%record.PacketBox = type { { i32, { ptr, i64, i64 } } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    auto const main_start = output.find("define i32 @main");
    auto const return_value = output.find("ret i32 %tmp", main_start);
    assert(main_start != std::string::npos);
    assert(return_value != std::string::npos);
    for (auto const* owner_fragment : {
             ".packets.element0.element0.packet.Primary.values.choice_dynamic_array_cleanup",
             ".packets.element0.element1.packet.Primary.values.choice_dynamic_array_cleanup",
             ".packets.element1.element0.packet.Primary.values.choice_dynamic_array_cleanup",
             ".packets.element1.element1.packet.Primary.values.choice_dynamic_array_cleanup",
         }) {
        assert_contains(output, owner_fragment);
        auto const cleanup = output.find(owner_fragment, main_start);
        assert(cleanup != std::string::npos);
        assert(cleanup < return_value);
    }
    assert_contains(output, "call { ptr, i64, i64 } @method.DynamicArray_UInt32_.forward__UInt32");
    assert_contains(output, "call i64 @method.DynamicArray_UInt32_.count__UInt32");
}

void assert_returned_dynamic_array_aggregate_field_maybe_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i1, { ptr, i64, i64 } } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(output, {".maybe_values.Some.value.maybe_dynamic_array_cleanup"});
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_fixed_array_maybe_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, [2 x { i1, { ptr, i64, i64 } }] }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".maybe_values.element0.Some.value.maybe_dynamic_array_cleanup",
            ".maybe_values.element1.Some.value.maybe_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, [2 x [2 x { i1, { ptr, i64, i64 } }]] }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".maybe_values.element0.element0.Some.value.maybe_dynamic_array_cleanup",
            ".maybe_values.element0.element1.Some.value.maybe_dynamic_array_cleanup",
            ".maybe_values.element1.element0.Some.value.maybe_dynamic_array_cleanup",
            ".maybe_values.element1.element1.Some.value.maybe_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_maybe_record_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Box = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i1, %record.Box } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(output, {".maybe_box.Some.value.values.maybe_dynamic_array_cleanup"});
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i1, [2 x { ptr, i64, i64 }] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".maybe_values.Some.value.element0.maybe_dynamic_array_cleanup",
            ".maybe_values.Some.value.element1.maybe_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i1, [2 x [2 x { ptr, i64, i64 }]] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".maybe_values.Some.value.element0.element0.maybe_dynamic_array_cleanup",
            ".maybe_values.Some.value.element0.element1.maybe_dynamic_array_cleanup",
            ".maybe_values.Some.value.element1.element0.maybe_dynamic_array_cleanup",
            ".maybe_values.Some.value.element1.element1.maybe_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Box = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i1, [2 x %record.Box] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".maybe_boxes.Some.value.element0.values.maybe_dynamic_array_cleanup",
            ".maybe_boxes.Some.value.element1.values.maybe_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.Box = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.Bundle = type { { ptr, i64, i64 }, { i1, [2 x [2 x %record.Box]] } }");
    assert_contains(output, "define %record.Bundle @make_bundle()");
    assert_contains(output, "%dynamic_array_receiver_aggregate_tmp");
    assert_main_cleanup_fragments_before_return(
        output,
        {
            ".maybe_boxes.Some.value.element0.element0.values.maybe_dynamic_array_cleanup",
            ".maybe_boxes.Some.value.element0.element1.values.maybe_dynamic_array_cleanup",
            ".maybe_boxes.Some.value.element1.element0.values.maybe_dynamic_array_cleanup",
            ".maybe_boxes.Some.value.element1.element1.values.maybe_dynamic_array_cleanup",
        }
    );
    assert_returned_dynamic_array_field_forwarding_calls(output);
}

void assert_returned_dynamic_array_aggregate_field_stored_choice_payload_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PayloadBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "define %record.PayloadBox @make_box()");
    assert_contains(output, "define { ptr, i64, i64 } @unwrap_payload");
    assert_contains(output, "define i32 @consume_items({ ptr, i64, i64 } %items)");
    assert_contains(output, "%returned.addr = alloca %record.PayloadBox");
    assert_contains(output, "%packet.addr = alloca { i32, { ptr, i64, i64 } }");
    assert_contains(output, "%unwrapped.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @unwrap_payload({ i32, { ptr, i64, i64 } } %tmp");
    assert_contains(output, "call i32 @consume_items({ ptr, i64, i64 } %tmp");
    assert_dynamic_array_payload_consumer_cleanup(output);
    assert_excludes(output, "%returned.values.dynamic_array_cleanup");
    assert_excludes(output, "%unwrapped.dynamic_array_cleanup");
    assert_excludes(output, "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry");
    auto const unwrap_start = output.find("define { ptr, i64, i64 } @unwrap_payload");
    auto const consume_start = output.find("define i32 @consume_items", unwrap_start);
    assert(unwrap_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", unwrap_start) > consume_start);
    assert_no_main_dynamic_array_deallocate(output);
}

void assert_returned_dynamic_array_aggregate_field_stored_choice_payload_branch_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PayloadBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "define %record.PayloadBox @make_box()");
    assert_contains(output, "define { ptr, i64, i64 } @unwrap_payload");
    assert_contains(output, "define i32 @consume_items({ ptr, i64, i64 } %items)");
    assert_contains(output, "define i32 @choose_items(i1 %flag, { ptr, i64, i64 } %items)");
    auto const first_consume = output.find("call i32 @consume_items({ ptr, i64, i64 } %items)");
    assert(first_consume != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %items)", first_consume + 1) !=
        std::string::npos);
    assert_contains(output, "%returned.addr = alloca %record.PayloadBox");
    assert_contains(output, "%packet.addr = alloca { i32, { ptr, i64, i64 } }");
    assert_contains(output, "%unwrapped.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @unwrap_payload({ i32, { ptr, i64, i64 } } %tmp");
    assert_contains(output, "call i32 @choose_items(i1 0, { ptr, i64, i64 } %tmp");
    assert_dynamic_array_payload_consumer_cleanup(output);
    assert_excludes(output, "%returned.values.dynamic_array_cleanup");
    assert_excludes(output, "%unwrapped.dynamic_array_cleanup");
    assert_excludes(output, "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry");
    auto const unwrap_start = output.find("define { ptr, i64, i64 } @unwrap_payload");
    auto const consume_start = output.find("define i32 @consume_items", unwrap_start);
    auto const choose_start = output.find("define i32 @choose_items", consume_start);
    auto const main_start = output.find("define i32 @main", choose_start);
    assert(unwrap_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(choose_start != std::string::npos);
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", unwrap_start) > consume_start);
    assert(output.find("__orison_dynamic_array_deallocate", choose_start) > main_start);
    assert_no_main_dynamic_array_deallocate(output);
}

void assert_returned_dynamic_array_nested_aggregate_field_stored_choice_payload_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PayloadBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.OuterBox = type { %record.PayloadBox }");
    assert_contains(output, "define %record.OuterBox @make_outer_box()");
    assert_contains(output, "define { ptr, i64, i64 } @unwrap_payload");
    assert_contains(output, "define i32 @consume_items({ ptr, i64, i64 } %items)");
    assert_contains(output, "%returned.addr = alloca %record.OuterBox");
    assert_contains(output, "%returned.inner.values.addr");
    assert_contains(output, "%packet.addr = alloca { i32, { ptr, i64, i64 } }");
    assert_contains(output, "%unwrapped.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @unwrap_payload({ i32, { ptr, i64, i64 } } %tmp");
    assert_contains(output, "call i32 @consume_items({ ptr, i64, i64 } %tmp");
    assert_dynamic_array_payload_consumer_cleanup(output);
    assert_excludes(output, "%inner.values.dynamic_array_cleanup");
    assert_excludes(output, "%returned.inner.values.dynamic_array_cleanup");
    assert_excludes(output, "%unwrapped.dynamic_array_cleanup");
    assert_excludes(output, "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry");
    auto const make_outer_start = output.find("define %record.OuterBox @make_outer_box");
    auto const unwrap_start = output.find("define { ptr, i64, i64 } @unwrap_payload", make_outer_start);
    auto const consume_start = output.find("define i32 @consume_items", unwrap_start);
    auto const main_start = output.find("define i32 @main", consume_start);
    assert(make_outer_start != std::string::npos);
    assert(unwrap_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", make_outer_start) > unwrap_start);
    assert(output.find("__orison_dynamic_array_deallocate", unwrap_start) > consume_start);
    assert_no_main_dynamic_array_deallocate(output);
}

void assert_returned_dynamic_array_nested_aggregate_field_stored_choice_payload_branch_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert_contains(output, "%record.PayloadBox = type { { ptr, i64, i64 } }");
    assert_contains(output, "%record.OuterBox = type { %record.PayloadBox }");
    assert_contains(output, "define %record.OuterBox @make_outer_box()");
    assert_contains(output, "define { ptr, i64, i64 } @unwrap_payload");
    assert_contains(output, "define i32 @consume_items({ ptr, i64, i64 } %items)");
    assert_contains(output, "define i32 @choose_items(i1 %flag, { ptr, i64, i64 } %items)");
    auto const first_consume = output.find("call i32 @consume_items({ ptr, i64, i64 } %items)");
    assert(first_consume != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %items)", first_consume + 1) !=
        std::string::npos);
    assert_contains(output, "%returned.addr = alloca %record.OuterBox");
    assert_contains(output, "%returned.inner.values.addr");
    assert_contains(output, "%packet.addr = alloca { i32, { ptr, i64, i64 } }");
    assert_contains(output, "%unwrapped.addr = alloca { ptr, i64, i64 }");
    assert_contains(output, "call { ptr, i64, i64 } @unwrap_payload({ i32, { ptr, i64, i64 } } %tmp");
    assert_contains(output, "call i32 @choose_items(i1 0, { ptr, i64, i64 } %tmp");
    assert_dynamic_array_payload_consumer_cleanup(output);
    assert_excludes(output, "%inner.values.dynamic_array_cleanup");
    assert_excludes(output, "%returned.inner.values.dynamic_array_cleanup");
    assert_excludes(output, "%unwrapped.dynamic_array_cleanup");
    assert_excludes(output, "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry");
    auto const make_outer_start = output.find("define %record.OuterBox @make_outer_box");
    auto const unwrap_start = output.find("define { ptr, i64, i64 } @unwrap_payload", make_outer_start);
    auto const consume_start = output.find("define i32 @consume_items", unwrap_start);
    auto const choose_start = output.find("define i32 @choose_items", consume_start);
    auto const main_start = output.find("define i32 @main", choose_start);
    assert(make_outer_start != std::string::npos);
    assert(unwrap_start != std::string::npos);
    assert(consume_start != std::string::npos);
    assert(choose_start != std::string::npos);
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", make_outer_start) > unwrap_start);
    assert(output.find("__orison_dynamic_array_deallocate", unwrap_start) > consume_start);
    assert(output.find("__orison_dynamic_array_deallocate", choose_start) > main_start);
    assert_no_main_dynamic_array_deallocate(output);
}

void assert_emit_object_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::filesystem::path const& object_path
) {
    auto output = read_successful_command_output(
        executable.string() + " --emit-object " + source_path.string() + " -o " + object_path.string()
    );
    assert(output.empty());
    assert(std::filesystem::file_size(object_path) > 0);
}

void assert_build_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::filesystem::path const& output_path
) {
    auto output = read_successful_command_output(
        executable.string() + " --build " + source_path.string() + " -o " + output_path.string()
    );
    assert(output.empty());
    auto status = std::system(output_path.string().c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

}  // namespace

auto main(int argc, char** argv) -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_array_cli_run_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    if (argc > 2) {
        std::fprintf(stderr, "usage: orison_driver_array_cli_run_smoke [mode]\n");
        return 2;
    }

    auto selected_mode = std::string_view {"all"};
    if (argc == 2) {
        selected_mode = argv[1];
    }
    constexpr auto valid_modes = std::array<std::string_view, 6> {
        "all",
        "examples",
        "core",
        "returned_cleanup",
        "forwarding",
        "final_control",
    };
    auto valid_mode = false;
    for (auto mode : valid_modes) {
        valid_mode = valid_mode || selected_mode == mode;
    }
    if (!valid_mode) {
        std::fprintf(stderr, "unknown mode: %s\n", std::string(selected_mode).c_str());
        return 2;
    }
    auto runs_mode = [selected_mode](std::string_view mode) {
        return selected_mode == "all" || selected_mode == mode;
    };

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto examples = std::filesystem::path(ORISON_SOURCE_DIR) / "examples";
    auto fixtures = std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures";
    constexpr auto run_examples = std::array<std::string_view, 18> {
        "local_array_for.or",
        "local_dynamic_array_computed_for.or",
        "local_dynamic_array_nested_computed_for.or",
        "local_dynamic_array_owned_computed_for.or",
        "local_dynamic_array_owned_nested_computed_for.or",
        "dynamic_array_owned_parameter.or",
        "local_dynamic_array_owned_replacement.or",
        "local_ternary_array_for.or",
        "local_ternary_array_literal_for.or",
        "local_ternary_record_array_literal_for.or",
        "local_generic_record_array_literal_for.or",
        "local_record_array_for.or",
        "local_record_index_for.or",
        "local_record_index_field_for.or",
        "local_helper_array_for.or",
        "local_method_array_for.or",
        "local_member_receiver_method_array_for.or",
        "local_record_method_array_for.or",
    };

    if (runs_mode("examples")) {
        for (auto name : run_examples) {
            assert_run_success(executable, examples / name);
        }
    }

    auto generic_record_literal_path = examples / "local_generic_record_array_literal_for.or";
    auto computed_dynamic_array_path = examples / "local_dynamic_array_computed_for.or";
    auto nested_computed_dynamic_array_path = examples / "local_dynamic_array_nested_computed_for.or";
    auto owned_computed_dynamic_array_path = examples / "local_dynamic_array_owned_computed_for.or";
    auto owned_nested_computed_dynamic_array_path =
        examples / "local_dynamic_array_owned_nested_computed_for.or";
    auto forwarded_parameter_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_parameter_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_parameter_multi_hop_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_multi_hop_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_multi_hop_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_parameter_multi_hop_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_parameter_cycle_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_cycle_owned_computed_rejected.or";
    auto forwarded_parameter_depth8_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_depth8_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_depth9_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_depth9_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_local_alias_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_local_alias_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_let_alias_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_let_alias_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_if_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_switch_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_if_alias_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_alias_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_switch_alias_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_alias_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_if_switch_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_switch_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_if_switch_alias_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_switch_alias_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_switch_if_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_if_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_switch_if_alias_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_if_alias_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_if_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_parameter_final_switch_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_parameter_final_if_switch_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_switch_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_parameter_final_switch_if_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_if_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_parameter_local_alias_harmless_local_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_local_alias_harmless_local_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_local_alias_multi_harmless_locals_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_parameter_local_alias_multi_harmless_locals_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_local_alias_ternary_alternate_owner_read_path =
        fixtures /
        "dynamic_array_forwarded_parameter_local_alias_ternary_alternate_owner_read_rejected.or";
    auto forwarded_parameter_local_alias_reassigned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_local_alias_reassigned_owned_computed_rejected.or";
    auto forwarded_parameter_final_if_alias_harmless_local_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_alias_harmless_local_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_if_alias_ternary_alternate_owner_read_path =
        fixtures /
        "dynamic_array_forwarded_parameter_final_if_alias_ternary_alternate_owner_read_rejected.or";
    auto forwarded_parameter_final_switch_alias_harmless_local_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_alias_harmless_local_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed_for_cleanup_run.or";
    auto forwarded_parameter_final_switch_alias_ternary_alternate_owner_read_path =
        fixtures /
        "dynamic_array_forwarded_parameter_final_switch_alias_ternary_alternate_owner_read_rejected.or";
    auto forwarded_parameter_final_if_switch_alias_ternary_alternate_owner_read_path =
        fixtures /
        "dynamic_array_forwarded_parameter_final_if_switch_alias_ternary_alternate_owner_read_rejected.or";
    auto forwarded_parameter_final_switch_if_alias_ternary_alternate_owner_read_path =
        fixtures /
        "dynamic_array_forwarded_parameter_final_switch_if_alias_ternary_alternate_owner_read_rejected.or";
    auto forwarded_parameter_scalar_owner_read_paths = std::array<std::filesystem::path, 9> {
        fixtures / "dynamic_array_forwarded_parameter_local_alias_binary_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_local_alias_cast_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_local_alias_unary_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_final_if_alias_binary_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_final_if_alias_cast_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_final_if_alias_unary_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_final_switch_alias_binary_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_final_switch_alias_cast_owner_read_rejected.or",
        fixtures / "dynamic_array_forwarded_parameter_final_switch_alias_unary_owner_read_rejected.or",
    };
    auto forwarded_parameter_scalar_owner_read_stems = std::array<std::string_view, 9> {
        "dynamic_array_forwarded_parameter_local_alias_binary_owner_read",
        "dynamic_array_forwarded_parameter_local_alias_cast_owner_read",
        "dynamic_array_forwarded_parameter_local_alias_unary_owner_read",
        "dynamic_array_forwarded_parameter_final_if_alias_binary_owner_read",
        "dynamic_array_forwarded_parameter_final_if_alias_cast_owner_read",
        "dynamic_array_forwarded_parameter_final_if_alias_unary_owner_read",
        "dynamic_array_forwarded_parameter_final_switch_alias_binary_owner_read",
        "dynamic_array_forwarded_parameter_final_switch_alias_cast_owner_read",
        "dynamic_array_forwarded_parameter_final_switch_alias_unary_owner_read",
    };
    auto forwarded_parameter_final_if_alias_reassigned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_alias_reassigned_owned_computed_rejected.or";
    auto forwarded_parameter_final_switch_alias_reassigned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_alias_reassigned_owned_computed_rejected.or";
    auto forwarded_parameter_final_if_switch_alias_reassigned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_if_switch_alias_reassigned_owned_computed_rejected.or";
    auto forwarded_parameter_final_switch_if_alias_reassigned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_parameter_final_switch_if_alias_reassigned_owned_computed_rejected.or";
    auto static_indexed_aggregate_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_static_indexed_aggregate_owned_computed_for_cleanup_run.or";
    auto static_indexed_aggregate_owned_nested_computed_dynamic_array_path =
        fixtures / "dynamic_array_static_indexed_aggregate_owned_nested_computed_for_cleanup_run.or";
    auto nested_static_indexed_aggregate_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_nested_static_indexed_aggregate_owned_computed_for_cleanup_run.or";
    auto direct_dynamic_index_aggregate_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_direct_dynamic_index_aggregate_owned_computed_for_cleanup_run.or";
    auto nested_dynamic_index_aggregate_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_nested_dynamic_index_aggregate_owned_computed_for_cleanup_run.or";
    auto forwarded_static_indexed_aggregate_helper_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_static_indexed_aggregate_helper_owned_computed_for_cleanup_run.or";
    auto static_indexed_aggregate_distinct_owner_computed_dynamic_array_path =
        fixtures / "dynamic_array_static_indexed_aggregate_distinct_owner_computed_for_run.or";
    auto nested_static_indexed_aggregate_distinct_owner_computed_dynamic_array_path =
        fixtures / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_computed_for_run.or";
    auto runtime_indexed_distinct_root_computed_dynamic_array_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_computed_for_run.or";
    auto runtime_indexed_distinct_root_pure_computed_dynamic_array_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_pure_computed_for_run.or";
    auto runtime_indexed_distinct_root_pure_ternary_dynamic_array_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_pure_ternary_for_run.or";
    auto runtime_indexed_distinct_root_pure_member_dynamic_array_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_pure_member_for_run.or";
    auto runtime_index_source_dependent_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_for_run.or";
    auto runtime_index_source_dependent_member_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_member_for_run.or";
    auto runtime_index_source_dependent_member_then_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_member_then_out_of_bounds.or";
    auto runtime_index_source_dependent_member_else_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_member_else_out_of_bounds.or";
    auto runtime_index_boolean_path_condition_path =
        fixtures / "dynamic_array_runtime_index_boolean_path_condition_for_run.or";
    auto runtime_index_boolean_member_path_condition_path =
        fixtures / "dynamic_array_runtime_index_boolean_member_path_condition_for_run.or";
    auto runtime_index_boolean_not_member_path_condition_path =
        fixtures / "dynamic_array_runtime_index_boolean_not_member_path_condition_for_run.or";
    auto runtime_index_boolean_and_member_path_condition_path =
        fixtures / "dynamic_array_runtime_index_boolean_and_member_path_condition_for_run.or";
    auto runtime_index_boolean_or_member_path_condition_path =
        fixtures / "dynamic_array_runtime_index_boolean_or_member_path_condition_for_run.or";
    auto runtime_index_boolean_nested_member_path_condition_path =
        fixtures / "dynamic_array_runtime_index_boolean_nested_member_path_condition_for_run.or";
    auto runtime_index_boolean_nested_and_or_member_path_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_nested_and_or_member_path_condition_for_run.or";
    auto runtime_index_boolean_left_nested_and_or_member_path_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_left_nested_and_or_member_path_condition_for_run.or";
    auto runtime_index_boolean_left_nested_or_and_member_path_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_left_nested_or_and_member_path_condition_for_run.or";
    auto runtime_index_boolean_two_member_paths_and_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_two_member_paths_and_condition_for_run.or";
    auto runtime_index_boolean_two_member_paths_or_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_two_member_paths_or_condition_for_run.or";
    auto runtime_index_boolean_three_member_paths_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_member_paths_nested_condition_for_run.or";
    auto runtime_index_boolean_three_member_paths_symmetric_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_member_paths_symmetric_nested_condition_for_run.or";
    auto runtime_index_boolean_three_member_paths_left_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_member_paths_left_nested_condition_for_run.or";
    auto runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_for_run.or";
    auto runtime_index_boolean_four_member_paths_balanced_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_four_member_paths_balanced_condition_for_run.or";
    auto runtime_index_boolean_four_member_paths_symmetric_balanced_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_four_member_paths_symmetric_balanced_condition_for_run.or";
    auto runtime_index_boolean_dependent_member_path_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_for_run.or";
    auto runtime_index_boolean_and_dependent_member_path_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_and_dependent_member_path_condition_for_run.or";
    auto runtime_index_boolean_or_dependent_member_path_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_or_dependent_member_path_condition_for_run.or";
    auto runtime_index_boolean_two_dependent_member_paths_and_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_two_dependent_member_paths_and_condition_for_run.or";
    auto runtime_index_boolean_two_dependent_member_paths_or_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_two_dependent_member_paths_or_condition_for_run.or";
    auto runtime_index_boolean_three_dependent_member_paths_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_dependent_member_paths_nested_condition_for_run.or";
    auto runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_for_run.or";
    auto runtime_index_boolean_three_dependent_member_paths_left_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_dependent_member_paths_left_nested_condition_for_run.or";
    auto runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_for_run.or";
    auto runtime_index_boolean_four_dependent_member_paths_balanced_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_four_dependent_member_paths_balanced_condition_for_run.or";
    auto runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_for_run.or";
    auto runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds.or";
    auto runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds.or";
    auto runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds.or";
    auto runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds.or";
    auto runtime_index_boolean_member_path_condition_then_out_of_bounds_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_member_path_condition_then_out_of_bounds.or";
    auto runtime_index_boolean_member_path_condition_else_out_of_bounds_path =
        fixtures /
        "dynamic_array_runtime_index_boolean_member_path_condition_else_out_of_bounds.or";
    auto runtime_index_boolean_path_condition_then_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_boolean_path_condition_then_out_of_bounds.or";
    auto runtime_index_boolean_path_condition_else_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_boolean_path_condition_else_out_of_bounds.or";
    auto runtime_index_source_dependent_then_slot_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_then_slot_out_of_bounds.or";
    auto runtime_index_source_dependent_else_slot_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_else_slot_out_of_bounds.or";
    auto runtime_index_source_dependent_then_row_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_then_row_out_of_bounds.or";
    auto runtime_index_source_dependent_else_row_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_dependent_else_row_out_of_bounds.or";
    auto runtime_index_source_then_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_then_out_of_bounds.or";
    auto runtime_index_source_else_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_index_source_else_out_of_bounds.or";
    auto runtime_indexed_distinct_root_call_index_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_call_index_rejected.or";
    auto runtime_indexed_distinct_root_record_constructor_index_path =
        fixtures /
        "dynamic_array_runtime_indexed_distinct_root_record_constructor_index_for_run.or";
    auto runtime_indexed_distinct_root_nested_record_constructor_index_path =
        fixtures /
        "dynamic_array_runtime_indexed_distinct_root_nested_record_constructor_index_for_run.or";
    auto runtime_indexed_distinct_root_generic_record_constructor_index_path =
        fixtures /
        "dynamic_array_runtime_indexed_distinct_root_generic_record_constructor_index_for_run.or";
    auto nested_runtime_indexed_distinct_root_computed_dynamic_array_path =
        fixtures / "dynamic_array_nested_runtime_indexed_distinct_root_computed_for_run.or";
    auto nested_runtime_indexed_distinct_root_pure_computed_dynamic_array_path =
        fixtures / "dynamic_array_nested_runtime_indexed_distinct_root_pure_computed_for_run.or";
    auto nested_runtime_indexed_distinct_root_computed_mismatch_path =
        fixtures / "dynamic_array_nested_runtime_indexed_distinct_root_computed_mismatch_rejected.or";
    auto runtime_indexed_distinct_root_mismatched_index_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_mismatched_index_rejected.or";
    auto runtime_indexed_distinct_root_left_reuse_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_left_reuse_rejected.or";
    auto runtime_indexed_distinct_root_right_reuse_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_right_reuse_rejected.or";
    auto nested_runtime_indexed_distinct_root_left_reuse_path =
        fixtures / "dynamic_array_nested_runtime_indexed_distinct_root_left_reuse_rejected.or";
    auto nested_runtime_indexed_distinct_root_right_reuse_path =
        fixtures / "dynamic_array_nested_runtime_indexed_distinct_root_right_reuse_rejected.or";
    auto runtime_indexed_distinct_root_then_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_then_out_of_bounds.or";
    auto runtime_indexed_distinct_root_else_out_of_bounds_path =
        fixtures / "dynamic_array_runtime_indexed_distinct_root_else_out_of_bounds.or";
    auto nested_runtime_indexed_distinct_root_then_out_of_bounds_path =
        fixtures / "dynamic_array_nested_runtime_indexed_distinct_root_then_out_of_bounds.or";
    auto nested_runtime_indexed_distinct_root_else_out_of_bounds_path =
        fixtures / "dynamic_array_nested_runtime_indexed_distinct_root_else_out_of_bounds.or";
    auto static_indexed_aggregate_distinct_owner_left_reuse_path =
        fixtures / "dynamic_array_static_indexed_aggregate_distinct_owner_left_reuse_rejected.or";
    auto static_indexed_aggregate_distinct_owner_right_reuse_path =
        fixtures / "dynamic_array_static_indexed_aggregate_distinct_owner_right_reuse_rejected.or";
    auto nested_static_indexed_aggregate_distinct_owner_left_reuse_path =
        fixtures / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_left_reuse_rejected.or";
    auto nested_static_indexed_aggregate_distinct_owner_right_reuse_path =
        fixtures / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_right_reuse_rejected.or";
    auto nested_dynamic_index_aggregate_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_nested_dynamic_index_aggregate_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_static_indexed_aggregate_helper_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_static_indexed_aggregate_helper_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed_for_cleanup_run.or";
    auto forwarded_nested_dynamic_index_aggregate_helper_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_nested_dynamic_index_aggregate_helper_owned_computed_for_cleanup_run.or";
    auto forwarded_nested_dynamic_index_aggregate_helper_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_nested_dynamic_index_aggregate_helper_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed_for_cleanup_run.or";
    auto owned_dynamic_array_parameter_path = examples / "dynamic_array_owned_parameter.or";
    auto owned_dynamic_array_parameter_cleanup_path =
        fixtures / "dynamic_array_owned_parameter_cleanup_run.or";
    auto owned_dynamic_array_parameter_forwarding_path =
        fixtures / "dynamic_array_owned_parameter_forwarding_run.or";
    auto returned_dynamic_array_parameter_forwarding_path =
        fixtures / "dynamic_array_returned_parameter_forwarding_run.or";
    auto returned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_owned_computed_for_cleanup_run.or";
    auto returned_alias_chain_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_alias_chain_owned_computed_for_cleanup_run.or";
    auto branch_returned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_returned_owned_computed_for_cleanup_run.or";
    auto switch_returned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_switch_returned_owned_computed_for_cleanup_run.or";
    auto returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto switch_returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_switch_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto switch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_switch_forwarded_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto branch_returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto branch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_forwarded_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto branch_mixed_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto forwarded_returned_aggregate_field_helper_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_returned_aggregate_field_helper_owned_computed_for_cleanup_run.or";
    auto forwarded_projected_argument_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_projected_argument_owned_computed_for_cleanup_run.or";
    auto forwarded_returned_aggregate_field_helper_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_returned_aggregate_field_helper_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_returned_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_returned_aggregate_field_helper_extra_statement_owned_computed_for_cleanup_run.or";
    auto returned_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto switch_returned_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_returned_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto returned_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto switch_returned_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_returned_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto returned_nested_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto switch_returned_nested_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_switch_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto switch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto forwarded_returned_nested_aggregate_field_helper_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_owned_computed_for_cleanup_run.or";
    auto forwarded_returned_nested_aggregate_field_helper_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_owned_computed_owner_mismatch_rejected.or";
    auto forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed_for_cleanup_run.or";
    auto returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto switch_returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto choice_payload_switch_binding_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_run.or";
    auto choice_payload_final_switch_binding_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_switch_binding_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_choice_payload_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_harmless_local_switch_binding_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_choice_payload_harmless_local_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_final_if_switch_binding_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_forwarded_choice_payload_final_if_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_final_switch_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_final_switch_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_for_cleanup_run.or";
    auto forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path =
        fixtures /
        "dynamic_array_forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed_for_cleanup_run.or";
    auto returned_choice_payload_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_choice_payload_owned_computed_for_cleanup_run.or";
    auto returned_dynamic_array_multi_hop_forwarding_path =
        fixtures / "dynamic_array_returned_multi_hop_forwarding_run.or";
    auto returned_dynamic_array_branch_join_forwarding_path =
        fixtures / "dynamic_array_returned_branch_join_forwarding_run.or";
    auto returned_dynamic_array_choice_branch_forwarding_path =
        fixtures / "dynamic_array_returned_choice_branch_forwarding_run.or";
    auto returned_dynamic_array_aggregate_field_forwarding_path =
        fixtures / "dynamic_array_returned_aggregate_field_forwarding_run.or";
    auto returned_dynamic_array_nested_aggregate_field_forwarding_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_forwarding_run.or";
    auto returned_dynamic_array_nested_aggregate_field_branch_forwarding_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_branch_forwarding_run.or";
    auto returned_dynamic_array_aggregate_field_choice_payload_forwarding_path =
        fixtures / "dynamic_array_returned_aggregate_field_choice_payload_forwarding_run.or";
    auto returned_dynamic_array_aggregate_field_choice_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_choice_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_choice_record_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_choice_record_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_choice_fixed_array_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_choice_fixed_array_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_fixed_array_choice_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_fixed_array_choice_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_nested_fixed_array_choice_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_nested_fixed_array_choice_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_maybe_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_maybe_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_fixed_array_maybe_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_fixed_array_maybe_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_maybe_record_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_maybe_record_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_run.or";
    auto returned_dynamic_array_aggregate_field_stored_choice_payload_forwarding_path =
        fixtures / "dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_run.or";
    auto returned_dynamic_array_aggregate_field_stored_choice_payload_branch_forwarding_path =
        fixtures / "dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_run.or";
    auto returned_dynamic_array_nested_aggregate_field_stored_choice_payload_forwarding_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_run.or";
    auto returned_dynamic_array_nested_aggregate_field_stored_choice_payload_branch_forwarding_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_run.or";
    auto returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_run.or";
    auto returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_run.or";
    auto dynamic_array_local_final_if_branch_cleanup_path =
        fixtures / "dynamic_array_local_final_if_branch_cleanup_run.or";
    auto dynamic_array_local_final_switch_case_cleanup_path =
        fixtures / "dynamic_array_local_final_switch_case_cleanup_run.or";
    auto dynamic_array_local_final_if_consumed_owner_cleanup_path =
        fixtures / "dynamic_array_local_final_if_consumed_owner_cleanup_run.or";
    auto dynamic_array_local_final_switch_consumed_owner_cleanup_path =
        fixtures / "dynamic_array_local_final_switch_consumed_owner_cleanup_run.or";
    auto dynamic_array_owned_result_final_if_branch_cleanup_path =
        fixtures / "dynamic_array_owned_result_final_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_final_switch_case_cleanup_path =
        fixtures / "dynamic_array_owned_result_final_switch_case_cleanup_run.or";
    auto dynamic_array_owned_result_returned_local_final_if_branch_cleanup_path =
        fixtures / "dynamic_array_owned_result_returned_local_final_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_returned_local_final_switch_case_cleanup_path =
        fixtures / "dynamic_array_owned_result_returned_local_final_switch_case_cleanup_run.or";
    auto dynamic_array_owned_result_nested_final_if_branch_cleanup_path =
        fixtures / "dynamic_array_owned_result_nested_final_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_nested_final_switch_case_cleanup_path =
        fixtures / "dynamic_array_owned_result_nested_final_switch_case_cleanup_run.or";
    auto dynamic_array_owned_result_if_switch_branch_cleanup_path =
        fixtures / "dynamic_array_owned_result_if_switch_branch_cleanup_run.or";
    auto dynamic_array_owned_result_switch_if_branch_cleanup_path =
        fixtures / "dynamic_array_owned_result_switch_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_direct_nested_if_branch_cleanup_path =
        fixtures / "dynamic_array_owned_result_direct_nested_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_direct_nested_switch_branch_cleanup_path =
        fixtures / "dynamic_array_owned_result_direct_nested_switch_branch_cleanup_run.or";
    auto dynamic_array_owned_result_three_case_switch_cleanup_path =
        fixtures / "dynamic_array_owned_result_three_case_switch_cleanup_run.or";
    auto dynamic_array_owned_result_three_case_mixed_switch_cleanup_path =
        fixtures / "dynamic_array_owned_result_three_case_mixed_switch_cleanup_run.or";
    auto dynamic_array_owned_result_three_case_nested_switch_cleanup_path =
        fixtures / "dynamic_array_owned_result_three_case_nested_switch_cleanup_run.or";
    auto dynamic_array_owned_result_multi_nested_switch_cleanup_path =
        fixtures / "dynamic_array_owned_result_multi_nested_switch_cleanup_run.or";
    auto dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_if_two_switches_cleanup_path =
        fixtures / "dynamic_array_owned_result_if_two_switches_cleanup_run.or";
    auto dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_path =
        fixtures / "dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_run.or";
    auto dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_path =
        fixtures / "dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_rejected.or";
    auto dynamic_array_owned_result_if_two_switches_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_if_two_switches_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_switch_two_ifs_cleanup_path =
        fixtures / "dynamic_array_owned_result_switch_two_ifs_cleanup_run.or";
    auto dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_named_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_named_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_run.or";
    auto dynamic_array_returned_helper_call_owned_computed_for_cleanup_path =
        fixtures / "dynamic_array_returned_helper_call_owned_computed_for_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_chained_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_scratch_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_alias_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_alias_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_local_return_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_stored_helper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_three_local_helper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_distinct_local_names_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_mixed_direct_distinct_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_helper_argument_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_nested_helper_argument_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_argument_local_chain_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_argument_local_chain_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_selected_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_helper_argument_selected_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_mixed_direct_distinct_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_distinct_local_names_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_three_local_helper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_stored_helper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_local_return_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_alias_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_alias_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_alias_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_scratch_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_scratch_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_chained_branch_consumer_selected_reuse_path =
        fixtures /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_chained_selected_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_local_return_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_local_return_branch_consumer_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_named_helper_call_reuse_path =
        fixtures / "dynamic_array_owned_result_ternary_named_helper_call_reuse_rejected.or";
    auto dynamic_array_owned_result_multi_nested_switch_returned_reuse_path =
        fixtures / "dynamic_array_owned_result_multi_nested_switch_returned_reuse_rejected.or";
    auto dynamic_array_owned_result_helper_call_returned_reuse_path =
        fixtures / "dynamic_array_owned_result_helper_call_returned_reuse_rejected.or";
    auto dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_path =
        fixtures / "dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_run.or";
    auto dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_path =
        fixtures / "dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_rejected.or";
    auto owned_dynamic_array_parameter_forwarding_reuse_path =
        fixtures / "dynamic_array_owned_parameter_forwarding_reuse_rejected.or";
    auto owned_dynamic_array_parameter_branch_join_path =
        fixtures / "dynamic_array_owned_parameter_branch_join_run.or";
    auto owned_dynamic_array_parameter_branch_cleanup_path =
        fixtures / "dynamic_array_owned_parameter_branch_cleanup_run.or";
    auto owned_dynamic_array_parameter_branch_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_branch_index_assignment_run.or";
    auto owned_dynamic_array_parameter_branch_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_branch_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_switch_cleanup_path =
        fixtures / "dynamic_array_owned_parameter_switch_cleanup_run.or";
    auto owned_dynamic_array_parameter_switch_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_switch_index_assignment_run.or";
    auto owned_dynamic_array_parameter_loop_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_loop_index_assignment_run.or";
    auto owned_dynamic_array_parameter_repeat_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_repeat_index_assignment_run.or";
    auto owned_dynamic_array_parameter_loop_break_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_loop_break_index_assignment_run.or";
    auto owned_dynamic_array_parameter_loop_continue_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_loop_continue_index_assignment_run.or";
    auto owned_dynamic_array_parameter_repeat_break_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_repeat_break_index_assignment_run.or";
    auto owned_dynamic_array_parameter_repeat_continue_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_repeat_continue_index_assignment_run.or";
    auto owned_dynamic_array_aggregate_parameter_field_index_assignment_path =
        fixtures / "dynamic_array_owned_aggregate_parameter_field_index_assignment_run.or";
    auto owned_dynamic_array_nested_aggregate_parameter_field_index_assignment_path =
        fixtures / "dynamic_array_owned_nested_aggregate_parameter_field_index_assignment_run.or";
    auto owned_dynamic_array_indexed_aggregate_parameter_field_index_assignment_path =
        fixtures / "dynamic_array_owned_indexed_aggregate_parameter_field_index_assignment_run.or";
    auto owned_dynamic_array_runtime_indexed_aggregate_parameter_field_index_assignment_path =
        fixtures / "dynamic_array_owned_runtime_indexed_aggregate_parameter_field_index_assignment_run.or";
    auto owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_path =
        fixtures / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_run.or";
    auto owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_out_of_bounds_path =
        fixtures / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_out_of_bounds.or";
    auto owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_item_out_of_bounds_path =
        fixtures / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_item_out_of_bounds.or";
    auto owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_out_of_bounds_path =
        fixtures / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_out_of_bounds.or";
    auto owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_path =
        fixtures / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_run.or";
    auto owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_reuse_path =
        fixtures /
        "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_reuse_rejected.or";
    auto owned_dynamic_array_parameter_nested_loop_break_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_nested_loop_break_index_assignment_run.or";
    auto owned_dynamic_array_parameter_nested_loop_continue_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_nested_loop_continue_index_assignment_run.or";
    auto owned_dynamic_array_parameter_nested_repeat_break_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_nested_repeat_break_index_assignment_run.or";
    auto owned_dynamic_array_parameter_nested_repeat_continue_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_nested_repeat_continue_index_assignment_run.or";
    auto owned_dynamic_array_parameter_loop_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_loop_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_loop_break_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_loop_break_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_loop_continue_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_loop_continue_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_repeat_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_repeat_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_repeat_break_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_repeat_break_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_repeat_continue_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_repeat_continue_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_aggregate_parameter_field_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_aggregate_parameter_field_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_nested_aggregate_parameter_field_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_nested_aggregate_parameter_field_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_indexed_aggregate_parameter_field_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_indexed_aggregate_parameter_field_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_runtime_indexed_aggregate_parameter_field_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_runtime_indexed_aggregate_parameter_field_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_cleanup_reuse_rejected.or";
    auto returned_nested_runtime_indexed_aggregate_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_nested_runtime_indexed_aggregate_owned_computed_for_cleanup_run.or";
    auto owned_dynamic_array_parameter_nested_loop_break_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_nested_loop_break_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_nested_loop_continue_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_nested_loop_continue_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_nested_repeat_break_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_nested_repeat_break_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_nested_repeat_continue_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_nested_repeat_continue_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_switch_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_switch_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_statement_branch_mismatch_path =
        fixtures / "dynamic_array_owned_parameter_statement_branch_mismatch_rejected.or";
    auto owned_dynamic_array_parameter_second_use_path =
        fixtures / "dynamic_array_owned_parameter_second_use_rejected.or";
    auto owned_dynamic_array_parameter_length_after_move_path =
        fixtures / "dynamic_array_owned_parameter_length_after_move_rejected.or";
    auto owned_dynamic_array_parameter_push_after_move_path =
        fixtures / "dynamic_array_owned_parameter_push_after_move_rejected.or";
    auto owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_owned_computed_cleanup_missing_drop.or";
    auto owned_nested_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_owned_nested_computed_cleanup_missing_drop.or";
    auto returned_owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_returned_owned_computed_cleanup_missing_drop.or";
    auto returned_aggregate_field_owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_returned_aggregate_field_owned_computed_cleanup_missing_drop.or";
    auto returned_nested_aggregate_field_owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_owned_computed_cleanup_missing_drop.or";
    auto returned_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto switch_returned_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_switch_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto switch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_switch_forwarded_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto branch_returned_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_branch_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto branch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_branch_forwarded_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto branch_mixed_forwarded_returned_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto switch_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_switch_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto switch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_branch_forwarded_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto returned_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto switch_returned_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_returned_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto switch_forwarded_returned_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_forwarded_returned_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto returned_nested_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto switch_returned_nested_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_returned_nested_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto forwarded_returned_nested_aggregate_field_final_if_owner_reuse_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_if_owner_reuse_rejected.or";
    auto returned_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto switch_returned_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_returned_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto switch_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto returned_nested_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto switch_returned_nested_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_returned_nested_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto forwarded_returned_nested_aggregate_field_final_switch_owner_reuse_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_switch_owner_reuse_rejected.or";
    auto choice_payload_switch_binding_owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop.or";
    auto choice_payload_final_switch_binding_owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop.or";
    auto choice_payload_final_switch_binding_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_rejected.or";
    auto returned_choice_payload_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_returned_choice_payload_owned_computed_reuse_rejected.or";
    auto forwarded_choice_payload_switch_binding_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_forwarded_choice_payload_switch_binding_owned_computed_reuse_rejected.or";
    auto forwarded_choice_payload_final_if_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_choice_payload_final_if_owner_mismatch_rejected.or";
    auto forwarded_choice_payload_final_switch_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_choice_payload_final_switch_owner_mismatch_rejected.or";
    auto forwarded_choice_payload_final_if_branch_local_alias_extra_path =
        fixtures / "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_extra_rejected.or";
    auto forwarded_choice_payload_final_switch_branch_local_alias_extra_path =
        fixtures / "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_extra_rejected.or";
    auto forwarded_choice_payload_final_if_branch_local_alias_reassigned_path =
        fixtures / "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_reassigned_rejected.or";
    auto forwarded_choice_payload_final_switch_branch_local_alias_reassigned_path =
        fixtures / "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_reassigned_rejected.or";
    auto forwarded_choice_payload_nested_final_if_switch_branch_local_alias_owner_mismatch_path =
        fixtures / "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_owner_mismatch_rejected.or";
    auto branch_returned_owned_computed_dynamic_array_distinct_owner_path =
        fixtures / "dynamic_array_branch_returned_owned_computed_distinct_owner_run.or";
    auto switch_returned_owned_computed_dynamic_array_distinct_owner_path =
        fixtures / "dynamic_array_switch_returned_owned_computed_distinct_owner_run.or";
    auto owned_dynamic_array_parameter_missing_drop_path =
        fixtures / "dynamic_array_owned_parameter_iteration_missing_drop.or";
    auto dynamic_array_parameter_index_assignment_path =
        fixtures / "dynamic_array_parameter_index_assignment_run.or";
    auto owned_dynamic_array_parameter_index_assignment_path =
        fixtures / "dynamic_array_owned_parameter_index_assignment_run.or";
    auto dynamic_array_parameter_push_path =
        fixtures / "dynamic_array_parameter_push_run.or";
    auto owned_dynamic_array_replacement_path = examples / "local_dynamic_array_owned_replacement.or";
    auto dynamic_array_complete_contract_path =
        fixtures / "dynamic_array_complete_contract.or";
    auto dynamic_array_push_owned_payload_reuse_path =
        fixtures / "dynamic_array_push_owned_payload_reuse_rejected.or";
    auto dynamic_array_push_owned_field_reuse_path =
        fixtures / "dynamic_array_push_owned_field_reuse_rejected.or";
    auto dynamic_array_owned_element_assignment_rhs_reuse_path =
        fixtures / "dynamic_array_owned_element_assignment_rhs_reuse_rejected.or";
    if (runs_mode("core")) {
    assert_emit_llvm_success(executable, generic_record_literal_path);
    assert_emit_object_success(
        executable,
        generic_record_literal_path,
        smoke_temp_root / "local_generic_record_array_literal_for.o"
    );
    assert_build_success(
        executable,
        generic_record_literal_path,
        smoke_temp_root / "local_generic_record_array_literal_for"
    );
    assert_computed_dynamic_array_emit_llvm_success(executable, computed_dynamic_array_path);
    assert_emit_object_success(
        executable,
        computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_computed_for.o"
    );
    assert_build_success(
        executable,
        computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_computed_for"
    );
    assert_computed_dynamic_array_emit_llvm_success(executable, nested_computed_dynamic_array_path);
    assert_emit_object_success(
        executable,
        nested_computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_nested_computed_for.o"
    );
    assert_build_success(
        executable,
        nested_computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_nested_computed_for"
    );
    assert_owned_computed_dynamic_array_emit_llvm_success(executable, owned_computed_dynamic_array_path);
    assert_emit_object_success(
        executable,
        owned_computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_owned_computed_for.o"
    );
    assert_build_success(
        executable,
        owned_computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_owned_computed_for"
    );
    assert_owned_nested_computed_dynamic_array_emit_llvm_success(
        executable,
        owned_nested_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        owned_nested_computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_owned_nested_computed_for.o"
    );
    assert_build_success(
        executable,
        owned_nested_computed_dynamic_array_path,
        smoke_temp_root / "local_dynamic_array_owned_nested_computed_for"
    );
    assert_run_success(executable, dynamic_array_complete_contract_path);
    assert_dynamic_array_complete_contract_emit_llvm_success(
        executable,
        dynamic_array_complete_contract_path
    );
    assert_build_success(
        executable,
        dynamic_array_complete_contract_path,
        smoke_temp_root / "dynamic_array_complete_contract"
    );
    assert_run_success(executable, forwarded_parameter_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_multi_hop_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_multi_hop_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_multi_hop_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_multi_hop_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_multi_hop_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_multi_hop_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_depth8_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_depth8_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_depth8_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_depth8_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_depth8_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_depth8_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_depth9_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_depth9_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_depth9_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_depth9_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_depth9_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_depth9_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_local_alias_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_local_alias_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_local_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_local_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_let_alias_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_let_alias_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_let_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_let_alias_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_let_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_let_alias_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_if_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_if_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_if_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_if_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_switch_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_switch_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_switch_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_switch_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_if_alias_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_if_alias_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_if_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_if_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_switch_alias_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_switch_alias_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_switch_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_switch_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_if_switch_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_if_switch_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_if_switch_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_if_switch_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_if_switch_alias_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_if_switch_alias_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_if_switch_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_alias_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_if_switch_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_alias_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_switch_if_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_switch_if_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_switch_if_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_switch_if_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_parameter_final_switch_if_alias_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_switch_if_alias_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_switch_if_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_alias_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_switch_if_alias_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_alias_owned_computed_for_cleanup"
    );
    assert_run_success(executable, static_indexed_aggregate_owned_computed_dynamic_array_path);
    assert_static_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        static_indexed_aggregate_owned_computed_dynamic_array_path,
        "holder.buckets.element0.values",
        "%record.Holder = type { [2 x %record.Bucket] }"
    );
    assert_emit_object_success(
        executable,
        static_indexed_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        static_indexed_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_owned_computed_for_cleanup"
    );
    assert_run_success(executable, static_indexed_aggregate_owned_nested_computed_dynamic_array_path);
    assert_static_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        static_indexed_aggregate_owned_nested_computed_dynamic_array_path,
        "holder.buckets.element0.values",
        "%record.Holder = type { [2 x %record.Bucket] }"
    );
    assert_emit_object_success(
        executable,
        static_indexed_aggregate_owned_nested_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_owned_nested_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        static_indexed_aggregate_owned_nested_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_owned_nested_computed_for_cleanup"
    );
    assert_run_success(executable, nested_static_indexed_aggregate_owned_computed_dynamic_array_path);
    assert_static_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        nested_static_indexed_aggregate_owned_computed_dynamic_array_path,
        "holder.grid.element1.element0.values",
        "%record.Holder = type { [2 x [2 x %record.Bucket]] }"
    );
    assert_emit_object_success(
        executable,
        nested_static_indexed_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        nested_static_indexed_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_owned_computed_for_cleanup"
    );
    assert_run_success(executable, direct_dynamic_index_aggregate_owned_computed_dynamic_array_path);
    assert_runtime_index_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        direct_dynamic_index_aggregate_owned_computed_dynamic_array_path,
        "holder.buckets[selected].values",
        "holder.buckets.selected..values",
        "%record.Holder = type { [2 x %record.Bucket] }",
        {"getelementptr [2 x %record.Bucket], ptr %tmp6, i64 0, i64 %tmp7"},
        "store { ptr, i64, i64 } zeroinitializer, ptr %tmp10",
        "holder.buckets.element0.values.dynamic_array_cleanup"
    );
    assert_emit_object_success(
        executable,
        direct_dynamic_index_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_direct_dynamic_index_aggregate_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        direct_dynamic_index_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_direct_dynamic_index_aggregate_owned_computed_for_cleanup"
    );
    assert_run_success(executable, nested_dynamic_index_aggregate_owned_computed_dynamic_array_path);
    assert_runtime_index_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        nested_dynamic_index_aggregate_owned_computed_dynamic_array_path,
        "holder.grid[row][column].values",
        "holder.grid.row..column..values",
        "%record.Holder = type { [2 x [2 x %record.Bucket]] }",
        {
            "getelementptr [2 x [2 x %record.Bucket]], ptr %tmp12, i64 0, i64 %tmp13",
            "getelementptr [2 x %record.Bucket], ptr %tmp15, i64 0, i64 %tmp16",
        },
        "store { ptr, i64, i64 } zeroinitializer, ptr %tmp19",
        "holder.grid.element0.element0.values.dynamic_array_cleanup"
    );
    assert_emit_object_success(
        executable,
        nested_dynamic_index_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_dynamic_index_aggregate_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        nested_dynamic_index_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_dynamic_index_aggregate_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_static_indexed_aggregate_helper_owned_computed_dynamic_array_path);
    assert_static_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_static_indexed_aggregate_helper_owned_computed_dynamic_array_path,
        "holder.buckets.element0.values",
        "%record.Holder = type { [2 x %record.Bucket] }"
    );
    assert_emit_object_success(
        executable,
        forwarded_static_indexed_aggregate_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_static_indexed_aggregate_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed_dynamic_array_path);
    assert_runtime_index_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed_dynamic_array_path,
        "holder.buckets[selected].values",
        "holder.buckets.selected..values",
        "%record.Holder = type { [2 x %record.Bucket] }",
        {"getelementptr [2 x %record.Bucket], ptr %tmp6, i64 0, i64 %tmp7"},
        "store { ptr, i64, i64 } zeroinitializer, ptr %tmp10",
        "holder.buckets.element0.values.dynamic_array_cleanup"
    );
    assert_emit_object_success(
        executable,
        forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_dynamic_index_owned_computed"
    );
    assert_run_success(executable, forwarded_nested_dynamic_index_aggregate_helper_owned_computed_dynamic_array_path);
    assert_runtime_index_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_nested_dynamic_index_aggregate_helper_owned_computed_dynamic_array_path,
        "holder.grid[row][column].values",
        "holder.grid.row..column..values",
        "%record.Holder = type { [2 x [2 x %record.Bucket]] }",
        {
            "getelementptr [2 x [2 x %record.Bucket]], ptr %tmp12, i64 0, i64 %tmp13",
            "getelementptr [2 x %record.Bucket], ptr %tmp15, i64 0, i64 %tmp16",
        },
        "store { ptr, i64, i64 } zeroinitializer, ptr %tmp19",
        "holder.grid.element0.element0.values.dynamic_array_cleanup"
    );
    assert_emit_object_success(
        executable,
        forwarded_nested_dynamic_index_aggregate_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_nested_dynamic_index_aggregate_helper_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_nested_dynamic_index_aggregate_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_nested_dynamic_index_aggregate_helper_owned_computed"
    );
    assert_computed_dynamic_array_owner_mismatch_failure_matrix(
        executable,
        forwarded_parameter_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_owned_computed_owner_mismatch",
        "left",
        "right"
    );
    assert_computed_dynamic_array_owner_mismatch_failure_matrix(
        executable,
        forwarded_parameter_multi_hop_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_multi_hop_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_multi_hop_owned_computed_owner_mismatch",
        "left",
        "right"
    );
    assert_computed_dynamic_array_unsupported_shape_failure_matrix(
        executable,
        forwarded_parameter_cycle_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_cycle_owned_computed.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_cycle_owned_computed"
    );
    assert_run_success(executable, returned_nested_runtime_indexed_aggregate_owned_computed_dynamic_array_path);
    assert_returned_runtime_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_nested_runtime_indexed_aggregate_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        returned_nested_runtime_indexed_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_nested_runtime_indexed_aggregate_owned_computed.o"
    );
    assert_build_success(
        executable,
        returned_nested_runtime_indexed_aggregate_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_nested_runtime_indexed_aggregate_owned_computed"
    );
    assert_run_success(executable, forwarded_parameter_local_alias_harmless_local_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_local_alias_harmless_local_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_local_alias_harmless_local_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_harmless_local_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_local_alias_harmless_local_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_harmless_local_owned_computed"
    );
    assert_run_success(executable, forwarded_parameter_local_alias_multi_harmless_locals_owned_computed_dynamic_array_path);
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_local_alias_multi_harmless_locals_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_local_alias_multi_harmless_locals_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_multi_harmless_locals_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_local_alias_multi_harmless_locals_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_multi_harmless_locals_owned_computed"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_local_alias_reassigned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_reassigned_owned_computed.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_reassigned_owned_computed",
        "use after move: items"
    );
    assert_run_success(
        executable,
        forwarded_parameter_final_if_alias_harmless_local_owned_computed_dynamic_array_path
    );
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_if_alias_harmless_local_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_if_alias_harmless_local_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_harmless_local_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_if_alias_harmless_local_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_harmless_local_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed_dynamic_array_path
    );
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_multi_harmless_locals_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_parameter_final_switch_alias_harmless_local_owned_computed_dynamic_array_path
    );
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_switch_alias_harmless_local_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_switch_alias_harmless_local_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_harmless_local_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_switch_alias_harmless_local_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_harmless_local_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed_dynamic_array_path
    );
    assert_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_multi_harmless_locals_owned_computed"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_local_alias_ternary_alternate_owner_read_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_ternary_alternate_owner_read.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_local_alias_ternary_alternate_owner_read",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_if_alias_ternary_alternate_owner_read_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_ternary_alternate_owner_read.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_ternary_alternate_owner_read",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_switch_alias_ternary_alternate_owner_read_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_ternary_alternate_owner_read.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_ternary_alternate_owner_read",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_if_switch_alias_ternary_alternate_owner_read_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_alias_ternary_alternate_owner_read.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_alias_ternary_alternate_owner_read",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_switch_if_alias_ternary_alternate_owner_read_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_alias_ternary_alternate_owner_read.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_alias_ternary_alternate_owner_read",
        "use after move: items"
    );
    for (auto index = std::size_t {0}; index < forwarded_parameter_scalar_owner_read_paths.size(); ++index) {
        auto stem = std::string {forwarded_parameter_scalar_owner_read_stems[index]};
        assert_diagnostic_failure_matrix(
            executable,
            forwarded_parameter_scalar_owner_read_paths[index],
            smoke_temp_root / (stem + ".o"),
            smoke_temp_root / stem,
            "use after move: items"
        );
    }
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_if_alias_reassigned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_reassigned_owned_computed.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_alias_reassigned_owned_computed",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_switch_alias_reassigned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_reassigned_owned_computed.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_alias_reassigned_owned_computed",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_if_switch_alias_reassigned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_alias_reassigned_owned_computed.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_alias_reassigned_owned_computed",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_switch_if_alias_reassigned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_alias_reassigned_owned_computed.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_alias_reassigned_owned_computed",
        "use after move: items"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_if_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_owned_computed_owner_mismatch",
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_switch_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_owned_computed_owner_mismatch",
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_if_switch_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_if_switch_owned_computed_owner_mismatch",
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_parameter_final_switch_if_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_parameter_final_switch_if_owned_computed_owner_mismatch",
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );
    assert_run_success(executable, static_indexed_aggregate_distinct_owner_computed_dynamic_array_path);
    auto static_indexed_distinct_owner_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        static_indexed_aggregate_distinct_owner_computed_dynamic_array_path.string()
    );
    assert_contains(static_indexed_distinct_owner_ir, "%holder.buckets.element0.values.addr");
    assert_contains(static_indexed_distinct_owner_ir, "%holder.buckets.element1.values.addr");
    assert_contains(
        static_indexed_distinct_owner_ir,
        "call void @__orison_owned_cleanup.Payload(ptr "
        "%computed_dynamic_array_selection.0.else.unselected.cleanup.drop.element.addr)"
    );
    assert_contains(
        static_indexed_distinct_owner_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        static_indexed_aggregate_distinct_owner_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_distinct_owner_computed_for_run.o"
    );
    assert_build_success(
        executable,
        static_indexed_aggregate_distinct_owner_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_distinct_owner_computed_for_run"
    );
    assert_run_success(executable, nested_static_indexed_aggregate_distinct_owner_computed_dynamic_array_path);
    auto nested_static_indexed_distinct_owner_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        nested_static_indexed_aggregate_distinct_owner_computed_dynamic_array_path.string()
    );
    assert_contains(
        nested_static_indexed_distinct_owner_ir,
        "%holder.grid.element1.element0.values.addr"
    );
    assert_contains(
        nested_static_indexed_distinct_owner_ir,
        "%holder.grid.element0.element1.values.addr"
    );
    assert_contains(
        nested_static_indexed_distinct_owner_ir,
        "call void @__orison_owned_cleanup.Payload(ptr "
        "%computed_dynamic_array_selection.0.then.unselected.cleanup.drop.element.addr)"
    );
    assert_contains(
        nested_static_indexed_distinct_owner_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        nested_static_indexed_aggregate_distinct_owner_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_computed_for_run.o"
    );
    assert_build_success(
        executable,
        nested_static_indexed_aggregate_distinct_owner_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_computed_for_run"
    );
    assert_run_success(executable, runtime_indexed_distinct_root_computed_dynamic_array_path);
    auto runtime_indexed_distinct_root_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_indexed_distinct_root_computed_dynamic_array_path.string()
    );
    assert_contains(
        runtime_indexed_distinct_root_ir,
        "computed_dynamic_array_runtime_aggregate_index"
    );
    assert_contains(
        runtime_indexed_distinct_root_ir,
        "call void @__orison_owned_cleanup.Payload(ptr "
        "%computed_dynamic_array_selection.0.else.unselected.cleanup.drop.element.addr)"
    );
    assert_contains(
        runtime_indexed_distinct_root_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_computed_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_indexed_distinct_root_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_computed_for_run"
    );
    assert_run_success(executable, runtime_indexed_distinct_root_pure_computed_dynamic_array_path);
    auto runtime_indexed_distinct_root_pure_computed_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_indexed_distinct_root_pure_computed_dynamic_array_path.string()
    );
    auto first_index_load = runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %index.addr"
    );
    assert(first_index_load != std::string::npos);
    auto second_index_load = runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %index.addr",
        first_index_load + 1
    );
    assert(second_index_load != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_computed_ir.find(
            "load i64, ptr %index.addr",
            second_index_load + 1
        ) == std::string::npos
    );
    auto first_offset_load = runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %offset.addr"
    );
    assert(first_offset_load != std::string::npos);
    auto second_offset_load = runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %offset.addr",
        first_offset_load + 1
    );
    assert(second_offset_load != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_computed_ir.find(
            "load i64, ptr %offset.addr",
            second_offset_load + 1
        ) == std::string::npos
    );
    assert_contains(
        runtime_indexed_distinct_root_pure_computed_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_pure_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_pure_computed_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_indexed_distinct_root_pure_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_pure_computed_for_run"
    );
    assert_run_success(executable, runtime_indexed_distinct_root_pure_ternary_dynamic_array_path);
    auto runtime_indexed_distinct_root_pure_ternary_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_indexed_distinct_root_pure_ternary_dynamic_array_path.string()
    );
    assert_contains(
        runtime_indexed_distinct_root_pure_ternary_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    auto first_ternary_first_index_load = runtime_indexed_distinct_root_pure_ternary_ir.find(
        "load i64, ptr %first_index.addr"
    );
    assert(first_ternary_first_index_load != std::string::npos);
    auto second_ternary_first_index_load = runtime_indexed_distinct_root_pure_ternary_ir.find(
        "load i64, ptr %first_index.addr",
        first_ternary_first_index_load + 1
    );
    assert(second_ternary_first_index_load != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_ternary_ir.find(
            "load i64, ptr %first_index.addr",
            second_ternary_first_index_load + 1
        ) == std::string::npos
    );
    auto first_ternary_second_index_load = runtime_indexed_distinct_root_pure_ternary_ir.find(
        "load i64, ptr %second_index.addr"
    );
    assert(first_ternary_second_index_load != std::string::npos);
    auto second_ternary_second_index_load = runtime_indexed_distinct_root_pure_ternary_ir.find(
        "load i64, ptr %second_index.addr",
        first_ternary_second_index_load + 1
    );
    assert(second_ternary_second_index_load != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_ternary_ir.find(
            "load i64, ptr %second_index.addr",
            second_ternary_second_index_load + 1
        ) == std::string::npos
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_pure_ternary_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_pure_ternary_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_indexed_distinct_root_pure_ternary_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_pure_ternary_for_run"
    );
    assert_run_success(executable, runtime_indexed_distinct_root_pure_member_dynamic_array_path);
    auto runtime_indexed_distinct_root_pure_member_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_indexed_distinct_root_pure_member_dynamic_array_path.string()
    );
    assert_contains(
        runtime_indexed_distinct_root_pure_member_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    auto first_member_index_address = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr %record.IndexConfig"
    );
    assert(first_member_index_address != std::string::npos);
    auto second_member_index_address = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr %record.IndexConfig",
        first_member_index_address + 1
    );
    assert(second_member_index_address != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            "getelementptr %record.IndexConfig",
            second_member_index_address + 1
        ) == std::string::npos
    );
    auto member_cleanup_index = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x %record.IndexConfig]"
    );
    assert(member_cleanup_index != std::string::npos);
    auto first_runtime_member_index = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x %record.IndexConfig]",
        member_cleanup_index + 1
    );
    assert(first_runtime_member_index != std::string::npos);
    auto second_runtime_member_index = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x %record.IndexConfig]",
        first_runtime_member_index + 1
    );
    assert(second_runtime_member_index != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            second_runtime_member_index + 1
        ) == std::string::npos
    );
    auto first_runtime_member_index_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %group_slot, 2"
    );
    assert(first_runtime_member_index_guard != std::string::npos);
    auto second_runtime_member_index_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %group_slot, 2",
        first_runtime_member_index_guard + 1
    );
    assert(second_runtime_member_index_guard != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            " = icmp ult i64 %group_slot, 2",
            second_runtime_member_index_guard + 1
        ) == std::string::npos
    );
    assert(first_runtime_member_index_guard < first_runtime_member_index);
    assert(second_runtime_member_index_guard < second_runtime_member_index);
    auto first_runtime_integer_index = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x i32]"
    );
    assert(first_runtime_integer_index != std::string::npos);
    auto second_runtime_integer_index = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x i32]",
        first_runtime_integer_index + 1
    );
    assert(second_runtime_integer_index != std::string::npos);
    auto third_runtime_integer_index = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x i32]",
        second_runtime_integer_index + 1
    );
    assert(third_runtime_integer_index != std::string::npos);
    auto fourth_runtime_integer_index = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x i32]",
        third_runtime_integer_index + 1
    );
    assert(fourth_runtime_integer_index != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            "getelementptr [2 x i32]",
            fourth_runtime_integer_index + 1
        ) == std::string::npos
    );
    auto first_runtime_integer_index_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %slot, 2"
    );
    assert(first_runtime_integer_index_guard != std::string::npos);
    auto second_runtime_integer_index_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %slot, 2",
        first_runtime_integer_index_guard + 1
    );
    assert(second_runtime_integer_index_guard != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            " = icmp ult i64 %slot, 2",
            second_runtime_integer_index_guard + 1
        ) == std::string::npos
    );
    auto first_runtime_matrix_row = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x [2 x i32]]"
    );
    assert(first_runtime_matrix_row != std::string::npos);
    auto second_runtime_matrix_row = runtime_indexed_distinct_root_pure_member_ir.find(
        "getelementptr [2 x [2 x i32]]",
        first_runtime_matrix_row + 1
    );
    assert(second_runtime_matrix_row != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            "getelementptr [2 x [2 x i32]]",
            second_runtime_matrix_row + 1
        ) == std::string::npos
    );
    auto first_runtime_matrix_row_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %row, 2"
    );
    assert(first_runtime_matrix_row_guard != std::string::npos);
    auto second_runtime_matrix_row_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %row, 2",
        first_runtime_matrix_row_guard + 1
    );
    assert(second_runtime_matrix_row_guard != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            " = icmp ult i64 %row, 2",
            second_runtime_matrix_row_guard + 1
        ) == std::string::npos
    );
    auto first_runtime_matrix_column_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %column, 2"
    );
    assert(first_runtime_matrix_column_guard != std::string::npos);
    auto second_runtime_matrix_column_guard = runtime_indexed_distinct_root_pure_member_ir.find(
        " = icmp ult i64 %column, 2",
        first_runtime_matrix_column_guard + 1
    );
    assert(second_runtime_matrix_column_guard != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            " = icmp ult i64 %column, 2",
            second_runtime_matrix_column_guard + 1
        ) == std::string::npos
    );
    assert(first_runtime_integer_index_guard < first_runtime_matrix_row_guard);
    assert(first_runtime_matrix_row_guard < first_runtime_matrix_column_guard);
    assert(second_runtime_integer_index_guard < second_runtime_matrix_row_guard);
    assert(second_runtime_matrix_row_guard < second_runtime_matrix_column_guard);
    auto first_member_index_cast = runtime_indexed_distinct_root_pure_member_ir.find("zext i32");
    assert(first_member_index_cast != std::string::npos);
    auto second_member_index_cast = runtime_indexed_distinct_root_pure_member_ir.find(
        "zext i32",
        first_member_index_cast + 1
    );
    assert(second_member_index_cast != std::string::npos);
    assert(
        runtime_indexed_distinct_root_pure_member_ir.find(
            "zext i32",
            second_member_index_cast + 1
        ) == std::string::npos
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_pure_member_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_pure_member_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_indexed_distinct_root_pure_member_dynamic_array_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_pure_member_for_run"
    );
    assert_run_success(executable, runtime_index_source_dependent_path);
    auto runtime_index_source_dependent_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " + runtime_index_source_dependent_path.string()
    );
    auto dependent_first_slot_guard = runtime_index_source_dependent_ir.find(
        " = icmp ult i64 %slot, 2"
    );
    assert(dependent_first_slot_guard != std::string::npos);
    auto dependent_second_slot_guard = runtime_index_source_dependent_ir.find(
        " = icmp ult i64 %slot, 2",
        dependent_first_slot_guard + 1
    );
    assert(dependent_second_slot_guard != std::string::npos);
    assert(
        runtime_index_source_dependent_ir.find(
            " = icmp ult i64 %slot, 2",
            dependent_second_slot_guard + 1
        ) == std::string::npos
    );
    auto dependent_first_source_index = runtime_index_source_dependent_ir.find(
        "getelementptr [2 x i32]",
        dependent_first_slot_guard
    );
    assert(dependent_first_source_index != std::string::npos);
    auto dependent_first_source_cast = runtime_index_source_dependent_ir.find(
        "zext i32",
        dependent_first_source_index
    );
    assert(dependent_first_source_cast != std::string::npos);
    auto dependent_first_row_guard = runtime_index_source_dependent_ir.find(
        " = icmp ult i64 ",
        dependent_first_source_cast
    );
    assert(dependent_first_row_guard != std::string::npos);
    auto dependent_first_row_projection = runtime_index_source_dependent_ir.find(
        "getelementptr [2 x [2 x i32]]",
        dependent_first_row_guard
    );
    assert(dependent_first_row_projection != std::string::npos);
    auto dependent_first_column_guard = runtime_index_source_dependent_ir.find(
        " = icmp ult i64 %column, 2",
        dependent_first_row_projection
    );
    assert(dependent_first_column_guard != std::string::npos);
    auto dependent_first_value_projection = runtime_index_source_dependent_ir.find(
        "getelementptr [2 x i32]",
        dependent_first_column_guard
    );
    assert(dependent_first_value_projection != std::string::npos);
    assert(dependent_first_slot_guard < dependent_first_source_index);
    assert(dependent_first_source_index < dependent_first_source_cast);
    assert(dependent_first_source_cast < dependent_first_row_guard);
    assert(dependent_first_row_guard < dependent_first_row_projection);
    assert(dependent_first_row_projection < dependent_first_column_guard);
    assert(dependent_first_column_guard < dependent_first_value_projection);
    auto dependent_second_source_index = runtime_index_source_dependent_ir.find(
        "getelementptr [2 x i32]",
        dependent_second_slot_guard
    );
    assert(dependent_second_source_index != std::string::npos);
    auto dependent_second_source_cast = runtime_index_source_dependent_ir.find(
        "zext i32",
        dependent_second_source_index
    );
    assert(dependent_second_source_cast != std::string::npos);
    auto dependent_second_row_guard = runtime_index_source_dependent_ir.find(
        " = icmp ult i64 ",
        dependent_second_source_cast
    );
    assert(dependent_second_row_guard != std::string::npos);
    auto dependent_second_row_projection = runtime_index_source_dependent_ir.find(
        "getelementptr [2 x [2 x i32]]",
        dependent_second_row_guard
    );
    assert(dependent_second_row_projection != std::string::npos);
    auto dependent_second_column_guard = runtime_index_source_dependent_ir.find(
        " = icmp ult i64 %column, 2",
        dependent_second_row_projection
    );
    assert(dependent_second_column_guard != std::string::npos);
    auto dependent_second_value_projection = runtime_index_source_dependent_ir.find(
        "getelementptr [2 x i32]",
        dependent_second_column_guard
    );
    assert(dependent_second_value_projection != std::string::npos);
    assert(
        runtime_index_source_dependent_ir.find(
            "getelementptr [2 x [2 x i32]]",
            dependent_second_row_projection + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_source_dependent_ir.find(
            " = icmp ult i64 %column, 2",
            dependent_second_column_guard + 1
        ) == std::string::npos
    );
    assert(dependent_second_slot_guard < dependent_second_source_index);
    assert(dependent_second_source_index < dependent_second_source_cast);
    assert(dependent_second_source_cast < dependent_second_row_guard);
    assert(dependent_second_row_guard < dependent_second_row_projection);
    assert(dependent_second_row_projection < dependent_second_column_guard);
    assert(dependent_second_column_guard < dependent_second_value_projection);
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_source_dependent_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_for_run"
    );
    assert_run_success(executable, runtime_index_source_dependent_member_path);
    auto runtime_index_source_dependent_member_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " + runtime_index_source_dependent_member_path.string()
    );
    auto dependent_member_first_slot_guard = runtime_index_source_dependent_member_ir.find(
        " = icmp ult i64 %slot, 2"
    );
    assert(dependent_member_first_slot_guard != std::string::npos);
    auto dependent_member_second_slot_guard = runtime_index_source_dependent_member_ir.find(
        " = icmp ult i64 %slot, 2",
        dependent_member_first_slot_guard + 1
    );
    assert(dependent_member_second_slot_guard != std::string::npos);
    assert(
        runtime_index_source_dependent_member_ir.find(
            " = icmp ult i64 %slot, 2",
            dependent_member_second_slot_guard + 1
        ) == std::string::npos
    );
    auto dependent_member_first_source_index = runtime_index_source_dependent_member_ir.find(
        "getelementptr [2 x i32]",
        dependent_member_first_slot_guard
    );
    assert(dependent_member_first_source_index != std::string::npos);
    auto dependent_member_first_source_load = runtime_index_source_dependent_member_ir.find(
        " = load i32, ptr ",
        dependent_member_first_source_index
    );
    assert(dependent_member_first_source_load != std::string::npos);
    auto dependent_member_first_source_cast = runtime_index_source_dependent_member_ir.find(
        " = zext i32 ",
        dependent_member_first_source_load
    );
    assert(dependent_member_first_source_cast != std::string::npos);
    auto dependent_member_first_group_guard = runtime_index_source_dependent_member_ir.find(
        " = icmp ult i64 ",
        dependent_member_first_source_cast
    );
    assert(dependent_member_first_group_guard != std::string::npos);
    auto dependent_member_first_group_projection = runtime_index_source_dependent_member_ir.find(
        "getelementptr [2 x %record.IndexConfig]",
        dependent_member_first_group_guard
    );
    assert(dependent_member_first_group_projection != std::string::npos);
    auto dependent_member_first_field_projection = runtime_index_source_dependent_member_ir.find(
        "getelementptr %record.IndexConfig",
        dependent_member_first_group_projection
    );
    assert(dependent_member_first_field_projection != std::string::npos);
    auto dependent_member_first_field_load = runtime_index_source_dependent_member_ir.find(
        " = load i32, ptr ",
        dependent_member_first_field_projection
    );
    assert(dependent_member_first_field_load != std::string::npos);
    auto dependent_member_first_field_cast = runtime_index_source_dependent_member_ir.find(
        " = zext i32 ",
        dependent_member_first_field_load
    );
    assert(dependent_member_first_field_cast != std::string::npos);
    auto dependent_member_first_outer_guard = runtime_index_source_dependent_member_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        dependent_member_first_field_cast
    );
    assert(dependent_member_first_outer_guard != std::string::npos);
    assert(dependent_member_first_slot_guard < dependent_member_first_source_index);
    assert(dependent_member_first_source_index < dependent_member_first_source_load);
    assert(dependent_member_first_source_load < dependent_member_first_source_cast);
    assert(dependent_member_first_source_cast < dependent_member_first_group_guard);
    assert(dependent_member_first_group_guard < dependent_member_first_group_projection);
    assert(dependent_member_first_group_projection < dependent_member_first_field_projection);
    assert(dependent_member_first_field_projection < dependent_member_first_field_load);
    assert(dependent_member_first_field_load < dependent_member_first_field_cast);
    assert(dependent_member_first_field_cast < dependent_member_first_outer_guard);
    auto dependent_member_second_source_index = runtime_index_source_dependent_member_ir.find(
        "getelementptr [2 x i32]",
        dependent_member_second_slot_guard
    );
    assert(dependent_member_second_source_index != std::string::npos);
    auto dependent_member_second_source_load = runtime_index_source_dependent_member_ir.find(
        " = load i32, ptr ",
        dependent_member_second_source_index
    );
    assert(dependent_member_second_source_load != std::string::npos);
    auto dependent_member_second_source_cast = runtime_index_source_dependent_member_ir.find(
        " = zext i32 ",
        dependent_member_second_source_load
    );
    assert(dependent_member_second_source_cast != std::string::npos);
    auto dependent_member_second_group_guard = runtime_index_source_dependent_member_ir.find(
        " = icmp ult i64 ",
        dependent_member_second_source_cast
    );
    assert(dependent_member_second_group_guard != std::string::npos);
    auto dependent_member_second_group_projection = runtime_index_source_dependent_member_ir.find(
        "getelementptr [2 x %record.IndexConfig]",
        dependent_member_second_group_guard
    );
    assert(dependent_member_second_group_projection != std::string::npos);
    auto dependent_member_second_field_projection = runtime_index_source_dependent_member_ir.find(
        "getelementptr %record.IndexConfig",
        dependent_member_second_group_projection
    );
    assert(dependent_member_second_field_projection != std::string::npos);
    auto dependent_member_second_field_load = runtime_index_source_dependent_member_ir.find(
        " = load i32, ptr ",
        dependent_member_second_field_projection
    );
    assert(dependent_member_second_field_load != std::string::npos);
    auto dependent_member_second_field_cast = runtime_index_source_dependent_member_ir.find(
        " = zext i32 ",
        dependent_member_second_field_load
    );
    assert(dependent_member_second_field_cast != std::string::npos);
    auto dependent_member_second_outer_guard = runtime_index_source_dependent_member_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        dependent_member_second_field_cast
    );
    assert(dependent_member_second_outer_guard != std::string::npos);
    assert(
        runtime_index_source_dependent_member_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            dependent_member_second_group_projection + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_source_dependent_member_ir.find(
            "getelementptr %record.IndexConfig",
            dependent_member_second_field_projection + 1
        ) == std::string::npos
    );
    assert(dependent_member_second_slot_guard < dependent_member_second_source_index);
    assert(dependent_member_second_source_index < dependent_member_second_source_load);
    assert(dependent_member_second_source_load < dependent_member_second_source_cast);
    assert(dependent_member_second_source_cast < dependent_member_second_group_guard);
    assert(dependent_member_second_group_guard < dependent_member_second_group_projection);
    assert(dependent_member_second_group_projection < dependent_member_second_field_projection);
    assert(dependent_member_second_field_projection < dependent_member_second_field_load);
    assert(dependent_member_second_field_load < dependent_member_second_field_cast);
    assert(dependent_member_second_field_cast < dependent_member_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_member_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_member_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_source_dependent_member_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_member_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_path_condition_path);
    auto runtime_index_boolean_path_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " + runtime_index_boolean_path_condition_path.string()
    );
    auto boolean_path_first_condition_guard = runtime_index_boolean_path_condition_ir.find(
        " = icmp ult i64 %condition_slot, 2"
    );
    assert(boolean_path_first_condition_guard != std::string::npos);
    auto boolean_path_second_condition_guard = runtime_index_boolean_path_condition_ir.find(
        " = icmp ult i64 %condition_slot, 2",
        boolean_path_first_condition_guard + 1
    );
    assert(boolean_path_second_condition_guard != std::string::npos);
    assert(
        runtime_index_boolean_path_condition_ir.find(
            " = icmp ult i64 %condition_slot, 2",
            boolean_path_second_condition_guard + 1
        ) == std::string::npos
    );
    auto boolean_path_first_projection = runtime_index_boolean_path_condition_ir.find(
        "getelementptr [2 x i1]",
        boolean_path_first_condition_guard
    );
    assert(boolean_path_first_projection != std::string::npos);
    auto boolean_path_first_load = runtime_index_boolean_path_condition_ir.find(
        " = load i1, ptr ",
        boolean_path_first_projection
    );
    assert(boolean_path_first_load != std::string::npos);
    auto boolean_path_first_index_phi = runtime_index_boolean_path_condition_ir.find(
        " = phi i64 [%first, ",
        boolean_path_first_load
    );
    assert(boolean_path_first_index_phi != std::string::npos);
    auto boolean_path_first_outer_guard = runtime_index_boolean_path_condition_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        boolean_path_first_index_phi
    );
    assert(boolean_path_first_outer_guard != std::string::npos);
    assert(boolean_path_first_condition_guard < boolean_path_first_projection);
    assert(boolean_path_first_projection < boolean_path_first_load);
    assert(boolean_path_first_load < boolean_path_first_index_phi);
    assert(boolean_path_first_index_phi < boolean_path_first_outer_guard);
    auto boolean_path_second_projection = runtime_index_boolean_path_condition_ir.find(
        "getelementptr [2 x i1]",
        boolean_path_second_condition_guard
    );
    assert(boolean_path_second_projection != std::string::npos);
    auto boolean_path_second_load = runtime_index_boolean_path_condition_ir.find(
        " = load i1, ptr ",
        boolean_path_second_projection
    );
    assert(boolean_path_second_load != std::string::npos);
    auto boolean_path_second_index_phi = runtime_index_boolean_path_condition_ir.find(
        " = phi i64 [%first, ",
        boolean_path_second_load
    );
    assert(boolean_path_second_index_phi != std::string::npos);
    auto boolean_path_second_outer_guard = runtime_index_boolean_path_condition_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        boolean_path_second_index_phi
    );
    assert(boolean_path_second_outer_guard != std::string::npos);
    assert(
        runtime_index_boolean_path_condition_ir.find(
            "getelementptr [2 x i1]",
            boolean_path_second_projection + 1
        ) == std::string::npos
    );
    assert(boolean_path_second_condition_guard < boolean_path_second_projection);
    assert(boolean_path_second_projection < boolean_path_second_load);
    assert(boolean_path_second_load < boolean_path_second_index_phi);
    assert(boolean_path_second_index_phi < boolean_path_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_member_path_condition_path);
    auto runtime_index_boolean_member_path_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_boolean_member_path_condition_path.string()
    );
    auto boolean_member_path_first_guard =
        runtime_index_boolean_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2"
        );
    assert(boolean_member_path_first_guard != std::string::npos);
    auto boolean_member_path_second_guard =
        runtime_index_boolean_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_member_path_first_guard + 1
        );
    assert(boolean_member_path_second_guard != std::string::npos);
    assert(
        runtime_index_boolean_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_member_path_second_guard + 1
        ) == std::string::npos
    );
    auto boolean_member_path_first_array_projection =
        runtime_index_boolean_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_member_path_first_guard
        );
    assert(boolean_member_path_first_array_projection != std::string::npos);
    auto boolean_member_path_first_field_projection =
        runtime_index_boolean_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_member_path_first_array_projection
        );
    assert(boolean_member_path_first_field_projection != std::string::npos);
    auto boolean_member_path_first_load =
        runtime_index_boolean_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_member_path_first_field_projection
        );
    assert(boolean_member_path_first_load != std::string::npos);
    auto boolean_member_path_first_index_phi =
        runtime_index_boolean_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_member_path_first_load
        );
    assert(boolean_member_path_first_index_phi != std::string::npos);
    auto boolean_member_path_first_outer_guard =
        runtime_index_boolean_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_member_path_first_index_phi
        );
    assert(boolean_member_path_first_outer_guard != std::string::npos);
    assert(boolean_member_path_first_guard < boolean_member_path_first_array_projection);
    assert(boolean_member_path_first_array_projection < boolean_member_path_first_field_projection);
    assert(boolean_member_path_first_field_projection < boolean_member_path_first_load);
    assert(boolean_member_path_first_load < boolean_member_path_first_index_phi);
    assert(boolean_member_path_first_index_phi < boolean_member_path_first_outer_guard);
    auto boolean_member_path_second_array_projection =
        runtime_index_boolean_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_member_path_second_guard
        );
    assert(boolean_member_path_second_array_projection != std::string::npos);
    auto boolean_member_path_second_field_projection =
        runtime_index_boolean_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_member_path_second_array_projection
        );
    assert(boolean_member_path_second_field_projection != std::string::npos);
    auto boolean_member_path_second_load =
        runtime_index_boolean_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_member_path_second_field_projection
        );
    assert(boolean_member_path_second_load != std::string::npos);
    auto boolean_member_path_second_index_phi =
        runtime_index_boolean_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_member_path_second_load
        );
    assert(boolean_member_path_second_index_phi != std::string::npos);
    auto boolean_member_path_second_outer_guard =
        runtime_index_boolean_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_member_path_second_index_phi
        );
    assert(boolean_member_path_second_outer_guard != std::string::npos);
    assert(boolean_member_path_second_guard < boolean_member_path_second_array_projection);
    assert(boolean_member_path_second_array_projection < boolean_member_path_second_field_projection);
    assert(boolean_member_path_second_field_projection < boolean_member_path_second_load);
    assert(boolean_member_path_second_load < boolean_member_path_second_index_phi);
    assert(boolean_member_path_second_index_phi < boolean_member_path_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_member_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_not_member_path_condition_path);
    auto runtime_index_boolean_not_member_path_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_boolean_not_member_path_condition_path.string()
    );
    auto boolean_not_member_path_first_guard =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2"
        );
    assert(boolean_not_member_path_first_guard != std::string::npos);
    auto boolean_not_member_path_second_guard =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_not_member_path_first_guard + 1
        );
    assert(boolean_not_member_path_second_guard != std::string::npos);
    assert(
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_not_member_path_second_guard + 1
        ) == std::string::npos
    );
    auto boolean_not_member_path_first_array_projection =
        runtime_index_boolean_not_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_not_member_path_first_guard
        );
    assert(boolean_not_member_path_first_array_projection != std::string::npos);
    auto boolean_not_member_path_first_field_projection =
        runtime_index_boolean_not_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_not_member_path_first_array_projection
        );
    assert(boolean_not_member_path_first_field_projection != std::string::npos);
    auto boolean_not_member_path_first_load =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_not_member_path_first_field_projection
        );
    assert(boolean_not_member_path_first_load != std::string::npos);
    auto boolean_not_member_path_first_inversion =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = xor i1 ",
            boolean_not_member_path_first_load
        );
    assert(boolean_not_member_path_first_inversion != std::string::npos);
    auto boolean_not_member_path_first_index_phi =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_not_member_path_first_inversion
        );
    assert(boolean_not_member_path_first_index_phi != std::string::npos);
    auto boolean_not_member_path_first_outer_guard =
        runtime_index_boolean_not_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_not_member_path_first_index_phi
        );
    assert(boolean_not_member_path_first_outer_guard != std::string::npos);
    assert(boolean_not_member_path_first_guard < boolean_not_member_path_first_array_projection);
    assert(
        boolean_not_member_path_first_array_projection <
        boolean_not_member_path_first_field_projection
    );
    assert(boolean_not_member_path_first_field_projection < boolean_not_member_path_first_load);
    assert(boolean_not_member_path_first_load < boolean_not_member_path_first_inversion);
    assert(boolean_not_member_path_first_inversion < boolean_not_member_path_first_index_phi);
    assert(boolean_not_member_path_first_index_phi < boolean_not_member_path_first_outer_guard);
    auto boolean_not_member_path_second_array_projection =
        runtime_index_boolean_not_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_not_member_path_second_guard
        );
    assert(boolean_not_member_path_second_array_projection != std::string::npos);
    auto boolean_not_member_path_second_field_projection =
        runtime_index_boolean_not_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_not_member_path_second_array_projection
        );
    assert(boolean_not_member_path_second_field_projection != std::string::npos);
    auto boolean_not_member_path_second_load =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_not_member_path_second_field_projection
        );
    assert(boolean_not_member_path_second_load != std::string::npos);
    auto boolean_not_member_path_second_inversion =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = xor i1 ",
            boolean_not_member_path_second_load
        );
    assert(boolean_not_member_path_second_inversion != std::string::npos);
    auto boolean_not_member_path_second_index_phi =
        runtime_index_boolean_not_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_not_member_path_second_inversion
        );
    assert(boolean_not_member_path_second_index_phi != std::string::npos);
    auto boolean_not_member_path_second_outer_guard =
        runtime_index_boolean_not_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_not_member_path_second_index_phi
        );
    assert(boolean_not_member_path_second_outer_guard != std::string::npos);
    assert(boolean_not_member_path_second_guard < boolean_not_member_path_second_array_projection);
    assert(
        boolean_not_member_path_second_array_projection <
        boolean_not_member_path_second_field_projection
    );
    assert(boolean_not_member_path_second_field_projection < boolean_not_member_path_second_load);
    assert(boolean_not_member_path_second_load < boolean_not_member_path_second_inversion);
    assert(boolean_not_member_path_second_inversion < boolean_not_member_path_second_index_phi);
    assert(boolean_not_member_path_second_index_phi < boolean_not_member_path_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_not_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_not_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_not_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_not_member_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_and_member_path_condition_path);
    auto runtime_index_boolean_and_member_path_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_boolean_and_member_path_condition_path.string()
    );
    auto boolean_and_member_path_first_branch =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs."
        );
    assert(boolean_and_member_path_first_branch != std::string::npos);
    auto boolean_and_member_path_second_branch =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs.",
            boolean_and_member_path_first_branch + 1
        );
    assert(boolean_and_member_path_second_branch != std::string::npos);
    assert(
        runtime_index_boolean_and_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs.",
            boolean_and_member_path_second_branch + 1
        ) == std::string::npos
    );
    auto boolean_and_member_path_first_rhs =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_and_member_path_first_branch
        );
    auto boolean_and_member_path_first_guard =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_and_member_path_first_rhs
        );
    auto boolean_and_member_path_first_array_projection =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_and_member_path_first_guard
        );
    auto boolean_and_member_path_first_field_projection =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_and_member_path_first_array_projection
        );
    auto boolean_and_member_path_first_load =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_and_member_path_first_field_projection
        );
    auto boolean_and_member_path_first_merge =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_and_member_path_first_load
        );
    auto boolean_and_member_path_first_phi =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = phi i1 [0, %",
            boolean_and_member_path_first_merge
        );
    auto boolean_and_member_path_first_index_phi =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_and_member_path_first_phi
        );
    auto boolean_and_member_path_first_outer_guard =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_and_member_path_first_index_phi
        );
    assert(boolean_and_member_path_first_rhs != std::string::npos);
    assert(boolean_and_member_path_first_guard != std::string::npos);
    assert(boolean_and_member_path_first_array_projection != std::string::npos);
    assert(boolean_and_member_path_first_field_projection != std::string::npos);
    assert(boolean_and_member_path_first_load != std::string::npos);
    assert(boolean_and_member_path_first_merge != std::string::npos);
    assert(boolean_and_member_path_first_phi != std::string::npos);
    assert(boolean_and_member_path_first_index_phi != std::string::npos);
    assert(boolean_and_member_path_first_outer_guard != std::string::npos);
    assert(boolean_and_member_path_first_branch < boolean_and_member_path_first_rhs);
    assert(boolean_and_member_path_first_rhs < boolean_and_member_path_first_guard);
    assert(boolean_and_member_path_first_guard < boolean_and_member_path_first_array_projection);
    assert(
        boolean_and_member_path_first_array_projection <
        boolean_and_member_path_first_field_projection
    );
    assert(boolean_and_member_path_first_field_projection < boolean_and_member_path_first_load);
    assert(boolean_and_member_path_first_load < boolean_and_member_path_first_merge);
    assert(boolean_and_member_path_first_merge < boolean_and_member_path_first_phi);
    assert(boolean_and_member_path_first_phi < boolean_and_member_path_first_index_phi);
    assert(boolean_and_member_path_first_index_phi < boolean_and_member_path_first_outer_guard);
    auto boolean_and_member_path_second_rhs =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_and_member_path_second_branch
        );
    auto boolean_and_member_path_second_guard =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_and_member_path_second_rhs
        );
    auto boolean_and_member_path_second_load =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_and_member_path_second_guard
        );
    auto boolean_and_member_path_second_merge =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_and_member_path_second_load
        );
    auto boolean_and_member_path_second_phi =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = phi i1 [0, %",
            boolean_and_member_path_second_merge
        );
    auto boolean_and_member_path_second_index_phi =
        runtime_index_boolean_and_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_and_member_path_second_phi
        );
    auto boolean_and_member_path_second_outer_guard =
        runtime_index_boolean_and_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_and_member_path_second_index_phi
        );
    assert(boolean_and_member_path_second_rhs != std::string::npos);
    assert(boolean_and_member_path_second_guard != std::string::npos);
    assert(boolean_and_member_path_second_load != std::string::npos);
    assert(boolean_and_member_path_second_merge != std::string::npos);
    assert(boolean_and_member_path_second_phi != std::string::npos);
    assert(boolean_and_member_path_second_index_phi != std::string::npos);
    assert(boolean_and_member_path_second_outer_guard != std::string::npos);
    assert(boolean_and_member_path_second_branch < boolean_and_member_path_second_rhs);
    assert(boolean_and_member_path_second_rhs < boolean_and_member_path_second_guard);
    assert(boolean_and_member_path_second_guard < boolean_and_member_path_second_load);
    assert(boolean_and_member_path_second_load < boolean_and_member_path_second_merge);
    assert(boolean_and_member_path_second_merge < boolean_and_member_path_second_phi);
    assert(boolean_and_member_path_second_phi < boolean_and_member_path_second_index_phi);
    assert(boolean_and_member_path_second_index_phi < boolean_and_member_path_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_and_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_and_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_and_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_and_member_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_or_member_path_condition_path);
    auto runtime_index_boolean_or_member_path_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_boolean_or_member_path_condition_path.string()
    );
    auto boolean_or_member_path_first_branch =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge."
        );
    assert(boolean_or_member_path_first_branch != std::string::npos);
    auto boolean_or_member_path_second_branch =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge.",
            boolean_or_member_path_first_branch + 1
        );
    assert(boolean_or_member_path_second_branch != std::string::npos);
    assert(
        runtime_index_boolean_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge.",
            boolean_or_member_path_second_branch + 1
        ) == std::string::npos
    );
    auto boolean_or_member_path_first_rhs =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_or_member_path_first_branch
        );
    auto boolean_or_member_path_first_guard =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_or_member_path_first_rhs
        );
    auto boolean_or_member_path_first_array_projection =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_or_member_path_first_guard
        );
    auto boolean_or_member_path_first_field_projection =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_or_member_path_first_array_projection
        );
    auto boolean_or_member_path_first_load =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_or_member_path_first_field_projection
        );
    auto boolean_or_member_path_first_merge =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_or_member_path_first_load
        );
    auto boolean_or_member_path_first_phi =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = phi i1 [1, %",
            boolean_or_member_path_first_merge
        );
    auto boolean_or_member_path_first_index_phi =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_or_member_path_first_phi
        );
    auto boolean_or_member_path_first_outer_guard =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_or_member_path_first_index_phi
        );
    assert(boolean_or_member_path_first_rhs != std::string::npos);
    assert(boolean_or_member_path_first_guard != std::string::npos);
    assert(boolean_or_member_path_first_array_projection != std::string::npos);
    assert(boolean_or_member_path_first_field_projection != std::string::npos);
    assert(boolean_or_member_path_first_load != std::string::npos);
    assert(boolean_or_member_path_first_merge != std::string::npos);
    assert(boolean_or_member_path_first_phi != std::string::npos);
    assert(boolean_or_member_path_first_index_phi != std::string::npos);
    assert(boolean_or_member_path_first_outer_guard != std::string::npos);
    assert(boolean_or_member_path_first_branch < boolean_or_member_path_first_rhs);
    assert(boolean_or_member_path_first_rhs < boolean_or_member_path_first_guard);
    assert(boolean_or_member_path_first_guard < boolean_or_member_path_first_array_projection);
    assert(
        boolean_or_member_path_first_array_projection <
        boolean_or_member_path_first_field_projection
    );
    assert(boolean_or_member_path_first_field_projection < boolean_or_member_path_first_load);
    assert(boolean_or_member_path_first_load < boolean_or_member_path_first_merge);
    assert(boolean_or_member_path_first_merge < boolean_or_member_path_first_phi);
    assert(boolean_or_member_path_first_phi < boolean_or_member_path_first_index_phi);
    assert(boolean_or_member_path_first_index_phi < boolean_or_member_path_first_outer_guard);
    auto boolean_or_member_path_second_rhs =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_or_member_path_second_branch
        );
    auto boolean_or_member_path_second_guard =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_or_member_path_second_rhs
        );
    auto boolean_or_member_path_second_load =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_or_member_path_second_guard
        );
    auto boolean_or_member_path_second_merge =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_or_member_path_second_load
        );
    auto boolean_or_member_path_second_phi =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = phi i1 [1, %",
            boolean_or_member_path_second_merge
        );
    auto boolean_or_member_path_second_index_phi =
        runtime_index_boolean_or_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_or_member_path_second_phi
        );
    auto boolean_or_member_path_second_outer_guard =
        runtime_index_boolean_or_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_or_member_path_second_index_phi
        );
    assert(boolean_or_member_path_second_rhs != std::string::npos);
    assert(boolean_or_member_path_second_guard != std::string::npos);
    assert(boolean_or_member_path_second_load != std::string::npos);
    assert(boolean_or_member_path_second_merge != std::string::npos);
    assert(boolean_or_member_path_second_phi != std::string::npos);
    assert(boolean_or_member_path_second_index_phi != std::string::npos);
    assert(boolean_or_member_path_second_outer_guard != std::string::npos);
    assert(boolean_or_member_path_second_branch < boolean_or_member_path_second_rhs);
    assert(boolean_or_member_path_second_rhs < boolean_or_member_path_second_guard);
    assert(boolean_or_member_path_second_guard < boolean_or_member_path_second_load);
    assert(boolean_or_member_path_second_load < boolean_or_member_path_second_merge);
    assert(boolean_or_member_path_second_merge < boolean_or_member_path_second_phi);
    assert(boolean_or_member_path_second_phi < boolean_or_member_path_second_index_phi);
    assert(boolean_or_member_path_second_index_phi < boolean_or_member_path_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_or_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_or_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_or_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_or_member_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_nested_member_path_condition_path);
    auto runtime_index_boolean_nested_member_path_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_boolean_nested_member_path_condition_path.string()
    );
    auto boolean_nested_member_path_first_outer_branch =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge."
        );
    assert(boolean_nested_member_path_first_outer_branch != std::string::npos);
    auto boolean_nested_member_path_second_outer_branch =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge.",
            boolean_nested_member_path_first_outer_branch + 1
        );
    assert(boolean_nested_member_path_second_outer_branch != std::string::npos);
    assert(
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge.",
            boolean_nested_member_path_second_outer_branch + 1
        ) == std::string::npos
    );
    auto boolean_nested_member_path_first_outer_rhs =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_nested_member_path_first_outer_branch
        );
    auto boolean_nested_member_path_first_inner_branch =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "br i1 %fallback, label %logical.and.rhs.",
            boolean_nested_member_path_first_outer_rhs
        );
    auto boolean_nested_member_path_first_inner_rhs =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_nested_member_path_first_inner_branch
        );
    auto boolean_nested_member_path_first_guard =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_nested_member_path_first_inner_rhs
        );
    auto boolean_nested_member_path_first_load =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_nested_member_path_first_guard
        );
    auto boolean_nested_member_path_first_inner_merge =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_nested_member_path_first_load
        );
    auto boolean_nested_member_path_first_inner_phi =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = phi i1 [0, %logical.or.rhs.",
            boolean_nested_member_path_first_inner_merge
        );
    auto boolean_nested_member_path_first_outer_merge =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_nested_member_path_first_inner_phi
        );
    auto boolean_nested_member_path_first_outer_phi =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = phi i1 [1, %ternary.then.",
            boolean_nested_member_path_first_outer_merge
        );
    auto boolean_nested_member_path_first_outer_phi_inner_predecessor =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            ", %logical.and.merge.",
            boolean_nested_member_path_first_outer_phi
        );
    auto boolean_nested_member_path_first_index_phi =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_nested_member_path_first_outer_phi
        );
    auto boolean_nested_member_path_first_outer_guard =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_nested_member_path_first_index_phi
        );
    assert(boolean_nested_member_path_first_outer_rhs != std::string::npos);
    assert(boolean_nested_member_path_first_inner_branch != std::string::npos);
    assert(boolean_nested_member_path_first_inner_rhs != std::string::npos);
    assert(boolean_nested_member_path_first_guard != std::string::npos);
    assert(boolean_nested_member_path_first_load != std::string::npos);
    assert(boolean_nested_member_path_first_inner_merge != std::string::npos);
    assert(boolean_nested_member_path_first_inner_phi != std::string::npos);
    assert(boolean_nested_member_path_first_outer_merge != std::string::npos);
    assert(boolean_nested_member_path_first_outer_phi != std::string::npos);
    assert(boolean_nested_member_path_first_outer_phi_inner_predecessor != std::string::npos);
    assert(boolean_nested_member_path_first_index_phi != std::string::npos);
    assert(boolean_nested_member_path_first_outer_guard != std::string::npos);
    assert(boolean_nested_member_path_first_outer_branch < boolean_nested_member_path_first_outer_rhs);
    assert(boolean_nested_member_path_first_outer_rhs < boolean_nested_member_path_first_inner_branch);
    assert(boolean_nested_member_path_first_inner_branch < boolean_nested_member_path_first_inner_rhs);
    assert(boolean_nested_member_path_first_inner_rhs < boolean_nested_member_path_first_guard);
    assert(boolean_nested_member_path_first_guard < boolean_nested_member_path_first_load);
    assert(boolean_nested_member_path_first_load < boolean_nested_member_path_first_inner_merge);
    assert(boolean_nested_member_path_first_inner_merge < boolean_nested_member_path_first_inner_phi);
    assert(boolean_nested_member_path_first_inner_phi < boolean_nested_member_path_first_outer_merge);
    assert(boolean_nested_member_path_first_outer_merge < boolean_nested_member_path_first_outer_phi);
    assert(
        boolean_nested_member_path_first_outer_phi <
        boolean_nested_member_path_first_outer_phi_inner_predecessor
    );
    assert(
        boolean_nested_member_path_first_outer_phi_inner_predecessor <
        boolean_nested_member_path_first_index_phi
    );
    assert(boolean_nested_member_path_first_index_phi < boolean_nested_member_path_first_outer_guard);
    auto boolean_nested_member_path_second_outer_rhs =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_nested_member_path_second_outer_branch
        );
    auto boolean_nested_member_path_second_inner_branch =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "br i1 %fallback, label %logical.and.rhs.",
            boolean_nested_member_path_second_outer_rhs
        );
    auto boolean_nested_member_path_second_guard =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_nested_member_path_second_inner_branch
        );
    auto boolean_nested_member_path_second_load =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_nested_member_path_second_guard
        );
    auto boolean_nested_member_path_second_inner_merge =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_nested_member_path_second_load
        );
    auto boolean_nested_member_path_second_inner_phi =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = phi i1 [0, %logical.or.rhs.",
            boolean_nested_member_path_second_inner_merge
        );
    auto boolean_nested_member_path_second_outer_merge =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_nested_member_path_second_inner_phi
        );
    auto boolean_nested_member_path_second_outer_phi =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = phi i1 [1, %ternary.else.",
            boolean_nested_member_path_second_outer_merge
        );
    auto boolean_nested_member_path_second_outer_phi_inner_predecessor =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            ", %logical.and.merge.",
            boolean_nested_member_path_second_outer_phi
        );
    auto boolean_nested_member_path_second_index_phi =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_nested_member_path_second_outer_phi
        );
    auto boolean_nested_member_path_second_outer_guard =
        runtime_index_boolean_nested_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_nested_member_path_second_index_phi
        );
    assert(boolean_nested_member_path_second_outer_rhs != std::string::npos);
    assert(boolean_nested_member_path_second_inner_branch != std::string::npos);
    assert(boolean_nested_member_path_second_guard != std::string::npos);
    assert(boolean_nested_member_path_second_load != std::string::npos);
    assert(boolean_nested_member_path_second_inner_merge != std::string::npos);
    assert(boolean_nested_member_path_second_inner_phi != std::string::npos);
    assert(boolean_nested_member_path_second_outer_merge != std::string::npos);
    assert(boolean_nested_member_path_second_outer_phi != std::string::npos);
    assert(boolean_nested_member_path_second_outer_phi_inner_predecessor != std::string::npos);
    assert(boolean_nested_member_path_second_index_phi != std::string::npos);
    assert(boolean_nested_member_path_second_outer_guard != std::string::npos);
    assert(boolean_nested_member_path_second_outer_branch < boolean_nested_member_path_second_outer_rhs);
    assert(boolean_nested_member_path_second_outer_rhs < boolean_nested_member_path_second_inner_branch);
    assert(boolean_nested_member_path_second_inner_branch < boolean_nested_member_path_second_guard);
    assert(boolean_nested_member_path_second_guard < boolean_nested_member_path_second_load);
    assert(boolean_nested_member_path_second_load < boolean_nested_member_path_second_inner_merge);
    assert(boolean_nested_member_path_second_inner_merge < boolean_nested_member_path_second_inner_phi);
    assert(boolean_nested_member_path_second_inner_phi < boolean_nested_member_path_second_outer_merge);
    assert(boolean_nested_member_path_second_outer_merge < boolean_nested_member_path_second_outer_phi);
    assert(
        boolean_nested_member_path_second_outer_phi <
        boolean_nested_member_path_second_outer_phi_inner_predecessor
    );
    assert(
        boolean_nested_member_path_second_outer_phi_inner_predecessor <
        boolean_nested_member_path_second_index_phi
    );
    assert(boolean_nested_member_path_second_index_phi < boolean_nested_member_path_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_nested_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_nested_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_nested_member_path_condition_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_nested_member_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_nested_and_or_member_path_condition_path);
    auto runtime_index_boolean_nested_and_or_member_path_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_nested_and_or_member_path_condition_path.string()
        );
    auto boolean_nested_and_or_first_outer_branch =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs."
        );
    assert(boolean_nested_and_or_first_outer_branch != std::string::npos);
    auto boolean_nested_and_or_second_outer_branch =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs.",
            boolean_nested_and_or_first_outer_branch + 1
        );
    assert(boolean_nested_and_or_second_outer_branch != std::string::npos);
    assert(
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs.",
            boolean_nested_and_or_second_outer_branch + 1
        ) == std::string::npos
    );
    auto boolean_nested_and_or_first_outer_rhs =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_nested_and_or_first_outer_branch
        );
    auto boolean_nested_and_or_first_inner_branch =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "br i1 %fallback, label %logical.or.merge.",
            boolean_nested_and_or_first_outer_rhs
        );
    auto boolean_nested_and_or_first_inner_rhs =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_nested_and_or_first_inner_branch
        );
    auto boolean_nested_and_or_first_guard =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_nested_and_or_first_inner_rhs
        );
    auto boolean_nested_and_or_first_load =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_nested_and_or_first_guard
        );
    auto boolean_nested_and_or_first_inner_merge =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_nested_and_or_first_load
        );
    auto boolean_nested_and_or_first_inner_phi =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [1, %logical.and.rhs.",
            boolean_nested_and_or_first_inner_merge
        );
    auto boolean_nested_and_or_first_outer_merge =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_nested_and_or_first_inner_phi
        );
    auto boolean_nested_and_or_first_outer_phi =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [0, %ternary.then.",
            boolean_nested_and_or_first_outer_merge
        );
    auto boolean_nested_and_or_first_outer_phi_inner_predecessor =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            ", %logical.or.merge.",
            boolean_nested_and_or_first_outer_phi
        );
    auto boolean_nested_and_or_first_index_phi =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_nested_and_or_first_outer_phi
        );
    auto boolean_nested_and_or_first_outer_guard =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_nested_and_or_first_index_phi
        );
    assert(boolean_nested_and_or_first_outer_rhs != std::string::npos);
    assert(boolean_nested_and_or_first_inner_branch != std::string::npos);
    assert(boolean_nested_and_or_first_inner_rhs != std::string::npos);
    assert(boolean_nested_and_or_first_guard != std::string::npos);
    assert(boolean_nested_and_or_first_load != std::string::npos);
    assert(boolean_nested_and_or_first_inner_merge != std::string::npos);
    assert(boolean_nested_and_or_first_inner_phi != std::string::npos);
    assert(boolean_nested_and_or_first_outer_merge != std::string::npos);
    assert(boolean_nested_and_or_first_outer_phi != std::string::npos);
    assert(boolean_nested_and_or_first_outer_phi_inner_predecessor != std::string::npos);
    assert(boolean_nested_and_or_first_index_phi != std::string::npos);
    assert(boolean_nested_and_or_first_outer_guard != std::string::npos);
    assert(boolean_nested_and_or_first_outer_branch < boolean_nested_and_or_first_outer_rhs);
    assert(boolean_nested_and_or_first_outer_rhs < boolean_nested_and_or_first_inner_branch);
    assert(boolean_nested_and_or_first_inner_branch < boolean_nested_and_or_first_inner_rhs);
    assert(boolean_nested_and_or_first_inner_rhs < boolean_nested_and_or_first_guard);
    assert(boolean_nested_and_or_first_guard < boolean_nested_and_or_first_load);
    assert(boolean_nested_and_or_first_load < boolean_nested_and_or_first_inner_merge);
    assert(boolean_nested_and_or_first_inner_merge < boolean_nested_and_or_first_inner_phi);
    assert(boolean_nested_and_or_first_inner_phi < boolean_nested_and_or_first_outer_merge);
    assert(boolean_nested_and_or_first_outer_merge < boolean_nested_and_or_first_outer_phi);
    assert(
        boolean_nested_and_or_first_outer_phi <
        boolean_nested_and_or_first_outer_phi_inner_predecessor
    );
    assert(
        boolean_nested_and_or_first_outer_phi_inner_predecessor <
        boolean_nested_and_or_first_index_phi
    );
    assert(boolean_nested_and_or_first_index_phi < boolean_nested_and_or_first_outer_guard);
    auto boolean_nested_and_or_second_outer_rhs =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_nested_and_or_second_outer_branch
        );
    auto boolean_nested_and_or_second_inner_branch =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "br i1 %fallback, label %logical.or.merge.",
            boolean_nested_and_or_second_outer_rhs
        );
    auto boolean_nested_and_or_second_guard =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_nested_and_or_second_inner_branch
        );
    auto boolean_nested_and_or_second_load =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_nested_and_or_second_guard
        );
    auto boolean_nested_and_or_second_inner_merge =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_nested_and_or_second_load
        );
    auto boolean_nested_and_or_second_inner_phi =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [1, %logical.and.rhs.",
            boolean_nested_and_or_second_inner_merge
        );
    auto boolean_nested_and_or_second_outer_merge =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_nested_and_or_second_inner_phi
        );
    auto boolean_nested_and_or_second_outer_phi =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [0, %ternary.else.",
            boolean_nested_and_or_second_outer_merge
        );
    auto boolean_nested_and_or_second_outer_phi_inner_predecessor =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            ", %logical.or.merge.",
            boolean_nested_and_or_second_outer_phi
        );
    auto boolean_nested_and_or_second_index_phi =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_nested_and_or_second_outer_phi
        );
    auto boolean_nested_and_or_second_outer_guard =
        runtime_index_boolean_nested_and_or_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_nested_and_or_second_index_phi
        );
    assert(boolean_nested_and_or_second_outer_rhs != std::string::npos);
    assert(boolean_nested_and_or_second_inner_branch != std::string::npos);
    assert(boolean_nested_and_or_second_guard != std::string::npos);
    assert(boolean_nested_and_or_second_load != std::string::npos);
    assert(boolean_nested_and_or_second_inner_merge != std::string::npos);
    assert(boolean_nested_and_or_second_inner_phi != std::string::npos);
    assert(boolean_nested_and_or_second_outer_merge != std::string::npos);
    assert(boolean_nested_and_or_second_outer_phi != std::string::npos);
    assert(boolean_nested_and_or_second_outer_phi_inner_predecessor != std::string::npos);
    assert(boolean_nested_and_or_second_index_phi != std::string::npos);
    assert(boolean_nested_and_or_second_outer_guard != std::string::npos);
    assert(boolean_nested_and_or_second_outer_branch < boolean_nested_and_or_second_outer_rhs);
    assert(boolean_nested_and_or_second_outer_rhs < boolean_nested_and_or_second_inner_branch);
    assert(boolean_nested_and_or_second_inner_branch < boolean_nested_and_or_second_guard);
    assert(boolean_nested_and_or_second_guard < boolean_nested_and_or_second_load);
    assert(boolean_nested_and_or_second_load < boolean_nested_and_or_second_inner_merge);
    assert(boolean_nested_and_or_second_inner_merge < boolean_nested_and_or_second_inner_phi);
    assert(boolean_nested_and_or_second_inner_phi < boolean_nested_and_or_second_outer_merge);
    assert(boolean_nested_and_or_second_outer_merge < boolean_nested_and_or_second_outer_phi);
    assert(
        boolean_nested_and_or_second_outer_phi <
        boolean_nested_and_or_second_outer_phi_inner_predecessor
    );
    assert(
        boolean_nested_and_or_second_outer_phi_inner_predecessor <
        boolean_nested_and_or_second_index_phi
    );
    assert(boolean_nested_and_or_second_index_phi < boolean_nested_and_or_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_nested_and_or_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_nested_and_or_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_nested_and_or_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_nested_and_or_member_path_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_left_nested_and_or_member_path_condition_path
    );
    auto runtime_index_boolean_left_nested_and_or_member_path_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_left_nested_and_or_member_path_condition_path.string()
        );
    auto boolean_left_nested_and_or_first_inner_branch =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs."
        );
    assert(boolean_left_nested_and_or_first_inner_branch != std::string::npos);
    auto boolean_left_nested_and_or_second_inner_branch =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs.",
            boolean_left_nested_and_or_first_inner_branch + 1
        );
    assert(boolean_left_nested_and_or_second_inner_branch != std::string::npos);
    assert(
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "br i1 %gate, label %logical.and.rhs.",
            boolean_left_nested_and_or_second_inner_branch + 1
        ) == std::string::npos
    );
    auto boolean_left_nested_and_or_first_inner_rhs =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_left_nested_and_or_first_inner_branch
        );
    auto boolean_left_nested_and_or_first_inner_merge =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_left_nested_and_or_first_inner_rhs
        );
    auto boolean_left_nested_and_or_first_inner_phi =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [0, %ternary.then.",
            boolean_left_nested_and_or_first_inner_merge
        );
    auto boolean_left_nested_and_or_first_outer_branch =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            ", label %logical.or.rhs.",
            boolean_left_nested_and_or_first_inner_phi
        );
    auto boolean_left_nested_and_or_first_outer_rhs =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_left_nested_and_or_first_outer_branch
        );
    auto boolean_left_nested_and_or_first_guard =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_left_nested_and_or_first_outer_rhs
        );
    auto boolean_left_nested_and_or_first_load =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_and_or_first_guard
        );
    auto boolean_left_nested_and_or_first_outer_merge =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_left_nested_and_or_first_load
        );
    auto boolean_left_nested_and_or_first_outer_phi =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.",
            boolean_left_nested_and_or_first_outer_merge
        );
    auto boolean_left_nested_and_or_first_index_phi =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_left_nested_and_or_first_outer_phi
        );
    auto boolean_left_nested_and_or_first_outer_guard =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_left_nested_and_or_first_index_phi
        );
    assert(boolean_left_nested_and_or_first_inner_rhs != std::string::npos);
    assert(boolean_left_nested_and_or_first_inner_merge != std::string::npos);
    assert(boolean_left_nested_and_or_first_inner_phi != std::string::npos);
    assert(boolean_left_nested_and_or_first_outer_branch != std::string::npos);
    assert(boolean_left_nested_and_or_first_outer_rhs != std::string::npos);
    assert(boolean_left_nested_and_or_first_guard != std::string::npos);
    assert(boolean_left_nested_and_or_first_load != std::string::npos);
    assert(boolean_left_nested_and_or_first_outer_merge != std::string::npos);
    assert(boolean_left_nested_and_or_first_outer_phi != std::string::npos);
    assert(boolean_left_nested_and_or_first_index_phi != std::string::npos);
    assert(boolean_left_nested_and_or_first_outer_guard != std::string::npos);
    assert(boolean_left_nested_and_or_first_inner_branch < boolean_left_nested_and_or_first_inner_rhs);
    assert(boolean_left_nested_and_or_first_inner_rhs < boolean_left_nested_and_or_first_inner_merge);
    assert(boolean_left_nested_and_or_first_inner_merge < boolean_left_nested_and_or_first_inner_phi);
    assert(boolean_left_nested_and_or_first_inner_phi < boolean_left_nested_and_or_first_outer_branch);
    assert(boolean_left_nested_and_or_first_outer_branch < boolean_left_nested_and_or_first_outer_rhs);
    assert(boolean_left_nested_and_or_first_outer_rhs < boolean_left_nested_and_or_first_guard);
    assert(boolean_left_nested_and_or_first_guard < boolean_left_nested_and_or_first_load);
    assert(boolean_left_nested_and_or_first_load < boolean_left_nested_and_or_first_outer_merge);
    assert(boolean_left_nested_and_or_first_outer_merge < boolean_left_nested_and_or_first_outer_phi);
    assert(boolean_left_nested_and_or_first_outer_phi < boolean_left_nested_and_or_first_index_phi);
    assert(boolean_left_nested_and_or_first_index_phi < boolean_left_nested_and_or_first_outer_guard);
    auto boolean_left_nested_and_or_second_inner_rhs =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_left_nested_and_or_second_inner_branch
        );
    auto boolean_left_nested_and_or_second_inner_merge =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_left_nested_and_or_second_inner_rhs
        );
    auto boolean_left_nested_and_or_second_inner_phi =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [0, %ternary.else.",
            boolean_left_nested_and_or_second_inner_merge
        );
    auto boolean_left_nested_and_or_second_outer_branch =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            ", label %logical.or.rhs.",
            boolean_left_nested_and_or_second_inner_phi
        );
    auto boolean_left_nested_and_or_second_outer_rhs =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_left_nested_and_or_second_outer_branch
        );
    auto boolean_left_nested_and_or_second_guard =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_left_nested_and_or_second_outer_rhs
        );
    auto boolean_left_nested_and_or_second_load =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_and_or_second_guard
        );
    auto boolean_left_nested_and_or_second_outer_merge =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_left_nested_and_or_second_load
        );
    auto boolean_left_nested_and_or_second_outer_phi =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.",
            boolean_left_nested_and_or_second_outer_merge
        );
    auto boolean_left_nested_and_or_second_index_phi =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_left_nested_and_or_second_outer_phi
        );
    auto boolean_left_nested_and_or_second_outer_guard =
        runtime_index_boolean_left_nested_and_or_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_left_nested_and_or_second_index_phi
        );
    assert(boolean_left_nested_and_or_second_inner_rhs != std::string::npos);
    assert(boolean_left_nested_and_or_second_inner_merge != std::string::npos);
    assert(boolean_left_nested_and_or_second_inner_phi != std::string::npos);
    assert(boolean_left_nested_and_or_second_outer_branch != std::string::npos);
    assert(boolean_left_nested_and_or_second_outer_rhs != std::string::npos);
    assert(boolean_left_nested_and_or_second_guard != std::string::npos);
    assert(boolean_left_nested_and_or_second_load != std::string::npos);
    assert(boolean_left_nested_and_or_second_outer_merge != std::string::npos);
    assert(boolean_left_nested_and_or_second_outer_phi != std::string::npos);
    assert(boolean_left_nested_and_or_second_index_phi != std::string::npos);
    assert(boolean_left_nested_and_or_second_outer_guard != std::string::npos);
    assert(boolean_left_nested_and_or_second_inner_branch < boolean_left_nested_and_or_second_inner_rhs);
    assert(boolean_left_nested_and_or_second_inner_rhs < boolean_left_nested_and_or_second_inner_merge);
    assert(boolean_left_nested_and_or_second_inner_merge < boolean_left_nested_and_or_second_inner_phi);
    assert(boolean_left_nested_and_or_second_inner_phi < boolean_left_nested_and_or_second_outer_branch);
    assert(boolean_left_nested_and_or_second_outer_branch < boolean_left_nested_and_or_second_outer_rhs);
    assert(boolean_left_nested_and_or_second_outer_rhs < boolean_left_nested_and_or_second_guard);
    assert(boolean_left_nested_and_or_second_guard < boolean_left_nested_and_or_second_load);
    assert(boolean_left_nested_and_or_second_load < boolean_left_nested_and_or_second_outer_merge);
    assert(boolean_left_nested_and_or_second_outer_merge < boolean_left_nested_and_or_second_outer_phi);
    assert(boolean_left_nested_and_or_second_outer_phi < boolean_left_nested_and_or_second_index_phi);
    assert(boolean_left_nested_and_or_second_index_phi < boolean_left_nested_and_or_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_left_nested_and_or_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_left_nested_and_or_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_left_nested_and_or_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_left_nested_and_or_member_path_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_left_nested_or_and_member_path_condition_path
    );
    auto runtime_index_boolean_left_nested_or_and_member_path_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_left_nested_or_and_member_path_condition_path.string()
        );
    auto boolean_left_nested_or_and_first_inner_branch =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge."
        );
    assert(boolean_left_nested_or_and_first_inner_branch != std::string::npos);
    auto boolean_left_nested_or_and_second_inner_branch =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge.",
            boolean_left_nested_or_and_first_inner_branch + 1
        );
    assert(boolean_left_nested_or_and_second_inner_branch != std::string::npos);
    assert(
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "br i1 %gate, label %logical.or.merge.",
            boolean_left_nested_or_and_second_inner_branch + 1
        ) == std::string::npos
    );
    auto boolean_left_nested_or_and_first_inner_merge =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_left_nested_or_and_first_inner_branch
        );
    auto boolean_left_nested_or_and_first_inner_phi =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = phi i1 [1, %ternary.then.",
            boolean_left_nested_or_and_first_inner_merge
        );
    auto boolean_left_nested_or_and_first_outer_branch =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_left_nested_or_and_first_inner_phi
        );
    auto boolean_left_nested_or_and_first_outer_rhs =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_left_nested_or_and_first_outer_branch
        );
    auto boolean_left_nested_or_and_first_guard =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_left_nested_or_and_first_outer_rhs
        );
    auto boolean_left_nested_or_and_first_load =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_or_and_first_guard
        );
    auto boolean_left_nested_or_and_first_outer_merge =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_left_nested_or_and_first_load
        );
    auto boolean_left_nested_or_and_first_outer_phi =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.",
            boolean_left_nested_or_and_first_outer_merge
        );
    auto boolean_left_nested_or_and_first_index_phi =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_left_nested_or_and_first_outer_phi
        );
    auto boolean_left_nested_or_and_first_owner_guard =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_left_nested_or_and_first_index_phi
        );
    assert(boolean_left_nested_or_and_first_inner_merge != std::string::npos);
    assert(boolean_left_nested_or_and_first_inner_phi != std::string::npos);
    assert(boolean_left_nested_or_and_first_outer_branch != std::string::npos);
    assert(boolean_left_nested_or_and_first_outer_rhs != std::string::npos);
    assert(boolean_left_nested_or_and_first_guard != std::string::npos);
    assert(boolean_left_nested_or_and_first_load != std::string::npos);
    assert(boolean_left_nested_or_and_first_outer_merge != std::string::npos);
    assert(boolean_left_nested_or_and_first_outer_phi != std::string::npos);
    assert(boolean_left_nested_or_and_first_index_phi != std::string::npos);
    assert(boolean_left_nested_or_and_first_owner_guard != std::string::npos);
    assert(boolean_left_nested_or_and_first_inner_branch < boolean_left_nested_or_and_first_inner_merge);
    assert(boolean_left_nested_or_and_first_inner_merge < boolean_left_nested_or_and_first_inner_phi);
    assert(boolean_left_nested_or_and_first_inner_phi < boolean_left_nested_or_and_first_outer_branch);
    assert(boolean_left_nested_or_and_first_outer_branch < boolean_left_nested_or_and_first_outer_rhs);
    assert(boolean_left_nested_or_and_first_outer_rhs < boolean_left_nested_or_and_first_guard);
    assert(boolean_left_nested_or_and_first_guard < boolean_left_nested_or_and_first_load);
    assert(boolean_left_nested_or_and_first_load < boolean_left_nested_or_and_first_outer_merge);
    assert(boolean_left_nested_or_and_first_outer_merge < boolean_left_nested_or_and_first_outer_phi);
    assert(boolean_left_nested_or_and_first_outer_phi < boolean_left_nested_or_and_first_index_phi);
    assert(boolean_left_nested_or_and_first_index_phi < boolean_left_nested_or_and_first_owner_guard);
    auto boolean_left_nested_or_and_second_inner_merge =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_left_nested_or_and_second_inner_branch
        );
    auto boolean_left_nested_or_and_second_inner_phi =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = phi i1 [1, %ternary.else.",
            boolean_left_nested_or_and_second_inner_merge
        );
    auto boolean_left_nested_or_and_second_outer_branch =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_left_nested_or_and_second_inner_phi
        );
    auto boolean_left_nested_or_and_second_outer_rhs =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_left_nested_or_and_second_outer_branch
        );
    auto boolean_left_nested_or_and_second_guard =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_left_nested_or_and_second_outer_rhs
        );
    auto boolean_left_nested_or_and_second_load =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_or_and_second_guard
        );
    auto boolean_left_nested_or_and_second_outer_merge =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_left_nested_or_and_second_load
        );
    auto boolean_left_nested_or_and_second_outer_phi =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.",
            boolean_left_nested_or_and_second_outer_merge
        );
    auto boolean_left_nested_or_and_second_index_phi =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_left_nested_or_and_second_outer_phi
        );
    auto boolean_left_nested_or_and_second_owner_guard =
        runtime_index_boolean_left_nested_or_and_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_left_nested_or_and_second_index_phi
        );
    assert(boolean_left_nested_or_and_second_inner_merge != std::string::npos);
    assert(boolean_left_nested_or_and_second_inner_phi != std::string::npos);
    assert(boolean_left_nested_or_and_second_outer_branch != std::string::npos);
    assert(boolean_left_nested_or_and_second_outer_rhs != std::string::npos);
    assert(boolean_left_nested_or_and_second_guard != std::string::npos);
    assert(boolean_left_nested_or_and_second_load != std::string::npos);
    assert(boolean_left_nested_or_and_second_outer_merge != std::string::npos);
    assert(boolean_left_nested_or_and_second_outer_phi != std::string::npos);
    assert(boolean_left_nested_or_and_second_index_phi != std::string::npos);
    assert(boolean_left_nested_or_and_second_owner_guard != std::string::npos);
    assert(boolean_left_nested_or_and_second_inner_branch < boolean_left_nested_or_and_second_inner_merge);
    assert(boolean_left_nested_or_and_second_inner_merge < boolean_left_nested_or_and_second_inner_phi);
    assert(boolean_left_nested_or_and_second_inner_phi < boolean_left_nested_or_and_second_outer_branch);
    assert(boolean_left_nested_or_and_second_outer_branch < boolean_left_nested_or_and_second_outer_rhs);
    assert(boolean_left_nested_or_and_second_outer_rhs < boolean_left_nested_or_and_second_guard);
    assert(boolean_left_nested_or_and_second_guard < boolean_left_nested_or_and_second_load);
    assert(boolean_left_nested_or_and_second_load < boolean_left_nested_or_and_second_outer_merge);
    assert(boolean_left_nested_or_and_second_outer_merge < boolean_left_nested_or_and_second_outer_phi);
    assert(boolean_left_nested_or_and_second_outer_phi < boolean_left_nested_or_and_second_index_phi);
    assert(boolean_left_nested_or_and_second_index_phi < boolean_left_nested_or_and_second_owner_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_left_nested_or_and_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_left_nested_or_and_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_left_nested_or_and_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_left_nested_or_and_member_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_two_member_paths_and_condition_path);
    auto runtime_index_boolean_two_member_paths_and_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_boolean_two_member_paths_and_condition_path.string()
    );
    auto boolean_two_member_paths_first_first_guard =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = icmp ult i64 %first_slot, 2"
        );
    assert(boolean_two_member_paths_first_first_guard != std::string::npos);
    auto boolean_two_member_paths_second_first_guard =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = icmp ult i64 %first_slot, 2",
            boolean_two_member_paths_first_first_guard + 1
        );
    assert(boolean_two_member_paths_second_first_guard != std::string::npos);
    assert(
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = icmp ult i64 %first_slot, 2",
            boolean_two_member_paths_second_first_guard + 1
        ) == std::string::npos
    );
    auto boolean_two_member_paths_first_second_guard =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = icmp ult i64 %second_slot, 2"
        );
    assert(boolean_two_member_paths_first_second_guard != std::string::npos);
    auto boolean_two_member_paths_second_second_guard =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = icmp ult i64 %second_slot, 2",
            boolean_two_member_paths_first_second_guard + 1
        );
    assert(boolean_two_member_paths_second_second_guard != std::string::npos);
    assert(
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = icmp ult i64 %second_slot, 2",
            boolean_two_member_paths_second_second_guard + 1
        ) == std::string::npos
    );
    auto boolean_two_member_paths_first_first_load =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_paths_first_first_guard
        );
    auto boolean_two_member_paths_first_branch =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_two_member_paths_first_first_load
        );
    auto boolean_two_member_paths_first_rhs =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_two_member_paths_first_branch
        );
    auto boolean_two_member_paths_first_second_load =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_paths_first_second_guard
        );
    auto boolean_two_member_paths_first_merge =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_two_member_paths_first_second_load
        );
    auto boolean_two_member_paths_first_phi =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_two_member_paths_first_merge
        );
    auto boolean_two_member_paths_first_index_phi =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_two_member_paths_first_phi
        );
    auto boolean_two_member_paths_first_owner_guard =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_two_member_paths_first_index_phi
        );
    assert(boolean_two_member_paths_first_first_load != std::string::npos);
    assert(boolean_two_member_paths_first_branch != std::string::npos);
    assert(boolean_two_member_paths_first_rhs != std::string::npos);
    assert(boolean_two_member_paths_first_second_load != std::string::npos);
    assert(boolean_two_member_paths_first_merge != std::string::npos);
    assert(boolean_two_member_paths_first_phi != std::string::npos);
    assert(boolean_two_member_paths_first_index_phi != std::string::npos);
    assert(boolean_two_member_paths_first_owner_guard != std::string::npos);
    assert(boolean_two_member_paths_first_first_guard < boolean_two_member_paths_first_first_load);
    assert(boolean_two_member_paths_first_first_load < boolean_two_member_paths_first_branch);
    assert(boolean_two_member_paths_first_branch < boolean_two_member_paths_first_rhs);
    assert(boolean_two_member_paths_first_rhs < boolean_two_member_paths_first_second_guard);
    assert(boolean_two_member_paths_first_second_guard < boolean_two_member_paths_first_second_load);
    assert(boolean_two_member_paths_first_second_load < boolean_two_member_paths_first_merge);
    assert(boolean_two_member_paths_first_merge < boolean_two_member_paths_first_phi);
    assert(boolean_two_member_paths_first_phi < boolean_two_member_paths_first_index_phi);
    assert(boolean_two_member_paths_first_index_phi < boolean_two_member_paths_first_owner_guard);
    auto boolean_two_member_paths_second_first_load =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_paths_second_first_guard
        );
    auto boolean_two_member_paths_second_branch =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_two_member_paths_second_first_load
        );
    auto boolean_two_member_paths_second_rhs =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_two_member_paths_second_branch
        );
    auto boolean_two_member_paths_second_second_load =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_paths_second_second_guard
        );
    auto boolean_two_member_paths_second_merge =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_two_member_paths_second_second_load
        );
    auto boolean_two_member_paths_second_phi =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_two_member_paths_second_merge
        );
    auto boolean_two_member_paths_second_index_phi =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_two_member_paths_second_phi
        );
    auto boolean_two_member_paths_second_owner_guard =
        runtime_index_boolean_two_member_paths_and_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_two_member_paths_second_index_phi
        );
    assert(boolean_two_member_paths_second_first_load != std::string::npos);
    assert(boolean_two_member_paths_second_branch != std::string::npos);
    assert(boolean_two_member_paths_second_rhs != std::string::npos);
    assert(boolean_two_member_paths_second_second_load != std::string::npos);
    assert(boolean_two_member_paths_second_merge != std::string::npos);
    assert(boolean_two_member_paths_second_phi != std::string::npos);
    assert(boolean_two_member_paths_second_index_phi != std::string::npos);
    assert(boolean_two_member_paths_second_owner_guard != std::string::npos);
    assert(boolean_two_member_paths_second_first_guard < boolean_two_member_paths_second_first_load);
    assert(boolean_two_member_paths_second_first_load < boolean_two_member_paths_second_branch);
    assert(boolean_two_member_paths_second_branch < boolean_two_member_paths_second_rhs);
    assert(boolean_two_member_paths_second_rhs < boolean_two_member_paths_second_second_guard);
    assert(boolean_two_member_paths_second_second_guard < boolean_two_member_paths_second_second_load);
    assert(boolean_two_member_paths_second_second_load < boolean_two_member_paths_second_merge);
    assert(boolean_two_member_paths_second_merge < boolean_two_member_paths_second_phi);
    assert(boolean_two_member_paths_second_phi < boolean_two_member_paths_second_index_phi);
    assert(boolean_two_member_paths_second_index_phi < boolean_two_member_paths_second_owner_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_two_member_paths_and_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_member_paths_and_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_two_member_paths_and_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_member_paths_and_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_two_member_paths_or_condition_path);
    auto runtime_index_boolean_two_member_paths_or_condition_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_boolean_two_member_paths_or_condition_path.string()
    );
    auto boolean_two_member_or_paths_first_first_guard =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = icmp ult i64 %first_slot, 2"
        );
    assert(boolean_two_member_or_paths_first_first_guard != std::string::npos);
    auto boolean_two_member_or_paths_second_first_guard =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = icmp ult i64 %first_slot, 2",
            boolean_two_member_or_paths_first_first_guard + 1
        );
    assert(boolean_two_member_or_paths_second_first_guard != std::string::npos);
    assert(
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = icmp ult i64 %first_slot, 2",
            boolean_two_member_or_paths_second_first_guard + 1
        ) == std::string::npos
    );
    auto boolean_two_member_or_paths_first_second_guard =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = icmp ult i64 %second_slot, 2"
        );
    assert(boolean_two_member_or_paths_first_second_guard != std::string::npos);
    auto boolean_two_member_or_paths_second_second_guard =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = icmp ult i64 %second_slot, 2",
            boolean_two_member_or_paths_first_second_guard + 1
        );
    assert(boolean_two_member_or_paths_second_second_guard != std::string::npos);
    assert(
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = icmp ult i64 %second_slot, 2",
            boolean_two_member_or_paths_second_second_guard + 1
        ) == std::string::npos
    );
    auto boolean_two_member_or_paths_first_first_load =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_or_paths_first_first_guard
        );
    auto boolean_two_member_or_paths_first_branch =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "label %logical.or.merge.",
            boolean_two_member_or_paths_first_first_load
        );
    auto boolean_two_member_or_paths_first_rhs =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_two_member_or_paths_first_branch
        );
    auto boolean_two_member_or_paths_first_second_load =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_or_paths_first_second_guard
        );
    auto boolean_two_member_or_paths_first_merge =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_two_member_or_paths_first_second_load
        );
    auto boolean_two_member_or_paths_first_phi =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_two_member_or_paths_first_merge
        );
    auto boolean_two_member_or_paths_first_index_phi =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_two_member_or_paths_first_phi
        );
    auto boolean_two_member_or_paths_first_owner_guard =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_two_member_or_paths_first_index_phi
        );
    assert(boolean_two_member_or_paths_first_first_load != std::string::npos);
    assert(boolean_two_member_or_paths_first_branch != std::string::npos);
    assert(boolean_two_member_or_paths_first_rhs != std::string::npos);
    assert(boolean_two_member_or_paths_first_second_load != std::string::npos);
    assert(boolean_two_member_or_paths_first_merge != std::string::npos);
    assert(boolean_two_member_or_paths_first_phi != std::string::npos);
    assert(boolean_two_member_or_paths_first_index_phi != std::string::npos);
    assert(boolean_two_member_or_paths_first_owner_guard != std::string::npos);
    assert(boolean_two_member_or_paths_first_first_guard < boolean_two_member_or_paths_first_first_load);
    assert(boolean_two_member_or_paths_first_first_load < boolean_two_member_or_paths_first_branch);
    assert(boolean_two_member_or_paths_first_branch < boolean_two_member_or_paths_first_rhs);
    assert(boolean_two_member_or_paths_first_rhs < boolean_two_member_or_paths_first_second_guard);
    assert(boolean_two_member_or_paths_first_second_guard < boolean_two_member_or_paths_first_second_load);
    assert(boolean_two_member_or_paths_first_second_load < boolean_two_member_or_paths_first_merge);
    assert(boolean_two_member_or_paths_first_merge < boolean_two_member_or_paths_first_phi);
    assert(boolean_two_member_or_paths_first_phi < boolean_two_member_or_paths_first_index_phi);
    assert(boolean_two_member_or_paths_first_index_phi < boolean_two_member_or_paths_first_owner_guard);
    auto boolean_two_member_or_paths_second_first_load =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_or_paths_second_first_guard
        );
    auto boolean_two_member_or_paths_second_branch =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "label %logical.or.merge.",
            boolean_two_member_or_paths_second_first_load
        );
    auto boolean_two_member_or_paths_second_rhs =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_two_member_or_paths_second_branch
        );
    auto boolean_two_member_or_paths_second_second_load =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = load i1, ptr ",
            boolean_two_member_or_paths_second_second_guard
        );
    auto boolean_two_member_or_paths_second_merge =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_two_member_or_paths_second_second_load
        );
    auto boolean_two_member_or_paths_second_phi =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_two_member_or_paths_second_merge
        );
    auto boolean_two_member_or_paths_second_index_phi =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_two_member_or_paths_second_phi
        );
    auto boolean_two_member_or_paths_second_owner_guard =
        runtime_index_boolean_two_member_paths_or_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_two_member_or_paths_second_index_phi
        );
    assert(boolean_two_member_or_paths_second_first_load != std::string::npos);
    assert(boolean_two_member_or_paths_second_branch != std::string::npos);
    assert(boolean_two_member_or_paths_second_rhs != std::string::npos);
    assert(boolean_two_member_or_paths_second_second_load != std::string::npos);
    assert(boolean_two_member_or_paths_second_merge != std::string::npos);
    assert(boolean_two_member_or_paths_second_phi != std::string::npos);
    assert(boolean_two_member_or_paths_second_index_phi != std::string::npos);
    assert(boolean_two_member_or_paths_second_owner_guard != std::string::npos);
    assert(boolean_two_member_or_paths_second_first_guard < boolean_two_member_or_paths_second_first_load);
    assert(boolean_two_member_or_paths_second_first_load < boolean_two_member_or_paths_second_branch);
    assert(boolean_two_member_or_paths_second_branch < boolean_two_member_or_paths_second_rhs);
    assert(boolean_two_member_or_paths_second_rhs < boolean_two_member_or_paths_second_second_guard);
    assert(boolean_two_member_or_paths_second_second_guard < boolean_two_member_or_paths_second_second_load);
    assert(boolean_two_member_or_paths_second_second_load < boolean_two_member_or_paths_second_merge);
    assert(boolean_two_member_or_paths_second_merge < boolean_two_member_or_paths_second_phi);
    assert(boolean_two_member_or_paths_second_phi < boolean_two_member_or_paths_second_index_phi);
    assert(boolean_two_member_or_paths_second_index_phi < boolean_two_member_or_paths_second_owner_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_two_member_paths_or_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_member_paths_or_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_two_member_paths_or_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_member_paths_or_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_three_member_paths_nested_condition_path);
    auto runtime_index_boolean_three_member_paths_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_member_paths_nested_condition_path.string()
        );
    auto boolean_three_paths_first_first_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    assert(boolean_three_paths_first_first_guard != std::string::npos);
    auto boolean_three_paths_second_first_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_three_paths_first_first_guard + 1
        );
    assert(boolean_three_paths_second_first_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_three_paths_second_first_guard + 1
        ) == std::string::npos
    );
    auto boolean_three_paths_first_second_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    assert(boolean_three_paths_first_second_guard != std::string::npos);
    auto boolean_three_paths_second_second_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_three_paths_first_second_guard + 1
        );
    assert(boolean_three_paths_second_second_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_three_paths_second_second_guard + 1
        ) == std::string::npos
    );
    auto boolean_three_paths_first_third_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    assert(boolean_three_paths_first_third_guard != std::string::npos);
    auto boolean_three_paths_second_third_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_three_paths_first_third_guard + 1
        );
    assert(boolean_three_paths_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_three_paths_second_third_guard + 1
        ) == std::string::npos
    );
    auto boolean_three_paths_first_first_load =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_three_paths_first_first_guard
        );
    auto boolean_three_paths_first_outer_branch =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "label %logical.or.merge.",
            boolean_three_paths_first_first_load
        );
    auto boolean_three_paths_first_outer_rhs =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_three_paths_first_outer_branch
        );
    auto boolean_three_paths_first_second_load =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_three_paths_first_second_guard
        );
    auto boolean_three_paths_first_inner_branch =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_three_paths_first_second_load
        );
    auto boolean_three_paths_first_inner_rhs =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_three_paths_first_inner_branch
        );
    auto boolean_three_paths_first_third_load =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_three_paths_first_third_guard
        );
    auto boolean_three_paths_first_inner_merge =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_three_paths_first_third_load
        );
    auto boolean_three_paths_first_inner_phi =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_three_paths_first_inner_merge
        );
    auto boolean_three_paths_first_outer_merge =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_three_paths_first_inner_phi
        );
    auto boolean_three_paths_first_outer_phi =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_three_paths_first_outer_merge
        );
    auto boolean_three_paths_first_inner_predecessor =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            ", %logical.and.merge.",
            boolean_three_paths_first_outer_phi
        );
    auto boolean_three_paths_first_index_phi =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_three_paths_first_outer_phi
        );
    auto boolean_three_paths_first_owner_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_three_paths_first_index_phi
        );
    assert(boolean_three_paths_first_first_load != std::string::npos);
    assert(boolean_three_paths_first_outer_branch != std::string::npos);
    assert(boolean_three_paths_first_outer_rhs != std::string::npos);
    assert(boolean_three_paths_first_second_load != std::string::npos);
    assert(boolean_three_paths_first_inner_branch != std::string::npos);
    assert(boolean_three_paths_first_inner_rhs != std::string::npos);
    assert(boolean_three_paths_first_third_load != std::string::npos);
    assert(boolean_three_paths_first_inner_merge != std::string::npos);
    assert(boolean_three_paths_first_inner_phi != std::string::npos);
    assert(boolean_three_paths_first_outer_merge != std::string::npos);
    assert(boolean_three_paths_first_outer_phi != std::string::npos);
    assert(boolean_three_paths_first_inner_predecessor != std::string::npos);
    assert(boolean_three_paths_first_index_phi != std::string::npos);
    assert(boolean_three_paths_first_owner_guard != std::string::npos);
    assert(boolean_three_paths_first_first_guard < boolean_three_paths_first_first_load);
    assert(boolean_three_paths_first_first_load < boolean_three_paths_first_outer_branch);
    assert(boolean_three_paths_first_outer_branch < boolean_three_paths_first_outer_rhs);
    assert(boolean_three_paths_first_outer_rhs < boolean_three_paths_first_second_guard);
    assert(boolean_three_paths_first_second_guard < boolean_three_paths_first_second_load);
    assert(boolean_three_paths_first_second_load < boolean_three_paths_first_inner_branch);
    assert(boolean_three_paths_first_inner_branch < boolean_three_paths_first_inner_rhs);
    assert(boolean_three_paths_first_inner_rhs < boolean_three_paths_first_third_guard);
    assert(boolean_three_paths_first_third_guard < boolean_three_paths_first_third_load);
    assert(boolean_three_paths_first_third_load < boolean_three_paths_first_inner_merge);
    assert(boolean_three_paths_first_inner_merge < boolean_three_paths_first_inner_phi);
    assert(boolean_three_paths_first_inner_phi < boolean_three_paths_first_outer_merge);
    assert(boolean_three_paths_first_outer_merge < boolean_three_paths_first_outer_phi);
    assert(boolean_three_paths_first_outer_phi < boolean_three_paths_first_inner_predecessor);
    assert(boolean_three_paths_first_inner_predecessor < boolean_three_paths_first_index_phi);
    assert(boolean_three_paths_first_index_phi < boolean_three_paths_first_owner_guard);
    auto boolean_three_paths_second_first_load =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_three_paths_second_first_guard
        );
    auto boolean_three_paths_second_outer_branch =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "label %logical.or.merge.",
            boolean_three_paths_second_first_load
        );
    auto boolean_three_paths_second_outer_rhs =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_three_paths_second_outer_branch
        );
    auto boolean_three_paths_second_second_load =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_three_paths_second_second_guard
        );
    auto boolean_three_paths_second_inner_branch =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_three_paths_second_second_load
        );
    auto boolean_three_paths_second_inner_rhs =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_three_paths_second_inner_branch
        );
    auto boolean_three_paths_second_third_load =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_three_paths_second_third_guard
        );
    auto boolean_three_paths_second_inner_merge =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_three_paths_second_third_load
        );
    auto boolean_three_paths_second_inner_phi =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_three_paths_second_inner_merge
        );
    auto boolean_three_paths_second_outer_merge =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_three_paths_second_inner_phi
        );
    auto boolean_three_paths_second_outer_phi =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_three_paths_second_outer_merge
        );
    auto boolean_three_paths_second_inner_predecessor =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            ", %logical.and.merge.",
            boolean_three_paths_second_outer_phi
        );
    auto boolean_three_paths_second_index_phi =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_three_paths_second_outer_phi
        );
    auto boolean_three_paths_second_owner_guard =
        runtime_index_boolean_three_member_paths_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_three_paths_second_index_phi
        );
    assert(boolean_three_paths_second_first_load != std::string::npos);
    assert(boolean_three_paths_second_outer_branch != std::string::npos);
    assert(boolean_three_paths_second_outer_rhs != std::string::npos);
    assert(boolean_three_paths_second_second_load != std::string::npos);
    assert(boolean_three_paths_second_inner_branch != std::string::npos);
    assert(boolean_three_paths_second_inner_rhs != std::string::npos);
    assert(boolean_three_paths_second_third_load != std::string::npos);
    assert(boolean_three_paths_second_inner_merge != std::string::npos);
    assert(boolean_three_paths_second_inner_phi != std::string::npos);
    assert(boolean_three_paths_second_outer_merge != std::string::npos);
    assert(boolean_three_paths_second_outer_phi != std::string::npos);
    assert(boolean_three_paths_second_inner_predecessor != std::string::npos);
    assert(boolean_three_paths_second_index_phi != std::string::npos);
    assert(boolean_three_paths_second_owner_guard != std::string::npos);
    assert(boolean_three_paths_second_first_guard < boolean_three_paths_second_first_load);
    assert(boolean_three_paths_second_first_load < boolean_three_paths_second_outer_branch);
    assert(boolean_three_paths_second_outer_branch < boolean_three_paths_second_outer_rhs);
    assert(boolean_three_paths_second_outer_rhs < boolean_three_paths_second_second_guard);
    assert(boolean_three_paths_second_second_guard < boolean_three_paths_second_second_load);
    assert(boolean_three_paths_second_second_load < boolean_three_paths_second_inner_branch);
    assert(boolean_three_paths_second_inner_branch < boolean_three_paths_second_inner_rhs);
    assert(boolean_three_paths_second_inner_rhs < boolean_three_paths_second_third_guard);
    assert(boolean_three_paths_second_third_guard < boolean_three_paths_second_third_load);
    assert(boolean_three_paths_second_third_load < boolean_three_paths_second_inner_merge);
    assert(boolean_three_paths_second_inner_merge < boolean_three_paths_second_inner_phi);
    assert(boolean_three_paths_second_inner_phi < boolean_three_paths_second_outer_merge);
    assert(boolean_three_paths_second_outer_merge < boolean_three_paths_second_outer_phi);
    assert(boolean_three_paths_second_outer_phi < boolean_three_paths_second_inner_predecessor);
    assert(boolean_three_paths_second_inner_predecessor < boolean_three_paths_second_index_phi);
    assert(boolean_three_paths_second_index_phi < boolean_three_paths_second_owner_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_member_paths_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_member_paths_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_nested_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_path
    );
    auto runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_member_paths_symmetric_nested_condition_path.string()
        );
    auto boolean_symmetric_three_paths_first_first_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    assert(boolean_symmetric_three_paths_first_first_guard != std::string::npos);
    auto boolean_symmetric_three_paths_second_first_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_symmetric_three_paths_first_first_guard + 1
        );
    assert(boolean_symmetric_three_paths_second_first_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_symmetric_three_paths_second_first_guard + 1
        ) == std::string::npos
    );
    auto boolean_symmetric_three_paths_first_second_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    assert(boolean_symmetric_three_paths_first_second_guard != std::string::npos);
    auto boolean_symmetric_three_paths_second_second_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_symmetric_three_paths_first_second_guard + 1
        );
    assert(boolean_symmetric_three_paths_second_second_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_symmetric_three_paths_second_second_guard + 1
        ) == std::string::npos
    );
    auto boolean_symmetric_three_paths_first_third_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    assert(boolean_symmetric_three_paths_first_third_guard != std::string::npos);
    auto boolean_symmetric_three_paths_second_third_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_symmetric_three_paths_first_third_guard + 1
        );
    assert(boolean_symmetric_three_paths_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_symmetric_three_paths_second_third_guard + 1
        ) == std::string::npos
    );
    auto boolean_symmetric_three_paths_first_first_load =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_symmetric_three_paths_first_first_guard
        );
    auto boolean_symmetric_three_paths_first_outer_branch =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_symmetric_three_paths_first_first_load
        );
    auto boolean_symmetric_three_paths_first_outer_rhs =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_symmetric_three_paths_first_outer_branch
        );
    auto boolean_symmetric_three_paths_first_second_load =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_symmetric_three_paths_first_second_guard
        );
    auto boolean_symmetric_three_paths_first_inner_branch =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "label %logical.or.merge.",
            boolean_symmetric_three_paths_first_second_load
        );
    auto boolean_symmetric_three_paths_first_inner_rhs =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_symmetric_three_paths_first_inner_branch
        );
    auto boolean_symmetric_three_paths_first_third_load =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_symmetric_three_paths_first_third_guard
        );
    auto boolean_symmetric_three_paths_first_inner_merge =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_symmetric_three_paths_first_third_load
        );
    auto boolean_symmetric_three_paths_first_inner_phi =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_symmetric_three_paths_first_inner_merge
        );
    auto boolean_symmetric_three_paths_first_outer_merge =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_symmetric_three_paths_first_inner_phi
        );
    auto boolean_symmetric_three_paths_first_outer_phi =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_symmetric_three_paths_first_outer_merge
        );
    auto boolean_symmetric_three_paths_first_inner_predecessor =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            ", %logical.or.merge.",
            boolean_symmetric_three_paths_first_outer_phi
        );
    auto boolean_symmetric_three_paths_first_index_phi =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_symmetric_three_paths_first_outer_phi
        );
    auto boolean_symmetric_three_paths_first_owner_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_symmetric_three_paths_first_index_phi
        );
    assert(boolean_symmetric_three_paths_first_first_load != std::string::npos);
    assert(boolean_symmetric_three_paths_first_outer_branch != std::string::npos);
    assert(boolean_symmetric_three_paths_first_outer_rhs != std::string::npos);
    assert(boolean_symmetric_three_paths_first_second_load != std::string::npos);
    assert(boolean_symmetric_three_paths_first_inner_branch != std::string::npos);
    assert(boolean_symmetric_three_paths_first_inner_rhs != std::string::npos);
    assert(boolean_symmetric_three_paths_first_third_load != std::string::npos);
    assert(boolean_symmetric_three_paths_first_inner_merge != std::string::npos);
    assert(boolean_symmetric_three_paths_first_inner_phi != std::string::npos);
    assert(boolean_symmetric_three_paths_first_outer_merge != std::string::npos);
    assert(boolean_symmetric_three_paths_first_outer_phi != std::string::npos);
    assert(boolean_symmetric_three_paths_first_inner_predecessor != std::string::npos);
    assert(boolean_symmetric_three_paths_first_index_phi != std::string::npos);
    assert(boolean_symmetric_three_paths_first_owner_guard != std::string::npos);
    assert(boolean_symmetric_three_paths_first_first_guard < boolean_symmetric_three_paths_first_first_load);
    assert(boolean_symmetric_three_paths_first_first_load < boolean_symmetric_three_paths_first_outer_branch);
    assert(boolean_symmetric_three_paths_first_outer_branch < boolean_symmetric_three_paths_first_outer_rhs);
    assert(boolean_symmetric_three_paths_first_outer_rhs < boolean_symmetric_three_paths_first_second_guard);
    assert(boolean_symmetric_three_paths_first_second_guard < boolean_symmetric_three_paths_first_second_load);
    assert(boolean_symmetric_three_paths_first_second_load < boolean_symmetric_three_paths_first_inner_branch);
    assert(boolean_symmetric_three_paths_first_inner_branch < boolean_symmetric_three_paths_first_inner_rhs);
    assert(boolean_symmetric_three_paths_first_inner_rhs < boolean_symmetric_three_paths_first_third_guard);
    assert(boolean_symmetric_three_paths_first_third_guard < boolean_symmetric_three_paths_first_third_load);
    assert(boolean_symmetric_three_paths_first_third_load < boolean_symmetric_three_paths_first_inner_merge);
    assert(boolean_symmetric_three_paths_first_inner_merge < boolean_symmetric_three_paths_first_inner_phi);
    assert(boolean_symmetric_three_paths_first_inner_phi < boolean_symmetric_three_paths_first_outer_merge);
    assert(boolean_symmetric_three_paths_first_outer_merge < boolean_symmetric_three_paths_first_outer_phi);
    assert(boolean_symmetric_three_paths_first_outer_phi < boolean_symmetric_three_paths_first_inner_predecessor);
    assert(boolean_symmetric_three_paths_first_inner_predecessor < boolean_symmetric_three_paths_first_index_phi);
    assert(boolean_symmetric_three_paths_first_index_phi < boolean_symmetric_three_paths_first_owner_guard);
    auto boolean_symmetric_three_paths_second_first_load =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_symmetric_three_paths_second_first_guard
        );
    auto boolean_symmetric_three_paths_second_outer_branch =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_symmetric_three_paths_second_first_load
        );
    auto boolean_symmetric_three_paths_second_outer_rhs =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_symmetric_three_paths_second_outer_branch
        );
    auto boolean_symmetric_three_paths_second_second_load =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_symmetric_three_paths_second_second_guard
        );
    auto boolean_symmetric_three_paths_second_inner_branch =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "label %logical.or.merge.",
            boolean_symmetric_three_paths_second_second_load
        );
    auto boolean_symmetric_three_paths_second_inner_rhs =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_symmetric_three_paths_second_inner_branch
        );
    auto boolean_symmetric_three_paths_second_third_load =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_symmetric_three_paths_second_third_guard
        );
    auto boolean_symmetric_three_paths_second_inner_merge =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_symmetric_three_paths_second_third_load
        );
    auto boolean_symmetric_three_paths_second_inner_phi =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_symmetric_three_paths_second_inner_merge
        );
    auto boolean_symmetric_three_paths_second_outer_merge =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_symmetric_three_paths_second_inner_phi
        );
    auto boolean_symmetric_three_paths_second_outer_phi =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_symmetric_three_paths_second_outer_merge
        );
    auto boolean_symmetric_three_paths_second_inner_predecessor =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            ", %logical.or.merge.",
            boolean_symmetric_three_paths_second_outer_phi
        );
    auto boolean_symmetric_three_paths_second_index_phi =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_symmetric_three_paths_second_outer_phi
        );
    auto boolean_symmetric_three_paths_second_owner_guard =
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_symmetric_three_paths_second_index_phi
        );
    assert(boolean_symmetric_three_paths_second_first_load != std::string::npos);
    assert(boolean_symmetric_three_paths_second_outer_branch != std::string::npos);
    assert(boolean_symmetric_three_paths_second_outer_rhs != std::string::npos);
    assert(boolean_symmetric_three_paths_second_second_load != std::string::npos);
    assert(boolean_symmetric_three_paths_second_inner_branch != std::string::npos);
    assert(boolean_symmetric_three_paths_second_inner_rhs != std::string::npos);
    assert(boolean_symmetric_three_paths_second_third_load != std::string::npos);
    assert(boolean_symmetric_three_paths_second_inner_merge != std::string::npos);
    assert(boolean_symmetric_three_paths_second_inner_phi != std::string::npos);
    assert(boolean_symmetric_three_paths_second_outer_merge != std::string::npos);
    assert(boolean_symmetric_three_paths_second_outer_phi != std::string::npos);
    assert(boolean_symmetric_three_paths_second_inner_predecessor != std::string::npos);
    assert(boolean_symmetric_three_paths_second_index_phi != std::string::npos);
    assert(boolean_symmetric_three_paths_second_owner_guard != std::string::npos);
    assert(boolean_symmetric_three_paths_second_first_guard < boolean_symmetric_three_paths_second_first_load);
    assert(boolean_symmetric_three_paths_second_first_load < boolean_symmetric_three_paths_second_outer_branch);
    assert(boolean_symmetric_three_paths_second_outer_branch < boolean_symmetric_three_paths_second_outer_rhs);
    assert(boolean_symmetric_three_paths_second_outer_rhs < boolean_symmetric_three_paths_second_second_guard);
    assert(boolean_symmetric_three_paths_second_second_guard < boolean_symmetric_three_paths_second_second_load);
    assert(boolean_symmetric_three_paths_second_second_load < boolean_symmetric_three_paths_second_inner_branch);
    assert(boolean_symmetric_three_paths_second_inner_branch < boolean_symmetric_three_paths_second_inner_rhs);
    assert(boolean_symmetric_three_paths_second_inner_rhs < boolean_symmetric_three_paths_second_third_guard);
    assert(boolean_symmetric_three_paths_second_third_guard < boolean_symmetric_three_paths_second_third_load);
    assert(boolean_symmetric_three_paths_second_third_load < boolean_symmetric_three_paths_second_inner_merge);
    assert(boolean_symmetric_three_paths_second_inner_merge < boolean_symmetric_three_paths_second_inner_phi);
    assert(boolean_symmetric_three_paths_second_inner_phi < boolean_symmetric_three_paths_second_outer_merge);
    assert(boolean_symmetric_three_paths_second_outer_merge < boolean_symmetric_three_paths_second_outer_phi);
    assert(boolean_symmetric_three_paths_second_outer_phi < boolean_symmetric_three_paths_second_inner_predecessor);
    assert(boolean_symmetric_three_paths_second_inner_predecessor < boolean_symmetric_three_paths_second_index_phi);
    assert(boolean_symmetric_three_paths_second_index_phi < boolean_symmetric_three_paths_second_owner_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_symmetric_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_member_paths_symmetric_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_symmetric_nested_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_three_member_paths_left_nested_condition_path
    );
    auto runtime_index_boolean_three_member_paths_left_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_member_paths_left_nested_condition_path.string()
        );
    auto boolean_left_nested_three_paths_first_first_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    assert(boolean_left_nested_three_paths_first_first_guard != std::string::npos);
    auto boolean_left_nested_three_paths_second_first_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_left_nested_three_paths_first_first_guard + 1
        );
    assert(boolean_left_nested_three_paths_second_first_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_left_nested_three_paths_second_first_guard + 1
        ) == std::string::npos
    );
    auto boolean_left_nested_three_paths_first_second_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    assert(boolean_left_nested_three_paths_first_second_guard != std::string::npos);
    auto boolean_left_nested_three_paths_second_second_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_left_nested_three_paths_first_second_guard + 1
        );
    assert(boolean_left_nested_three_paths_second_second_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_left_nested_three_paths_second_second_guard + 1
        ) == std::string::npos
    );
    auto boolean_left_nested_three_paths_first_third_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    assert(boolean_left_nested_three_paths_first_third_guard != std::string::npos);
    auto boolean_left_nested_three_paths_second_third_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_left_nested_three_paths_first_third_guard + 1
        );
    assert(boolean_left_nested_three_paths_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_left_nested_three_paths_second_third_guard + 1
        ) == std::string::npos
    );
    auto boolean_left_nested_three_paths_first_first_load =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_three_paths_first_first_guard
        );
    auto boolean_left_nested_three_paths_first_inner_branch =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_left_nested_three_paths_first_first_load
        );
    auto boolean_left_nested_three_paths_first_inner_rhs =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_left_nested_three_paths_first_inner_branch
        );
    auto boolean_left_nested_three_paths_first_second_load =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_three_paths_first_second_guard
        );
    auto boolean_left_nested_three_paths_first_inner_merge =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_left_nested_three_paths_first_second_load
        );
    auto boolean_left_nested_three_paths_first_inner_phi =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_left_nested_three_paths_first_inner_merge
        );
    auto boolean_left_nested_three_paths_first_outer_branch =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "label %logical.or.merge.",
            boolean_left_nested_three_paths_first_inner_phi
        );
    auto boolean_left_nested_three_paths_first_outer_rhs =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_left_nested_three_paths_first_outer_branch
        );
    auto boolean_left_nested_three_paths_first_third_load =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_three_paths_first_third_guard
        );
    auto boolean_left_nested_three_paths_first_outer_merge =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_left_nested_three_paths_first_third_load
        );
    auto boolean_left_nested_three_paths_first_outer_phi =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.",
            boolean_left_nested_three_paths_first_outer_merge
        );
    auto boolean_left_nested_three_paths_first_index_phi =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_left_nested_three_paths_first_outer_phi
        );
    auto boolean_left_nested_three_paths_first_owner_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_left_nested_three_paths_first_index_phi
        );
    assert(boolean_left_nested_three_paths_first_first_load != std::string::npos);
    assert(boolean_left_nested_three_paths_first_inner_branch != std::string::npos);
    assert(boolean_left_nested_three_paths_first_inner_rhs != std::string::npos);
    assert(boolean_left_nested_three_paths_first_second_load != std::string::npos);
    assert(boolean_left_nested_three_paths_first_inner_merge != std::string::npos);
    assert(boolean_left_nested_three_paths_first_inner_phi != std::string::npos);
    assert(boolean_left_nested_three_paths_first_outer_branch != std::string::npos);
    assert(boolean_left_nested_three_paths_first_outer_rhs != std::string::npos);
    assert(boolean_left_nested_three_paths_first_third_load != std::string::npos);
    assert(boolean_left_nested_three_paths_first_outer_merge != std::string::npos);
    assert(boolean_left_nested_three_paths_first_outer_phi != std::string::npos);
    assert(boolean_left_nested_three_paths_first_index_phi != std::string::npos);
    assert(boolean_left_nested_three_paths_first_owner_guard != std::string::npos);
    assert(boolean_left_nested_three_paths_first_first_guard < boolean_left_nested_three_paths_first_first_load);
    assert(boolean_left_nested_three_paths_first_first_load < boolean_left_nested_three_paths_first_inner_branch);
    assert(boolean_left_nested_three_paths_first_inner_branch < boolean_left_nested_three_paths_first_inner_rhs);
    assert(boolean_left_nested_three_paths_first_inner_rhs < boolean_left_nested_three_paths_first_second_guard);
    assert(boolean_left_nested_three_paths_first_second_guard < boolean_left_nested_three_paths_first_second_load);
    assert(boolean_left_nested_three_paths_first_second_load < boolean_left_nested_three_paths_first_inner_merge);
    assert(boolean_left_nested_three_paths_first_inner_merge < boolean_left_nested_three_paths_first_inner_phi);
    assert(boolean_left_nested_three_paths_first_inner_phi < boolean_left_nested_three_paths_first_outer_branch);
    assert(boolean_left_nested_three_paths_first_outer_branch < boolean_left_nested_three_paths_first_outer_rhs);
    assert(boolean_left_nested_three_paths_first_outer_rhs < boolean_left_nested_three_paths_first_third_guard);
    assert(boolean_left_nested_three_paths_first_third_guard < boolean_left_nested_three_paths_first_third_load);
    assert(boolean_left_nested_three_paths_first_third_load < boolean_left_nested_three_paths_first_outer_merge);
    assert(boolean_left_nested_three_paths_first_outer_merge < boolean_left_nested_three_paths_first_outer_phi);
    assert(boolean_left_nested_three_paths_first_outer_phi < boolean_left_nested_three_paths_first_index_phi);
    assert(boolean_left_nested_three_paths_first_index_phi < boolean_left_nested_three_paths_first_owner_guard);
    auto boolean_left_nested_three_paths_second_first_load =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_three_paths_second_first_guard
        );
    auto boolean_left_nested_three_paths_second_inner_branch =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "label %logical.and.rhs.",
            boolean_left_nested_three_paths_second_first_load
        );
    auto boolean_left_nested_three_paths_second_inner_rhs =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.and.rhs.",
            boolean_left_nested_three_paths_second_inner_branch
        );
    auto boolean_left_nested_three_paths_second_second_load =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_three_paths_second_second_guard
        );
    auto boolean_left_nested_three_paths_second_inner_merge =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.and.merge.",
            boolean_left_nested_three_paths_second_second_load
        );
    auto boolean_left_nested_three_paths_second_inner_phi =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.",
            boolean_left_nested_three_paths_second_inner_merge
        );
    auto boolean_left_nested_three_paths_second_outer_branch =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "label %logical.or.merge.",
            boolean_left_nested_three_paths_second_inner_phi
        );
    auto boolean_left_nested_three_paths_second_outer_rhs =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.or.rhs.",
            boolean_left_nested_three_paths_second_outer_branch
        );
    auto boolean_left_nested_three_paths_second_third_load =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ",
            boolean_left_nested_three_paths_second_third_guard
        );
    auto boolean_left_nested_three_paths_second_outer_merge =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "\nlogical.or.merge.",
            boolean_left_nested_three_paths_second_third_load
        );
    auto boolean_left_nested_three_paths_second_outer_phi =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.",
            boolean_left_nested_three_paths_second_outer_merge
        );
    auto boolean_left_nested_three_paths_second_index_phi =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_left_nested_three_paths_second_outer_phi
        );
    auto boolean_left_nested_three_paths_second_owner_guard =
        runtime_index_boolean_three_member_paths_left_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_left_nested_three_paths_second_index_phi
        );
    assert(boolean_left_nested_three_paths_second_first_load != std::string::npos);
    assert(boolean_left_nested_three_paths_second_inner_branch != std::string::npos);
    assert(boolean_left_nested_three_paths_second_inner_rhs != std::string::npos);
    assert(boolean_left_nested_three_paths_second_second_load != std::string::npos);
    assert(boolean_left_nested_three_paths_second_inner_merge != std::string::npos);
    assert(boolean_left_nested_three_paths_second_inner_phi != std::string::npos);
    assert(boolean_left_nested_three_paths_second_outer_branch != std::string::npos);
    assert(boolean_left_nested_three_paths_second_outer_rhs != std::string::npos);
    assert(boolean_left_nested_three_paths_second_third_load != std::string::npos);
    assert(boolean_left_nested_three_paths_second_outer_merge != std::string::npos);
    assert(boolean_left_nested_three_paths_second_outer_phi != std::string::npos);
    assert(boolean_left_nested_three_paths_second_index_phi != std::string::npos);
    assert(boolean_left_nested_three_paths_second_owner_guard != std::string::npos);
    assert(boolean_left_nested_three_paths_second_first_guard < boolean_left_nested_three_paths_second_first_load);
    assert(boolean_left_nested_three_paths_second_first_load < boolean_left_nested_three_paths_second_inner_branch);
    assert(boolean_left_nested_three_paths_second_inner_branch < boolean_left_nested_three_paths_second_inner_rhs);
    assert(boolean_left_nested_three_paths_second_inner_rhs < boolean_left_nested_three_paths_second_second_guard);
    assert(boolean_left_nested_three_paths_second_second_guard < boolean_left_nested_three_paths_second_second_load);
    assert(boolean_left_nested_three_paths_second_second_load < boolean_left_nested_three_paths_second_inner_merge);
    assert(boolean_left_nested_three_paths_second_inner_merge < boolean_left_nested_three_paths_second_inner_phi);
    assert(boolean_left_nested_three_paths_second_inner_phi < boolean_left_nested_three_paths_second_outer_branch);
    assert(boolean_left_nested_three_paths_second_outer_branch < boolean_left_nested_three_paths_second_outer_rhs);
    assert(boolean_left_nested_three_paths_second_outer_rhs < boolean_left_nested_three_paths_second_third_guard);
    assert(boolean_left_nested_three_paths_second_third_guard < boolean_left_nested_three_paths_second_third_load);
    assert(boolean_left_nested_three_paths_second_third_load < boolean_left_nested_three_paths_second_outer_merge);
    assert(boolean_left_nested_three_paths_second_outer_merge < boolean_left_nested_three_paths_second_outer_phi);
    assert(boolean_left_nested_three_paths_second_outer_phi < boolean_left_nested_three_paths_second_index_phi);
    assert(boolean_left_nested_three_paths_second_index_phi < boolean_left_nested_three_paths_second_owner_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_member_paths_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_left_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_member_paths_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_left_nested_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_path
    );
    auto runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_path.string()
        );
    auto boolean_symmetric_left_nested_first_first_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto boolean_symmetric_left_nested_second_first_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_symmetric_left_nested_first_first_guard + 1
        );
    auto boolean_symmetric_left_nested_first_second_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto boolean_symmetric_left_nested_second_second_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_symmetric_left_nested_first_second_guard + 1
        );
    auto boolean_symmetric_left_nested_first_third_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto boolean_symmetric_left_nested_second_third_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_symmetric_left_nested_first_third_guard + 1
        );
    assert(boolean_symmetric_left_nested_first_first_guard != std::string::npos);
    assert(boolean_symmetric_left_nested_second_first_guard != std::string::npos);
    assert(boolean_symmetric_left_nested_first_second_guard != std::string::npos);
    assert(boolean_symmetric_left_nested_second_second_guard != std::string::npos);
    assert(boolean_symmetric_left_nested_first_third_guard != std::string::npos);
    assert(boolean_symmetric_left_nested_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3",
            boolean_symmetric_left_nested_second_first_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3",
            boolean_symmetric_left_nested_second_second_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3",
            boolean_symmetric_left_nested_second_third_guard + 1
        ) == std::string::npos
    );
    auto boolean_symmetric_left_nested_first_first_load =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", boolean_symmetric_left_nested_first_first_guard
        );
    auto boolean_symmetric_left_nested_first_inner_rhs =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.or.rhs.", boolean_symmetric_left_nested_first_first_load
        );
    auto boolean_symmetric_left_nested_first_second_load =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", boolean_symmetric_left_nested_first_second_guard
        );
    auto boolean_symmetric_left_nested_first_inner_merge =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.or.merge.", boolean_symmetric_left_nested_first_second_load
        );
    auto boolean_symmetric_left_nested_first_inner_phi =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_symmetric_left_nested_first_inner_merge
        );
    auto boolean_symmetric_left_nested_first_outer_rhs =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.and.rhs.", boolean_symmetric_left_nested_first_inner_phi
        );
    auto boolean_symmetric_left_nested_first_third_load =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", boolean_symmetric_left_nested_first_third_guard
        );
    auto boolean_symmetric_left_nested_first_outer_merge =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.and.merge.", boolean_symmetric_left_nested_first_third_load
        );
    auto boolean_symmetric_left_nested_first_outer_phi =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.",
            boolean_symmetric_left_nested_first_outer_merge
        );
    auto boolean_symmetric_left_nested_first_index_phi =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i64 [%first, ", boolean_symmetric_left_nested_first_outer_phi
        );
    auto boolean_symmetric_left_nested_first_owner_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_symmetric_left_nested_first_index_phi
        );
    assert(boolean_symmetric_left_nested_first_first_load < boolean_symmetric_left_nested_first_inner_rhs);
    assert(boolean_symmetric_left_nested_first_inner_rhs < boolean_symmetric_left_nested_first_second_guard);
    assert(boolean_symmetric_left_nested_first_second_guard < boolean_symmetric_left_nested_first_second_load);
    assert(boolean_symmetric_left_nested_first_second_load < boolean_symmetric_left_nested_first_inner_merge);
    assert(boolean_symmetric_left_nested_first_inner_merge < boolean_symmetric_left_nested_first_inner_phi);
    assert(boolean_symmetric_left_nested_first_inner_phi < boolean_symmetric_left_nested_first_outer_rhs);
    assert(boolean_symmetric_left_nested_first_outer_rhs < boolean_symmetric_left_nested_first_third_guard);
    assert(boolean_symmetric_left_nested_first_third_guard < boolean_symmetric_left_nested_first_third_load);
    assert(boolean_symmetric_left_nested_first_third_load < boolean_symmetric_left_nested_first_outer_merge);
    assert(boolean_symmetric_left_nested_first_outer_merge < boolean_symmetric_left_nested_first_outer_phi);
    assert(boolean_symmetric_left_nested_first_outer_phi < boolean_symmetric_left_nested_first_index_phi);
    assert(boolean_symmetric_left_nested_first_index_phi < boolean_symmetric_left_nested_first_owner_guard);
    auto boolean_symmetric_left_nested_second_first_load =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", boolean_symmetric_left_nested_second_first_guard
        );
    auto boolean_symmetric_left_nested_second_inner_rhs =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.or.rhs.", boolean_symmetric_left_nested_second_first_load
        );
    auto boolean_symmetric_left_nested_second_second_load =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", boolean_symmetric_left_nested_second_second_guard
        );
    auto boolean_symmetric_left_nested_second_inner_merge =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.or.merge.", boolean_symmetric_left_nested_second_second_load
        );
    auto boolean_symmetric_left_nested_second_inner_phi =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            boolean_symmetric_left_nested_second_inner_merge
        );
    auto boolean_symmetric_left_nested_second_outer_rhs =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.and.rhs.", boolean_symmetric_left_nested_second_inner_phi
        );
    auto boolean_symmetric_left_nested_second_third_load =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", boolean_symmetric_left_nested_second_third_guard
        );
    auto boolean_symmetric_left_nested_second_outer_merge =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.and.merge.", boolean_symmetric_left_nested_second_third_load
        );
    auto boolean_symmetric_left_nested_second_outer_phi =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.",
            boolean_symmetric_left_nested_second_outer_merge
        );
    auto boolean_symmetric_left_nested_second_index_phi =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i64 [%first, ", boolean_symmetric_left_nested_second_outer_phi
        );
    auto boolean_symmetric_left_nested_second_owner_guard =
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_symmetric_left_nested_second_index_phi
        );
    assert(boolean_symmetric_left_nested_second_first_load < boolean_symmetric_left_nested_second_inner_rhs);
    assert(boolean_symmetric_left_nested_second_inner_rhs < boolean_symmetric_left_nested_second_second_guard);
    assert(boolean_symmetric_left_nested_second_second_guard < boolean_symmetric_left_nested_second_second_load);
    assert(boolean_symmetric_left_nested_second_second_load < boolean_symmetric_left_nested_second_inner_merge);
    assert(boolean_symmetric_left_nested_second_inner_merge < boolean_symmetric_left_nested_second_inner_phi);
    assert(boolean_symmetric_left_nested_second_inner_phi < boolean_symmetric_left_nested_second_outer_rhs);
    assert(boolean_symmetric_left_nested_second_outer_rhs < boolean_symmetric_left_nested_second_third_guard);
    assert(boolean_symmetric_left_nested_second_third_guard < boolean_symmetric_left_nested_second_third_load);
    assert(boolean_symmetric_left_nested_second_third_load < boolean_symmetric_left_nested_second_outer_merge);
    assert(boolean_symmetric_left_nested_second_outer_merge < boolean_symmetric_left_nested_second_outer_phi);
    assert(boolean_symmetric_left_nested_second_outer_phi < boolean_symmetric_left_nested_second_index_phi);
    assert(boolean_symmetric_left_nested_second_index_phi < boolean_symmetric_left_nested_second_owner_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_member_paths_symmetric_left_nested_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_four_member_paths_balanced_condition_path);
    auto runtime_index_boolean_four_member_paths_balanced_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_four_member_paths_balanced_condition_path.string()
        );
    auto balanced_first_guard =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto balanced_first_guard_second =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", balanced_first_guard + 1
        );
    auto balanced_second_guard =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto balanced_second_guard_second =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", balanced_second_guard + 1
        );
    auto balanced_third_guard =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto balanced_third_guard_second =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", balanced_third_guard + 1
        );
    auto balanced_fourth_guard =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3"
        );
    auto balanced_fourth_guard_second =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3", balanced_fourth_guard + 1
        );
    assert(balanced_first_guard != std::string::npos);
    assert(balanced_first_guard_second != std::string::npos);
    assert(balanced_second_guard != std::string::npos);
    assert(balanced_second_guard_second != std::string::npos);
    assert(balanced_third_guard != std::string::npos);
    assert(balanced_third_guard_second != std::string::npos);
    assert(balanced_fourth_guard != std::string::npos);
    assert(balanced_fourth_guard_second != std::string::npos);
    assert(
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3", balanced_fourth_guard_second + 1
        ) == std::string::npos
    );
    auto balanced_first_inner_phi =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds."
        );
    auto balanced_first_right_inner_phi =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", balanced_first_inner_phi + 1
        );
    auto balanced_second_inner_phi =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", balanced_first_right_inner_phi + 1
        );
    auto balanced_second_right_inner_phi =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", balanced_second_inner_phi + 1
        );
    auto balanced_first_outer_phi =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = phi i1 [1, %logical.and.merge."
        );
    auto balanced_second_outer_phi =
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.", balanced_first_outer_phi + 1
        );
    assert(balanced_first_inner_phi != std::string::npos);
    assert(balanced_first_right_inner_phi != std::string::npos);
    assert(balanced_second_inner_phi != std::string::npos);
    assert(balanced_second_right_inner_phi != std::string::npos);
    assert(balanced_first_outer_phi != std::string::npos);
    assert(balanced_second_outer_phi != std::string::npos);
    assert(balanced_first_inner_phi < balanced_first_right_inner_phi);
    assert(balanced_first_right_inner_phi < balanced_first_outer_phi);
    assert(balanced_first_outer_phi < balanced_second_inner_phi);
    assert(balanced_second_inner_phi < balanced_second_right_inner_phi);
    assert(balanced_second_right_inner_phi < balanced_second_outer_phi);
    assert(
        runtime_index_boolean_four_member_paths_balanced_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.", balanced_second_outer_phi + 1
        ) == std::string::npos
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_four_member_paths_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_member_paths_balanced_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_four_member_paths_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_member_paths_balanced_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_path
    );
    auto runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_four_member_paths_symmetric_balanced_condition_path.string()
        );
    auto symmetric_balanced_first_inner_phi =
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds."
        );
    auto symmetric_balanced_first_right_inner_phi =
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_balanced_first_inner_phi + 1
        );
    auto symmetric_balanced_second_inner_phi =
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_balanced_first_right_inner_phi + 1
        );
    auto symmetric_balanced_second_right_inner_phi =
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_balanced_second_inner_phi + 1
        );
    auto symmetric_balanced_first_outer_phi =
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [0, %logical.or.merge."
        );
    auto symmetric_balanced_second_outer_phi =
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.", symmetric_balanced_first_outer_phi + 1
        );
    assert(symmetric_balanced_first_inner_phi != std::string::npos);
    assert(symmetric_balanced_first_right_inner_phi != std::string::npos);
    assert(symmetric_balanced_second_inner_phi != std::string::npos);
    assert(symmetric_balanced_second_right_inner_phi != std::string::npos);
    assert(symmetric_balanced_first_outer_phi != std::string::npos);
    assert(symmetric_balanced_second_outer_phi != std::string::npos);
    assert(symmetric_balanced_first_inner_phi < symmetric_balanced_first_right_inner_phi);
    assert(symmetric_balanced_first_right_inner_phi < symmetric_balanced_first_outer_phi);
    assert(symmetric_balanced_first_outer_phi < symmetric_balanced_second_inner_phi);
    assert(symmetric_balanced_second_inner_phi < symmetric_balanced_second_right_inner_phi);
    assert(symmetric_balanced_second_right_inner_phi < symmetric_balanced_second_outer_phi);
    assert(
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.", symmetric_balanced_second_outer_phi + 1
        ) == std::string::npos
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_member_paths_symmetric_balanced_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_four_member_paths_symmetric_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_member_paths_symmetric_balanced_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_and_dependent_member_path_condition_path);
    auto runtime_index_boolean_and_dependent_member_path_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_and_dependent_member_path_condition_path.string()
        );
    auto and_dependent_first_rhs =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find("\nlogical.and.rhs.");
    auto and_dependent_second_rhs =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            "\nlogical.and.rhs.", and_dependent_first_rhs + 1
        );
    auto and_dependent_first_slot_guard =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = icmp ult i64 %slot, 2", and_dependent_first_rhs
        );
    auto and_dependent_second_slot_guard =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = icmp ult i64 %slot, 2", and_dependent_first_slot_guard + 1
        );
    auto and_dependent_first_source_load =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = load i32, ptr ", and_dependent_first_slot_guard
        );
    auto and_dependent_first_cast =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = zext i32 ", and_dependent_first_source_load
        );
    auto and_dependent_first_group_guard =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = icmp ult i64 ", and_dependent_first_cast
        );
    auto and_dependent_first_member_load =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = load i1, ptr ", and_dependent_first_group_guard
        );
    auto and_dependent_first_phi =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = phi i1 [0, %ternary.then.", and_dependent_first_member_load
        );
    auto and_dependent_second_source_load =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = load i32, ptr ", and_dependent_second_slot_guard
        );
    auto and_dependent_second_cast =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = zext i32 ", and_dependent_second_source_load
        );
    auto and_dependent_second_group_guard =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = icmp ult i64 ", and_dependent_second_cast
        );
    auto and_dependent_second_member_load =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = load i1, ptr ", and_dependent_second_group_guard
        );
    auto and_dependent_second_phi =
        runtime_index_boolean_and_dependent_member_path_condition_ir.find(
            " = phi i1 [0, %ternary.else.", and_dependent_second_member_load
        );
    assert(and_dependent_first_rhs != std::string::npos);
    assert(and_dependent_second_rhs != std::string::npos);
    assert(and_dependent_first_rhs < and_dependent_first_slot_guard);
    assert(and_dependent_first_slot_guard < and_dependent_first_source_load);
    assert(and_dependent_first_source_load < and_dependent_first_cast);
    assert(and_dependent_first_cast < and_dependent_first_group_guard);
    assert(and_dependent_first_group_guard < and_dependent_first_member_load);
    assert(and_dependent_first_member_load < and_dependent_first_phi);
    assert(and_dependent_second_rhs < and_dependent_second_slot_guard);
    assert(and_dependent_second_slot_guard < and_dependent_second_source_load);
    assert(and_dependent_second_source_load < and_dependent_second_cast);
    assert(and_dependent_second_cast < and_dependent_second_group_guard);
    assert(and_dependent_second_group_guard < and_dependent_second_member_load);
    assert(and_dependent_second_member_load < and_dependent_second_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_and_dependent_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_and_dependent_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_and_dependent_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_and_dependent_member_path_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_or_dependent_member_path_condition_path);
    auto runtime_index_boolean_or_dependent_member_path_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_or_dependent_member_path_condition_path.string()
        );
    auto or_dependent_first_rhs =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find("\nlogical.or.rhs.");
    auto or_dependent_second_rhs =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            "\nlogical.or.rhs.", or_dependent_first_rhs + 1
        );
    auto or_dependent_first_slot_guard =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = icmp ult i64 %slot, 2", or_dependent_first_rhs
        );
    auto or_dependent_second_slot_guard =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = icmp ult i64 %slot, 2", or_dependent_first_slot_guard + 1
        );
    auto or_dependent_first_source_load =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = load i32, ptr ", or_dependent_first_slot_guard
        );
    auto or_dependent_first_cast =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = zext i32 ", or_dependent_first_source_load
        );
    auto or_dependent_first_member_load =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = load i1, ptr ", or_dependent_first_cast
        );
    auto or_dependent_first_phi =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = phi i1 [1, %ternary.then.", or_dependent_first_member_load
        );
    auto or_dependent_second_source_load =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = load i32, ptr ", or_dependent_second_slot_guard
        );
    auto or_dependent_second_cast =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = zext i32 ", or_dependent_second_source_load
        );
    auto or_dependent_second_member_load =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = load i1, ptr ", or_dependent_second_cast
        );
    auto or_dependent_second_phi =
        runtime_index_boolean_or_dependent_member_path_condition_ir.find(
            " = phi i1 [1, %ternary.else.", or_dependent_second_member_load
        );
    assert(or_dependent_first_rhs < or_dependent_first_slot_guard);
    assert(or_dependent_first_slot_guard < or_dependent_first_source_load);
    assert(or_dependent_first_source_load < or_dependent_first_cast);
    assert(or_dependent_first_cast < or_dependent_first_member_load);
    assert(or_dependent_first_member_load < or_dependent_first_phi);
    assert(or_dependent_second_rhs < or_dependent_second_slot_guard);
    assert(or_dependent_second_slot_guard < or_dependent_second_source_load);
    assert(or_dependent_second_source_load < or_dependent_second_cast);
    assert(or_dependent_second_cast < or_dependent_second_member_load);
    assert(or_dependent_second_member_load < or_dependent_second_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_or_dependent_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_or_dependent_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_or_dependent_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_or_dependent_member_path_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_two_dependent_member_paths_and_condition_path
    );
    auto runtime_index_boolean_two_dependent_member_paths_and_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_two_dependent_member_paths_and_condition_path.string()
        );
    auto two_dependent_first_first_guard =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = icmp ult i64 %first_slot, 2"
        );
    auto two_dependent_second_first_guard =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = icmp ult i64 %first_slot, 2", two_dependent_first_first_guard + 1
        );
    auto two_dependent_first_second_guard =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = icmp ult i64 %second_slot, 2"
        );
    auto two_dependent_second_second_guard =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = icmp ult i64 %second_slot, 2", two_dependent_first_second_guard + 1
        );
    auto two_dependent_first_first_load =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i32, ptr ", two_dependent_first_first_guard
        );
    auto two_dependent_first_first_cast =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = zext i32 ", two_dependent_first_first_load
        );
    auto two_dependent_first_first_member =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i1, ptr ", two_dependent_first_first_cast
        );
    auto two_dependent_first_rhs =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            "\nlogical.and.rhs.", two_dependent_first_first_member
        );
    auto two_dependent_first_second_load =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i32, ptr ", two_dependent_first_second_guard
        );
    auto two_dependent_first_second_cast =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = zext i32 ", two_dependent_first_second_load
        );
    auto two_dependent_first_second_member =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i1, ptr ", two_dependent_first_second_cast
        );
    auto two_dependent_first_phi =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", two_dependent_first_second_member
        );
    auto two_dependent_second_first_load =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i32, ptr ", two_dependent_second_first_guard
        );
    auto two_dependent_second_first_cast =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = zext i32 ", two_dependent_second_first_load
        );
    auto two_dependent_second_first_member =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i1, ptr ", two_dependent_second_first_cast
        );
    auto two_dependent_second_rhs =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            "\nlogical.and.rhs.", two_dependent_second_first_member
        );
    auto two_dependent_second_second_load =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i32, ptr ", two_dependent_second_second_guard
        );
    auto two_dependent_second_second_cast =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = zext i32 ", two_dependent_second_second_load
        );
    auto two_dependent_second_second_member =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = load i1, ptr ", two_dependent_second_second_cast
        );
    auto two_dependent_second_phi =
        runtime_index_boolean_two_dependent_member_paths_and_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", two_dependent_second_second_member
        );
    assert(two_dependent_first_first_guard < two_dependent_first_first_load);
    assert(two_dependent_first_first_load < two_dependent_first_first_cast);
    assert(two_dependent_first_first_cast < two_dependent_first_first_member);
    assert(two_dependent_first_first_member < two_dependent_first_rhs);
    assert(two_dependent_first_rhs < two_dependent_first_second_guard);
    assert(two_dependent_first_second_guard < two_dependent_first_second_load);
    assert(two_dependent_first_second_load < two_dependent_first_second_cast);
    assert(two_dependent_first_second_cast < two_dependent_first_second_member);
    assert(two_dependent_first_second_member < two_dependent_first_phi);
    assert(two_dependent_second_first_guard < two_dependent_second_first_load);
    assert(two_dependent_second_first_load < two_dependent_second_first_cast);
    assert(two_dependent_second_first_cast < two_dependent_second_first_member);
    assert(two_dependent_second_first_member < two_dependent_second_rhs);
    assert(two_dependent_second_rhs < two_dependent_second_second_guard);
    assert(two_dependent_second_second_guard < two_dependent_second_second_load);
    assert(two_dependent_second_second_load < two_dependent_second_second_cast);
    assert(two_dependent_second_second_cast < two_dependent_second_second_member);
    assert(two_dependent_second_second_member < two_dependent_second_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_two_dependent_member_paths_and_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_dependent_member_paths_and_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_two_dependent_member_paths_and_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_dependent_member_paths_and_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_two_dependent_member_paths_or_condition_path
    );
    auto runtime_index_boolean_two_dependent_member_paths_or_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_two_dependent_member_paths_or_condition_path.string()
        );
    auto two_dependent_or_first_first_guard =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = icmp ult i64 %first_slot, 2"
        );
    auto two_dependent_or_second_first_guard =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = icmp ult i64 %first_slot, 2", two_dependent_or_first_first_guard + 1
        );
    auto two_dependent_or_first_second_guard =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = icmp ult i64 %second_slot, 2"
        );
    auto two_dependent_or_second_second_guard =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = icmp ult i64 %second_slot, 2", two_dependent_or_first_second_guard + 1
        );
    auto two_dependent_or_first_first_load =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i32, ptr ", two_dependent_or_first_first_guard
        );
    auto two_dependent_or_first_first_member =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i1, ptr ", two_dependent_or_first_first_load
        );
    auto two_dependent_or_first_rhs =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            "\nlogical.or.rhs.", two_dependent_or_first_first_member
        );
    auto two_dependent_or_first_second_load =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i32, ptr ", two_dependent_or_first_second_guard
        );
    auto two_dependent_or_first_second_member =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i1, ptr ", two_dependent_or_first_second_load
        );
    auto two_dependent_or_first_phi =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", two_dependent_or_first_second_member
        );
    auto two_dependent_or_second_first_load =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i32, ptr ", two_dependent_or_second_first_guard
        );
    auto two_dependent_or_second_first_member =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i1, ptr ", two_dependent_or_second_first_load
        );
    auto two_dependent_or_second_rhs =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            "\nlogical.or.rhs.", two_dependent_or_second_first_member
        );
    auto two_dependent_or_second_second_load =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i32, ptr ", two_dependent_or_second_second_guard
        );
    auto two_dependent_or_second_second_member =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = load i1, ptr ", two_dependent_or_second_second_load
        );
    auto two_dependent_or_second_phi =
        runtime_index_boolean_two_dependent_member_paths_or_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", two_dependent_or_second_second_member
        );
    assert(two_dependent_or_first_first_guard < two_dependent_or_first_first_load);
    assert(two_dependent_or_first_first_load < two_dependent_or_first_first_member);
    assert(two_dependent_or_first_first_member < two_dependent_or_first_rhs);
    assert(two_dependent_or_first_rhs < two_dependent_or_first_second_guard);
    assert(two_dependent_or_first_second_guard < two_dependent_or_first_second_load);
    assert(two_dependent_or_first_second_load < two_dependent_or_first_second_member);
    assert(two_dependent_or_first_second_member < two_dependent_or_first_phi);
    assert(two_dependent_or_second_first_guard < two_dependent_or_second_first_load);
    assert(two_dependent_or_second_first_load < two_dependent_or_second_first_member);
    assert(two_dependent_or_second_first_member < two_dependent_or_second_rhs);
    assert(two_dependent_or_second_rhs < two_dependent_or_second_second_guard);
    assert(two_dependent_or_second_second_guard < two_dependent_or_second_second_load);
    assert(two_dependent_or_second_second_load < two_dependent_or_second_second_member);
    assert(two_dependent_or_second_second_member < two_dependent_or_second_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_two_dependent_member_paths_or_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_dependent_member_paths_or_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_two_dependent_member_paths_or_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_two_dependent_member_paths_or_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_nested_condition_path
    );
    auto runtime_index_boolean_three_dependent_member_paths_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_dependent_member_paths_nested_condition_path.string()
        );
    auto three_dependent_first_first_guard =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto three_dependent_second_first_guard =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", three_dependent_first_first_guard + 1
        );
    auto three_dependent_first_second_guard =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto three_dependent_second_second_guard =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", three_dependent_first_second_guard + 1
        );
    auto three_dependent_first_third_guard =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto three_dependent_second_third_guard =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", three_dependent_first_third_guard + 1
        );
    assert(three_dependent_first_first_guard != std::string::npos);
    assert(three_dependent_second_first_guard != std::string::npos);
    assert(three_dependent_first_second_guard != std::string::npos);
    assert(three_dependent_second_second_guard != std::string::npos);
    assert(three_dependent_first_third_guard != std::string::npos);
    assert(three_dependent_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", three_dependent_second_first_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", three_dependent_second_second_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", three_dependent_second_third_guard + 1
        ) == std::string::npos
    );
    auto three_dependent_first_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i32, ptr ", three_dependent_first_first_guard
        );
    auto three_dependent_first_first_cast =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = zext i32 ", three_dependent_first_first_source_load
        );
    auto three_dependent_first_first_member =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i1, ptr ", three_dependent_first_first_cast
        );
    auto three_dependent_first_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            "\nlogical.or.rhs.", three_dependent_first_first_member
        );
    auto three_dependent_first_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i32, ptr ", three_dependent_first_second_guard
        );
    auto three_dependent_first_second_cast =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = zext i32 ", three_dependent_first_second_source_load
        );
    auto three_dependent_first_second_member =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i1, ptr ", three_dependent_first_second_cast
        );
    auto three_dependent_first_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            "\nlogical.and.rhs.", three_dependent_first_second_member
        );
    auto three_dependent_first_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i32, ptr ", three_dependent_first_third_guard
        );
    auto three_dependent_first_third_cast =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = zext i32 ", three_dependent_first_third_source_load
        );
    auto three_dependent_first_third_member =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i1, ptr ", three_dependent_first_third_cast
        );
    auto three_dependent_first_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", three_dependent_first_third_member
        );
    auto three_dependent_first_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", three_dependent_first_inner_phi
        );
    auto three_dependent_first_inner_predecessor =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            ", %logical.and.merge.", three_dependent_first_outer_phi
        );
    assert(three_dependent_first_first_guard < three_dependent_first_first_source_load);
    assert(three_dependent_first_first_source_load < three_dependent_first_first_cast);
    assert(three_dependent_first_first_cast < three_dependent_first_first_member);
    assert(three_dependent_first_first_member < three_dependent_first_outer_rhs);
    assert(three_dependent_first_outer_rhs < three_dependent_first_second_guard);
    assert(three_dependent_first_second_guard < three_dependent_first_second_source_load);
    assert(three_dependent_first_second_source_load < three_dependent_first_second_cast);
    assert(three_dependent_first_second_cast < three_dependent_first_second_member);
    assert(three_dependent_first_second_member < three_dependent_first_inner_rhs);
    assert(three_dependent_first_inner_rhs < three_dependent_first_third_guard);
    assert(three_dependent_first_third_guard < three_dependent_first_third_source_load);
    assert(three_dependent_first_third_source_load < three_dependent_first_third_cast);
    assert(three_dependent_first_third_cast < three_dependent_first_third_member);
    assert(three_dependent_first_third_member < three_dependent_first_inner_phi);
    assert(three_dependent_first_inner_phi < three_dependent_first_outer_phi);
    assert(three_dependent_first_outer_phi < three_dependent_first_inner_predecessor);
    auto three_dependent_second_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i32, ptr ", three_dependent_second_first_guard
        );
    auto three_dependent_second_first_cast =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = zext i32 ", three_dependent_second_first_source_load
        );
    auto three_dependent_second_first_member =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i1, ptr ", three_dependent_second_first_cast
        );
    auto three_dependent_second_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            "\nlogical.or.rhs.", three_dependent_second_first_member
        );
    auto three_dependent_second_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i32, ptr ", three_dependent_second_second_guard
        );
    auto three_dependent_second_second_cast =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = zext i32 ", three_dependent_second_second_source_load
        );
    auto three_dependent_second_second_member =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i1, ptr ", three_dependent_second_second_cast
        );
    auto three_dependent_second_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            "\nlogical.and.rhs.", three_dependent_second_second_member
        );
    auto three_dependent_second_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i32, ptr ", three_dependent_second_third_guard
        );
    auto three_dependent_second_third_cast =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = zext i32 ", three_dependent_second_third_source_load
        );
    auto three_dependent_second_third_member =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = load i1, ptr ", three_dependent_second_third_cast
        );
    auto three_dependent_second_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", three_dependent_second_third_member
        );
    auto three_dependent_second_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", three_dependent_second_inner_phi
        );
    auto three_dependent_second_inner_predecessor =
        runtime_index_boolean_three_dependent_member_paths_nested_condition_ir.find(
            ", %logical.and.merge.", three_dependent_second_outer_phi
        );
    assert(three_dependent_second_first_guard < three_dependent_second_first_source_load);
    assert(three_dependent_second_first_source_load < three_dependent_second_first_cast);
    assert(three_dependent_second_first_cast < three_dependent_second_first_member);
    assert(three_dependent_second_first_member < three_dependent_second_outer_rhs);
    assert(three_dependent_second_outer_rhs < three_dependent_second_second_guard);
    assert(three_dependent_second_second_guard < three_dependent_second_second_source_load);
    assert(three_dependent_second_second_source_load < three_dependent_second_second_cast);
    assert(three_dependent_second_second_cast < three_dependent_second_second_member);
    assert(three_dependent_second_second_member < three_dependent_second_inner_rhs);
    assert(three_dependent_second_inner_rhs < three_dependent_second_third_guard);
    assert(three_dependent_second_third_guard < three_dependent_second_third_source_load);
    assert(three_dependent_second_third_source_load < three_dependent_second_third_cast);
    assert(three_dependent_second_third_cast < three_dependent_second_third_member);
    assert(three_dependent_second_third_member < three_dependent_second_inner_phi);
    assert(three_dependent_second_inner_phi < three_dependent_second_outer_phi);
    assert(three_dependent_second_outer_phi < three_dependent_second_inner_predecessor);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_nested_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_path
    );
    auto runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_path.string()
        );
    auto symmetric_dependent_first_first_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto symmetric_dependent_second_first_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", symmetric_dependent_first_first_guard + 1
        );
    auto symmetric_dependent_first_second_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto symmetric_dependent_second_second_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", symmetric_dependent_first_second_guard + 1
        );
    auto symmetric_dependent_first_third_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto symmetric_dependent_second_third_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", symmetric_dependent_first_third_guard + 1
        );
    assert(symmetric_dependent_first_first_guard != std::string::npos);
    assert(symmetric_dependent_second_first_guard != std::string::npos);
    assert(symmetric_dependent_first_second_guard != std::string::npos);
    assert(symmetric_dependent_second_second_guard != std::string::npos);
    assert(symmetric_dependent_first_third_guard != std::string::npos);
    assert(symmetric_dependent_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", symmetric_dependent_second_first_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", symmetric_dependent_second_second_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", symmetric_dependent_second_third_guard + 1
        ) == std::string::npos
    );
    auto symmetric_dependent_first_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_dependent_first_first_guard
        );
    auto symmetric_dependent_first_first_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = zext i32 ", symmetric_dependent_first_first_source_load
        );
    auto symmetric_dependent_first_first_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_dependent_first_first_cast
        );
    auto symmetric_dependent_first_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.and.rhs.", symmetric_dependent_first_first_member
        );
    auto symmetric_dependent_first_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_dependent_first_second_guard
        );
    auto symmetric_dependent_first_second_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = zext i32 ", symmetric_dependent_first_second_source_load
        );
    auto symmetric_dependent_first_second_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_dependent_first_second_cast
        );
    auto symmetric_dependent_first_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.or.rhs.", symmetric_dependent_first_second_member
        );
    auto symmetric_dependent_first_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_dependent_first_third_guard
        );
    auto symmetric_dependent_first_third_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = zext i32 ", symmetric_dependent_first_third_source_load
        );
    auto symmetric_dependent_first_third_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_dependent_first_third_cast
        );
    auto symmetric_dependent_first_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_dependent_first_third_member
        );
    auto symmetric_dependent_first_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", symmetric_dependent_first_inner_phi
        );
    auto symmetric_dependent_first_inner_predecessor =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            ", %logical.or.merge.", symmetric_dependent_first_outer_phi
        );
    assert(symmetric_dependent_first_first_guard < symmetric_dependent_first_first_source_load);
    assert(symmetric_dependent_first_first_source_load < symmetric_dependent_first_first_cast);
    assert(symmetric_dependent_first_first_cast < symmetric_dependent_first_first_member);
    assert(symmetric_dependent_first_first_member < symmetric_dependent_first_outer_rhs);
    assert(symmetric_dependent_first_outer_rhs < symmetric_dependent_first_second_guard);
    assert(symmetric_dependent_first_second_guard < symmetric_dependent_first_second_source_load);
    assert(symmetric_dependent_first_second_source_load < symmetric_dependent_first_second_cast);
    assert(symmetric_dependent_first_second_cast < symmetric_dependent_first_second_member);
    assert(symmetric_dependent_first_second_member < symmetric_dependent_first_inner_rhs);
    assert(symmetric_dependent_first_inner_rhs < symmetric_dependent_first_third_guard);
    assert(symmetric_dependent_first_third_guard < symmetric_dependent_first_third_source_load);
    assert(symmetric_dependent_first_third_source_load < symmetric_dependent_first_third_cast);
    assert(symmetric_dependent_first_third_cast < symmetric_dependent_first_third_member);
    assert(symmetric_dependent_first_third_member < symmetric_dependent_first_inner_phi);
    assert(symmetric_dependent_first_inner_phi < symmetric_dependent_first_outer_phi);
    assert(symmetric_dependent_first_outer_phi < symmetric_dependent_first_inner_predecessor);
    auto symmetric_dependent_second_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_dependent_second_first_guard
        );
    auto symmetric_dependent_second_first_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = zext i32 ", symmetric_dependent_second_first_source_load
        );
    auto symmetric_dependent_second_first_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_dependent_second_first_cast
        );
    auto symmetric_dependent_second_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.and.rhs.", symmetric_dependent_second_first_member
        );
    auto symmetric_dependent_second_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_dependent_second_second_guard
        );
    auto symmetric_dependent_second_second_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = zext i32 ", symmetric_dependent_second_second_source_load
        );
    auto symmetric_dependent_second_second_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_dependent_second_second_cast
        );
    auto symmetric_dependent_second_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            "\nlogical.or.rhs.", symmetric_dependent_second_second_member
        );
    auto symmetric_dependent_second_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_dependent_second_third_guard
        );
    auto symmetric_dependent_second_third_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = zext i32 ", symmetric_dependent_second_third_source_load
        );
    auto symmetric_dependent_second_third_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_dependent_second_third_cast
        );
    auto symmetric_dependent_second_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_dependent_second_third_member
        );
    auto symmetric_dependent_second_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", symmetric_dependent_second_inner_phi
        );
    auto symmetric_dependent_second_inner_predecessor =
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_ir.find(
            ", %logical.or.merge.", symmetric_dependent_second_outer_phi
        );
    assert(symmetric_dependent_second_first_guard < symmetric_dependent_second_first_source_load);
    assert(symmetric_dependent_second_first_source_load < symmetric_dependent_second_first_cast);
    assert(symmetric_dependent_second_first_cast < symmetric_dependent_second_first_member);
    assert(symmetric_dependent_second_first_member < symmetric_dependent_second_outer_rhs);
    assert(symmetric_dependent_second_outer_rhs < symmetric_dependent_second_second_guard);
    assert(symmetric_dependent_second_second_guard < symmetric_dependent_second_second_source_load);
    assert(symmetric_dependent_second_second_source_load < symmetric_dependent_second_second_cast);
    assert(symmetric_dependent_second_second_cast < symmetric_dependent_second_second_member);
    assert(symmetric_dependent_second_second_member < symmetric_dependent_second_inner_rhs);
    assert(symmetric_dependent_second_inner_rhs < symmetric_dependent_second_third_guard);
    assert(symmetric_dependent_second_third_guard < symmetric_dependent_second_third_source_load);
    assert(symmetric_dependent_second_third_source_load < symmetric_dependent_second_third_cast);
    assert(symmetric_dependent_second_third_cast < symmetric_dependent_second_third_member);
    assert(symmetric_dependent_second_third_member < symmetric_dependent_second_inner_phi);
    assert(symmetric_dependent_second_inner_phi < symmetric_dependent_second_outer_phi);
    assert(symmetric_dependent_second_outer_phi < symmetric_dependent_second_inner_predecessor);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_symmetric_nested_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_path
    );
    auto runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_dependent_member_paths_left_nested_condition_path.string()
        );
    auto left_nested_dependent_first_first_guard =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto left_nested_dependent_second_first_guard =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", left_nested_dependent_first_first_guard + 1
        );
    auto left_nested_dependent_first_second_guard =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto left_nested_dependent_second_second_guard =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", left_nested_dependent_first_second_guard + 1
        );
    auto left_nested_dependent_first_third_guard =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto left_nested_dependent_second_third_guard =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", left_nested_dependent_first_third_guard + 1
        );
    assert(left_nested_dependent_first_first_guard != std::string::npos);
    assert(left_nested_dependent_second_first_guard != std::string::npos);
    assert(left_nested_dependent_first_second_guard != std::string::npos);
    assert(left_nested_dependent_second_second_guard != std::string::npos);
    assert(left_nested_dependent_first_third_guard != std::string::npos);
    assert(left_nested_dependent_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", left_nested_dependent_second_first_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", left_nested_dependent_second_second_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", left_nested_dependent_second_third_guard + 1
        ) == std::string::npos
    );
    auto left_nested_dependent_first_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i32, ptr ", left_nested_dependent_first_first_guard
        );
    auto left_nested_dependent_first_first_cast =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = zext i32 ", left_nested_dependent_first_first_source_load
        );
    auto left_nested_dependent_first_first_member =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ", left_nested_dependent_first_first_cast
        );
    auto left_nested_dependent_first_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            "\nlogical.and.rhs.", left_nested_dependent_first_first_member
        );
    auto left_nested_dependent_first_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i32, ptr ", left_nested_dependent_first_second_guard
        );
    auto left_nested_dependent_first_second_cast =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = zext i32 ", left_nested_dependent_first_second_source_load
        );
    auto left_nested_dependent_first_second_member =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ", left_nested_dependent_first_second_cast
        );
    auto left_nested_dependent_first_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", left_nested_dependent_first_second_member
        );
    auto left_nested_dependent_first_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            "\nlogical.or.rhs.", left_nested_dependent_first_inner_phi
        );
    auto left_nested_dependent_first_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i32, ptr ", left_nested_dependent_first_third_guard
        );
    auto left_nested_dependent_first_third_cast =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = zext i32 ", left_nested_dependent_first_third_source_load
        );
    auto left_nested_dependent_first_third_member =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ", left_nested_dependent_first_third_cast
        );
    auto left_nested_dependent_first_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.", left_nested_dependent_first_third_member
        );
    auto left_nested_dependent_first_index_phi =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = phi i64 [%first, ", left_nested_dependent_first_outer_phi
        );
    assert(left_nested_dependent_first_first_guard < left_nested_dependent_first_first_source_load);
    assert(left_nested_dependent_first_first_source_load < left_nested_dependent_first_first_cast);
    assert(left_nested_dependent_first_first_cast < left_nested_dependent_first_first_member);
    assert(left_nested_dependent_first_first_member < left_nested_dependent_first_inner_rhs);
    assert(left_nested_dependent_first_inner_rhs < left_nested_dependent_first_second_guard);
    assert(left_nested_dependent_first_second_guard < left_nested_dependent_first_second_source_load);
    assert(left_nested_dependent_first_second_source_load < left_nested_dependent_first_second_cast);
    assert(left_nested_dependent_first_second_cast < left_nested_dependent_first_second_member);
    assert(left_nested_dependent_first_second_member < left_nested_dependent_first_inner_phi);
    assert(left_nested_dependent_first_inner_phi < left_nested_dependent_first_outer_rhs);
    assert(left_nested_dependent_first_outer_rhs < left_nested_dependent_first_third_guard);
    assert(left_nested_dependent_first_third_guard < left_nested_dependent_first_third_source_load);
    assert(left_nested_dependent_first_third_source_load < left_nested_dependent_first_third_cast);
    assert(left_nested_dependent_first_third_cast < left_nested_dependent_first_third_member);
    assert(left_nested_dependent_first_third_member < left_nested_dependent_first_outer_phi);
    assert(left_nested_dependent_first_outer_phi < left_nested_dependent_first_index_phi);
    auto left_nested_dependent_second_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i32, ptr ", left_nested_dependent_second_first_guard
        );
    auto left_nested_dependent_second_first_cast =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = zext i32 ", left_nested_dependent_second_first_source_load
        );
    auto left_nested_dependent_second_first_member =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ", left_nested_dependent_second_first_cast
        );
    auto left_nested_dependent_second_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            "\nlogical.and.rhs.", left_nested_dependent_second_first_member
        );
    auto left_nested_dependent_second_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i32, ptr ", left_nested_dependent_second_second_guard
        );
    auto left_nested_dependent_second_second_cast =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = zext i32 ", left_nested_dependent_second_second_source_load
        );
    auto left_nested_dependent_second_second_member =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ", left_nested_dependent_second_second_cast
        );
    auto left_nested_dependent_second_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", left_nested_dependent_second_second_member
        );
    auto left_nested_dependent_second_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            "\nlogical.or.rhs.", left_nested_dependent_second_inner_phi
        );
    auto left_nested_dependent_second_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i32, ptr ", left_nested_dependent_second_third_guard
        );
    auto left_nested_dependent_second_third_cast =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = zext i32 ", left_nested_dependent_second_third_source_load
        );
    auto left_nested_dependent_second_third_member =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = load i1, ptr ", left_nested_dependent_second_third_cast
        );
    auto left_nested_dependent_second_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.", left_nested_dependent_second_third_member
        );
    auto left_nested_dependent_second_index_phi =
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_ir.find(
            " = phi i64 [%first, ", left_nested_dependent_second_outer_phi
        );
    assert(left_nested_dependent_second_first_guard < left_nested_dependent_second_first_source_load);
    assert(left_nested_dependent_second_first_source_load < left_nested_dependent_second_first_cast);
    assert(left_nested_dependent_second_first_cast < left_nested_dependent_second_first_member);
    assert(left_nested_dependent_second_first_member < left_nested_dependent_second_inner_rhs);
    assert(left_nested_dependent_second_inner_rhs < left_nested_dependent_second_second_guard);
    assert(left_nested_dependent_second_second_guard < left_nested_dependent_second_second_source_load);
    assert(left_nested_dependent_second_second_source_load < left_nested_dependent_second_second_cast);
    assert(left_nested_dependent_second_second_cast < left_nested_dependent_second_second_member);
    assert(left_nested_dependent_second_second_member < left_nested_dependent_second_inner_phi);
    assert(left_nested_dependent_second_inner_phi < left_nested_dependent_second_outer_rhs);
    assert(left_nested_dependent_second_outer_rhs < left_nested_dependent_second_third_guard);
    assert(left_nested_dependent_second_third_guard < left_nested_dependent_second_third_source_load);
    assert(left_nested_dependent_second_third_source_load < left_nested_dependent_second_third_cast);
    assert(left_nested_dependent_second_third_cast < left_nested_dependent_second_third_member);
    assert(left_nested_dependent_second_third_member < left_nested_dependent_second_outer_phi);
    assert(left_nested_dependent_second_outer_phi < left_nested_dependent_second_index_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_left_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_left_nested_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_path
    );
    auto runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_path.string()
        );
    auto symmetric_left_dependent_first_first_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto symmetric_left_dependent_second_first_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", symmetric_left_dependent_first_first_guard + 1
        );
    auto symmetric_left_dependent_first_second_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto symmetric_left_dependent_second_second_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", symmetric_left_dependent_first_second_guard + 1
        );
    auto symmetric_left_dependent_first_third_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto symmetric_left_dependent_second_third_guard =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", symmetric_left_dependent_first_third_guard + 1
        );
    assert(symmetric_left_dependent_first_first_guard != std::string::npos);
    assert(symmetric_left_dependent_second_first_guard != std::string::npos);
    assert(symmetric_left_dependent_first_second_guard != std::string::npos);
    assert(symmetric_left_dependent_second_second_guard != std::string::npos);
    assert(symmetric_left_dependent_first_third_guard != std::string::npos);
    assert(symmetric_left_dependent_second_third_guard != std::string::npos);
    assert(
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", symmetric_left_dependent_second_first_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", symmetric_left_dependent_second_second_guard + 1
        ) == std::string::npos
    );
    assert(
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", symmetric_left_dependent_second_third_guard + 1
        ) == std::string::npos
    );
    auto symmetric_left_dependent_first_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_left_dependent_first_first_guard
        );
    auto symmetric_left_dependent_first_first_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = zext i32 ", symmetric_left_dependent_first_first_source_load
        );
    auto symmetric_left_dependent_first_first_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_left_dependent_first_first_cast
        );
    auto symmetric_left_dependent_first_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.or.rhs.", symmetric_left_dependent_first_first_member
        );
    auto symmetric_left_dependent_first_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_left_dependent_first_second_guard
        );
    auto symmetric_left_dependent_first_second_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = zext i32 ", symmetric_left_dependent_first_second_source_load
        );
    auto symmetric_left_dependent_first_second_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_left_dependent_first_second_cast
        );
    auto symmetric_left_dependent_first_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_left_dependent_first_second_member
        );
    auto symmetric_left_dependent_first_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.and.rhs.", symmetric_left_dependent_first_inner_phi
        );
    auto symmetric_left_dependent_first_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_left_dependent_first_third_guard
        );
    auto symmetric_left_dependent_first_third_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = zext i32 ", symmetric_left_dependent_first_third_source_load
        );
    auto symmetric_left_dependent_first_third_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_left_dependent_first_third_cast
        );
    auto symmetric_left_dependent_first_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.", symmetric_left_dependent_first_third_member
        );
    auto symmetric_left_dependent_first_index_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i64 [%first, ", symmetric_left_dependent_first_outer_phi
        );
    assert(symmetric_left_dependent_first_first_guard < symmetric_left_dependent_first_first_source_load);
    assert(symmetric_left_dependent_first_first_source_load < symmetric_left_dependent_first_first_cast);
    assert(symmetric_left_dependent_first_first_cast < symmetric_left_dependent_first_first_member);
    assert(symmetric_left_dependent_first_first_member < symmetric_left_dependent_first_inner_rhs);
    assert(symmetric_left_dependent_first_inner_rhs < symmetric_left_dependent_first_second_guard);
    assert(symmetric_left_dependent_first_second_guard < symmetric_left_dependent_first_second_source_load);
    assert(symmetric_left_dependent_first_second_source_load < symmetric_left_dependent_first_second_cast);
    assert(symmetric_left_dependent_first_second_cast < symmetric_left_dependent_first_second_member);
    assert(symmetric_left_dependent_first_second_member < symmetric_left_dependent_first_inner_phi);
    assert(symmetric_left_dependent_first_inner_phi < symmetric_left_dependent_first_outer_rhs);
    assert(symmetric_left_dependent_first_outer_rhs < symmetric_left_dependent_first_third_guard);
    assert(symmetric_left_dependent_first_third_guard < symmetric_left_dependent_first_third_source_load);
    assert(symmetric_left_dependent_first_third_source_load < symmetric_left_dependent_first_third_cast);
    assert(symmetric_left_dependent_first_third_cast < symmetric_left_dependent_first_third_member);
    assert(symmetric_left_dependent_first_third_member < symmetric_left_dependent_first_outer_phi);
    assert(symmetric_left_dependent_first_outer_phi < symmetric_left_dependent_first_index_phi);
    auto symmetric_left_dependent_second_first_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_left_dependent_second_first_guard
        );
    auto symmetric_left_dependent_second_first_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = zext i32 ", symmetric_left_dependent_second_first_source_load
        );
    auto symmetric_left_dependent_second_first_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_left_dependent_second_first_cast
        );
    auto symmetric_left_dependent_second_inner_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.or.rhs.", symmetric_left_dependent_second_first_member
        );
    auto symmetric_left_dependent_second_second_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_left_dependent_second_second_guard
        );
    auto symmetric_left_dependent_second_second_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = zext i32 ", symmetric_left_dependent_second_second_source_load
        );
    auto symmetric_left_dependent_second_second_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_left_dependent_second_second_cast
        );
    auto symmetric_left_dependent_second_inner_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_left_dependent_second_second_member
        );
    auto symmetric_left_dependent_second_outer_rhs =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            "\nlogical.and.rhs.", symmetric_left_dependent_second_inner_phi
        );
    auto symmetric_left_dependent_second_third_source_load =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i32, ptr ", symmetric_left_dependent_second_third_guard
        );
    auto symmetric_left_dependent_second_third_cast =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = zext i32 ", symmetric_left_dependent_second_third_source_load
        );
    auto symmetric_left_dependent_second_third_member =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = load i1, ptr ", symmetric_left_dependent_second_third_cast
        );
    auto symmetric_left_dependent_second_outer_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.", symmetric_left_dependent_second_third_member
        );
    auto symmetric_left_dependent_second_index_phi =
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_ir.find(
            " = phi i64 [%first, ", symmetric_left_dependent_second_outer_phi
        );
    assert(symmetric_left_dependent_second_first_guard < symmetric_left_dependent_second_first_source_load);
    assert(symmetric_left_dependent_second_first_source_load < symmetric_left_dependent_second_first_cast);
    assert(symmetric_left_dependent_second_first_cast < symmetric_left_dependent_second_first_member);
    assert(symmetric_left_dependent_second_first_member < symmetric_left_dependent_second_inner_rhs);
    assert(symmetric_left_dependent_second_inner_rhs < symmetric_left_dependent_second_second_guard);
    assert(symmetric_left_dependent_second_second_guard < symmetric_left_dependent_second_second_source_load);
    assert(symmetric_left_dependent_second_second_source_load < symmetric_left_dependent_second_second_cast);
    assert(symmetric_left_dependent_second_second_cast < symmetric_left_dependent_second_second_member);
    assert(symmetric_left_dependent_second_second_member < symmetric_left_dependent_second_inner_phi);
    assert(symmetric_left_dependent_second_inner_phi < symmetric_left_dependent_second_outer_rhs);
    assert(symmetric_left_dependent_second_outer_rhs < symmetric_left_dependent_second_third_guard);
    assert(symmetric_left_dependent_second_third_guard < symmetric_left_dependent_second_third_source_load);
    assert(symmetric_left_dependent_second_third_source_load < symmetric_left_dependent_second_third_cast);
    assert(symmetric_left_dependent_second_third_cast < symmetric_left_dependent_second_third_member);
    assert(symmetric_left_dependent_second_third_member < symmetric_left_dependent_second_outer_phi);
    assert(symmetric_left_dependent_second_outer_phi < symmetric_left_dependent_second_index_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_three_dependent_member_paths_symmetric_left_nested_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_path
    );
    auto runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_four_dependent_member_paths_balanced_condition_path.string()
        );
    auto balanced_dependent_first_first_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto balanced_dependent_second_first_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", balanced_dependent_first_first_guard + 1
        );
    auto balanced_dependent_first_second_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto balanced_dependent_second_second_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", balanced_dependent_first_second_guard + 1
        );
    auto balanced_dependent_first_third_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto balanced_dependent_second_third_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", balanced_dependent_first_third_guard + 1
        );
    auto balanced_dependent_first_fourth_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3"
        );
    auto balanced_dependent_second_fourth_guard =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3", balanced_dependent_first_fourth_guard + 1
        );
    assert(balanced_dependent_first_first_guard != std::string::npos);
    assert(balanced_dependent_second_first_guard != std::string::npos);
    assert(balanced_dependent_first_second_guard != std::string::npos);
    assert(balanced_dependent_second_second_guard != std::string::npos);
    assert(balanced_dependent_first_third_guard != std::string::npos);
    assert(balanced_dependent_second_third_guard != std::string::npos);
    assert(balanced_dependent_first_fourth_guard != std::string::npos);
    assert(balanced_dependent_second_fourth_guard != std::string::npos);
    assert(
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3", balanced_dependent_second_fourth_guard + 1
        ) == std::string::npos
    );
    auto balanced_dependent_first_first_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_first_first_guard
        );
    auto balanced_dependent_first_first_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_first_first_source_load
        );
    auto balanced_dependent_first_second_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_first_second_guard
        );
    auto balanced_dependent_first_second_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_first_second_source_load
        );
    auto balanced_dependent_first_third_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_first_third_guard
        );
    auto balanced_dependent_first_third_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_first_third_source_load
        );
    auto balanced_dependent_first_fourth_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_first_fourth_guard
        );
    auto balanced_dependent_first_fourth_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_first_fourth_source_load
        );
    auto balanced_dependent_second_first_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_second_first_guard
        );
    auto balanced_dependent_second_first_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_second_first_source_load
        );
    auto balanced_dependent_second_second_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_second_second_guard
        );
    auto balanced_dependent_second_second_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_second_second_source_load
        );
    auto balanced_dependent_second_third_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_second_third_guard
        );
    auto balanced_dependent_second_third_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_second_third_source_load
        );
    auto balanced_dependent_second_fourth_source_load =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = load i32, ptr ", balanced_dependent_second_fourth_guard
        );
    auto balanced_dependent_second_fourth_cast =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = zext i32 ", balanced_dependent_second_fourth_source_load
        );
    assert(balanced_dependent_first_first_guard < balanced_dependent_first_first_source_load);
    assert(balanced_dependent_first_first_source_load < balanced_dependent_first_first_cast);
    assert(balanced_dependent_first_second_guard < balanced_dependent_first_second_source_load);
    assert(balanced_dependent_first_second_source_load < balanced_dependent_first_second_cast);
    assert(balanced_dependent_first_third_guard < balanced_dependent_first_third_source_load);
    assert(balanced_dependent_first_third_source_load < balanced_dependent_first_third_cast);
    assert(balanced_dependent_first_fourth_guard < balanced_dependent_first_fourth_source_load);
    assert(balanced_dependent_first_fourth_source_load < balanced_dependent_first_fourth_cast);
    assert(balanced_dependent_second_first_guard < balanced_dependent_second_first_source_load);
    assert(balanced_dependent_second_first_source_load < balanced_dependent_second_first_cast);
    assert(balanced_dependent_second_second_guard < balanced_dependent_second_second_source_load);
    assert(balanced_dependent_second_second_source_load < balanced_dependent_second_second_cast);
    assert(balanced_dependent_second_third_guard < balanced_dependent_second_third_source_load);
    assert(balanced_dependent_second_third_source_load < balanced_dependent_second_third_cast);
    assert(balanced_dependent_second_fourth_guard < balanced_dependent_second_fourth_source_load);
    assert(balanced_dependent_second_fourth_source_load < balanced_dependent_second_fourth_cast);
    auto balanced_dependent_first_left_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds."
        );
    auto balanced_dependent_first_right_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", balanced_dependent_first_left_inner_phi + 1
        );
    auto balanced_dependent_first_outer_phi =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.", balanced_dependent_first_right_inner_phi
        );
    auto balanced_dependent_second_left_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", balanced_dependent_first_outer_phi
        );
    auto balanced_dependent_second_right_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = phi i1 [0, %fixed_array.index.in_bounds.", balanced_dependent_second_left_inner_phi + 1
        );
    auto balanced_dependent_second_outer_phi =
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_ir.find(
            " = phi i1 [1, %logical.and.merge.", balanced_dependent_second_right_inner_phi
        );
    assert(balanced_dependent_first_left_inner_phi != std::string::npos);
    assert(balanced_dependent_first_right_inner_phi != std::string::npos);
    assert(balanced_dependent_first_outer_phi != std::string::npos);
    assert(balanced_dependent_second_left_inner_phi != std::string::npos);
    assert(balanced_dependent_second_right_inner_phi != std::string::npos);
    assert(balanced_dependent_second_outer_phi != std::string::npos);
    assert(balanced_dependent_first_left_inner_phi < balanced_dependent_first_right_inner_phi);
    assert(balanced_dependent_first_right_inner_phi < balanced_dependent_first_outer_phi);
    assert(balanced_dependent_first_outer_phi < balanced_dependent_second_left_inner_phi);
    assert(balanced_dependent_second_left_inner_phi < balanced_dependent_second_right_inner_phi);
    assert(balanced_dependent_second_right_inner_phi < balanced_dependent_second_outer_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_dependent_member_paths_balanced_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_four_dependent_member_paths_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_dependent_member_paths_balanced_condition_for_run"
    );
    assert_run_success(
        executable,
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_path
    );
    auto runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_path.string()
        );
    auto symmetric_balanced_dependent_first_first_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %first_slot, 3"
        );
    auto symmetric_balanced_dependent_second_first_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %first_slot, 3", symmetric_balanced_dependent_first_first_guard + 1
        );
    auto symmetric_balanced_dependent_first_second_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %second_slot, 3"
        );
    auto symmetric_balanced_dependent_second_second_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %second_slot, 3", symmetric_balanced_dependent_first_second_guard + 1
        );
    auto symmetric_balanced_dependent_first_third_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %third_slot, 3"
        );
    auto symmetric_balanced_dependent_second_third_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %third_slot, 3", symmetric_balanced_dependent_first_third_guard + 1
        );
    auto symmetric_balanced_dependent_first_fourth_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3"
        );
    auto symmetric_balanced_dependent_second_fourth_guard =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = icmp ult i64 %fourth_slot, 3", symmetric_balanced_dependent_first_fourth_guard + 1
        );
    assert(symmetric_balanced_dependent_first_first_guard != std::string::npos);
    assert(symmetric_balanced_dependent_second_first_guard != std::string::npos);
    assert(symmetric_balanced_dependent_first_second_guard != std::string::npos);
    assert(symmetric_balanced_dependent_second_second_guard != std::string::npos);
    assert(symmetric_balanced_dependent_first_third_guard != std::string::npos);
    assert(symmetric_balanced_dependent_second_third_guard != std::string::npos);
    assert(symmetric_balanced_dependent_first_fourth_guard != std::string::npos);
    assert(symmetric_balanced_dependent_second_fourth_guard != std::string::npos);
    auto symmetric_balanced_dependent_first_first_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_first_first_guard
        );
    auto symmetric_balanced_dependent_second_first_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_second_first_guard
        );
    auto symmetric_balanced_dependent_first_second_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_first_second_guard
        );
    auto symmetric_balanced_dependent_second_second_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_second_second_guard
        );
    auto symmetric_balanced_dependent_first_third_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_first_third_guard
        );
    auto symmetric_balanced_dependent_second_third_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_second_third_guard
        );
    auto symmetric_balanced_dependent_first_fourth_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_first_fourth_guard
        );
    auto symmetric_balanced_dependent_second_fourth_source_load =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = load i32, ptr ", symmetric_balanced_dependent_second_fourth_guard
        );
    assert(symmetric_balanced_dependent_first_first_guard < symmetric_balanced_dependent_first_first_source_load);
    assert(symmetric_balanced_dependent_second_first_guard < symmetric_balanced_dependent_second_first_source_load);
    assert(symmetric_balanced_dependent_first_second_guard < symmetric_balanced_dependent_first_second_source_load);
    assert(symmetric_balanced_dependent_second_second_guard < symmetric_balanced_dependent_second_second_source_load);
    assert(symmetric_balanced_dependent_first_third_guard < symmetric_balanced_dependent_first_third_source_load);
    assert(symmetric_balanced_dependent_second_third_guard < symmetric_balanced_dependent_second_third_source_load);
    assert(symmetric_balanced_dependent_first_fourth_guard < symmetric_balanced_dependent_first_fourth_source_load);
    assert(symmetric_balanced_dependent_second_fourth_guard < symmetric_balanced_dependent_second_fourth_source_load);
    auto symmetric_balanced_dependent_first_left_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds."
        );
    auto symmetric_balanced_dependent_first_right_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            symmetric_balanced_dependent_first_left_inner_phi + 1
        );
    auto symmetric_balanced_dependent_first_outer_phi =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.", symmetric_balanced_dependent_first_right_inner_phi
        );
    auto symmetric_balanced_dependent_second_left_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.", symmetric_balanced_dependent_first_outer_phi
        );
    auto symmetric_balanced_dependent_second_right_inner_phi =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [1, %fixed_array.index.in_bounds.",
            symmetric_balanced_dependent_second_left_inner_phi + 1
        );
    auto symmetric_balanced_dependent_second_outer_phi =
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_ir.find(
            " = phi i1 [0, %logical.or.merge.", symmetric_balanced_dependent_second_right_inner_phi
        );
    assert(symmetric_balanced_dependent_first_left_inner_phi != std::string::npos);
    assert(symmetric_balanced_dependent_first_right_inner_phi != std::string::npos);
    assert(symmetric_balanced_dependent_first_outer_phi != std::string::npos);
    assert(symmetric_balanced_dependent_second_left_inner_phi != std::string::npos);
    assert(symmetric_balanced_dependent_second_right_inner_phi != std::string::npos);
    assert(symmetric_balanced_dependent_second_outer_phi != std::string::npos);
    assert(symmetric_balanced_dependent_first_left_inner_phi < symmetric_balanced_dependent_first_right_inner_phi);
    assert(symmetric_balanced_dependent_first_right_inner_phi < symmetric_balanced_dependent_first_outer_phi);
    assert(symmetric_balanced_dependent_first_outer_phi < symmetric_balanced_dependent_second_left_inner_phi);
    assert(symmetric_balanced_dependent_second_left_inner_phi < symmetric_balanced_dependent_second_right_inner_phi);
    assert(symmetric_balanced_dependent_second_right_inner_phi < symmetric_balanced_dependent_second_outer_phi);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_four_dependent_member_paths_symmetric_balanced_condition_for_run"
    );
    assert_run_success(executable, runtime_index_boolean_dependent_member_path_condition_path);
    auto runtime_index_boolean_dependent_member_path_condition_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_dependent_member_path_condition_path.string()
        );
    auto boolean_dependent_member_first_slot_guard =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(boolean_dependent_member_first_slot_guard != std::string::npos);
    auto boolean_dependent_member_second_slot_guard =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = icmp ult i64 %slot, 2",
            boolean_dependent_member_first_slot_guard + 1
        );
    assert(boolean_dependent_member_second_slot_guard != std::string::npos);
    assert(
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = icmp ult i64 %slot, 2",
            boolean_dependent_member_second_slot_guard + 1
        ) == std::string::npos
    );
    auto boolean_dependent_member_first_source_index =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "getelementptr [2 x i32]",
            boolean_dependent_member_first_slot_guard
        );
    assert(boolean_dependent_member_first_source_index != std::string::npos);
    auto boolean_dependent_member_first_source_load =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = load i32, ptr ",
            boolean_dependent_member_first_source_index
        );
    assert(boolean_dependent_member_first_source_load != std::string::npos);
    auto boolean_dependent_member_first_source_cast =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = zext i32 ",
            boolean_dependent_member_first_source_load
        );
    assert(boolean_dependent_member_first_source_cast != std::string::npos);
    auto boolean_dependent_member_first_group_guard =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = icmp ult i64 ",
            boolean_dependent_member_first_source_cast
        );
    assert(boolean_dependent_member_first_group_guard != std::string::npos);
    auto boolean_dependent_member_first_group_projection =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_dependent_member_first_group_guard
        );
    assert(boolean_dependent_member_first_group_projection != std::string::npos);
    auto boolean_dependent_member_first_field_projection =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_dependent_member_first_group_projection
        );
    assert(boolean_dependent_member_first_field_projection != std::string::npos);
    auto boolean_dependent_member_first_field_load =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_dependent_member_first_field_projection
        );
    assert(boolean_dependent_member_first_field_load != std::string::npos);
    auto boolean_dependent_member_first_index_phi =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_dependent_member_first_field_load
        );
    assert(boolean_dependent_member_first_index_phi != std::string::npos);
    auto boolean_dependent_member_first_outer_guard =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_dependent_member_first_index_phi
        );
    assert(boolean_dependent_member_first_outer_guard != std::string::npos);
    assert(boolean_dependent_member_first_slot_guard < boolean_dependent_member_first_source_index);
    assert(boolean_dependent_member_first_source_index < boolean_dependent_member_first_source_load);
    assert(boolean_dependent_member_first_source_load < boolean_dependent_member_first_source_cast);
    assert(boolean_dependent_member_first_source_cast < boolean_dependent_member_first_group_guard);
    assert(
        boolean_dependent_member_first_group_guard <
        boolean_dependent_member_first_group_projection
    );
    assert(
        boolean_dependent_member_first_group_projection <
        boolean_dependent_member_first_field_projection
    );
    assert(boolean_dependent_member_first_field_projection < boolean_dependent_member_first_field_load);
    assert(boolean_dependent_member_first_field_load < boolean_dependent_member_first_index_phi);
    assert(boolean_dependent_member_first_index_phi < boolean_dependent_member_first_outer_guard);
    auto boolean_dependent_member_second_source_index =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "getelementptr [2 x i32]",
            boolean_dependent_member_second_slot_guard
        );
    assert(boolean_dependent_member_second_source_index != std::string::npos);
    auto boolean_dependent_member_second_source_load =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = load i32, ptr ",
            boolean_dependent_member_second_source_index
        );
    assert(boolean_dependent_member_second_source_load != std::string::npos);
    auto boolean_dependent_member_second_source_cast =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = zext i32 ",
            boolean_dependent_member_second_source_load
        );
    assert(boolean_dependent_member_second_source_cast != std::string::npos);
    auto boolean_dependent_member_second_group_guard =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = icmp ult i64 ",
            boolean_dependent_member_second_source_cast
        );
    assert(boolean_dependent_member_second_group_guard != std::string::npos);
    auto boolean_dependent_member_second_group_projection =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_dependent_member_second_group_guard
        );
    assert(boolean_dependent_member_second_group_projection != std::string::npos);
    auto boolean_dependent_member_second_field_projection =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_dependent_member_second_group_projection
        );
    assert(boolean_dependent_member_second_field_projection != std::string::npos);
    auto boolean_dependent_member_second_field_load =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = load i1, ptr ",
            boolean_dependent_member_second_field_projection
        );
    assert(boolean_dependent_member_second_field_load != std::string::npos);
    auto boolean_dependent_member_second_index_phi =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            " = phi i64 [%first, ",
            boolean_dependent_member_second_field_load
        );
    assert(boolean_dependent_member_second_index_phi != std::string::npos);
    auto boolean_dependent_member_second_outer_guard =
        runtime_index_boolean_dependent_member_path_condition_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_dependent_member_second_index_phi
        );
    assert(boolean_dependent_member_second_outer_guard != std::string::npos);
    assert(boolean_dependent_member_second_slot_guard < boolean_dependent_member_second_source_index);
    assert(boolean_dependent_member_second_source_index < boolean_dependent_member_second_source_load);
    assert(boolean_dependent_member_second_source_load < boolean_dependent_member_second_source_cast);
    assert(boolean_dependent_member_second_source_cast < boolean_dependent_member_second_group_guard);
    assert(
        boolean_dependent_member_second_group_guard <
        boolean_dependent_member_second_group_projection
    );
    assert(
        boolean_dependent_member_second_group_projection <
        boolean_dependent_member_second_field_projection
    );
    assert(boolean_dependent_member_second_field_projection < boolean_dependent_member_second_field_load);
    assert(boolean_dependent_member_second_field_load < boolean_dependent_member_second_index_phi);
    assert(boolean_dependent_member_second_index_phi < boolean_dependent_member_second_outer_guard);
    assert_emit_object_success(
        executable,
        runtime_index_boolean_dependent_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_dependent_member_path_condition_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_index_boolean_dependent_member_path_condition_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_dependent_member_path_condition_for_run"
    );
    auto runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_path.string()
        );
    auto boolean_dependent_member_then_group_slot_guard =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(boolean_dependent_member_then_group_slot_guard != std::string::npos);
    auto boolean_dependent_member_then_group_source_index =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            boolean_dependent_member_then_group_slot_guard
        );
    assert(boolean_dependent_member_then_group_source_index != std::string::npos);
    auto boolean_dependent_member_then_group_source_load =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            " = load i32, ptr ",
            boolean_dependent_member_then_group_source_index
        );
    assert(boolean_dependent_member_then_group_source_load != std::string::npos);
    auto boolean_dependent_member_then_group_source_cast =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            " = zext i32 ",
            boolean_dependent_member_then_group_source_load
        );
    assert(boolean_dependent_member_then_group_source_cast != std::string::npos);
    auto boolean_dependent_member_then_group_guard =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            boolean_dependent_member_then_group_source_cast
        );
    assert(boolean_dependent_member_then_group_guard != std::string::npos);
    auto boolean_dependent_member_then_group_trap =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_dependent_member_then_group_guard
        );
    assert(boolean_dependent_member_then_group_trap != std::string::npos);
    auto boolean_dependent_member_then_group_projection =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_dependent_member_then_group_trap
        );
    assert(boolean_dependent_member_then_group_projection != std::string::npos);
    auto boolean_dependent_member_then_group_field_projection =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_dependent_member_then_group_projection
        );
    assert(boolean_dependent_member_then_group_field_projection != std::string::npos);
    auto boolean_dependent_member_then_group_field_load =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_dependent_member_then_group_field_projection
        );
    assert(boolean_dependent_member_then_group_field_load != std::string::npos);
    auto boolean_dependent_member_then_group_index_phi =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_dependent_member_then_group_field_load
        );
    assert(boolean_dependent_member_then_group_index_phi != std::string::npos);
    auto boolean_dependent_member_then_group_outer_guard =
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_dependent_member_then_group_index_phi
        );
    assert(boolean_dependent_member_then_group_outer_guard != std::string::npos);
    assert(
        boolean_dependent_member_then_group_slot_guard <
        boolean_dependent_member_then_group_source_index
    );
    assert(
        boolean_dependent_member_then_group_source_index <
        boolean_dependent_member_then_group_source_load
    );
    assert(
        boolean_dependent_member_then_group_source_load <
        boolean_dependent_member_then_group_source_cast
    );
    assert(
        boolean_dependent_member_then_group_source_cast <
        boolean_dependent_member_then_group_guard
    );
    assert(boolean_dependent_member_then_group_guard < boolean_dependent_member_then_group_trap);
    assert(
        boolean_dependent_member_then_group_trap <
        boolean_dependent_member_then_group_projection
    );
    assert(
        boolean_dependent_member_then_group_projection <
        boolean_dependent_member_then_group_field_projection
    );
    assert(
        boolean_dependent_member_then_group_field_projection <
        boolean_dependent_member_then_group_field_load
    );
    assert(
        boolean_dependent_member_then_group_field_load <
        boolean_dependent_member_then_group_index_phi
    );
    assert(
        boolean_dependent_member_then_group_index_phi <
        boolean_dependent_member_then_group_outer_guard
    );
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds.o"
    );
    auto boolean_dependent_member_then_group_out_of_bounds_executable =
        smoke_temp_root /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_dependent_member_path_condition_then_group_out_of_bounds_path.string() +
            " -o " + boolean_dependent_member_then_group_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(boolean_dependent_member_then_group_out_of_bounds_executable) > 0);
    auto runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_path.string()
        );
    auto boolean_dependent_member_else_group_first_slot_guard =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(boolean_dependent_member_else_group_first_slot_guard != std::string::npos);
    auto boolean_dependent_member_else_group_slot_guard =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2",
            boolean_dependent_member_else_group_first_slot_guard + 1
        );
    assert(boolean_dependent_member_else_group_slot_guard != std::string::npos);
    auto boolean_dependent_member_else_group_source_index =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            boolean_dependent_member_else_group_slot_guard
        );
    assert(boolean_dependent_member_else_group_source_index != std::string::npos);
    auto boolean_dependent_member_else_group_source_load =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            " = load i32, ptr ",
            boolean_dependent_member_else_group_source_index
        );
    assert(boolean_dependent_member_else_group_source_load != std::string::npos);
    auto boolean_dependent_member_else_group_source_cast =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            " = zext i32 ",
            boolean_dependent_member_else_group_source_load
        );
    assert(boolean_dependent_member_else_group_source_cast != std::string::npos);
    auto boolean_dependent_member_else_group_guard =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            boolean_dependent_member_else_group_source_cast
        );
    assert(boolean_dependent_member_else_group_guard != std::string::npos);
    auto boolean_dependent_member_else_group_trap =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_dependent_member_else_group_guard
        );
    assert(boolean_dependent_member_else_group_trap != std::string::npos);
    auto boolean_dependent_member_else_group_projection =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_dependent_member_else_group_trap
        );
    assert(boolean_dependent_member_else_group_projection != std::string::npos);
    auto boolean_dependent_member_else_group_field_projection =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_dependent_member_else_group_projection
        );
    assert(boolean_dependent_member_else_group_field_projection != std::string::npos);
    auto boolean_dependent_member_else_group_field_load =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_dependent_member_else_group_field_projection
        );
    assert(boolean_dependent_member_else_group_field_load != std::string::npos);
    auto boolean_dependent_member_else_group_index_phi =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_dependent_member_else_group_field_load
        );
    assert(boolean_dependent_member_else_group_index_phi != std::string::npos);
    auto boolean_dependent_member_else_group_outer_guard =
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_dependent_member_else_group_index_phi
        );
    assert(boolean_dependent_member_else_group_outer_guard != std::string::npos);
    assert(
        boolean_dependent_member_else_group_slot_guard <
        boolean_dependent_member_else_group_source_index
    );
    assert(
        boolean_dependent_member_else_group_source_index <
        boolean_dependent_member_else_group_source_load
    );
    assert(
        boolean_dependent_member_else_group_source_load <
        boolean_dependent_member_else_group_source_cast
    );
    assert(
        boolean_dependent_member_else_group_source_cast <
        boolean_dependent_member_else_group_guard
    );
    assert(boolean_dependent_member_else_group_guard < boolean_dependent_member_else_group_trap);
    assert(
        boolean_dependent_member_else_group_trap <
        boolean_dependent_member_else_group_projection
    );
    assert(
        boolean_dependent_member_else_group_projection <
        boolean_dependent_member_else_group_field_projection
    );
    assert(
        boolean_dependent_member_else_group_field_projection <
        boolean_dependent_member_else_group_field_load
    );
    assert(
        boolean_dependent_member_else_group_field_load <
        boolean_dependent_member_else_group_index_phi
    );
    assert(
        boolean_dependent_member_else_group_index_phi <
        boolean_dependent_member_else_group_outer_guard
    );
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds.o"
    );
    auto boolean_dependent_member_else_group_out_of_bounds_executable =
        smoke_temp_root /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_dependent_member_path_condition_else_group_out_of_bounds_path.string() +
            " -o " + boolean_dependent_member_else_group_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(boolean_dependent_member_else_group_out_of_bounds_executable) > 0);
    auto runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_path.string()
        );
    auto boolean_dependent_member_then_slot_guard =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(boolean_dependent_member_then_slot_guard != std::string::npos);
    auto boolean_dependent_member_then_slot_trap =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_dependent_member_then_slot_guard
        );
    assert(boolean_dependent_member_then_slot_trap != std::string::npos);
    auto boolean_dependent_member_then_slot_source_index =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            boolean_dependent_member_then_slot_trap
        );
    assert(boolean_dependent_member_then_slot_source_index != std::string::npos);
    auto boolean_dependent_member_then_slot_source_load =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            " = load i32, ptr ",
            boolean_dependent_member_then_slot_source_index
        );
    assert(boolean_dependent_member_then_slot_source_load != std::string::npos);
    auto boolean_dependent_member_then_slot_source_cast =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            " = zext i32 ",
            boolean_dependent_member_then_slot_source_load
        );
    assert(boolean_dependent_member_then_slot_source_cast != std::string::npos);
    auto boolean_dependent_member_then_slot_group_guard =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            boolean_dependent_member_then_slot_source_cast
        );
    assert(boolean_dependent_member_then_slot_group_guard != std::string::npos);
    auto boolean_dependent_member_then_slot_group_projection =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_dependent_member_then_slot_group_guard
        );
    assert(boolean_dependent_member_then_slot_group_projection != std::string::npos);
    auto boolean_dependent_member_then_slot_field_projection =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_dependent_member_then_slot_group_projection
        );
    assert(boolean_dependent_member_then_slot_field_projection != std::string::npos);
    auto boolean_dependent_member_then_slot_field_load =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_dependent_member_then_slot_field_projection
        );
    assert(boolean_dependent_member_then_slot_field_load != std::string::npos);
    auto boolean_dependent_member_then_slot_index_phi =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_dependent_member_then_slot_field_load
        );
    assert(boolean_dependent_member_then_slot_index_phi != std::string::npos);
    auto boolean_dependent_member_then_slot_outer_guard =
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_dependent_member_then_slot_index_phi
        );
    assert(boolean_dependent_member_then_slot_outer_guard != std::string::npos);
    assert(boolean_dependent_member_then_slot_guard < boolean_dependent_member_then_slot_trap);
    assert(
        boolean_dependent_member_then_slot_trap <
        boolean_dependent_member_then_slot_source_index
    );
    assert(
        boolean_dependent_member_then_slot_source_index <
        boolean_dependent_member_then_slot_source_load
    );
    assert(
        boolean_dependent_member_then_slot_source_load <
        boolean_dependent_member_then_slot_source_cast
    );
    assert(
        boolean_dependent_member_then_slot_source_cast <
        boolean_dependent_member_then_slot_group_guard
    );
    assert(
        boolean_dependent_member_then_slot_group_guard <
        boolean_dependent_member_then_slot_group_projection
    );
    assert(
        boolean_dependent_member_then_slot_group_projection <
        boolean_dependent_member_then_slot_field_projection
    );
    assert(
        boolean_dependent_member_then_slot_field_projection <
        boolean_dependent_member_then_slot_field_load
    );
    assert(
        boolean_dependent_member_then_slot_field_load <
        boolean_dependent_member_then_slot_index_phi
    );
    assert(
        boolean_dependent_member_then_slot_index_phi <
        boolean_dependent_member_then_slot_outer_guard
    );
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds.o"
    );
    auto boolean_dependent_member_then_slot_out_of_bounds_executable =
        smoke_temp_root /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_dependent_member_path_condition_then_slot_out_of_bounds_path.string() +
            " -o " + boolean_dependent_member_then_slot_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(boolean_dependent_member_then_slot_out_of_bounds_executable) > 0);
    auto runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_path.string()
        );
    auto boolean_dependent_member_else_slot_first_guard =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(boolean_dependent_member_else_slot_first_guard != std::string::npos);
    auto boolean_dependent_member_else_slot_guard =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2",
            boolean_dependent_member_else_slot_first_guard + 1
        );
    assert(boolean_dependent_member_else_slot_guard != std::string::npos);
    auto boolean_dependent_member_else_slot_trap =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_dependent_member_else_slot_guard
        );
    assert(boolean_dependent_member_else_slot_trap != std::string::npos);
    auto boolean_dependent_member_else_slot_source_index =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            boolean_dependent_member_else_slot_trap
        );
    assert(boolean_dependent_member_else_slot_source_index != std::string::npos);
    auto boolean_dependent_member_else_slot_source_load =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            " = load i32, ptr ",
            boolean_dependent_member_else_slot_source_index
        );
    assert(boolean_dependent_member_else_slot_source_load != std::string::npos);
    auto boolean_dependent_member_else_slot_source_cast =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            " = zext i32 ",
            boolean_dependent_member_else_slot_source_load
        );
    assert(boolean_dependent_member_else_slot_source_cast != std::string::npos);
    auto boolean_dependent_member_else_slot_group_guard =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            boolean_dependent_member_else_slot_source_cast
        );
    assert(boolean_dependent_member_else_slot_group_guard != std::string::npos);
    auto boolean_dependent_member_else_slot_group_projection =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_dependent_member_else_slot_group_guard
        );
    assert(boolean_dependent_member_else_slot_group_projection != std::string::npos);
    auto boolean_dependent_member_else_slot_field_projection =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_dependent_member_else_slot_group_projection
        );
    assert(boolean_dependent_member_else_slot_field_projection != std::string::npos);
    auto boolean_dependent_member_else_slot_field_load =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_dependent_member_else_slot_field_projection
        );
    assert(boolean_dependent_member_else_slot_field_load != std::string::npos);
    auto boolean_dependent_member_else_slot_index_phi =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_dependent_member_else_slot_field_load
        );
    assert(boolean_dependent_member_else_slot_index_phi != std::string::npos);
    auto boolean_dependent_member_else_slot_outer_guard =
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_dependent_member_else_slot_index_phi
        );
    assert(boolean_dependent_member_else_slot_outer_guard != std::string::npos);
    assert(boolean_dependent_member_else_slot_guard < boolean_dependent_member_else_slot_trap);
    assert(
        boolean_dependent_member_else_slot_trap <
        boolean_dependent_member_else_slot_source_index
    );
    assert(
        boolean_dependent_member_else_slot_source_index <
        boolean_dependent_member_else_slot_source_load
    );
    assert(
        boolean_dependent_member_else_slot_source_load <
        boolean_dependent_member_else_slot_source_cast
    );
    assert(
        boolean_dependent_member_else_slot_source_cast <
        boolean_dependent_member_else_slot_group_guard
    );
    assert(
        boolean_dependent_member_else_slot_group_guard <
        boolean_dependent_member_else_slot_group_projection
    );
    assert(
        boolean_dependent_member_else_slot_group_projection <
        boolean_dependent_member_else_slot_field_projection
    );
    assert(
        boolean_dependent_member_else_slot_field_projection <
        boolean_dependent_member_else_slot_field_load
    );
    assert(
        boolean_dependent_member_else_slot_field_load <
        boolean_dependent_member_else_slot_index_phi
    );
    assert(
        boolean_dependent_member_else_slot_index_phi <
        boolean_dependent_member_else_slot_outer_guard
    );
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds.o"
    );
    auto boolean_dependent_member_else_slot_out_of_bounds_executable =
        smoke_temp_root /
        "dynamic_array_runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_dependent_member_path_condition_else_slot_out_of_bounds_path.string() +
            " -o " + boolean_dependent_member_else_slot_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(boolean_dependent_member_else_slot_out_of_bounds_executable) > 0);
    auto runtime_index_boolean_member_path_condition_then_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_member_path_condition_then_out_of_bounds_path.string()
        );
    auto boolean_member_path_then_guard =
        runtime_index_boolean_member_path_condition_then_out_of_bounds_ir.find(
            " = icmp ult i64 %group_slot, 2"
        );
    assert(boolean_member_path_then_guard != std::string::npos);
    auto boolean_member_path_then_trap =
        runtime_index_boolean_member_path_condition_then_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_member_path_then_guard
        );
    assert(boolean_member_path_then_trap != std::string::npos);
    auto boolean_member_path_then_array_projection =
        runtime_index_boolean_member_path_condition_then_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_member_path_then_trap
        );
    assert(boolean_member_path_then_array_projection != std::string::npos);
    auto boolean_member_path_then_field_projection =
        runtime_index_boolean_member_path_condition_then_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_member_path_then_array_projection
        );
    assert(boolean_member_path_then_field_projection != std::string::npos);
    auto boolean_member_path_then_load =
        runtime_index_boolean_member_path_condition_then_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_member_path_then_field_projection
        );
    assert(boolean_member_path_then_load != std::string::npos);
    auto boolean_member_path_then_index_phi =
        runtime_index_boolean_member_path_condition_then_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_member_path_then_load
        );
    assert(boolean_member_path_then_index_phi != std::string::npos);
    auto boolean_member_path_then_outer_guard =
        runtime_index_boolean_member_path_condition_then_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_member_path_then_index_phi
        );
    assert(boolean_member_path_then_outer_guard != std::string::npos);
    assert(boolean_member_path_then_guard < boolean_member_path_then_trap);
    assert(boolean_member_path_then_trap < boolean_member_path_then_array_projection);
    assert(boolean_member_path_then_array_projection < boolean_member_path_then_field_projection);
    assert(boolean_member_path_then_field_projection < boolean_member_path_then_load);
    assert(boolean_member_path_then_load < boolean_member_path_then_index_phi);
    assert(boolean_member_path_then_index_phi < boolean_member_path_then_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_member_path_condition_then_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_member_path_condition_then_out_of_bounds_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_member_path_condition_then_out_of_bounds.o"
    );
    auto runtime_index_boolean_member_path_condition_then_out_of_bounds_executable =
        smoke_temp_root /
        "dynamic_array_runtime_index_boolean_member_path_condition_then_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_member_path_condition_then_out_of_bounds_path.string() +
            " -o " +
            runtime_index_boolean_member_path_condition_then_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_boolean_member_path_condition_then_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_boolean_member_path_condition_else_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_member_path_condition_else_out_of_bounds_path.string()
        );
    auto boolean_member_path_else_first_guard =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            " = icmp ult i64 %group_slot, 2"
        );
    assert(boolean_member_path_else_first_guard != std::string::npos);
    auto boolean_member_path_else_guard =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            " = icmp ult i64 %group_slot, 2",
            boolean_member_path_else_first_guard + 1
        );
    assert(boolean_member_path_else_guard != std::string::npos);
    auto boolean_member_path_else_trap =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_member_path_else_guard
        );
    assert(boolean_member_path_else_trap != std::string::npos);
    auto boolean_member_path_else_array_projection =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            boolean_member_path_else_trap
        );
    assert(boolean_member_path_else_array_projection != std::string::npos);
    auto boolean_member_path_else_field_projection =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            boolean_member_path_else_array_projection
        );
    assert(boolean_member_path_else_field_projection != std::string::npos);
    auto boolean_member_path_else_load =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_member_path_else_field_projection
        );
    assert(boolean_member_path_else_load != std::string::npos);
    auto boolean_member_path_else_index_phi =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_member_path_else_load
        );
    assert(boolean_member_path_else_index_phi != std::string::npos);
    auto boolean_member_path_else_outer_guard =
        runtime_index_boolean_member_path_condition_else_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_member_path_else_index_phi
        );
    assert(boolean_member_path_else_outer_guard != std::string::npos);
    assert(boolean_member_path_else_guard < boolean_member_path_else_trap);
    assert(boolean_member_path_else_trap < boolean_member_path_else_array_projection);
    assert(boolean_member_path_else_array_projection < boolean_member_path_else_field_projection);
    assert(boolean_member_path_else_field_projection < boolean_member_path_else_load);
    assert(boolean_member_path_else_load < boolean_member_path_else_index_phi);
    assert(boolean_member_path_else_index_phi < boolean_member_path_else_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_member_path_condition_else_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_member_path_condition_else_out_of_bounds_path,
        smoke_temp_root /
            "dynamic_array_runtime_index_boolean_member_path_condition_else_out_of_bounds.o"
    );
    auto runtime_index_boolean_member_path_condition_else_out_of_bounds_executable =
        smoke_temp_root /
        "dynamic_array_runtime_index_boolean_member_path_condition_else_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_member_path_condition_else_out_of_bounds_path.string() +
            " -o " +
            runtime_index_boolean_member_path_condition_else_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_boolean_member_path_condition_else_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_boolean_path_condition_then_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_path_condition_then_out_of_bounds_path.string()
        );
    auto boolean_path_then_condition_guard =
        runtime_index_boolean_path_condition_then_out_of_bounds_ir.find(
            " = icmp ult i64 %condition_slot, 2"
        );
    assert(boolean_path_then_condition_guard != std::string::npos);
    auto boolean_path_then_trap =
        runtime_index_boolean_path_condition_then_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_path_then_condition_guard
        );
    assert(boolean_path_then_trap != std::string::npos);
    auto boolean_path_then_projection =
        runtime_index_boolean_path_condition_then_out_of_bounds_ir.find(
            "getelementptr [2 x i1]",
            boolean_path_then_trap
        );
    assert(boolean_path_then_projection != std::string::npos);
    auto boolean_path_then_load =
        runtime_index_boolean_path_condition_then_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_path_then_projection
        );
    assert(boolean_path_then_load != std::string::npos);
    auto boolean_path_then_index_phi =
        runtime_index_boolean_path_condition_then_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_path_then_load
        );
    assert(boolean_path_then_index_phi != std::string::npos);
    auto boolean_path_then_outer_guard =
        runtime_index_boolean_path_condition_then_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_path_then_index_phi
        );
    assert(boolean_path_then_outer_guard != std::string::npos);
    assert(boolean_path_then_condition_guard < boolean_path_then_trap);
    assert(boolean_path_then_trap < boolean_path_then_projection);
    assert(boolean_path_then_projection < boolean_path_then_load);
    assert(boolean_path_then_load < boolean_path_then_index_phi);
    assert(boolean_path_then_index_phi < boolean_path_then_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_path_condition_then_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_path_condition_then_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_path_condition_then_out_of_bounds.o"
    );
    auto runtime_index_boolean_path_condition_then_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_boolean_path_condition_then_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_path_condition_then_out_of_bounds_path.string() + " -o " +
            runtime_index_boolean_path_condition_then_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_boolean_path_condition_then_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_boolean_path_condition_else_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_boolean_path_condition_else_out_of_bounds_path.string()
        );
    auto boolean_path_else_first_condition_guard =
        runtime_index_boolean_path_condition_else_out_of_bounds_ir.find(
            " = icmp ult i64 %condition_slot, 2"
        );
    assert(boolean_path_else_first_condition_guard != std::string::npos);
    auto boolean_path_else_condition_guard =
        runtime_index_boolean_path_condition_else_out_of_bounds_ir.find(
            " = icmp ult i64 %condition_slot, 2",
            boolean_path_else_first_condition_guard + 1
        );
    assert(boolean_path_else_condition_guard != std::string::npos);
    auto boolean_path_else_trap =
        runtime_index_boolean_path_condition_else_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            boolean_path_else_condition_guard
        );
    assert(boolean_path_else_trap != std::string::npos);
    auto boolean_path_else_projection =
        runtime_index_boolean_path_condition_else_out_of_bounds_ir.find(
            "getelementptr [2 x i1]",
            boolean_path_else_trap
        );
    assert(boolean_path_else_projection != std::string::npos);
    auto boolean_path_else_load =
        runtime_index_boolean_path_condition_else_out_of_bounds_ir.find(
            " = load i1, ptr ",
            boolean_path_else_projection
        );
    assert(boolean_path_else_load != std::string::npos);
    auto boolean_path_else_index_phi =
        runtime_index_boolean_path_condition_else_out_of_bounds_ir.find(
            " = phi i64 [%first, ",
            boolean_path_else_load
        );
    assert(boolean_path_else_index_phi != std::string::npos);
    auto boolean_path_else_outer_guard =
        runtime_index_boolean_path_condition_else_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            boolean_path_else_index_phi
        );
    assert(boolean_path_else_outer_guard != std::string::npos);
    assert(boolean_path_else_condition_guard < boolean_path_else_trap);
    assert(boolean_path_else_trap < boolean_path_else_projection);
    assert(boolean_path_else_projection < boolean_path_else_load);
    assert(boolean_path_else_load < boolean_path_else_index_phi);
    assert(boolean_path_else_index_phi < boolean_path_else_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_boolean_path_condition_else_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_boolean_path_condition_else_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_boolean_path_condition_else_out_of_bounds.o"
    );
    auto runtime_index_boolean_path_condition_else_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_boolean_path_condition_else_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_boolean_path_condition_else_out_of_bounds_path.string() + " -o " +
            runtime_index_boolean_path_condition_else_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_boolean_path_condition_else_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_source_dependent_member_then_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_source_dependent_member_then_out_of_bounds_path.string()
        );
    auto dependent_member_then_slot_guard =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(dependent_member_then_slot_guard != std::string::npos);
    auto dependent_member_then_source_index =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            dependent_member_then_slot_guard
        );
    assert(dependent_member_then_source_index != std::string::npos);
    auto dependent_member_then_source_load =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            " = load i32, ptr ",
            dependent_member_then_source_index
        );
    assert(dependent_member_then_source_load != std::string::npos);
    auto dependent_member_then_source_cast =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            " = zext i32 ",
            dependent_member_then_source_load
        );
    assert(dependent_member_then_source_cast != std::string::npos);
    auto dependent_member_then_group_guard =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            dependent_member_then_source_cast
        );
    assert(dependent_member_then_group_guard != std::string::npos);
    auto dependent_member_then_group_trap =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            dependent_member_then_group_guard
        );
    assert(dependent_member_then_group_trap != std::string::npos);
    auto dependent_member_then_group_projection =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            dependent_member_then_group_trap
        );
    assert(dependent_member_then_group_projection != std::string::npos);
    auto dependent_member_then_field_projection =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            dependent_member_then_group_projection
        );
    assert(dependent_member_then_field_projection != std::string::npos);
    auto dependent_member_then_field_load =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            " = load i32, ptr ",
            dependent_member_then_field_projection
        );
    assert(dependent_member_then_field_load != std::string::npos);
    auto dependent_member_then_outer_guard =
        runtime_index_source_dependent_member_then_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            dependent_member_then_field_load
        );
    assert(dependent_member_then_outer_guard != std::string::npos);
    assert(dependent_member_then_slot_guard < dependent_member_then_source_index);
    assert(dependent_member_then_source_index < dependent_member_then_source_load);
    assert(dependent_member_then_source_load < dependent_member_then_source_cast);
    assert(dependent_member_then_source_cast < dependent_member_then_group_guard);
    assert(dependent_member_then_group_guard < dependent_member_then_group_trap);
    assert(dependent_member_then_group_trap < dependent_member_then_group_projection);
    assert(dependent_member_then_group_projection < dependent_member_then_field_projection);
    assert(dependent_member_then_field_projection < dependent_member_then_field_load);
    assert(dependent_member_then_field_load < dependent_member_then_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_source_dependent_member_then_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_member_then_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_member_then_out_of_bounds.o"
    );
    auto runtime_index_source_dependent_member_then_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_member_then_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_dependent_member_then_out_of_bounds_path.string() + " -o " +
            runtime_index_source_dependent_member_then_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_source_dependent_member_then_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_source_dependent_member_else_out_of_bounds_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_index_source_dependent_member_else_out_of_bounds_path.string()
        );
    auto dependent_member_else_first_slot_guard =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(dependent_member_else_first_slot_guard != std::string::npos);
    auto dependent_member_else_slot_guard =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2",
            dependent_member_else_first_slot_guard + 1
        );
    assert(dependent_member_else_slot_guard != std::string::npos);
    auto dependent_member_else_source_index =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            dependent_member_else_slot_guard
        );
    assert(dependent_member_else_source_index != std::string::npos);
    auto dependent_member_else_source_load =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            " = load i32, ptr ",
            dependent_member_else_source_index
        );
    assert(dependent_member_else_source_load != std::string::npos);
    auto dependent_member_else_source_cast =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            " = zext i32 ",
            dependent_member_else_source_load
        );
    assert(dependent_member_else_source_cast != std::string::npos);
    auto dependent_member_else_group_guard =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            dependent_member_else_source_cast
        );
    assert(dependent_member_else_group_guard != std::string::npos);
    auto dependent_member_else_group_trap =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            dependent_member_else_group_guard
        );
    assert(dependent_member_else_group_trap != std::string::npos);
    auto dependent_member_else_group_projection =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            "getelementptr [2 x %record.IndexConfig]",
            dependent_member_else_group_trap
        );
    assert(dependent_member_else_group_projection != std::string::npos);
    auto dependent_member_else_field_projection =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            "getelementptr %record.IndexConfig",
            dependent_member_else_group_projection
        );
    assert(dependent_member_else_field_projection != std::string::npos);
    auto dependent_member_else_field_load =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            " = load i32, ptr ",
            dependent_member_else_field_projection
        );
    assert(dependent_member_else_field_load != std::string::npos);
    auto dependent_member_else_outer_guard =
        runtime_index_source_dependent_member_else_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            dependent_member_else_field_load
        );
    assert(dependent_member_else_outer_guard != std::string::npos);
    assert(dependent_member_else_slot_guard < dependent_member_else_source_index);
    assert(dependent_member_else_source_index < dependent_member_else_source_load);
    assert(dependent_member_else_source_load < dependent_member_else_source_cast);
    assert(dependent_member_else_source_cast < dependent_member_else_group_guard);
    assert(dependent_member_else_group_guard < dependent_member_else_group_trap);
    assert(dependent_member_else_group_trap < dependent_member_else_group_projection);
    assert(dependent_member_else_group_projection < dependent_member_else_field_projection);
    assert(dependent_member_else_field_projection < dependent_member_else_field_load);
    assert(dependent_member_else_field_load < dependent_member_else_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_source_dependent_member_else_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_member_else_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_member_else_out_of_bounds.o"
    );
    auto runtime_index_source_dependent_member_else_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_member_else_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_dependent_member_else_out_of_bounds_path.string() + " -o " +
            runtime_index_source_dependent_member_else_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_source_dependent_member_else_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_source_dependent_then_slot_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_source_dependent_then_slot_out_of_bounds_path.string()
    );
    auto dependent_then_slot_failure_guard =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(dependent_then_slot_failure_guard != std::string::npos);
    auto dependent_then_slot_failure_trap =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            dependent_then_slot_failure_guard
        );
    assert(dependent_then_slot_failure_trap != std::string::npos);
    auto dependent_then_slot_failure_source_index =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            dependent_then_slot_failure_trap
        );
    assert(dependent_then_slot_failure_source_index != std::string::npos);
    auto dependent_then_slot_failure_source_load =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            " = load i32, ptr ",
            dependent_then_slot_failure_source_index
        );
    assert(dependent_then_slot_failure_source_load != std::string::npos);
    auto dependent_then_slot_failure_source_cast =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            " = zext i32 ",
            dependent_then_slot_failure_source_load
        );
    assert(dependent_then_slot_failure_source_cast != std::string::npos);
    auto dependent_then_slot_failure_row_guard =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            dependent_then_slot_failure_source_cast
        );
    assert(dependent_then_slot_failure_row_guard != std::string::npos);
    auto dependent_then_slot_failure_column_guard =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %column, 2",
            dependent_then_slot_failure_row_guard
        );
    assert(dependent_then_slot_failure_column_guard != std::string::npos);
    auto dependent_then_slot_failure_outer_guard =
        runtime_index_source_dependent_then_slot_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            dependent_then_slot_failure_column_guard
        );
    assert(dependent_then_slot_failure_outer_guard != std::string::npos);
    assert(dependent_then_slot_failure_guard < dependent_then_slot_failure_trap);
    assert(dependent_then_slot_failure_trap < dependent_then_slot_failure_source_index);
    assert(dependent_then_slot_failure_source_index < dependent_then_slot_failure_source_load);
    assert(dependent_then_slot_failure_source_load < dependent_then_slot_failure_source_cast);
    assert(dependent_then_slot_failure_source_cast < dependent_then_slot_failure_row_guard);
    assert(dependent_then_slot_failure_row_guard < dependent_then_slot_failure_column_guard);
    assert(dependent_then_slot_failure_column_guard < dependent_then_slot_failure_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_source_dependent_then_slot_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_then_slot_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_then_slot_out_of_bounds.o"
    );
    auto runtime_index_source_dependent_then_slot_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_then_slot_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_dependent_then_slot_out_of_bounds_path.string() + " -o " +
            runtime_index_source_dependent_then_slot_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_source_dependent_then_slot_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_source_dependent_else_slot_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_source_dependent_else_slot_out_of_bounds_path.string()
    );
    auto dependent_else_first_slot_failure_guard =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2"
        );
    assert(dependent_else_first_slot_failure_guard != std::string::npos);
    auto dependent_else_slot_failure_guard =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %slot, 2",
            dependent_else_first_slot_failure_guard + 1
        );
    assert(dependent_else_slot_failure_guard != std::string::npos);
    auto dependent_else_slot_failure_trap =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            "call void @__orison_dynamic_array_bounds_failed()",
            dependent_else_slot_failure_guard
        );
    assert(dependent_else_slot_failure_trap != std::string::npos);
    auto dependent_else_slot_failure_source_index =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            "getelementptr [2 x i32]",
            dependent_else_slot_failure_trap
        );
    assert(dependent_else_slot_failure_source_index != std::string::npos);
    auto dependent_else_slot_failure_source_load =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            " = load i32, ptr ",
            dependent_else_slot_failure_source_index
        );
    assert(dependent_else_slot_failure_source_load != std::string::npos);
    auto dependent_else_slot_failure_source_cast =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            " = zext i32 ",
            dependent_else_slot_failure_source_load
        );
    assert(dependent_else_slot_failure_source_cast != std::string::npos);
    auto dependent_else_slot_failure_row_guard =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            " = icmp ult i64 ",
            dependent_else_slot_failure_source_cast
        );
    assert(dependent_else_slot_failure_row_guard != std::string::npos);
    auto dependent_else_slot_failure_column_guard =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            " = icmp ult i64 %column, 2",
            dependent_else_slot_failure_row_guard
        );
    assert(dependent_else_slot_failure_column_guard != std::string::npos);
    auto dependent_else_slot_failure_outer_guard =
        runtime_index_source_dependent_else_slot_out_of_bounds_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            dependent_else_slot_failure_column_guard
        );
    assert(dependent_else_slot_failure_outer_guard != std::string::npos);
    assert(dependent_else_slot_failure_guard < dependent_else_slot_failure_trap);
    assert(dependent_else_slot_failure_trap < dependent_else_slot_failure_source_index);
    assert(dependent_else_slot_failure_source_index < dependent_else_slot_failure_source_load);
    assert(dependent_else_slot_failure_source_load < dependent_else_slot_failure_source_cast);
    assert(dependent_else_slot_failure_source_cast < dependent_else_slot_failure_row_guard);
    assert(dependent_else_slot_failure_row_guard < dependent_else_slot_failure_column_guard);
    assert(dependent_else_slot_failure_column_guard < dependent_else_slot_failure_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_source_dependent_else_slot_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_else_slot_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_else_slot_out_of_bounds.o"
    );
    auto runtime_index_source_dependent_else_slot_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_else_slot_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_dependent_else_slot_out_of_bounds_path.string() + " -o " +
            runtime_index_source_dependent_else_slot_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_source_dependent_else_slot_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_source_dependent_then_row_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_source_dependent_then_row_out_of_bounds_path.string()
    );
    auto dependent_then_slot_guard = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        " = icmp ult i64 %slot, 2"
    );
    assert(dependent_then_slot_guard != std::string::npos);
    auto dependent_then_source_index = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        "getelementptr [2 x i32]",
        dependent_then_slot_guard
    );
    assert(dependent_then_source_index != std::string::npos);
    auto dependent_then_source_load = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        " = load i32, ptr ",
        dependent_then_source_index
    );
    assert(dependent_then_source_load != std::string::npos);
    auto dependent_then_source_cast = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        " = zext i32 ",
        dependent_then_source_load
    );
    assert(dependent_then_source_cast != std::string::npos);
    auto dependent_then_row_guard = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        " = icmp ult i64 ",
        dependent_then_source_cast
    );
    assert(dependent_then_row_guard != std::string::npos);
    auto dependent_then_row_trap = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        "call void @__orison_dynamic_array_bounds_failed()",
        dependent_then_row_guard
    );
    assert(dependent_then_row_trap != std::string::npos);
    auto dependent_then_column_guard = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        " = icmp ult i64 %column, 2",
        dependent_then_row_trap
    );
    assert(dependent_then_column_guard != std::string::npos);
    auto dependent_then_outer_guard = runtime_index_source_dependent_then_row_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        dependent_then_column_guard
    );
    assert(dependent_then_outer_guard != std::string::npos);
    assert(dependent_then_slot_guard < dependent_then_source_index);
    assert(dependent_then_source_index < dependent_then_source_load);
    assert(dependent_then_source_load < dependent_then_source_cast);
    assert(dependent_then_source_cast < dependent_then_row_guard);
    assert(dependent_then_row_guard < dependent_then_row_trap);
    assert(dependent_then_row_trap < dependent_then_column_guard);
    assert(dependent_then_column_guard < dependent_then_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_source_dependent_then_row_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_then_row_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_then_row_out_of_bounds.o"
    );
    auto runtime_index_source_dependent_then_row_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_then_row_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_dependent_then_row_out_of_bounds_path.string() + " -o " +
            runtime_index_source_dependent_then_row_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_source_dependent_then_row_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_source_dependent_else_row_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_index_source_dependent_else_row_out_of_bounds_path.string()
    );
    auto dependent_else_first_slot_guard = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        " = icmp ult i64 %slot, 2"
    );
    assert(dependent_else_first_slot_guard != std::string::npos);
    auto dependent_else_slot_guard = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        " = icmp ult i64 %slot, 2",
        dependent_else_first_slot_guard + 1
    );
    assert(dependent_else_slot_guard != std::string::npos);
    auto dependent_else_source_index = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        "getelementptr [2 x i32]",
        dependent_else_slot_guard
    );
    assert(dependent_else_source_index != std::string::npos);
    auto dependent_else_source_load = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        " = load i32, ptr ",
        dependent_else_source_index
    );
    assert(dependent_else_source_load != std::string::npos);
    auto dependent_else_source_cast = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        " = zext i32 ",
        dependent_else_source_load
    );
    assert(dependent_else_source_cast != std::string::npos);
    auto dependent_else_row_guard = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        " = icmp ult i64 ",
        dependent_else_source_cast
    );
    assert(dependent_else_row_guard != std::string::npos);
    auto dependent_else_row_trap = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        "call void @__orison_dynamic_array_bounds_failed()",
        dependent_else_row_guard
    );
    assert(dependent_else_row_trap != std::string::npos);
    auto dependent_else_column_guard = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        " = icmp ult i64 %column, 2",
        dependent_else_row_trap
    );
    assert(dependent_else_column_guard != std::string::npos);
    auto dependent_else_outer_guard = runtime_index_source_dependent_else_row_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        dependent_else_column_guard
    );
    assert(dependent_else_outer_guard != std::string::npos);
    assert(dependent_else_slot_guard < dependent_else_source_index);
    assert(dependent_else_source_index < dependent_else_source_load);
    assert(dependent_else_source_load < dependent_else_source_cast);
    assert(dependent_else_source_cast < dependent_else_row_guard);
    assert(dependent_else_row_guard < dependent_else_row_trap);
    assert(dependent_else_row_trap < dependent_else_column_guard);
    assert(dependent_else_column_guard < dependent_else_outer_guard);
    read_failing_command_output(
        executable.string() + " run " +
        runtime_index_source_dependent_else_row_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_dependent_else_row_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_else_row_out_of_bounds.o"
    );
    auto runtime_index_source_dependent_else_row_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_dependent_else_row_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_dependent_else_row_out_of_bounds_path.string() + " -o " +
            runtime_index_source_dependent_else_row_out_of_bounds_executable.string()
        ).empty()
    );
    assert(
        std::filesystem::file_size(
            runtime_index_source_dependent_else_row_out_of_bounds_executable
        ) > 0
    );
    auto runtime_index_source_then_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " + runtime_index_source_then_out_of_bounds_path.string()
    );
    auto runtime_index_source_then_column_guard = runtime_index_source_then_out_of_bounds_ir.find(
        " = icmp ult i64 %column, 2"
    );
    assert(runtime_index_source_then_column_guard != std::string::npos);
    auto runtime_index_source_then_trap = runtime_index_source_then_out_of_bounds_ir.find(
        "call void @__orison_dynamic_array_bounds_failed()",
        runtime_index_source_then_column_guard
    );
    assert(runtime_index_source_then_trap != std::string::npos);
    auto runtime_index_source_then_outer_guard = runtime_index_source_then_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        runtime_index_source_then_column_guard
    );
    assert(runtime_index_source_then_outer_guard != std::string::npos);
    assert(runtime_index_source_then_column_guard < runtime_index_source_then_trap);
    assert(runtime_index_source_then_trap < runtime_index_source_then_outer_guard);
    read_failing_command_output(
        executable.string() + " run " + runtime_index_source_then_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_then_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_then_out_of_bounds.o"
    );
    auto runtime_index_source_then_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_then_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_then_out_of_bounds_path.string() + " -o " +
            runtime_index_source_then_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(runtime_index_source_then_out_of_bounds_executable) > 0);
    auto runtime_index_source_else_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " + runtime_index_source_else_out_of_bounds_path.string()
    );
    auto runtime_index_source_else_first_column_guard = runtime_index_source_else_out_of_bounds_ir.find(
        " = icmp ult i64 %column, 2"
    );
    assert(runtime_index_source_else_first_column_guard != std::string::npos);
    auto runtime_index_source_else_column_guard = runtime_index_source_else_out_of_bounds_ir.find(
        " = icmp ult i64 %column, 2",
        runtime_index_source_else_first_column_guard + 1
    );
    assert(runtime_index_source_else_column_guard != std::string::npos);
    auto runtime_index_source_else_trap = runtime_index_source_else_out_of_bounds_ir.find(
        "call void @__orison_dynamic_array_bounds_failed()",
        runtime_index_source_else_column_guard
    );
    assert(runtime_index_source_else_trap != std::string::npos);
    auto runtime_index_source_else_outer_guard = runtime_index_source_else_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        runtime_index_source_else_column_guard
    );
    assert(runtime_index_source_else_outer_guard != std::string::npos);
    assert(runtime_index_source_else_column_guard < runtime_index_source_else_trap);
    assert(runtime_index_source_else_trap < runtime_index_source_else_outer_guard);
    read_failing_command_output(
        executable.string() + " run " + runtime_index_source_else_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_index_source_else_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_index_source_else_out_of_bounds.o"
    );
    auto runtime_index_source_else_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_index_source_else_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_index_source_else_out_of_bounds_path.string() + " -o " +
            runtime_index_source_else_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(runtime_index_source_else_out_of_bounds_executable) > 0);
    assert_computed_dynamic_array_runtime_aggregate_owner_mismatch_failure_matrix(
        executable,
        runtime_indexed_distinct_root_call_index_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_call_index_rejected.o",
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_call_index_rejected",
        "left.buckets[select_index(index)].values",
        "right.buckets[select_index(index)].values"
    );
    assert_run_success(executable, runtime_indexed_distinct_root_record_constructor_index_path);
    auto runtime_indexed_distinct_root_record_constructor_index_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_indexed_distinct_root_record_constructor_index_path.string()
        );
    auto record_constructor_first_insert =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = insertvalue %record.IndexValue undef, i64 %index, 0"
        );
    auto record_constructor_second_insert =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = insertvalue %record.IndexValue undef, i64 %index, 0",
            record_constructor_first_insert + 1
        );
    assert(record_constructor_first_insert != std::string::npos);
    assert(record_constructor_second_insert != std::string::npos);
    assert(
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = insertvalue %record.IndexValue undef, i64 %index, 0",
            record_constructor_second_insert + 1
        ) == std::string::npos
    );
    auto record_constructor_first_spill =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = alloca %record.IndexValue",
            record_constructor_first_insert
        );
    auto record_constructor_first_projection =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = getelementptr %record.IndexValue",
            record_constructor_first_spill
        );
    auto record_constructor_first_load =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = load i64, ptr ",
            record_constructor_first_projection
        );
    auto record_constructor_first_guard =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            record_constructor_first_load
        );
    auto record_constructor_second_spill =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = alloca %record.IndexValue",
            record_constructor_second_insert
        );
    auto record_constructor_second_projection =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = getelementptr %record.IndexValue",
            record_constructor_second_spill
        );
    auto record_constructor_second_load =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            " = load i64, ptr ",
            record_constructor_second_projection
        );
    auto record_constructor_second_guard =
        runtime_indexed_distinct_root_record_constructor_index_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            record_constructor_second_load
        );
    assert(record_constructor_first_insert < record_constructor_first_spill);
    assert(record_constructor_first_spill < record_constructor_first_projection);
    assert(record_constructor_first_projection < record_constructor_first_load);
    assert(record_constructor_first_load < record_constructor_first_guard);
    assert(record_constructor_first_guard < record_constructor_second_insert);
    assert(record_constructor_second_insert < record_constructor_second_spill);
    assert(record_constructor_second_spill < record_constructor_second_projection);
    assert(record_constructor_second_projection < record_constructor_second_load);
    assert(record_constructor_second_load < record_constructor_second_guard);
    assert_contains(
        runtime_indexed_distinct_root_record_constructor_index_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_record_constructor_index_path,
        smoke_temp_root /
            "dynamic_array_runtime_indexed_distinct_root_record_constructor_index_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_indexed_distinct_root_record_constructor_index_path,
        smoke_temp_root /
            "dynamic_array_runtime_indexed_distinct_root_record_constructor_index_for_run"
    );
    assert_run_success(
        executable,
        runtime_indexed_distinct_root_nested_record_constructor_index_path
    );
    auto runtime_indexed_distinct_root_nested_record_constructor_index_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_indexed_distinct_root_nested_record_constructor_index_path.string()
        );
    auto nested_record_constructor_first_insert =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = insertvalue %record.IndexEnvelope undef, %record.IndexValue "
        );
    auto nested_record_constructor_second_insert =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = insertvalue %record.IndexEnvelope undef, %record.IndexValue ",
            nested_record_constructor_first_insert + 1
        );
    assert(nested_record_constructor_first_insert != std::string::npos);
    assert(nested_record_constructor_second_insert != std::string::npos);
    assert(
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = insertvalue %record.IndexEnvelope undef, %record.IndexValue ",
            nested_record_constructor_second_insert + 1
        ) == std::string::npos
    );
    auto nested_record_constructor_first_spill =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = alloca %record.IndexEnvelope",
            nested_record_constructor_first_insert
        );
    auto nested_record_constructor_first_outer_projection =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = getelementptr %record.IndexEnvelope",
            nested_record_constructor_first_spill
        );
    auto nested_record_constructor_first_inner_projection =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = getelementptr %record.IndexValue",
            nested_record_constructor_first_outer_projection
        );
    auto nested_record_constructor_first_load =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = load i64, ptr ",
            nested_record_constructor_first_inner_projection
        );
    auto nested_record_constructor_first_guard =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            nested_record_constructor_first_load
        );
    auto nested_record_constructor_second_spill =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = alloca %record.IndexEnvelope",
            nested_record_constructor_second_insert
        );
    auto nested_record_constructor_second_outer_projection =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = getelementptr %record.IndexEnvelope",
            nested_record_constructor_second_spill
        );
    auto nested_record_constructor_second_inner_projection =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = getelementptr %record.IndexValue",
            nested_record_constructor_second_outer_projection
        );
    auto nested_record_constructor_second_load =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            " = load i64, ptr ",
            nested_record_constructor_second_inner_projection
        );
    auto nested_record_constructor_second_guard =
        runtime_indexed_distinct_root_nested_record_constructor_index_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            nested_record_constructor_second_load
        );
    assert(nested_record_constructor_first_insert < nested_record_constructor_first_spill);
    assert(nested_record_constructor_first_spill < nested_record_constructor_first_outer_projection);
    assert(nested_record_constructor_first_outer_projection < nested_record_constructor_first_inner_projection);
    assert(nested_record_constructor_first_inner_projection < nested_record_constructor_first_load);
    assert(nested_record_constructor_first_load < nested_record_constructor_first_guard);
    assert(nested_record_constructor_first_guard < nested_record_constructor_second_insert);
    assert(nested_record_constructor_second_insert < nested_record_constructor_second_spill);
    assert(nested_record_constructor_second_spill < nested_record_constructor_second_outer_projection);
    assert(nested_record_constructor_second_outer_projection < nested_record_constructor_second_inner_projection);
    assert(nested_record_constructor_second_inner_projection < nested_record_constructor_second_load);
    assert(nested_record_constructor_second_load < nested_record_constructor_second_guard);
    assert_contains(
        runtime_indexed_distinct_root_nested_record_constructor_index_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_nested_record_constructor_index_path,
        smoke_temp_root /
            "dynamic_array_runtime_indexed_distinct_root_nested_record_constructor_index_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_indexed_distinct_root_nested_record_constructor_index_path,
        smoke_temp_root /
            "dynamic_array_runtime_indexed_distinct_root_nested_record_constructor_index_for_run"
    );
    assert_run_success(
        executable,
        runtime_indexed_distinct_root_generic_record_constructor_index_path
    );
    auto runtime_indexed_distinct_root_generic_record_constructor_index_ir =
        read_successful_command_output(
            executable.string() + " --emit-llvm " +
            runtime_indexed_distinct_root_generic_record_constructor_index_path.string()
        );
    assert_contains(
        runtime_indexed_distinct_root_generic_record_constructor_index_ir,
        "%record.IndexValue_UInt64_ = type { i64 }"
    );
    assert_contains(
        runtime_indexed_distinct_root_generic_record_constructor_index_ir,
        "%record.IndexEnvelope_UInt64_ = type { %record.IndexValue_UInt64_ }"
    );
    assert_contains(
        runtime_indexed_distinct_root_generic_record_constructor_index_ir,
        "%record.IndexConfig = type { [1 x i64] }"
    );
    auto generic_constructor_record_projection =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = getelementptr %record.IndexConfig, ptr %config.addr, i32 0, i32 0"
        );
    auto generic_constructor_array_projection =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = getelementptr [1 x i64], ptr ",
            generic_constructor_record_projection
        );
    auto generic_constructor_projected_load =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = load i64, ptr ",
            generic_constructor_array_projection
        );
    auto generic_constructor_ordinary_call =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = call i64 @normalize_index(i64 %",
            generic_constructor_projected_load
        );
    auto generic_constructor_projected_local =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "%projected = add i64 0, %",
            generic_constructor_ordinary_call
        );
    auto generic_constructor_generic_method_call =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = call i64 @method.UInt64.choose__UInt64(i64 %projected, i64 %index)",
            generic_constructor_projected_local
        );
    auto generic_constructor_chosen_local =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "%chosen = add i64 0, %",
            generic_constructor_generic_method_call
        );
    auto generic_constructor_wrapped_insert =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = insertvalue %record.IndexValue_UInt64_ undef, i64 %chosen, 0",
            generic_constructor_chosen_local
        );
    auto generic_constructor_wrapped_local =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "%wrapped.addr = alloca %record.IndexValue_UInt64_",
            generic_constructor_wrapped_insert
        );
    auto generic_constructor_receiver_method_call =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = call i64 @method.IndexValue_UInt64_.value_copy__UInt64(",
            generic_constructor_wrapped_local
        );
    auto generic_constructor_normalized_local =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "%normalized = add i64 0, %",
            generic_constructor_receiver_method_call
        );
    auto generic_constructor_ternary_branch =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "br i1 %root_flag, label %ternary.then.",
            generic_constructor_normalized_local
        );
    auto generic_constructor_ternary_phi =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = phi i64 [%normalized, %ternary.then.",
            generic_constructor_ternary_branch
        );
    auto generic_constructor_inferred_local =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "%selected = add i64 0, %",
            generic_constructor_ternary_phi
        );
    auto generic_constructor_generic_call =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = call i64 @identity__UInt64(i64 %selected)",
            generic_constructor_inferred_local
        );
    auto generic_constructor_generic_local =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "%generic_selected = add i64 0, %",
            generic_constructor_generic_call
        );
    auto generic_constructor_first_inner_insert =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = insertvalue %record.IndexValue_UInt64_ undef, i64 %generic_selected, 0"
        );
    auto generic_constructor_first_outer_insert =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = insertvalue %record.IndexEnvelope_UInt64_ undef, %record.IndexValue_UInt64_ ",
            generic_constructor_first_inner_insert
        );
    auto generic_constructor_first_outer_extract =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = extractvalue %record.IndexEnvelope_UInt64_ ",
            generic_constructor_first_outer_insert
        );
    auto generic_constructor_first_inner_extract =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = extractvalue %record.IndexValue_UInt64_ ",
            generic_constructor_first_outer_extract
        );
    auto generic_constructor_first_guard =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            generic_constructor_first_inner_extract
        );
    auto generic_constructor_second_inner_insert =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = insertvalue %record.IndexValue_UInt64_ undef, i64 %generic_selected, 0",
            generic_constructor_first_guard
        );
    auto generic_constructor_second_outer_insert =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = insertvalue %record.IndexEnvelope_UInt64_ undef, %record.IndexValue_UInt64_ ",
            generic_constructor_second_inner_insert
        );
    auto generic_constructor_second_outer_extract =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = extractvalue %record.IndexEnvelope_UInt64_ ",
            generic_constructor_second_outer_insert
        );
    auto generic_constructor_second_inner_extract =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = extractvalue %record.IndexValue_UInt64_ ",
            generic_constructor_second_outer_extract
        );
    auto generic_constructor_second_guard =
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            "computed_dynamic_array_runtime_aggregate_index",
            generic_constructor_second_inner_extract
        );
    assert(generic_constructor_record_projection != std::string::npos);
    assert(generic_constructor_record_projection < generic_constructor_array_projection);
    assert(generic_constructor_array_projection < generic_constructor_projected_load);
    assert(generic_constructor_projected_load < generic_constructor_ordinary_call);
    assert(generic_constructor_ordinary_call < generic_constructor_projected_local);
    assert(generic_constructor_projected_local < generic_constructor_generic_method_call);
    assert(generic_constructor_generic_method_call < generic_constructor_chosen_local);
    assert(generic_constructor_chosen_local < generic_constructor_wrapped_insert);
    assert(generic_constructor_wrapped_insert < generic_constructor_wrapped_local);
    assert(generic_constructor_wrapped_local < generic_constructor_receiver_method_call);
    assert(generic_constructor_receiver_method_call < generic_constructor_normalized_local);
    assert(generic_constructor_normalized_local < generic_constructor_ternary_branch);
    assert(generic_constructor_ternary_branch < generic_constructor_ternary_phi);
    assert(generic_constructor_ternary_phi < generic_constructor_inferred_local);
    assert(generic_constructor_inferred_local < generic_constructor_generic_call);
    assert(generic_constructor_generic_call < generic_constructor_generic_local);
    assert(generic_constructor_generic_local < generic_constructor_first_inner_insert);
    assert(generic_constructor_first_inner_insert < generic_constructor_first_outer_insert);
    assert(generic_constructor_first_outer_insert < generic_constructor_first_outer_extract);
    assert(generic_constructor_first_outer_extract < generic_constructor_first_inner_extract);
    assert(generic_constructor_first_inner_extract < generic_constructor_first_guard);
    assert(generic_constructor_first_guard < generic_constructor_second_inner_insert);
    assert(generic_constructor_second_inner_insert < generic_constructor_second_outer_insert);
    assert(generic_constructor_second_outer_insert < generic_constructor_second_outer_extract);
    assert(generic_constructor_second_outer_extract < generic_constructor_second_inner_extract);
    assert(generic_constructor_second_inner_extract < generic_constructor_second_guard);
    assert(
        runtime_indexed_distinct_root_generic_record_constructor_index_ir.find(
            " = insertvalue %record.IndexValue_UInt64_ undef, i64 %generic_selected, 0",
            generic_constructor_second_guard
        ) == std::string::npos
    );
    assert_contains(
        runtime_indexed_distinct_root_generic_record_constructor_index_ir,
        "%computed_dynamic_array_selection.1.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_generic_record_constructor_index_path,
        smoke_temp_root /
            "dynamic_array_runtime_indexed_distinct_root_generic_record_constructor_index_for_run.o"
    );
    assert_build_success(
        executable,
        runtime_indexed_distinct_root_generic_record_constructor_index_path,
        smoke_temp_root /
            "dynamic_array_runtime_indexed_distinct_root_generic_record_constructor_index_for_run"
    );
    assert_run_success(executable, nested_runtime_indexed_distinct_root_computed_dynamic_array_path);
    auto nested_runtime_indexed_distinct_root_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        nested_runtime_indexed_distinct_root_computed_dynamic_array_path.string()
    );
    assert_contains(
        nested_runtime_indexed_distinct_root_ir,
        "computed_dynamic_array_runtime_aggregate_index"
    );
    assert_contains(
        nested_runtime_indexed_distinct_root_ir,
        "call void @__orison_owned_cleanup.Payload(ptr "
        "%computed_dynamic_array_selection.0.then.unselected.cleanup.drop.element.addr)"
    );
    assert_contains(
        nested_runtime_indexed_distinct_root_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        nested_runtime_indexed_distinct_root_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_computed_for_run.o"
    );
    assert_build_success(
        executable,
        nested_runtime_indexed_distinct_root_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_computed_for_run"
    );
    assert_run_success(executable, nested_runtime_indexed_distinct_root_pure_computed_dynamic_array_path);
    auto nested_runtime_indexed_distinct_root_pure_computed_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        nested_runtime_indexed_distinct_root_pure_computed_dynamic_array_path.string()
    );
    auto first_row_load = nested_runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %row.addr"
    );
    assert(first_row_load != std::string::npos);
    auto second_row_load = nested_runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %row.addr",
        first_row_load + 1
    );
    assert(second_row_load != std::string::npos);
    assert(
        nested_runtime_indexed_distinct_root_pure_computed_ir.find(
            "load i64, ptr %row.addr",
            second_row_load + 1
        ) == std::string::npos
    );
    auto first_column_load = nested_runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %column.addr"
    );
    assert(first_column_load != std::string::npos);
    auto second_column_load = nested_runtime_indexed_distinct_root_pure_computed_ir.find(
        "load i64, ptr %column.addr",
        first_column_load + 1
    );
    assert(second_column_load != std::string::npos);
    assert(
        nested_runtime_indexed_distinct_root_pure_computed_ir.find(
            "load i64, ptr %column.addr",
            second_column_load + 1
        ) == std::string::npos
    );
    auto first_row_guard = nested_runtime_indexed_distinct_root_pure_computed_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        first_row_load
    );
    assert(first_row_guard != std::string::npos);
    assert(first_row_guard < first_column_load);
    assert_contains(
        nested_runtime_indexed_distinct_root_pure_computed_ir,
        "%computed_dynamic_array_selection.0.descriptor = phi { ptr, i64, i64 }"
    );
    assert_emit_object_success(
        executable,
        nested_runtime_indexed_distinct_root_pure_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_pure_computed_for_run.o"
    );
    assert_build_success(
        executable,
        nested_runtime_indexed_distinct_root_pure_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_pure_computed_for_run"
    );
    assert_computed_dynamic_array_runtime_aggregate_owner_mismatch_failure_matrix(
        executable,
        nested_runtime_indexed_distinct_root_computed_mismatch_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_computed_mismatch_rejected.o",
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_computed_mismatch_rejected",
        "left.grid[(row + row_offset)][(column + left_column_offset)].values",
        "right.grid[(row + row_offset)][(column + right_column_offset)].values"
    );
    assert_computed_dynamic_array_runtime_aggregate_owner_mismatch_failure_matrix(
        executable,
        runtime_indexed_distinct_root_mismatched_index_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_mismatched_index_rejected.o",
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_mismatched_index_rejected",
        "left.buckets[left_index].values",
        "right.buckets[right_index].values"
    );
    assert_diagnostic_failure_matrix(
        executable,
        runtime_indexed_distinct_root_left_reuse_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_left_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_left_reuse_rejected",
        "use after move: left.buckets[index].values"
    );
    assert_diagnostic_failure_matrix(
        executable,
        runtime_indexed_distinct_root_right_reuse_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_right_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_right_reuse_rejected",
        "use after move: right.buckets[index].values"
    );
    assert_diagnostic_failure_matrix(
        executable,
        nested_runtime_indexed_distinct_root_left_reuse_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_left_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_left_reuse_rejected",
        "use after move: left.grid[row][column].values"
    );
    assert_diagnostic_failure_matrix(
        executable,
        nested_runtime_indexed_distinct_root_right_reuse_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_right_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_right_reuse_rejected",
        "use after move: right.grid[row][column].values"
    );
    auto runtime_indexed_then_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_indexed_distinct_root_then_out_of_bounds_path.string()
    );
    assert_contains(runtime_indexed_then_out_of_bounds_ir, "computed_dynamic_array_runtime_aggregate_index");
    assert_contains(runtime_indexed_then_out_of_bounds_ir, "call void @__orison_dynamic_array_bounds_failed()");
    read_failing_command_output(
        executable.string() + " run " + runtime_indexed_distinct_root_then_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_then_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_then_out_of_bounds.o"
    );
    auto runtime_indexed_then_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_then_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_indexed_distinct_root_then_out_of_bounds_path.string() + " -o " +
            runtime_indexed_then_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(runtime_indexed_then_out_of_bounds_executable) > 0);
    auto runtime_indexed_else_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        runtime_indexed_distinct_root_else_out_of_bounds_path.string()
    );
    assert_contains(runtime_indexed_else_out_of_bounds_ir, "computed_dynamic_array_runtime_aggregate_index");
    assert_contains(runtime_indexed_else_out_of_bounds_ir, "call void @__orison_dynamic_array_bounds_failed()");
    read_failing_command_output(
        executable.string() + " run " + runtime_indexed_distinct_root_else_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        runtime_indexed_distinct_root_else_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_else_out_of_bounds.o"
    );
    auto runtime_indexed_else_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_runtime_indexed_distinct_root_else_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            runtime_indexed_distinct_root_else_out_of_bounds_path.string() + " -o " +
            runtime_indexed_else_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(runtime_indexed_else_out_of_bounds_executable) > 0);
    auto nested_runtime_indexed_then_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        nested_runtime_indexed_distinct_root_then_out_of_bounds_path.string()
    );
    auto nested_then_first_guard = nested_runtime_indexed_then_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index"
    );
    assert(nested_then_first_guard != std::string::npos);
    auto nested_then_second_guard = nested_runtime_indexed_then_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        nested_then_first_guard + 1
    );
    assert(nested_then_second_guard != std::string::npos);
    assert_contains(
        nested_runtime_indexed_then_out_of_bounds_ir,
        "call void @__orison_dynamic_array_bounds_failed()"
    );
    read_failing_command_output(
        executable.string() + " run " + nested_runtime_indexed_distinct_root_then_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        nested_runtime_indexed_distinct_root_then_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_then_out_of_bounds.o"
    );
    auto nested_runtime_indexed_then_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_then_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            nested_runtime_indexed_distinct_root_then_out_of_bounds_path.string() + " -o " +
            nested_runtime_indexed_then_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(nested_runtime_indexed_then_out_of_bounds_executable) > 0);
    auto nested_runtime_indexed_else_out_of_bounds_ir = read_successful_command_output(
        executable.string() + " --emit-llvm " +
        nested_runtime_indexed_distinct_root_else_out_of_bounds_path.string()
    );
    auto nested_else_first_guard = nested_runtime_indexed_else_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index"
    );
    assert(nested_else_first_guard != std::string::npos);
    auto nested_else_second_guard = nested_runtime_indexed_else_out_of_bounds_ir.find(
        "computed_dynamic_array_runtime_aggregate_index",
        nested_else_first_guard + 1
    );
    assert(nested_else_second_guard != std::string::npos);
    assert_contains(
        nested_runtime_indexed_else_out_of_bounds_ir,
        "call void @__orison_dynamic_array_bounds_failed()"
    );
    read_failing_command_output(
        executable.string() + " run " + nested_runtime_indexed_distinct_root_else_out_of_bounds_path.string()
    );
    assert_emit_object_success(
        executable,
        nested_runtime_indexed_distinct_root_else_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_else_out_of_bounds.o"
    );
    auto nested_runtime_indexed_else_out_of_bounds_executable =
        smoke_temp_root / "dynamic_array_nested_runtime_indexed_distinct_root_else_out_of_bounds";
    assert(
        read_successful_command_output(
            executable.string() + " --build " +
            nested_runtime_indexed_distinct_root_else_out_of_bounds_path.string() + " -o " +
            nested_runtime_indexed_else_out_of_bounds_executable.string()
        ).empty()
    );
    assert(std::filesystem::file_size(nested_runtime_indexed_else_out_of_bounds_executable) > 0);
    assert_diagnostic_failure_matrix(
        executable,
        static_indexed_aggregate_distinct_owner_left_reuse_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_distinct_owner_left_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_distinct_owner_left_reuse_rejected",
        "use after move: holder.buckets.element0.values"
    );
    assert_diagnostic_failure_matrix(
        executable,
        static_indexed_aggregate_distinct_owner_right_reuse_path,
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_distinct_owner_right_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_static_indexed_aggregate_distinct_owner_right_reuse_rejected",
        "use after move: holder.buckets.element1.values"
    );
    assert_diagnostic_failure_matrix(
        executable,
        nested_static_indexed_aggregate_distinct_owner_left_reuse_path,
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_left_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_left_reuse_rejected",
        "use after move: holder.grid.element1.element0.values"
    );
    assert_diagnostic_failure_matrix(
        executable,
        nested_static_indexed_aggregate_distinct_owner_right_reuse_path,
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_right_reuse_rejected.o",
        smoke_temp_root / "dynamic_array_nested_static_indexed_aggregate_distinct_owner_right_reuse_rejected",
        "use after move: holder.grid.element0.element1.values"
    );
    assert_computed_dynamic_array_runtime_aggregate_owner_mismatch_failure_matrix(
        executable,
        nested_dynamic_index_aggregate_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_nested_dynamic_index_aggregate_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_nested_dynamic_index_aggregate_owned_computed_owner_mismatch",
        "holder.grid[row][column].values",
        "holder.grid[column][row].values"
    );
    assert_computed_dynamic_array_owner_mismatch_failure_matrix(
        executable,
        forwarded_static_indexed_aggregate_helper_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_owned_computed_owner_mismatch",
        "left.buckets.element0.values",
        "right.buckets.element0.values"
    );
    assert_computed_dynamic_array_runtime_aggregate_owner_mismatch_failure_matrix(
        executable,
        forwarded_nested_dynamic_index_aggregate_helper_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_nested_dynamic_index_aggregate_helper_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_nested_dynamic_index_aggregate_helper_owned_computed_owner_mismatch",
        "holder.grid[row][column].values",
        "holder.grid[column][row].values"
    );
    assert_run_success(executable, forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed_dynamic_array_path);
    assert_static_indexed_aggregate_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed_dynamic_array_path,
        "holder.buckets.element0.values",
        "%record.Holder = type { [2 x %record.Bucket] }"
    );
    assert_emit_object_success(
        executable,
        forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_static_indexed_aggregate_helper_extra_statement_owned_computed"
    );
    }
    if (runs_mode("returned_cleanup")) {
    assert_returned_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        returned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_owned_computed_for_cleanup"
    );
    assert_returned_alias_chain_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_alias_chain_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        returned_alias_chain_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_alias_chain_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_alias_chain_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_alias_chain_owned_computed_for_cleanup"
    );
    assert_run_success(
        executable,
        returned_alias_chain_owned_computed_dynamic_array_path
    );
    assert_branch_returned_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        branch_returned_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        branch_returned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_returned_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_returned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_returned_owned_computed_for_cleanup"
    );
    assert_switch_returned_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        switch_returned_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        switch_returned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_returned_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_returned_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_returned_owned_computed_for_cleanup"
    );
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_aggregate_field_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @make_box()"
    );
    assert_emit_object_success(
        executable,
        returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, returned_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        switch_returned_aggregate_field_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @choose_box(i32 %selector)"
    );
    assert_emit_object_success(
        executable,
        switch_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_returned_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_returned_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, switch_returned_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        switch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @choose_box(i32 %selector)"
    );
    assert_emit_object_success(
        executable,
        switch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, switch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        branch_returned_aggregate_field_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @choose_box(i1 %flag)"
    );
    assert_emit_object_success(
        executable,
        branch_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_returned_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_returned_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, branch_returned_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        branch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @choose_box(i1 %flag)"
    );
    assert_emit_object_success(
        executable,
        branch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_forwarded_returned_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_forwarded_returned_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, branch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @choose_box(i1 %flag)"
    );
    assert_emit_object_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, branch_mixed_forwarded_returned_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_returned_aggregate_field_helper_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @forward_box(%record.PayloadBox %box)"
    );
    assert_emit_object_success(
        executable,
        forwarded_returned_aggregate_field_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_aggregate_field_helper_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_returned_aggregate_field_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_aggregate_field_helper_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_returned_aggregate_field_helper_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_projected_argument_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define { ptr, i64, i64 } @forward_projected_values(%record.PayloadBox %box)"
    );
    assert_emit_object_success(
        executable,
        forwarded_projected_argument_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_projected_argument_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_projected_argument_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_projected_argument_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_projected_argument_owned_computed_dynamic_array_path);
    assert_computed_dynamic_array_owner_mismatch_failure_matrix(
        executable,
        forwarded_returned_aggregate_field_helper_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_aggregate_field_helper_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_returned_aggregate_field_helper_owned_computed_owner_mismatch",
        "left.values",
        "right.values"
    );
    assert_run_success(executable, forwarded_returned_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_returned_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @forward_box_extra(%record.PayloadBox %box)"
    );
    assert_emit_object_success(
        executable,
        forwarded_returned_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_aggregate_field_helper_extra_statement_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_returned_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_aggregate_field_helper_extra_statement_owned_computed"
    );
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        returned_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, returned_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_returned_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        switch_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, switch_returned_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, switch_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, returned_nested_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        switch_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_nested_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_nested_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, switch_returned_nested_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_if_branch_local_cleanup_emit_llvm_success(
        executable,
        forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup"
    );
    assert_run_success(executable, forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        returned_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, returned_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        switch_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, switch_returned_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, switch_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.values"
    );
    assert_emit_object_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, returned_nested_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, switch_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_final_switch_branch_local_cleanup_emit_llvm_success(
        executable,
        forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        "returned.inner.values"
    );
    assert_emit_object_success(
        executable,
        forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup"
    );
    assert_run_success(executable, forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @make_outer_box()"
    );
    assert_emit_object_success(
        executable,
        returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, returned_nested_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        switch_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @choose_outer(i32 %selector)"
    );
    assert_emit_object_success(
        executable,
        switch_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_returned_nested_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_returned_nested_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, switch_returned_nested_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @choose_outer(i32 %selector)"
    );
    assert_emit_object_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        switch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_switch_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, switch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @choose_outer(i1 %flag)"
    );
    assert_emit_object_success(
        executable,
        branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @choose_outer(i1 %flag)"
    );
    assert_emit_object_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup"
    );
    assert_run_success(executable, branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_returned_nested_aggregate_field_helper_owned_computed_dynamic_array_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @forward_outer(%record.OuterBox %box)"
    );
    assert_emit_object_success(
        executable,
        forwarded_returned_nested_aggregate_field_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_returned_nested_aggregate_field_helper_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_owned_computed_for_cleanup"
    );
    assert_run_success(executable, forwarded_returned_nested_aggregate_field_helper_owned_computed_dynamic_array_path);
    assert_computed_dynamic_array_owner_mismatch_failure_matrix(
        executable,
        forwarded_returned_nested_aggregate_field_helper_owned_computed_dynamic_array_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_owned_computed_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_owned_computed_owner_mismatch",
        "left.inner.values",
        "right.inner.values"
    );
    assert_run_success(executable, forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @forward_outer_extra(%record.OuterBox %box)"
    );
    assert_emit_object_success(
        executable,
        forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_returned_nested_aggregate_field_helper_extra_statement_owned_computed"
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        choice_payload_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        choice_payload_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        choice_payload_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup"
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        choice_payload_final_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        choice_payload_final_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        choice_payload_final_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup"
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_switch_binding_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_switch_binding_owned_computed_for_cleanup"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_harmless_local_switch_binding_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_harmless_local_switch_binding_owned_computed_for_cleanup"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_final_if_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_final_if_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_final_if_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_final_if_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_final_switch_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_final_switch_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_final_switch_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_final_switch_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_choice_payload_switch_binding_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed.o"
    );
    assert_build_success(
        executable,
        forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path,
        smoke_temp_root /
            "dynamic_array_forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed"
    );
    assert_run_success(
        executable,
        forwarded_choice_payload_nested_final_switch_if_branch_local_alias_harmless_local_switch_binding_owned_computed_dynamic_array_path
    );
    assert_returned_choice_payload_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_choice_payload_owned_computed_dynamic_array_path
    );
    assert_emit_object_success(
        executable,
        returned_choice_payload_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_choice_payload_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_choice_payload_owned_computed_dynamic_array_path,
        smoke_temp_root / "dynamic_array_returned_choice_payload_owned_computed_for_cleanup"
    );
    assert_run_success(
        executable,
        returned_choice_payload_owned_computed_dynamic_array_path
    );
    }
    if (runs_mode("forwarding")) {
    assert_owned_dynamic_array_parameter_emit_llvm_success(executable, owned_dynamic_array_parameter_path);
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_path,
        smoke_temp_root / "dynamic_array_owned_parameter.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_path,
        smoke_temp_root / "dynamic_array_owned_parameter"
    );
    assert_cli_emit_llvm_existing_fixture_success(
        executable,
        owned_dynamic_array_parameter_cleanup_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_cleanup.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_cleanup"
    );
    assert_run_success(executable, owned_dynamic_array_parameter_cleanup_path);
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_forwarding_path,
        smoke_temp_root / "dynamic_array_owned_parameter_forwarding.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_forwarding_path,
        smoke_temp_root / "dynamic_array_owned_parameter_forwarding"
    );
    assert_returned_dynamic_array_parameter_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_parameter_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_parameter_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_parameter_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_parameter_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_parameter_forwarding"
    );
    assert_returned_dynamic_array_multi_hop_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_multi_hop_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_multi_hop_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_multi_hop_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_multi_hop_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_multi_hop_forwarding"
    );
    assert_returned_dynamic_array_branch_join_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_branch_join_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_branch_join_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_branch_join_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_branch_join_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_branch_join_forwarding"
    );
    assert_returned_dynamic_array_choice_branch_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_choice_branch_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_choice_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_choice_branch_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_choice_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_choice_branch_forwarding"
    );
    assert_returned_dynamic_array_aggregate_field_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_forwarding"
    );
    assert_returned_dynamic_array_nested_aggregate_field_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_forwarding"
    );
    assert_returned_dynamic_array_nested_aggregate_field_branch_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_branch_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_branch_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_branch_forwarding"
    );
    assert_returned_dynamic_array_aggregate_field_choice_payload_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_payload_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_payload_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_payload_forwarding"
    );
    assert_returned_dynamic_array_aggregate_field_choice_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_choice_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_choice_record_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_record_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_record_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_record_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_record_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_record_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_choice_record_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_choice_fixed_array_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_fixed_array_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_fixed_array_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_fixed_array_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_choice_fixed_array_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_choice_nested_fixed_array_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_choice_fixed_array_record_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_choice_nested_fixed_array_record_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_fixed_array_choice_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_fixed_array_choice_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_fixed_array_choice_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_fixed_array_choice_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_fixed_array_choice_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_fixed_array_choice_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_fixed_array_choice_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_nested_fixed_array_choice_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_nested_fixed_array_choice_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_nested_fixed_array_choice_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_nested_fixed_array_choice_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_nested_fixed_array_choice_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_nested_fixed_array_choice_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_nested_fixed_array_choice_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_maybe_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_maybe_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_fixed_array_maybe_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_fixed_array_maybe_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_fixed_array_maybe_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_fixed_array_maybe_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_fixed_array_maybe_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_fixed_array_maybe_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_fixed_array_maybe_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_nested_fixed_array_maybe_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_nested_fixed_array_maybe_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_nested_fixed_array_maybe_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_maybe_record_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_record_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_record_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_record_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_record_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_record_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_maybe_record_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_fixed_array_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_fixed_array_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_maybe_fixed_array_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_maybe_fixed_array_record_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_maybe_nested_fixed_array_record_payload_sibling_cleanup_path);
    assert_returned_dynamic_array_aggregate_field_stored_choice_payload_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_stored_choice_payload_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_stored_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_stored_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_stored_choice_payload_forwarding_path);
    assert_returned_dynamic_array_aggregate_field_stored_choice_payload_branch_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_aggregate_field_stored_choice_payload_branch_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_aggregate_field_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_aggregate_field_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding"
    );
    assert_run_success(executable, returned_dynamic_array_aggregate_field_stored_choice_payload_branch_forwarding_path);
    assert_returned_dynamic_array_nested_aggregate_field_stored_choice_payload_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_stored_choice_payload_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_stored_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_stored_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding"
    );
    assert_run_success(executable, returned_dynamic_array_nested_aggregate_field_stored_choice_payload_forwarding_path);
    assert_returned_dynamic_array_nested_aggregate_field_stored_choice_payload_branch_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_stored_choice_payload_branch_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding"
    );
    assert_run_success(executable, returned_dynamic_array_nested_aggregate_field_stored_choice_payload_branch_forwarding_path);
    assert_returned_dynamic_array_distinct_choice_payload_branch_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding"
    );
    assert_run_success(executable, returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path);
    assert_returned_dynamic_array_distinct_choice_payload_branch_forwarding_emit_llvm_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path
    );
    assert_emit_object_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding.o"
    );
    assert_build_success(
        executable,
        returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding"
    );
    assert_run_success(executable, returned_dynamic_array_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path);
    }
    if (runs_mode("final_control")) {
    assert_dynamic_array_local_final_if_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_local_final_if_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_local_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_if_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_local_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_if_branch_cleanup"
    );
    assert_dynamic_array_local_final_switch_case_cleanup_emit_llvm_success(
        executable,
        dynamic_array_local_final_switch_case_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_local_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_switch_case_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_local_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_switch_case_cleanup"
    );
    assert_dynamic_array_local_final_if_consumed_owner_cleanup_emit_llvm_success(
        executable,
        dynamic_array_local_final_if_consumed_owner_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_local_final_if_consumed_owner_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_if_consumed_owner_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_local_final_if_consumed_owner_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_if_consumed_owner_cleanup"
    );
    assert_dynamic_array_local_final_switch_consumed_owner_cleanup_emit_llvm_success(
        executable,
        dynamic_array_local_final_switch_consumed_owner_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_local_final_switch_consumed_owner_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_switch_consumed_owner_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_local_final_switch_consumed_owner_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_switch_consumed_owner_cleanup"
    );
    assert_dynamic_array_owned_result_final_if_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_final_if_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_final_if_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_final_if_branch_cleanup"
    );
    assert_dynamic_array_owned_result_final_switch_case_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_final_switch_case_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_final_switch_case_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_final_switch_case_cleanup"
    );
    assert_dynamic_array_owned_result_returned_local_final_if_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_returned_local_final_if_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_returned_local_final_if_branch_cleanup"
    );
    assert_dynamic_array_owned_result_returned_local_final_switch_case_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_returned_local_final_switch_case_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_returned_local_final_switch_case_cleanup"
    );
    assert_dynamic_array_owned_result_nested_final_if_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_nested_final_if_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_nested_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_nested_final_if_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_nested_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_nested_final_if_branch_cleanup"
    );
    assert_dynamic_array_owned_result_nested_final_switch_case_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_nested_final_switch_case_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_nested_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_nested_final_switch_case_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_nested_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_nested_final_switch_case_cleanup"
    );
    assert_dynamic_array_owned_result_if_switch_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_if_switch_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_if_switch_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_switch_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_if_switch_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_switch_branch_cleanup"
    );
    assert_dynamic_array_owned_result_switch_if_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_switch_if_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_switch_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_if_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_switch_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_if_branch_cleanup"
    );
    assert_dynamic_array_owned_result_direct_nested_if_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_direct_nested_if_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_direct_nested_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_direct_nested_if_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_direct_nested_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_direct_nested_if_branch_cleanup"
    );
    assert_dynamic_array_owned_result_direct_nested_switch_branch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_direct_nested_switch_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_direct_nested_switch_branch_cleanup"
    );
    assert_dynamic_array_owned_result_three_case_switch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_three_case_switch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_three_case_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_switch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_three_case_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_switch_cleanup"
    );
    assert_dynamic_array_owned_result_three_case_mixed_switch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_mixed_switch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_mixed_switch_cleanup"
    );
    assert_dynamic_array_owned_result_three_case_nested_switch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_three_case_nested_switch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_three_case_nested_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_nested_switch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_three_case_nested_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_nested_switch_cleanup"
    );
    assert_dynamic_array_owned_result_multi_nested_switch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_cleanup"
    );
    assert_dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup"
    );
    assert_dynamic_array_owned_result_if_two_switches_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_if_two_switches_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_if_two_switches_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_if_two_switches_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_cleanup"
    );
    assert_dynamic_array_if_two_switches_consumed_scratch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_path,
        "first_left_scratch"
    );
    assert_dynamic_array_owned_result_if_two_switches_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_helper_call_cleanup"
    );
    assert_dynamic_array_owned_result_switch_two_ifs_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_switch_two_ifs_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_switch_two_ifs_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_two_ifs_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_switch_two_ifs_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_two_ifs_cleanup"
    );
    assert_dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup"
    );
    assert_dynamic_array_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_helper_call_cleanup"
    );
    assert_dynamic_array_ternary_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_helper_call_cleanup"
    );
    assert_dynamic_array_ternary_named_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_helper_call_cleanup"
    );
    assert_dynamic_array_ternary_named_chained_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup"
    );
    assert_dynamic_array_ternary_named_helper_call_local_return_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup"
    );
    assert_returned_helper_call_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        dynamic_array_returned_helper_call_owned_computed_for_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_returned_helper_call_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_helper_call_owned_computed_for_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_returned_helper_call_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_helper_call_owned_computed_for_cleanup"
    );
    assert_run_success(
        executable,
        dynamic_array_returned_helper_call_owned_computed_for_cleanup_path
    );
    assert_dynamic_array_ternary_branch_consumer_local_return_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup"
    );
    assert_dynamic_array_ternary_chained_branch_consumer_local_return_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_scratch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_alias_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_nested_alias_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_asymmetric_alias_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_alias_helper_call_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_alias_helper_call_local_return_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_asymmetric_stored_helper_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_three_local_helper_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_distinct_local_names_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_mixed_direct_distinct_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_nested_helper_argument_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_nested_argument_local_chain_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup"
    );
    assert_dynamic_array_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_path,
        smoke_temp_root /
            "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_path,
        smoke_temp_root /
            "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_path,
        "final_selected"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_reuse_path,
        "finished"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_path,
        "finished"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_path,
        "wrapped_right_middle"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_path,
        "wrapped_right_middle"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_reuse_path,
        "right_middle"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_reuse_path,
        "right_middle"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_reuse_path,
        "right_middle"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_selected_reuse_path,
        "selected"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_path,
        "selected"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_reuse_path,
        "middle_return"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_reuse_path,
        "final_return"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_reuse_path,
        "returned"
    );
    assert_dynamic_array_ternary_alias_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_reuse_path
    );
    assert_dynamic_array_ternary_final_local_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_local_return_reuse_path
    );
    assert_dynamic_array_ternary_scratch_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_scratch_reuse_path
    );
    assert_dynamic_array_ternary_parameter_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_alias_reuse_path
    );
    assert_dynamic_array_ternary_alias_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_reuse_path
    );
    assert_dynamic_array_ternary_alias_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_reuse_path
    );
    assert_dynamic_array_ternary_local_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_chained_branch_consumer_selected_reuse_path
    );
    assert_dynamic_array_ternary_local_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_path
    );
    assert_dynamic_array_ternary_named_helper_call_returned_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_named_helper_call_reuse_path
    );
    assert_dynamic_array_ternary_named_helper_call_returned_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_path
    );
    assert_dynamic_array_switch_returned_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_multi_nested_switch_returned_reuse_path
    );
    assert_dynamic_array_helper_call_returned_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_helper_call_returned_reuse_path
    );
    assert_dynamic_array_multi_nested_switch_consumed_scratch_cleanup_emit_llvm_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_path
    );
    assert_emit_object_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup.o"
    );
    assert_build_success(
        executable,
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_path,
        "first_left_scratch"
    );
    assert_owned_dynamic_array_parameter_branch_join_emit_llvm_success(
        executable,
        owned_dynamic_array_parameter_branch_join_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_branch_join_path,
        smoke_temp_root / "dynamic_array_owned_parameter_branch_join.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_branch_join_path,
        smoke_temp_root / "dynamic_array_owned_parameter_branch_join"
    );
    assert_cli_emit_llvm_existing_fixture_success(
        executable,
        owned_dynamic_array_parameter_missing_drop_path
    );
    assert_owned_dynamic_array_parameter_branch_cleanup_emit_llvm_success(
        executable,
        owned_dynamic_array_parameter_branch_cleanup_path,
        "br i1 %flag"
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_branch_cleanup.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_branch_cleanup"
    );
    assert_owned_dynamic_array_parameter_branch_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_branch_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_branch_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_branch_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_branch_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_branch_index_assignment"
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_branch_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_branch_cleanup_emit_llvm_success(
        executable,
        owned_dynamic_array_parameter_switch_cleanup_path,
        "switch i1 %flag"
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_switch_cleanup.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_switch_cleanup"
    );
    assert_owned_dynamic_array_parameter_switch_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_switch_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_switch_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_switch_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_switch_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_switch_index_assignment"
    );
    assert_owned_dynamic_array_parameter_loop_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_loop_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_loop_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_loop_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_loop_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_loop_index_assignment"
    );
    assert_owned_dynamic_array_parameter_repeat_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_repeat_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_repeat_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_repeat_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_repeat_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_repeat_index_assignment"
    );
    assert_owned_dynamic_array_parameter_loop_break_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_loop_break_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_loop_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_loop_break_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_loop_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_loop_break_index_assignment"
    );
    assert_owned_dynamic_array_parameter_loop_continue_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_loop_continue_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_loop_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_loop_continue_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_loop_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_loop_continue_index_assignment"
    );
    assert_owned_dynamic_array_parameter_repeat_break_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_repeat_break_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_repeat_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_repeat_break_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_repeat_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_repeat_break_index_assignment"
    );
    assert_owned_dynamic_array_parameter_repeat_continue_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_repeat_continue_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_repeat_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_repeat_continue_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_repeat_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_repeat_continue_index_assignment"
    );
    assert_owned_dynamic_array_aggregate_parameter_field_index_assignment_run_success(
        executable,
        owned_dynamic_array_aggregate_parameter_field_index_assignment_path,
        "%record.Holder = type { { ptr, i64, i64 } }",
        "define i32 @replace(%record.Holder %holder)",
        "current.items.dynamic_array_index",
        "call void @__orison_owned_cleanup.Holder(ptr %current.addr)"
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_aggregate_parameter_field_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_aggregate_parameter_field_index_assignment"
    );
    assert_owned_dynamic_array_aggregate_parameter_field_index_assignment_run_success(
        executable,
        owned_dynamic_array_nested_aggregate_parameter_field_index_assignment_path,
        "%record.Holder = type { %record.Inner }",
        "define i32 @replace(%record.Holder %holder)",
        "current.inner.items.dynamic_array_index",
        "call void @__orison_owned_cleanup.Holder(ptr %current.addr)"
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_nested_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_nested_aggregate_parameter_field_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_nested_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_nested_aggregate_parameter_field_index_assignment"
    );
    assert_owned_dynamic_array_aggregate_parameter_field_index_assignment_run_success(
        executable,
        owned_dynamic_array_indexed_aggregate_parameter_field_index_assignment_path,
        "%record.Holder = type { [2 x %record.Item] }",
        "define i32 @replace(%record.Holder %holder)",
        "current.items.element0.values.dynamic_array_index",
        "call void @__orison_owned_cleanup.Holder(ptr %current.addr)"
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_indexed_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_indexed_aggregate_parameter_field_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_indexed_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_indexed_aggregate_parameter_field_index_assignment"
    );
    assert_owned_dynamic_array_aggregate_parameter_field_index_assignment_run_success(
        executable,
        owned_dynamic_array_runtime_indexed_aggregate_parameter_field_index_assignment_path,
        "%record.Holder = type { [2 x %record.Item] }",
        "define i32 @replace(%record.Holder %holder, i64 %index)",
        "current.items.element.values.dynamic_array_index",
        "call void @__orison_owned_cleanup.Holder(ptr %current.addr)"
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_runtime_indexed_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_runtime_indexed_aggregate_parameter_field_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_runtime_indexed_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_runtime_indexed_aggregate_parameter_field_index_assignment"
    );
    assert_owned_dynamic_array_aggregate_parameter_field_index_assignment_run_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_path,
        "%record.Holder = type { [2 x %record.Group] }",
        "define i32 @replace(%record.Holder %holder, i64 %group_index, i64 %item_index)",
        "current.groups.element.items.element.values.dynamic_array_index",
        "call void @__orison_owned_cleanup.Holder(ptr %current.addr)"
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment"
    );
    assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_out_of_bounds(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_out_of_bounds_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_out_of_bounds.o"
    );
    assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_item_out_of_bounds(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_item_out_of_bounds_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_item_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_item_out_of_bounds.o"
    );
    assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_out_of_bounds(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_out_of_bounds_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_out_of_bounds_path,
        smoke_temp_root / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_out_of_bounds.o"
    );
    assert_owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_run_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_path,
        smoke_temp_root / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_path,
        smoke_temp_root / "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic"
    );
    assert_diagnostic_failure_matrix(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_reuse_path,
        smoke_temp_root /
            "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_reuse.o",
        smoke_temp_root /
            "dynamic_array_owned_nested_runtime_indexed_aggregate_parameter_field_index_assignment_dynamic_reuse",
        "use after move: current.groups[group_index].items[item_index].values"
    );
    assert_owned_dynamic_array_parameter_loop_break_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_nested_loop_break_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_nested_loop_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_loop_break_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_nested_loop_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_loop_break_index_assignment"
    );
    assert_owned_dynamic_array_parameter_loop_continue_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_nested_loop_continue_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_nested_loop_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_loop_continue_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_nested_loop_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_loop_continue_index_assignment"
    );
    assert_owned_dynamic_array_parameter_repeat_break_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_nested_repeat_break_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_nested_repeat_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_repeat_break_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_nested_repeat_break_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_repeat_break_index_assignment"
    );
    assert_owned_dynamic_array_parameter_repeat_continue_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_nested_repeat_continue_index_assignment_path
    );
    assert_emit_object_success(
        executable,
        owned_dynamic_array_parameter_nested_repeat_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_repeat_continue_index_assignment.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_parameter_nested_repeat_continue_index_assignment_path,
        smoke_temp_root / "dynamic_array_owned_parameter_nested_repeat_continue_index_assignment"
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_switch_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_loop_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_loop_break_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_loop_continue_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_repeat_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_repeat_break_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_repeat_continue_cleanup_reuse_path
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_aggregate_parameter_field_cleanup_reuse_path,
        "holder.items"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_nested_aggregate_parameter_field_cleanup_reuse_path,
        "holder.inner.items"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_indexed_aggregate_parameter_field_cleanup_reuse_path,
        "holder.items.element0.values"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_runtime_indexed_aggregate_parameter_field_cleanup_reuse_path,
        "holder.items[index].values"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_nested_runtime_indexed_aggregate_parameter_field_cleanup_reuse_path,
        "holder.groups[group_index].items[item_index].values"
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_nested_loop_break_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_nested_loop_continue_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_nested_repeat_break_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_nested_repeat_continue_cleanup_reuse_path
    );
    assert_owned_dynamic_array_parameter_statement_branch_mismatch_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_statement_branch_mismatch_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_forwarding_reuse_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_second_use_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_length_after_move_path
    );
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_push_after_move_path
    );
    assert_dynamic_array_parameter_index_assignment_run_success(
        executable,
        dynamic_array_parameter_index_assignment_path
    );
    assert_owned_dynamic_array_parameter_index_assignment_run_success(
        executable,
        owned_dynamic_array_parameter_index_assignment_path
    );
    assert_dynamic_array_parameter_push_run_success(
        executable,
        dynamic_array_parameter_push_path
    );
    assert_cli_emit_llvm_existing_fixture_success(
        executable,
        owned_computed_dynamic_array_missing_drop_path
    );
    assert_cli_emit_llvm_existing_fixture_success(
        executable,
        owned_nested_computed_dynamic_array_missing_drop_path
    );
    assert_cli_emit_llvm_existing_fixture_success(
        executable,
        returned_owned_computed_dynamic_array_missing_drop_path
    );
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_aggregate_field_owned_computed_dynamic_array_missing_drop_path,
        "returned.values",
        "%record.PayloadBox = type { { ptr, i64, i64 } }",
        "define %record.PayloadBox @make_box()"
    );
    assert_run_exit_code(executable, returned_aggregate_field_owned_computed_dynamic_array_missing_drop_path, 42);
    assert_returned_aggregate_field_owned_computed_dynamic_array_emit_llvm_success(
        executable,
        returned_nested_aggregate_field_owned_computed_dynamic_array_missing_drop_path,
        "returned.inner.values",
        "%record.OuterBox = type { %record.PayloadBox }",
        "define %record.OuterBox @make_outer_box()"
    );
    assert_run_exit_code(executable, returned_nested_aggregate_field_owned_computed_dynamic_array_missing_drop_path, 60);
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_forwarded_returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_returned_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_forwarded_returned_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_nested_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_returned_nested_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        forwarded_returned_nested_aggregate_field_final_if_owner_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_returned_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_mixed_forwarded_returned_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_nested_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_returned_nested_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        switch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_mixed_forwarded_returned_nested_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        forwarded_returned_nested_aggregate_field_final_switch_owner_reuse_path,
        "returned.inner.values"
    );
    assert_choice_payload_owned_computed_cleanup_run_success(
        executable,
        choice_payload_switch_binding_owned_computed_dynamic_array_missing_drop_path
    );
    assert_choice_payload_owned_computed_cleanup_run_success(
        executable,
        choice_payload_final_switch_binding_owned_computed_dynamic_array_missing_drop_path
    );
    assert_choice_payload_final_switch_computed_reuse_emit_llvm_failure(
        executable,
        choice_payload_final_switch_binding_owned_computed_dynamic_array_reuse_path
    );
    assert_choice_payload_final_switch_computed_reuse_emit_llvm_failure(
        executable,
        returned_choice_payload_owned_computed_dynamic_array_reuse_path
    );
    assert_choice_payload_final_switch_computed_reuse_emit_llvm_failure(
        executable,
        forwarded_choice_payload_switch_binding_owned_computed_dynamic_array_reuse_path
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_choice_payload_final_if_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_owner_mismatch",
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_choice_payload_final_switch_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_owner_mismatch",
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_extra_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_extra.o",
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_extra",
        "use after move: packet"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_extra_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_extra.o",
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_extra",
        "use after move: packet"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_choice_payload_final_if_branch_local_alias_reassigned_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_reassigned.o",
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_if_branch_local_alias_reassigned",
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_choice_payload_final_switch_branch_local_alias_reassigned_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_reassigned.o",
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_final_switch_branch_local_alias_reassigned",
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
    assert_diagnostic_failure_matrix(
        executable,
        forwarded_choice_payload_nested_final_if_switch_branch_local_alias_owner_mismatch_path,
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_owner_mismatch.o",
        smoke_temp_root / "dynamic_array_forwarded_choice_payload_nested_final_if_switch_branch_local_alias_owner_mismatch",
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
    assert_run_success(executable, branch_returned_owned_computed_dynamic_array_distinct_owner_path);
    assert_run_success(executable, switch_returned_owned_computed_dynamic_array_distinct_owner_path);
    assert_owned_dynamic_array_replacement_emit_llvm_success(executable, owned_dynamic_array_replacement_path);
    assert_emit_object_success(
        executable,
        owned_dynamic_array_replacement_path,
        smoke_temp_root / "local_dynamic_array_owned_replacement.o"
    );
    assert_build_success(
        executable,
        owned_dynamic_array_replacement_path,
        smoke_temp_root / "local_dynamic_array_owned_replacement"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_push_owned_payload_reuse_path,
        "payload"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_push_owned_field_reuse_path,
        "box.payload"
    );
    assert_dynamic_array_use_after_move_emit_llvm_failure(
        executable,
        dynamic_array_owned_element_assignment_rhs_reuse_path,
        "payload"
    );
    }

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
