#pragma once

#include "orison/driver/compile_result.hpp"

#include <span>

namespace orison::driver {

class CompilerApp {
public:
    [[nodiscard]] static auto run(std::span<char const* const> args) -> CompileResult;
};

}  // namespace orison::driver
