#pragma once

#include <string>

namespace orison::driver {

struct CompileResult {
    int exit_code = 0;
    std::string stdout_text;
    std::string stderr_text;
};

}  // namespace orison::driver
