#include "orison/lowering/dynamic_array_runtime.hpp"

#include "orison/lowering/source_type_queries.hpp"

#include <sstream>

namespace orison::lowering {
namespace {

auto has_reported_symbol(
    std::vector<std::string_view> const& reported_symbols,
    std::string_view symbol_name
) -> bool {
    for (auto reported_symbol : reported_symbols) {
        if (reported_symbol == symbol_name) {
            return true;
        }
    }
    return false;
}

}  // namespace

auto dynamic_array_runtime_call(
    DynamicArrayRuntimeOperation operation
) -> DynamicArrayRuntimeCall {
    switch (operation) {
    case DynamicArrayRuntimeOperation::allocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_allocate",
            .return_type = dynamic_array_descriptor_llvm_type(),
            .parameter_types = {"i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::grow:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_grow",
            .return_type = dynamic_array_descriptor_llvm_type(),
            .parameter_types = {dynamic_array_descriptor_llvm_type(), "i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::deallocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_deallocate",
            .return_type = "void",
            .parameter_types = {"ptr", "i64", "i64"},
        };
    }
    return {};
}

auto format_dynamic_array_runtime_request(
    DynamicArrayRuntimeOperation operation
) -> std::string {
    auto runtime_call = dynamic_array_runtime_call(operation);
    auto output = std::ostringstream {};
    output << "dynamic array runtime " << runtime_call.symbol_name;
    output << " returns " << runtime_call.return_type;
    output << " params";
    for (auto parameter_type : runtime_call.parameter_types) {
        output << " " << parameter_type;
    }
    return output.str();
}

auto format_dynamic_array_runtime_request_report(
    std::vector<DynamicArrayRuntimeOperation> const& operations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    auto reported_symbols = std::vector<std::string_view> {};
    for (auto operation : operations) {
        auto runtime_call = dynamic_array_runtime_call(operation);
        if (has_reported_symbol(reported_symbols, runtime_call.symbol_name)) {
            continue;
        }
        report.push_back(format_dynamic_array_runtime_request(operation));
        reported_symbols.push_back(runtime_call.symbol_name);
    }
    return report;
}

}  // namespace orison::lowering
