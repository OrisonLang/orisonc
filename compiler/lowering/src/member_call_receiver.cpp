#include "orison/lowering/member_call_receiver.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

auto split_top_level_generic_arguments(std::string_view text) -> std::vector<std::string> {
    auto arguments = std::vector<std::string> {};
    auto depth = std::size_t {0};
    auto start = std::size_t {0};
    for (auto index = std::size_t {0}; index < text.size(); ++index) {
        auto const character = text[index];
        if (character == '<') {
            ++depth;
            continue;
        }
        if (character == '>') {
            if (depth > 0) {
                --depth;
            }
            continue;
        }
        if (character == ',' && depth == 0) {
            auto argument = std::string {text.substr(start, index - start)};
            if (!argument.empty() && argument.front() == ' ') {
                argument.erase(argument.begin());
            }
            if (!argument.empty() && argument.back() == ' ') {
                argument.pop_back();
            }
            arguments.push_back(std::move(argument));
            start = index + 1;
        }
    }

    if (start < text.size()) {
        auto argument = std::string {text.substr(start)};
        if (!argument.empty() && argument.front() == ' ') {
            argument.erase(argument.begin());
        }
        if (!argument.empty() && argument.back() == ' ') {
            argument.pop_back();
        }
        arguments.push_back(std::move(argument));
    }
    return arguments;
}

struct ParsedLlvmArrayType {
    std::string element_type;
    std::size_t length = 0;
};

auto parse_llvm_array_type(std::string_view type) -> std::optional<ParsedLlvmArrayType> {
    if (!type.starts_with("[") || !type.ends_with("]") || type.size() < 6) {
        return std::nullopt;
    }

    auto depth = std::size_t {0};
    auto separator = std::size_t {0};
    for (auto index = std::size_t {1}; index + 1 < type.size(); ++index) {
        auto const character = type[index];
        if (character == '[') {
            ++depth;
            continue;
        }
        if (character == ']') {
            if (depth > 0) {
                --depth;
            }
            continue;
        }
        if (depth == 0 && character == 'x' && index > 1 && index + 2 < type.size() &&
            type[index - 1] == ' ' && type[index + 1] == ' ') {
            separator = index;
            break;
        }
    }
    if (separator == 0) {
        return std::nullopt;
    }

    auto length_text = std::string {type.substr(1, separator - 2)};
    auto element_type = std::string {type.substr(separator + 2, type.size() - separator - 3)};
    if (length_text.empty() || element_type.empty()) {
        return std::nullopt;
    }

    auto length = std::size_t {0};
    for (auto character : length_text) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        length = (length * 10) + static_cast<std::size_t>(character - '0');
    }

    return ParsedLlvmArrayType {
        .element_type = std::move(element_type),
        .length = length,
    };
}

auto array_element_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Array<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    auto arguments = split_top_level_generic_arguments(
        type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
    );
    if (arguments.size() != 2 || arguments[0].empty()) {
        return std::nullopt;
    }
    return arguments[0];
}

auto source_type_name_for_llvm_type(
    std::string_view llvm_type,
    LoweringContext const& context
) -> std::optional<std::string> {
    if (llvm_type == "i1") {
        return std::string {"Bool"};
    }
    if (llvm_type == "i8") {
        return std::string {"UInt8"};
    }
    if (llvm_type == "i16") {
        return std::string {"UInt16"};
    }
    if (llvm_type == "i32") {
        return std::string {"UInt32"};
    }
    if (llvm_type == "i64") {
        return std::string {"UInt64"};
    }
    if (llvm_type == "float") {
        return std::string {"Float32"};
    }
    if (llvm_type == "double") {
        return std::string {"Float64"};
    }

    for (auto const& [record_name, layout] : context.records) {
        if (layout.llvm_type_name == llvm_type) {
            return record_name;
        }
    }

    if (auto array = parse_llvm_array_type(llvm_type)) {
        auto element_source_type = source_type_name_for_llvm_type(array->element_type, context);
        if (!element_source_type.has_value()) {
            return std::nullopt;
        }

        return std::string {"Array<"} + *element_source_type + ", " + std::to_string(array->length) + ">";
    }

    return std::nullopt;
}

auto find_record_field(
    LoweredRecordLayout const& layout,
    std::string_view field_name
) -> LoweredRecordField const* {
    for (auto const& field : layout.fields) {
        if (field.name == field_name) {
            return &field;
        }
    }
    return nullptr;
}

auto source_type_name_for_expression(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto source_type = state.source_type_names.find(expression.text);
        if (source_type != state.source_type_names.end()) {
            return source_type->second;
        }
    }

    if (expression.kind == syntax::ExpressionKind::member_access && expression.left != nullptr) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            return std::nullopt;
        }

        auto layout = context.records.find(*base_source_type);
        if (layout == context.records.end()) {
            return std::nullopt;
        }

        auto const* field = find_record_field(layout->second, expression.text);
        if (field == nullptr || field->source_type_name.empty()) {
            return std::nullopt;
        }
        return field->source_type_name;
    }

    if (expression.kind == syntax::ExpressionKind::index_access && expression.left != nullptr &&
        expression.arguments.size() == 1) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            return std::nullopt;
        }

        return array_element_source_type_name(*base_source_type);
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto function = context.functions.find(expression.left->text);
        if (function == context.functions.end() || function->second.return_type.empty() ||
            function->second.return_type == "void") {
            return std::nullopt;
        }

        return source_type_name_for_llvm_type(function->second.return_type, context);
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        (expression.left->kind == syntax::ExpressionKind::member_access ||
         expression.left->kind == syntax::ExpressionKind::null_safe_member_access)) {
        auto receiver = infer_member_call_receiver(expression, context, state);
        if (receiver.result != MemberCallReceiverInferenceResult::found) {
            return std::nullopt;
        }

        auto method = find_lowered_method_signature(
            context,
            receiver.receiver_type_name,
            receiver.method_name
        );
        if (method.result != LoweredMethodLookupResult::found || method.method == nullptr ||
            method.method->signature.return_type.empty() || method.method->signature.return_type == "void") {
            return std::nullopt;
        }

        return source_type_name_for_llvm_type(method.method->signature.return_type, context);
    }

    return std::nullopt;
}

}  // namespace

auto render_source_type_name(syntax::TypeSyntax const& type) -> std::string {
    auto rendered = type.name;
    if (type.generic_arguments.empty()) {
        return rendered;
    }

    rendered += "<";
    for (auto index = std::size_t {0}; index < type.generic_arguments.size(); ++index) {
        if (index > 0) {
            rendered += ", ";
        }
        rendered += render_source_type_name(type.generic_arguments[index]);
    }
    rendered += ">";
    return rendered;
}

auto infer_member_call_receiver(
    syntax::ExpressionSyntax const& call_expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> MemberCallReceiverInference {
    if (call_expression.kind != syntax::ExpressionKind::call || call_expression.left == nullptr ||
        (call_expression.left->kind != syntax::ExpressionKind::member_access &&
         call_expression.left->kind != syntax::ExpressionKind::null_safe_member_access) ||
        call_expression.left->left == nullptr) {
        return {};
    }

    auto receiver_type = source_type_name_for_expression(*call_expression.left->left, context, state);
    if (!receiver_type.has_value()) {
        return MemberCallReceiverInference {
            .result = MemberCallReceiverInferenceResult::not_found,
            .method_name = call_expression.left->text,
        };
    }

    return MemberCallReceiverInference {
        .result = MemberCallReceiverInferenceResult::found,
        .receiver_type_name = std::move(*receiver_type),
        .method_name = call_expression.left->text,
    };
}

}  // namespace orison::lowering
