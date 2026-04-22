#include "orison/driver/compiler_app.hpp"

#include <string_view>

namespace orison::driver {

std::string CompilerApp::run(std::span<char const* const> args) const {
    if (args.size() > 1 && std::string_view(args[1]) == "--version") {
        return "orisonc 0.1.0-dev";
    }

    return "orisonc bootstrap driver";
}

}  // namespace orison::driver
