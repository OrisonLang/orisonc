#pragma once

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

void populate_lowering_emission_reports(
    CompilePipelineResult& result,
    lowering::LlvmIrEmissionResult&& emission,
    CompilePipelineOptions const& options
);

}  // namespace orison::pipeline
