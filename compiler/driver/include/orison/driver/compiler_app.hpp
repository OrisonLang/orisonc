#pragma once

#include <span>
#include <string>

namespace orison::driver {

class CompilerApp {
public:
    std::string run(std::span<char const* const> args) const;
};

}  // namespace orison::driver
