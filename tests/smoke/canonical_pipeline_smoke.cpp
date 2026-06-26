#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string_view>
#include <sys/wait.h>

namespace {

auto run_driver(std::array<char const*, 3> const& args) -> orison::driver::CompileResult {
    orison::driver::CompilerApp app;
    return app.run(std::span<char const* const>(args.data(), args.size()));
}

auto run_driver(std::array<char const*, 5> const& args) -> orison::driver::CompileResult {
    orison::driver::CompilerApp app;
    return app.run(std::span<char const* const>(args.data(), args.size()));
}

void assert_emit_llvm(std::filesystem::path const& source_path, std::string_view required_text) {
    auto source_path_text = source_path.string();
    auto args = std::array<char const*, 3> {
        "orisonc",
        "--emit-llvm",
        source_path_text.c_str(),
    };
    auto result = run_driver(args);
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    assert(result.stdout_text.find(required_text) != std::string::npos);
}

void assert_emit_object(std::filesystem::path const& source_path, std::filesystem::path const& object_path) {
    auto source_path_text = source_path.string();
    auto object_path_text = object_path.string();
    auto args = std::array<char const*, 5> {
        "orisonc",
        "--emit-object",
        source_path_text.c_str(),
        "-o",
        object_path_text.c_str(),
    };
    auto result = run_driver(args);
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
    assert(std::filesystem::file_size(object_path) > 0);
}

void assert_run(std::filesystem::path const& source_path) {
    auto source_path_text = source_path.string();
    auto args = std::array<char const*, 3> {
        "orisonc",
        "run",
        source_path_text.c_str(),
    };
    auto result = run_driver(args);
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

void assert_build(std::filesystem::path const& source_path, std::filesystem::path const& executable_path) {
    auto source_path_text = source_path.string();
    auto executable_path_text = executable_path.string();
    auto args = std::array<char const*, 5> {
        "orisonc",
        "--build",
        source_path_text.c_str(),
        "-o",
        executable_path_text.c_str(),
    };
    auto result = run_driver(args);
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());

    auto status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

void assert_pipeline_demo(
    std::string_view name,
    std::string_view required_ir_text
) {
    auto source_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / name;
    auto output_stem = std::filesystem::temp_directory_path() / std::filesystem::path(name).stem();

    assert_emit_llvm(source_path, required_ir_text);
    assert_emit_object(source_path, output_stem.string() + ".o");
    assert_run(source_path);
    assert_build(source_path, output_stem.string() + ".exe");
}

}  // namespace

auto main() -> int {
    assert_pipeline_demo("minimal.or", "ret i32 0");
    assert_pipeline_demo("concurrency_task_main.or", "call ptr @__orison_task_spawn");
    assert_pipeline_demo("local_record_field_assignment.or", "store i32 8, ptr %tmp");
    assert_pipeline_demo("pointer_record_field_assignment.or", "store i32 8, ptr %tmp");
    return 0;
}
