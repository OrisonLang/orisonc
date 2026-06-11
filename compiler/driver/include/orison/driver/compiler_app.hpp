#pragma once

#include "orison/driver/compile_result.hpp"

#include <span>

namespace orison::driver {

class CompilerApp {
public:
    auto run(std::span<char const* const> args) const -> CompileResult;
};

}  // namespace orison::driver
