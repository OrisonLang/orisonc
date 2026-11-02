#pragma once

#include "orison/driver/compile_result.hpp"
#include "orison/pipeline/compile_pipeline_options.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::driver {

struct BuildPlanPackage {
    std::string package_name;
    std::optional<std::filesystem::path> source_path;
    std::optional<std::filesystem::path> interface_path;
    std::optional<std::filesystem::path> object_path;
    std::vector<std::string> dependencies;
    std::vector<std::string> link_libraries;
};

struct BuildPlan {
    std::string entry_package;
    std::vector<BuildPlanPackage> packages;
};

struct BuildPlanParseResult {
    std::optional<BuildPlan> plan;
    std::string error_message;

    auto has_error() const -> bool {
        return !error_message.empty();
    }
};

auto parse_build_plan(std::string_view text, std::filesystem::path const& base_directory)
    -> BuildPlanParseResult;

auto compile_build_plan(
    std::filesystem::path const& plan_path,
    std::filesystem::path const& output_path,
    pipeline::CompilePipelineOptions const& base_options
) -> CompileResult;

}  // namespace orison::driver
