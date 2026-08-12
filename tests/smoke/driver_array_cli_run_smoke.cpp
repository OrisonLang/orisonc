#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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

void assert_dynamic_array_parameter_index_assignment_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find("lowering assignment target is not a mutable local") !=
        std::string::npos
    );
}

void assert_dynamic_array_parameter_push_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& source_path
) {
    auto output = read_failing_command_output(executable.string() + " --emit-llvm " + source_path.string());
    assert(
        output.find("lowering member call target is unknown: DynamicArray<UInt32>.push") !=
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
    auto owned_dynamic_array_parameter_branch_join_path =
        fixtures / "dynamic_array_owned_parameter_branch_join_run.or";
    auto owned_dynamic_array_parameter_branch_mismatch_path =
        fixtures / "dynamic_array_owned_parameter_branch_mismatch_rejected.or";
    auto owned_computed_dynamic_array_missing_drop_path =
        fixtures / "dynamic_array_owned_computed_cleanup_missing_drop.or";
    auto owned_dynamic_array_parameter_missing_drop_path =
        fixtures / "dynamic_array_owned_parameter_iteration_missing_drop.or";
    auto dynamic_array_parameter_index_assignment_path =
        fixtures / "dynamic_array_parameter_index_assignment_rejected.or";
    auto dynamic_array_parameter_push_path =
        fixtures / "dynamic_array_parameter_push_rejected.or";
    auto owned_dynamic_array_replacement_path = examples / "local_dynamic_array_owned_replacement.or";
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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
