#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <span>

int main() {
    orison::driver::CompilerApp app;

    std::array<char const*, 2> version_argv {"orisonc", "--version"};
    auto result = app.run(std::span<char const* const>(version_argv.data(), version_argv.size()));

    assert(result.exit_code == 0);
    assert(result.stdout_text == "orisonc 0.1.0-dev\n");
    assert(result.stderr_text.empty());

    auto path = std::filesystem::temp_directory_path() / "orison_compiler_app_await_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.await\n";
        output << "function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return await request(url)\n";
    }

    auto path_text = path.string();
    std::array<char const*, 3> parse_argv {"orisonc", "--parse", path_text.c_str()};
    auto parse_result = app.run(std::span<char const* const>(parse_argv.data(), parse_argv.size()));

    assert(parse_result.exit_code == 1);
    assert(parse_result.stdout_text.empty());
    assert(parse_result.stderr_text.find("await expression is only valid inside async functions") != std::string::npos);
    return 0;
}
