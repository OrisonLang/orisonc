#pragma once

#include "orison/pipeline/compile_pipeline_options.hpp"
#include "orison/pipeline/compile_pipeline_readiness.hpp"
#include "orison/pipeline/compile_pipeline_result.hpp"

#include <filesystem>

namespace orison::pipeline {

class CompilePipeline {
public:
    auto analyze(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto analyze(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_llvm(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_object(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto collect_dynamic_array_cleanup_metadata(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
};

}  // namespace orison::pipeline
