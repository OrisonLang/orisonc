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
#include <unistd.h>

namespace {

void write_loop_control_fixture(
    std::filesystem::path const& path,
    std::string_view function_header,
    std::initializer_list<std::string_view> body_lines
) {
    std::ofstream output(path);
    output << "package demo.loops\n";
    output << "function " << function_header << "\n";
    for (auto line : body_lines) {
        output << "    " << line << "\n";
    }
}

auto run_parse(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "--parse",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_parse_success(orison::driver::CompileResult const& result) {
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    assert(result.stdout_text.find("parsed ") != std::string_view::npos);
}

void assert_parse_failure_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_message) != std::string_view::npos);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_loop_control_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto break_outside_loop_failure_path = std::filesystem::temp_directory_path() / "break_outside_loop_failure.or";
    write_loop_control_fixture(break_outside_loop_failure_path, "stop() -> Unit", {"break"});
    assert_parse_failure_contains(
        run_parse(app, break_outside_loop_failure_path),
        "break statement is only valid inside loops"
    );

    auto continue_inside_loop_success_path = std::filesystem::temp_directory_path() / "continue_inside_loop_success.or";
    write_loop_control_fixture(
        continue_inside_loop_success_path,
        "scan(items: shared View<Int64>) -> Unit",
        {"for item in items", "    continue"}
    );
    assert_parse_success(run_parse(app, continue_inside_loop_success_path));

    return 0;
}
