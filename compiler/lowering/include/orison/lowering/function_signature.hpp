#pragma once

#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::lowering {

struct LoweredFunctionSignature {
    std::string return_type;
    std::string source_return_type_name;
    IntegerSignedness return_signedness = IntegerSignedness::not_integer;
    std::vector<std::string> parameter_types;
    std::vector<IntegerSignedness> parameter_signedness;
    std::string symbol_name;
    CAbiAdapterKind adapter = CAbiAdapterKind::none;
    std::size_t fixed_abi_parameter_count = 0;
};

auto lower_function_signature(
    syntax::TypeSyntax const& return_type,
    std::vector<syntax::ParameterSyntax> const& parameters,
    std::string symbol_name
) -> LoweredFunctionSignature;

auto has_supported_function_signature_types(LoweredFunctionSignature const& signature) -> bool;

auto apply_c_abi_adapter(
    LoweredFunctionSignature& signature,
    CAbiAdapterMetadata const& adapter
) -> CAbiAdapterValidationResult;

}  // namespace orison::lowering
