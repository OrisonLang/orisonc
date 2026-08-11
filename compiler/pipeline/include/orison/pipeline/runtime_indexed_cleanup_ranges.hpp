#pragma once

#include <cstddef>

namespace orison::pipeline {

struct RuntimeIndexedCleanupTextSpliceRange {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
};

} // namespace orison::pipeline
