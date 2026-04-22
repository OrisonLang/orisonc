#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

int main(int argc, char** argv) {
    std::array<char const*, 64> storage {};
    auto count = static_cast<std::size_t>(argc);
    for (std::size_t i = 0; i < count; ++i) {
        storage[i] = argv[i];
    }

    orison::driver::CompilerApp app;
    std::string output = app.run(std::span<char const* const>(storage.data(), count));
    std::cout << output << '\n';
    return EXIT_SUCCESS;
}
