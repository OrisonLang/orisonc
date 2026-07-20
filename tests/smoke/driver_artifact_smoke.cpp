#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void write_fixture(
    std::filesystem::path const& path,
    std::string_view package_name,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package " << package_name << "\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

auto run_emit_object(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& source_path,
    std::filesystem::path const& output_path
) -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    auto output_path_text = output_path.string();
    std::array<char const*, 5> argv {
        "orisonc",
        "--emit-object",
        source_path_text.c_str(),
        "-o",
        output_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_build(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& source_path,
    std::filesystem::path const& output_path
) -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    auto output_path_text = output_path.string();
    std::array<char const*, 5> argv {
        "orisonc",
        "--build",
        source_path_text.c_str(),
        "-o",
        output_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_program(orison::driver::CompilerApp const& app, std::filesystem::path const& source_path)
    -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "run",
        source_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_success_with_empty_stdout(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

void assert_result_with_empty_output(orison::driver::CompileResult const& result, int expected_exit_code) {
    assert(result.exit_code == expected_exit_code);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_artifact_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};
    auto scalar_path = smoke_temp_root / "artifact_scalar.or";
    write_fixture(
        scalar_path,
        "demo.artifact",
        {
            "function main() -> UInt32",
            "    42 as UInt32",
        }
    );

    auto object_path = smoke_temp_root / "artifact_scalar.o";
    auto object_result = run_emit_object(app, scalar_path, object_path);
    assert_success_with_empty_stdout(object_result);
    assert(std::filesystem::file_size(object_path) > 0);

    auto missing_directory = smoke_temp_root / "orison_missing_object_directory";
    std::filesystem::remove_all(missing_directory);
    auto object_write_failure = run_emit_object(app, scalar_path, missing_directory / "output.o");
    assert(object_write_failure.exit_code == 1);
    assert(object_write_failure.stdout_text.empty());
    assert(object_write_failure.stderr_text == "error: unable to write object file\n");

    auto view_descriptor_path = smoke_temp_root / "view_descriptor_reads.or";
    write_fixture(
        view_descriptor_path,
        "demo.viewdescriptor",
        {
            "function count(values: shared.View<UInt32>) -> IntSize",
            "    values.length()",
            "function first(values: exclusive.View<UInt32>) -> UInt32",
            "    values[0]",
            "function sum(values: shared.View<UInt32>) -> UInt32",
            "    var total = 0 as UInt32",
            "    for item in values",
            "        total = total + item",
            "    total",
        }
    );
    auto view_descriptor_object_path = smoke_temp_root / "view_descriptor_reads.o";
    auto view_descriptor_object_result =
        run_emit_object(app, view_descriptor_path, view_descriptor_object_path);
    assert_success_with_empty_stdout(view_descriptor_object_result);
    assert(std::filesystem::file_size(view_descriptor_object_path) > 0);

    auto executable_path = smoke_temp_root / "artifact_scalar";
    auto build_result = run_build(app, scalar_path, executable_path);
    assert_success_with_empty_stdout(build_result);
    auto executable_status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(executable_status));
    assert(WEXITSTATUS(executable_status) == 42);

    auto replacement_path = smoke_temp_root / "artifact_replacement.or";
    write_fixture(
        replacement_path,
        "demo.replacement",
        {
            "function main() -> UInt32",
            "    7 as UInt32",
        }
    );
    auto replacement_result = run_build(app, replacement_path, executable_path);
    assert_success_with_empty_stdout(replacement_result);
    auto replacement_status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(replacement_status));
    assert(WEXITSTATUS(replacement_status) == 7);

    auto run_result = run_program(app, scalar_path);
    assert_result_with_empty_output(run_result, 42);

    auto run_concurrency_path = smoke_temp_root / "artifact_concurrency.or";
    write_fixture(
        run_concurrency_path,
        "demo.run",
        {
            "async function main() -> UInt32",
            "    let pending = task",
            "        42 as UInt32",
            "",
            "    await pending",
        }
    );
    auto run_concurrency = run_program(app, run_concurrency_path);
    assert_result_with_empty_output(run_concurrency, 42);

    auto link_failure = run_build(app, scalar_path, missing_directory / "output");
    assert(link_failure.exit_code == 1);
    assert(link_failure.stdout_text.empty());
    assert(link_failure.stderr_text.find("output directory does not exist") != std::string::npos);

    auto file_parent_path = smoke_temp_root / "orison_file_parent";
    {
        auto file_parent = std::ofstream(file_parent_path);
        file_parent << "not a directory\n";
    }
    auto file_parent_failure = run_build(app, scalar_path, file_parent_path / "output");
    assert(file_parent_failure.exit_code == 1);
    assert(file_parent_failure.stdout_text.empty());
    assert(file_parent_failure.stderr_text.find("output path parent is not a directory") != std::string::npos);

    auto directory_output_path = smoke_temp_root / "orison_directory_output";
    std::filesystem::create_directory(directory_output_path);
    auto directory_output_failure = run_build(app, scalar_path, directory_output_path);
    assert(directory_output_failure.exit_code == 1);
    assert(directory_output_failure.stdout_text.empty());
    assert(directory_output_failure.stderr_text.find("output path is a directory") != std::string::npos);

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
