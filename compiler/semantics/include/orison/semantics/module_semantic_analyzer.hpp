#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/semantics/drop_model.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::semantics {

enum class ConcurrencyExpressionKind {
    task,
    thread,
};

enum class ConcurrencyCaptureKind {
    parameter,
    immutable_outer_local,
    mutable_outer_local,
    receiver,
};

struct ConcurrencyCapture {
    std::size_t line = 0;
    std::string name;
    std::string type_name;
    ConcurrencyExpressionKind expression_kind = ConcurrencyExpressionKind::task;
    ConcurrencyCaptureKind capture_kind = ConcurrencyCaptureKind::parameter;
};

enum class DynamicArrayDescriptorOriginKind {
    local_binding,
    parameter_binding,
    returned_binding,
};

struct DynamicArrayDescriptorOrigin {
    std::string owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    DynamicArrayDescriptorOriginKind origin_kind = DynamicArrayDescriptorOriginKind::local_binding;
    std::size_t line = 0;
};

enum class SemanticFunctionKind {
    source_function,
    foreign_import_function,
    foreign_export_function,
    implementation_method,
    extension_method,
};

struct SemanticParameterSummary {
    std::size_t line = 0;
    std::string name;
    std::string type_name;
};

struct SemanticFunctionSummary {
    std::size_t line = 0;
    std::string name;
    std::string owner_type_name;
    std::vector<std::string> generic_parameters;
    std::vector<SemanticParameterSummary> parameters;
    std::string return_type_name;
    SemanticFunctionKind kind = SemanticFunctionKind::source_function;
    bool foreign = false;
    bool async_function = false;
    bool unsafe_function = false;
};

struct SemanticRecordFieldSummary {
    std::size_t line = 0;
    std::string record_type_name;
    std::string field_name;
    std::string field_type_name;
};

struct SemanticChoiceVariantSummary {
    std::size_t line = 0;
    std::string choice_type_name;
    std::string variant_name;
    std::vector<SemanticParameterSummary> payloads;
};

enum class SemanticExpressionTargetKind {
    none,
    direct_function,
    method,
    record_constructor,
    choice_constructor,
    unsafe_intrinsic,
};

struct SemanticExpressionSummary {
    std::size_t line = 0;
    std::string text;
    std::string type_name;
    SemanticExpressionTargetKind target_kind = SemanticExpressionTargetKind::none;
    std::string target_name;
    std::string receiver_type_name;
    bool foreign = false;
};

enum class SemanticOwnershipOriginKind {
    local_binding,
    parameter_binding,
    returned_binding,
    receiver_binding,
    module_constant,
};

struct SemanticOwnershipSummary {
    std::size_t line = 0;
    std::string owner_name;
    std::string type_name;
    SemanticOwnershipOriginKind origin_kind = SemanticOwnershipOriginKind::local_binding;
    bool mutable_binding = false;
    bool requires_drop = false;
};

struct SemanticDropObligationSummary {
    std::size_t line = 0;
    std::string owner_name;
    std::string source_type_name;
    std::string abi_symbol_name;
};

struct SemanticModuleSummary {
    std::vector<SemanticFunctionSummary> functions;
    std::vector<SemanticRecordFieldSummary> record_fields;
    std::vector<SemanticChoiceVariantSummary> choice_variants;
    std::vector<SemanticExpressionSummary> expressions;
    std::vector<SemanticOwnershipSummary> ownership_facts;
    std::vector<SemanticDropObligationSummary> drop_obligations;
};

struct SemanticAnalysisResult {
    diagnostics::DiagnosticBag diagnostics;
    SemanticModuleSummary semantic_module;
    std::vector<ConcurrencyCapture> concurrency_captures;
    std::vector<PlannedDropSite> planned_drop_sites;
    std::vector<DynamicArrayDescriptorOrigin> dynamic_array_descriptor_origins;

    auto has_errors() const -> bool {
        return diagnostics.has_errors();
    }

    auto entries() const -> std::vector<diagnostics::Diagnostic> const& {
        return diagnostics.entries();
    }

    auto render(std::string_view path) const -> std::string {
        return diagnostics.render(path);
    }
};

auto format_dynamic_array_descriptor_origin(DynamicArrayDescriptorOrigin const& origin) -> std::string;

auto format_dynamic_array_descriptor_origin_report(
    std::vector<DynamicArrayDescriptorOrigin> const& origins
) -> std::vector<std::string>;

class ModuleSemanticAnalyzer {
public:
    auto analyze(syntax::ModuleSyntax const& module) const -> SemanticAnalysisResult;
};

}  // namespace orison::semantics
