#include "semantic_analysis_stage.hpp"

#include "link_library_collection.hpp"
#include "semantic_owned_cleanup_reports.hpp"

#include <algorithm>

namespace orison::pipeline {

auto run_semantic_analysis_stage(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult {
    auto result = CompilePipelineResult {};
    result.source_file = source::SourceFile::read(source_path);
    if (!result.source_file.has_value()) {
        result.error_text = "error: unable to read source file\n";
        return result;
    }

    syntax::ModuleParser parser;
    result.parse_result = parser.parse(*result.source_file);
    if (result.parse_result.diagnostics.has_errors()) {
        result.error_text = result.parse_result.diagnostics.render(result.source_file->path().string());
        return result;
    }
    collect_link_libraries(result.parse_result.module, result.link_libraries);

    auto semantic_analysis_options = options.semantic_analysis;
    auto resolved_imported_declarations = semantics::resolve_imported_type_declarations(
        result.parse_result.module,
        options.available_package_interfaces
    );
    for (auto const& resolved_declaration : resolved_imported_declarations) {
        auto already_supplied = std::ranges::any_of(
            semantic_analysis_options.imported_type_declarations,
            [&](semantics::ImportedTypeDeclarationMetadata const& supplied_declaration) {
                return supplied_declaration.package_name == resolved_declaration.package_name &&
                       supplied_declaration.declaration_name == resolved_declaration.declaration_name;
            }
        );
        if (!already_supplied) {
            semantic_analysis_options.imported_type_declarations.push_back(resolved_declaration);
        }
    }
    auto resolved_imported_aggregates = semantics::resolve_imported_aggregate_declarations(
        result.parse_result.module,
        options.available_package_interfaces
    );
    for (auto const& record : resolved_imported_aggregates.records) {
        auto already_supplied = std::ranges::any_of(
            semantic_analysis_options.imported_record_declarations,
            [&](syntax::RecordSyntax const& supplied_record) {
                return supplied_record.name == record.name;
            }
        );
        if (!already_supplied) {
            semantic_analysis_options.imported_record_declarations.push_back(record);
        }
    }
    for (auto const& choice : resolved_imported_aggregates.choices) {
        auto already_supplied = std::ranges::any_of(
            semantic_analysis_options.imported_choice_declarations,
            [&](syntax::ChoiceSyntax const& supplied_choice) {
                return supplied_choice.name == choice.name;
            }
        );
        if (!already_supplied) {
            semantic_analysis_options.imported_choice_declarations.push_back(choice);
        }
    }

    semantics::ModuleSemanticAnalyzer semantic_analyzer;
    result.semantic_result = semantic_analyzer.analyze(result.parse_result.module, semantic_analysis_options);
    if (result.semantic_result.has_errors()) {
        result.error_text = result.semantic_result.render(result.source_file->path().string());
    }
    populate_semantic_owned_cleanup_reports(result, options);
    return result;
}

}  // namespace orison::pipeline
