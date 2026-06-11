#pragma once

#include "orison/syntax/module_parser.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace orison::lowering {

struct StringConstant {
    std::string name;
    std::string bytes;
    std::string llvm_bytes;
};

struct StringConstantTable {
    std::unordered_map<std::string, std::size_t> source_indices;
    std::vector<StringConstant> constants;

    auto find(std::string_view source_literal) const -> StringConstant const*;
};

auto decode_string_literal(std::string_view source_literal) -> std::string;

auto encode_llvm_string_bytes(std::string_view bytes) -> std::string;

auto collect_string_constants(syntax::ModuleSyntax const& module) -> StringConstantTable;

}  // namespace orison::lowering
