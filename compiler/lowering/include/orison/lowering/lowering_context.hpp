#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace orison::lowering {

struct LoweredMethodSignature {
    std::string receiver_type_name;
    std::string method_name;
    LoweredFunctionSignature signature;
};

struct LoweredRecordField {
    std::string name;
    std::string source_type_name;
    std::string llvm_type;
    std::size_t index = 0;
};

struct LoweredRecordLayout {
    std::string name;
    std::string llvm_type_name;
    std::vector<LoweredRecordField> fields;
};

struct LoweredChoicePayload {
    std::string name;
    std::string source_type_name;
    std::string llvm_type;
    std::size_t index = 0;
};

struct LoweredChoiceVariant {
    std::string name;
    std::size_t tag = 0;
    std::vector<LoweredChoicePayload> payloads;
};

struct LoweredChoiceLayout {
    std::string name;
    std::string source_type_name;
    std::vector<std::string> generic_parameters;
    std::vector<LoweredChoiceVariant> variants;
};

struct LoweringContext {
    std::unordered_map<std::string, LoweredFunctionSignature> functions;
    std::unordered_map<std::string, LoweredRecordLayout> records;
    std::unordered_map<std::string, LoweredChoiceLayout> choices;
    std::vector<LoweredMethodSignature> methods;
    std::vector<LoweredFunctionSignature> foreign_declarations;
};

enum class LoweredMethodLookupResult {
    not_found,
    found,
    ambiguous,
};

struct LoweredMethodLookup {
    LoweredMethodLookupResult result = LoweredMethodLookupResult::not_found;
    LoweredMethodSignature const* method = nullptr;
};

auto build_lowering_context(
    syntax::ModuleSyntax const& module,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweringContext;

auto find_lowered_method_signature(
    LoweringContext const& context,
    std::string_view receiver_type_name,
    std::string_view method_name
) -> LoweredMethodLookup;

auto lowered_method_symbol_name(
    std::string_view receiver_type_name,
    std::string_view method_name
) -> std::string;

auto lowered_record_type_name(std::string_view record_name) -> std::string;

}  // namespace orison::lowering
