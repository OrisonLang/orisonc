#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct ComputedDynamicArrayForProductionSequenceState {
    std::vector<std::string> cleanup_owner_names;
    bool sequence_metadata_available = false;
    bool module_comments_emitted = false;
    std::size_t sequence_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
    std::size_t module_comment_line_count = 0;
};

struct ComputedDynamicArrayForProductionSequenceModuleIrArtifactState {
    std::vector<std::string> comment_ir_lines;
};

} // namespace orison::pipeline
