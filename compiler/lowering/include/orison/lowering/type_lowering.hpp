#pragma once

#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

enum class IntegerSignedness {
    not_integer,
    signed_integer,
    unsigned_integer,
};

auto llvm_type_for(syntax::TypeSyntax const& type) -> std::optional<std::string_view>;

auto integer_signedness_for(syntax::TypeSyntax const& type) -> IntegerSignedness;

auto llvm_parameter_types_for(
    std::vector<syntax::ParameterSyntax> const& parameters
) -> std::optional<std::vector<std::string>>;

auto parameter_signedness_for(
    std::vector<syntax::ParameterSyntax> const& parameters
) -> std::vector<IntegerSignedness>;

}  // namespace orison::lowering
