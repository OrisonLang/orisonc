#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cstdlib>
#include <cstdio>
#include <span>

int main(int argc, char** argv) {
    std::array<char const*, 64> storage {};
    auto count = static_cast<std::size_t>(argc);
    for (std::size_t i = 0; i < count; ++i) {
        storage[i] = argv[i];
    }

    orison::driver::CompilerApp app;
    auto result = app.run(std::span<char const* const>(storage.data(), count));
    if (!result.stdout_text.empty()) {
        std::fputs(result.stdout_text.c_str(), stdout);
    }
    if (!result.stderr_text.empty()) {
        std::fputs(result.stderr_text.c_str(), stderr);
    }
    return result.exit_code;
}
