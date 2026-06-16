#include "orison/lowering/type_lowering.hpp"

#include <array>

namespace orison::lowering {
namespace {

auto has_generic_arguments(syntax::TypeSyntax const& type) -> bool {
    return !type.generic_arguments.empty();
}

}  // namespace

auto llvm_type_for(syntax::TypeSyntax const& type) -> std::optional<std::string_view> {
    if (type.name == "Pointer" && type.generic_arguments.size() == 1) {
        return "ptr";
    }
    if (has_generic_arguments(type)) {
        return std::nullopt;
    }

    struct TypeMapping {
        std::string_view orison_type;
        std::string_view llvm_type;
    };

    constexpr auto mappings = std::array {
        TypeMapping {"Unit", "void"},
        TypeMapping {"Bool", "i1"},
        TypeMapping {"Byte", "i8"},
        TypeMapping {"Address", "i64"},
        TypeMapping {"UInt8", "i8"},
        TypeMapping {"Int8", "i8"},
        TypeMapping {"UInt16", "i16"},
        TypeMapping {"Int16", "i16"},
        TypeMapping {"UInt32", "i32"},
        TypeMapping {"Int32", "i32"},
        TypeMapping {"UInt64", "i64"},
        TypeMapping {"Int64", "i64"},
        TypeMapping {"Float32", "float"},
        TypeMapping {"Float64", "double"},
    };

    for (auto const& mapping : mappings) {
        if (type.name == mapping.orison_type) {
            return mapping.llvm_type;
        }
    }
    return std::nullopt;
}

auto integer_signedness_for(syntax::TypeSyntax const& type) -> IntegerSignedness {
    if (has_generic_arguments(type)) {
        return IntegerSignedness::not_integer;
    }
    if (type.name == "Byte" || type.name == "UInt8" || type.name == "UInt16" || type.name == "UInt32" ||
        type.name == "UInt64") {
        return IntegerSignedness::unsigned_integer;
    }
    if (type.name == "Address") {
        return IntegerSignedness::not_integer;
    }
    if (type.name == "Int8" || type.name == "Int16" || type.name == "Int32" || type.name == "Int64") {
        return IntegerSignedness::signed_integer;
    }
    return IntegerSignedness::not_integer;
}

auto llvm_parameter_types_for(
    std::vector<syntax::ParameterSyntax> const& parameters
) -> std::optional<std::vector<std::string>> {
    auto parameter_types = std::vector<std::string> {};
    parameter_types.reserve(parameters.size());
    for (auto const& parameter : parameters) {
        auto parameter_type = llvm_type_for(parameter.type);
        if (!parameter_type.has_value() || *parameter_type == "void") {
            return std::nullopt;
        }
        parameter_types.emplace_back(*parameter_type);
    }
    return parameter_types;
}

auto parameter_signedness_for(
    std::vector<syntax::ParameterSyntax> const& parameters
) -> std::vector<IntegerSignedness> {
    auto signedness = std::vector<IntegerSignedness> {};
    signedness.reserve(parameters.size());
    for (auto const& parameter : parameters) {
        signedness.push_back(integer_signedness_for(parameter.type));
    }
    return signedness;
}

}  // namespace orison::lowering
