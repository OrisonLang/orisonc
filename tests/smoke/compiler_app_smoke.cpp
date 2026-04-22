#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <span>

int main() {
    orison::driver::CompilerApp app;

    std::array<char const*, 2> argv {"orisonc", "--version"};
    auto result = app.run(std::span<char const* const>(argv.data(), argv.size()));

    assert(result.exit_code == 0);
    assert(result.stdout_text == "orisonc 0.1.0-dev\n");
    assert(result.stderr_text.empty());
    return 0;
}
