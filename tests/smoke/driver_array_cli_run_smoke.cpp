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
    constexpr auto run_examples = std::array<std::string_view, 12> {
        "local_array_for.or",
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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
