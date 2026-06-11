#include "orison/pipeline/compile_pipeline.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <string_view>

auto main() -> int {
    auto examples = std::filesystem::path(ORISON_SOURCE_DIR) / "examples";
    constexpr auto frontend_examples = std::array<std::string_view, 11> {
        "tour_01_packages_imports.or",
        "tour_02_records_choices.or",
        "tour_03_interfaces_methods.or",
        "tour_04_generics_ownership.or",
        "tour_05_bindings_operators.or",
        "tour_06_control_flow.or",
        "tour_07_recursion.or",
        "tour_08_collections.or",
        "tour_09_ffi_printf.or",
        "tour_10_unsafe_memory.or",
        "tour_11_concurrency.or",
    };

    orison::pipeline::CompilePipeline pipeline;
    for (auto name : frontend_examples) {
        auto result = pipeline.analyze(examples / name);
        assert(!result.has_errors());
    }

    auto backend = pipeline.emit_object(examples / "minimal.or");
    assert(!backend.has_errors());
    assert(!backend.object_bytes.empty());
    return 0;
}
