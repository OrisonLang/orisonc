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
    assert(output.find("items.computed_for.0.condition:") != std::string::npos);
    assert(output.find("items.computed_for.0.body:") != std::string::npos);
    assert(
        output.find(
            "cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "cleanup state handoff resume operation items.computed_for.0.cleanup.resume "
            "from items.loop.entry to items [cleanup calls enabled]"
        ) != std::string::npos
    );
    assert(
        output.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data, "
            "i64 4, i64 %items.computed_for.0.capacity)"
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

void assert_owned_dynamic_array_parameter_branch_mismatch_emit_llvm_failure(
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
    assert(output.find("switch case lowering failed: use after move: first_left_values") != std::string::npos);
}

void assert_dynamic_array_switch_scratch_owner_reuse_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(output.find("switch case ownership mismatch: owned transfers must match across all continuing cases") !=
           std::string::npos);
    assert(output.find("branch-local cleanup plan: owner first_left_scratch") != std::string::npos);
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
    constexpr auto run_examples = std::array<std::string_view, 16> {
        "local_array_for.or",
        "local_dynamic_array_computed_for.or",
        "local_dynamic_array_owned_computed_for.or",
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
    auto owned_computed_dynamic_array_path = examples / "local_dynamic_array_owned_computed_for.or";
    auto owned_dynamic_array_parameter_path = examples / "dynamic_array_owned_parameter.or";
    auto owned_dynamic_array_parameter_forwarding_path =
        fixtures / "dynamic_array_owned_parameter_forwarding_run.or";
    auto returned_dynamic_array_parameter_forwarding_path =
        fixtures / "dynamic_array_returned_parameter_forwarding_run.or";
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
    auto dynamic_array_owned_result_if_two_switches_cleanup_path =
        fixtures / "dynamic_array_owned_result_if_two_switches_cleanup_run.or";
    auto dynamic_array_owned_result_switch_two_ifs_cleanup_path =
        fixtures / "dynamic_array_owned_result_switch_two_ifs_cleanup_run.or";
    auto dynamic_array_owned_result_multi_nested_switch_returned_reuse_path =
        fixtures / "dynamic_array_owned_result_multi_nested_switch_returned_reuse_rejected.or";
    auto dynamic_array_owned_result_multi_nested_switch_scratch_reuse_path =
        fixtures / "dynamic_array_owned_result_multi_nested_switch_scratch_reuse_rejected.or";
    auto owned_dynamic_array_parameter_forwarding_reuse_path =
        fixtures / "dynamic_array_owned_parameter_forwarding_reuse_rejected.or";
    auto owned_dynamic_array_parameter_branch_join_path =
        fixtures / "dynamic_array_owned_parameter_branch_join_run.or";
    auto owned_dynamic_array_parameter_branch_mismatch_path =
        fixtures / "dynamic_array_owned_parameter_branch_mismatch_rejected.or";
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
    assert_dynamic_array_switch_returned_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_multi_nested_switch_returned_reuse_path
    );
    assert_dynamic_array_switch_scratch_owner_reuse_emit_llvm_failure(
        executable,
        dynamic_array_owned_result_multi_nested_switch_scratch_reuse_path
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
    assert_owned_dynamic_array_parameter_branch_mismatch_emit_llvm_failure(
        executable,
        owned_dynamic_array_parameter_branch_mismatch_path
    );
    assert_owned_dynamic_array_parameter_branch_mismatch_emit_llvm_failure(
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
