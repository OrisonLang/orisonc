#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

#include <filesystem>

namespace orison::pipeline {

class DynamicArrayCleanupMetadataCollector {
public:
    explicit DynamicArrayCleanupMetadataCollector(CompilePipeline const& pipeline);

    auto collect(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;

private:
    CompilePipeline const& pipeline_;
};

}  // namespace orison::pipeline
