#include "orison/pipeline/compile_pipeline.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
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

    constexpr auto backend_examples = std::array<std::string_view, 27> {
        "ffi_fixed_parameters.or",
        "local_helper_array_for.or",
        "local_array_for.or",
        "local_aggregate_let.or",
        "local_inferred_record_let.or",
        "local_inferred_nested_record_let.or",
        "local_inferred_record_array_let.or",
        "local_inferred_array_let.or",
        "local_inferred_nested_array_let.or",
        "local_nested_aggregate_let.or",
        "local_record_array_for.or",
        "local_record_aggregate_reassignment.or",
        "local_record_field_assignment.or",
        "local_record_index_for.or",
        "local_record_index_field_for.or",
        "local_member_receiver_method_array_for.or",
        "local_method_array_for.or",
        "local_record_method_array_for.or",
        "local_record_nested_addressing.or",
        "local_record_nested_record_addressing.or",
        "local_record_nested_record_assignment.or",
        "minimal.or",
        "nested_pointer_aggregate_assignment.or",
        "pointer_array_nested_assignment.or",
        "pointer_record_field_assignment.or",
        "pointer_record_nested_addressing.or",
        "tour_09_ffi_printf.or",
    };
    for (auto name : backend_examples) {
        auto backend = pipeline.emit_object(examples / name);
        assert(!backend.has_errors());
        assert(!backend.object_bytes.empty());
    }

    auto ordinary_pointer_path = std::filesystem::temp_directory_path() / "orison_string_pointer_boundary.or";
    {
        std::ofstream source(ordinary_pointer_path);
        source << "package examples.boundary\n";
        source << "function consume(value: Pointer<Byte>) -> Int32\n";
        source << "    0 as Int32\n";
        source << "function main() -> Int32\n";
        source << "    consume(\"not an ordinary pointer\")\n";
    }
    auto ordinary_pointer = pipeline.analyze(ordinary_pointer_path);
    assert(ordinary_pointer.has_errors());
    assert(
        ordinary_pointer.error_text.find("requires a structurally pointer-like expression") != std::string::npos
    );

    auto wrong_arity_path = std::filesystem::temp_directory_path() / "orison_fixed_ffi_wrong_arity.or";
    {
        std::ofstream source(wrong_arity_path);
        source << "package examples.boundary\n";
        source << "package foreign \"c\"\n";
        source << "    function strcmp(left: Pointer<Byte>, right: Pointer<Byte>) -> Int32\n";
        source << "function main() -> Int32\n";
        source << "    strcmp(\"Orison\")\n";
    }
    auto wrong_arity = pipeline.analyze(wrong_arity_path);
    assert(wrong_arity.has_errors());
    assert(
        wrong_arity.error_text.find("function 'strcmp' expects 2 arguments but received 1") != std::string::npos
    );
    return 0;
}
