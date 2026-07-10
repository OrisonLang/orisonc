#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

auto run_minimal(orison::driver::CompilerApp const& app, std::filesystem::path const& source_path)
    -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "run",
        source_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto build_minimal(
    orison::driver::CompilerApp const& app,
    std::filesystem::path const& source_path,
    std::filesystem::path const& executable_path
) -> orison::driver::CompileResult {
    auto source_path_text = source_path.string();
    auto executable_path_text = executable_path.string();
    std::array<char const*, 5> argv {
        "orisonc",
        "--build",
        source_path_text.c_str(),
        "-o",
        executable_path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_success_without_output(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.empty());
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_minimal_demo_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};
    auto minimal_source = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "minimal.or";
    assert(std::filesystem::exists(minimal_source));

    auto run_result = run_minimal(app, minimal_source);
    assert_success_without_output(run_result);

    auto executable_path = smoke_temp_root / "minimal";
    auto build_result = build_minimal(app, minimal_source, executable_path);
    assert_success_without_output(build_result);
    assert(std::filesystem::file_size(executable_path) > 0);

    auto executable_status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(executable_status));
    assert(WEXITSTATUS(executable_status) == 0);

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
