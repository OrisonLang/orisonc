#include "dynamic_array_cleanup_metadata_stage.hpp"

#include "orison/pipeline/dynamic_array_cleanup_metadata.hpp"

namespace orison::pipeline {

auto run_dynamic_array_cleanup_metadata_stage(
    CompilePipeline const& pipeline,
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult {
    return DynamicArrayCleanupMetadataCollector(pipeline).collect(source_path, options);
}

}  // namespace orison::pipeline
