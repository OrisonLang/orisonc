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

void assert_contains(std::string const& text, std::string_view expected_fragment) {
    assert(text.find(expected_fragment) != std::string::npos);
}

void assert_excludes(std::string const& text, std::string_view unexpected_fragment) {
    assert(text.find(unexpected_fragment) == std::string::npos);
}

void assert_dynamic_array_payload_consumer_cleanup(std::string const& output) {
    assert_contains(output, "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup");
}

void assert_no_main_dynamic_array_deallocate(std::string const& output) {
    auto const main_start = output.find("define i32 @main");
    assert(main_start != std::string::npos);
    assert(output.find("__orison_dynamic_array_deallocate", main_start) == std::string::npos);
}

void assert_run_success(std::filesystem::path const& executable, std::filesystem::path const& source_path) {
    auto status = std::system((executable.string() + " run " + source_path.string()).c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
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
    assert(output.find("define void @__orison_drop.Payload(ptr %value)") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_allocate") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_deallocate") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_bounds_failed") != std::string::npos);

    auto const replacement_drop = output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_assign");
    assert(replacement_drop != std::string::npos);
    auto const replacement_store = output.find("store %record.Payload ", replacement_drop);
    assert(replacement_store != std::string::npos);
    auto const replacement_store_line_end = output.find('\n', replacement_store);
    assert(
        output.substr(replacement_store, replacement_store_line_end - replacement_store)
            .find("%items.dynamic_array_assign") != std::string::npos
    );
    auto const cleanup_drop = output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup");
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
}

void assert_owned_computed_dynamic_array_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(output.find("define void @__orison_drop.Payload(ptr %value)") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_allocate") != std::string::npos);
    assert(output.find("declare void @__orison_dynamic_array_deallocate") != std::string::npos);
    assert(output.find("items.computed_for.") != std::string::npos);
    assert(output.find(".condition:") != std::string::npos);
    assert(output.find(".body:") != std::string::npos);

    auto const computed_drop = output.find(
        "call void @__orison_drop.Payload(ptr %items.computed_dynamic_array_cleanup"
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
    assert(output.find("define void @__orison_drop.Payload(ptr %value)") != std::string::npos);
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
        output.find("call void @__orison_drop.Payload(ptr %items.computed_dynamic_array_cleanup");
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

void assert_owned_dynamic_array_parameter_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(output.find("define void @__orison_drop.Payload(ptr %value)") != std::string::npos);
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
        output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") !=
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
        output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") !=
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
        output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") !=
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
    assert_contains(output, "call void @__orison_drop.Payload(ptr %" + left_cleanup);
    assert_contains(output, "call void @__orison_drop.Payload(ptr %" + right_cleanup);
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %" + left_cleanup);
    assert_contains(output, "call void @__orison_dynamic_array_deallocate(ptr %" + right_cleanup);
    assert_contains(output, "phi " + std::string {phi_type} + " [0, %" + left_cleanup);
    assert_contains(output, "[1, %" + right_cleanup);
}

void assert_branch_local_scratch_dynamic_array_cleanup(
    std::string const& output
) {
    assert_contains(output, "scratch.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_drop.Payload(ptr %scratch.dynamic_array_cleanup");
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
    assert_contains(output, "call void @__orison_drop.Payload(ptr %left_values.dynamic_array_cleanup");
    assert_contains(output, "call void @__orison_drop.Payload(ptr %right_values.dynamic_array_cleanup");
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
        assert_contains(output, "call void @__orison_drop.Payload(ptr %" + cleanup_name);
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
    assert_contains(output, "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup");
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
    assert_contains(output, "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup");
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

void assert_dynamic_array_parameter_index_assignment_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find(
            "lowering DynamicArray parameter indexed assignment is unsupported; use exclusive.View<T> for mutable "
            "parameter element writes"
        ) !=
        std::string::npos
    );
}

void assert_dynamic_array_parameter_push_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find(
            "lowering DynamicArray parameter push is unsupported; pass an owned local DynamicArray<T> or use "
            "exclusive.View<T> for mutable parameter element writes"
        ) !=
        std::string::npos
    );
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

void assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path,
    std::string_view owner_name
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("use after move: " + std::string {owner_name}) != std::string::npos);
}

void assert_returned_owned_computed_dynamic_array_owner_mismatch_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<Payload> "
            "element Payload owners left right [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        output.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<Payload> "
            "element Payload [descriptor storage blocked] [cleanup owner blocked] [lowering disabled] (metadata only)"
        ) != std::string::npos
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
    assert(output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
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
    auto const drop_call = output.find("call void @__orison_drop.Payload(ptr %returned.computed_dynamic_array_cleanup");
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
    auto const drop_call = output.find("call void @__orison_drop.Payload(ptr %selected.computed_dynamic_array_cleanup");
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
    auto const drop_call = output.find("call void @__orison_drop.Payload(ptr %selected.computed_dynamic_array_cleanup");
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
        output.find("call void @__orison_drop.Payload(ptr %" + owner + ".computed_dynamic_array_cleanup");
    auto const deallocation =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %" + owner + ".computed_for.0.data", drop_call);
    auto const finalization =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr", deallocation);
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
        output.find("call void @__orison_drop.Payload(ptr %" + owner + ".computed_dynamic_array_cleanup");
    auto const returned_deallocate =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %" + owner + ".computed_for.1.data");
    auto const returned_finalize =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr", returned_deallocate);
    auto const scratch_drop =
        output.find("call void @__orison_drop.Payload(ptr %scratch.dynamic_array_cleanup", returned_finalize);
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
        output.find("call void @__orison_drop.Payload(ptr %" + owner + ".computed_dynamic_array_cleanup");
    auto const returned_deallocate =
        output.find("call void @__orison_dynamic_array_deallocate(ptr %" + owner + ".computed_for.1.data");
    auto const returned_finalize =
        output.find("store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr", returned_deallocate);
    auto const scratch_drop =
        output.find("call void @__orison_drop.Payload(ptr %scratch.dynamic_array_cleanup", returned_finalize);
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
    assert(output.find("switch i32 %tmp0, label %switch.unreachable.0") != std::string::npos);
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
    auto const drop_call = output.find("call void @__orison_drop.Payload(ptr %values.computed_dynamic_array_cleanup");
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

void assert_returned_dynamic_array_multi_hop_forwarding_emit_llvm_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_successful_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("define i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("define i32 @forward_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("call i32 @consume_items({ ptr, i64, i64 } %items)") != std::string::npos);
    assert(output.find("call i32 @forward_items({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
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
    assert(output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
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
    assert(output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
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
    assert(output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
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
    assert(output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
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
    assert(output.find("call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup") != std::string::npos);
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

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_array_cli_run_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

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

    for (auto name : run_examples) {
        assert_run_success(executable, examples / name);
    }

    auto generic_record_literal_path = examples / "local_generic_record_array_literal_for.or";
    auto computed_dynamic_array_path = examples / "local_dynamic_array_computed_for.or";
    auto nested_computed_dynamic_array_path = examples / "local_dynamic_array_nested_computed_for.or";
    auto owned_computed_dynamic_array_path = examples / "local_dynamic_array_owned_computed_for.or";
    auto owned_nested_computed_dynamic_array_path =
        examples / "local_dynamic_array_owned_nested_computed_for.or";
    auto owned_dynamic_array_parameter_path = examples / "dynamic_array_owned_parameter.or";
    auto owned_dynamic_array_parameter_forwarding_path =
        fixtures / "dynamic_array_owned_parameter_forwarding_run.or";
    auto returned_dynamic_array_parameter_forwarding_path =
        fixtures / "dynamic_array_returned_parameter_forwarding_run.or";
    auto returned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_owned_computed_for_cleanup_run.or";
    auto branch_returned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_returned_owned_computed_for_cleanup_run.or";
    auto switch_returned_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_switch_returned_owned_computed_for_cleanup_run.or";
    auto returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto branch_returned_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto returned_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto returned_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto returned_nested_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_branch_forwarded_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_if_branch_local_cleanup_run.or";
    auto returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto choice_payload_switch_binding_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_run.or";
    auto choice_payload_final_switch_binding_owned_computed_dynamic_array_path =
        fixtures / "dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_run.or";
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
    auto owned_dynamic_array_parameter_branch_cleanup_reuse_path =
        fixtures / "dynamic_array_owned_parameter_branch_cleanup_reuse_rejected.or";
    auto owned_dynamic_array_parameter_switch_cleanup_path =
        fixtures / "dynamic_array_owned_parameter_switch_cleanup_run.or";
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
    auto branch_returned_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_branch_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_branch_forwarded_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto returned_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto returned_nested_aggregate_field_final_if_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_if_branch_local_reuse_rejected.or";
    auto forwarded_returned_nested_aggregate_field_final_if_owner_reuse_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_if_owner_reuse_rejected.or";
    auto returned_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto returned_nested_aggregate_field_final_switch_branch_local_reuse_path =
        fixtures / "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto forwarded_returned_nested_aggregate_field_final_switch_owner_reuse_path =
        fixtures / "dynamic_array_forwarded_returned_nested_aggregate_field_final_switch_owner_reuse_rejected.or";
    auto choice_payload_switch_binding_owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop.or";
    auto choice_payload_final_switch_binding_owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop.or";
    auto choice_payload_final_switch_binding_owned_computed_dynamic_array_reuse_path =
        fixtures / "dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_rejected.or";
    auto branch_returned_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_branch_returned_owned_computed_owner_mismatch_rejected.or";
    auto switch_returned_owned_computed_dynamic_array_owner_mismatch_path =
        fixtures / "dynamic_array_switch_returned_owned_computed_owner_mismatch_rejected.or";
    auto owned_dynamic_array_parameter_missing_drop_path =
        fixtures / "dynamic_array_owned_parameter_iteration_missing_drop.or";
    auto dynamic_array_parameter_index_assignment_path =
        fixtures / "dynamic_array_parameter_index_assignment_rejected.or";
    auto dynamic_array_parameter_push_path =
        fixtures / "dynamic_array_parameter_push_rejected.or";
    auto owned_dynamic_array_replacement_path = examples / "local_dynamic_array_owned_replacement.or";
    auto dynamic_array_push_owned_payload_reuse_path =
        fixtures / "dynamic_array_push_owned_payload_reuse_rejected.or";
    auto dynamic_array_push_owned_field_reuse_path =
        fixtures / "dynamic_array_push_owned_field_reuse_rejected.or";
    auto dynamic_array_owned_element_assignment_rhs_reuse_path =
        fixtures / "dynamic_array_owned_element_assignment_rhs_reuse_rejected.or";
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
    assert_owned_dynamic_array_parameter_missing_drop_emit_llvm_failure(
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
    assert_owned_dynamic_array_parameter_use_after_move_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_switch_cleanup_reuse_path
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
    assert_dynamic_array_parameter_index_assignment_emit_llvm_failure(
        executable,
        dynamic_array_parameter_index_assignment_path
    );
    assert_dynamic_array_parameter_push_emit_llvm_failure(
        executable,
        dynamic_array_parameter_push_path
    );
    assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
        executable,
        owned_computed_dynamic_array_missing_drop_path
    );
    assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
        executable,
        owned_nested_computed_dynamic_array_missing_drop_path
    );
    assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
        executable,
        returned_owned_computed_dynamic_array_missing_drop_path
    );
    assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
        executable,
        returned_aggregate_field_owned_computed_dynamic_array_missing_drop_path
    );
    assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
        executable,
        returned_nested_aggregate_field_owned_computed_dynamic_array_missing_drop_path
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_returned_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        branch_forwarded_returned_nested_aggregate_field_owned_computed_dynamic_array_reuse_path,
        "returned.inner.values"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_aggregate_field_final_if_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        returned_nested_aggregate_field_final_if_branch_local_reuse_path,
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
        returned_nested_aggregate_field_final_switch_branch_local_reuse_path,
        "scratch"
    );
    assert_computed_dynamic_array_owner_reuse_emit_llvm_failure(
        executable,
        forwarded_returned_nested_aggregate_field_final_switch_owner_reuse_path,
        "returned.inner.values"
    );
    assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
        executable,
        choice_payload_switch_binding_owned_computed_dynamic_array_missing_drop_path
    );
    assert_owned_computed_dynamic_array_missing_drop_emit_llvm_failure(
        executable,
        choice_payload_final_switch_binding_owned_computed_dynamic_array_missing_drop_path
    );
    assert_choice_payload_final_switch_computed_reuse_emit_llvm_failure(
        executable,
        choice_payload_final_switch_binding_owned_computed_dynamic_array_reuse_path
    );
    assert_returned_owned_computed_dynamic_array_owner_mismatch_emit_llvm_failure(
        executable,
        branch_returned_owned_computed_dynamic_array_owner_mismatch_path
    );
    assert_returned_owned_computed_dynamic_array_owner_mismatch_emit_llvm_failure(
        executable,
        switch_returned_owned_computed_dynamic_array_owner_mismatch_path
    );
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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
