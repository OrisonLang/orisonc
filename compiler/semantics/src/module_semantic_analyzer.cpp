#include "orison/semantics/module_semantic_analyzer.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace orison::semantics {
namespace {

enum class ConcurrencyMarkerKind {
    transferable,
    shareable,
};

enum class ValueOriginKind {
    none,
    task,
    thread,
    async_call,
};

enum class SwitchPatternKind {
    value,
    constructor,
    invalid,
};

struct AsyncMethodSignature {
    syntax::TypeSyntax receiver_type;
    std::string method_name;
};

struct UnsafeMethodSignature {
    syntax::TypeSyntax receiver_type;
    std::string method_name;
};

struct MethodReturnSignature {
    syntax::TypeSyntax receiver_type;
    std::string method_name;
    std::vector<std::string> generic_parameters;
    std::vector<syntax::ParameterSyntax> parameters;
    syntax::TypeSyntax return_type;
};

struct CallableSignature {
    std::string name;
    std::vector<std::string> generic_parameters;
    std::vector<syntax::ParameterSyntax> parameters;
    syntax::TypeSyntax return_type;
};

struct RecordFieldSignature {
    syntax::TypeSyntax record_type;
    std::vector<std::string> generic_parameters;
    std::string field_name;
    syntax::TypeSyntax field_type;
};

struct ChoiceVariantSignature {
    syntax::TypeSyntax choice_type;
    std::vector<std::string> generic_parameters;
    std::string variant_name;
    std::vector<syntax::NamedTypeSyntax> payloads;
};

struct PayloadConstructorPatternKey {
    std::string constructor_name;
    std::vector<std::string> payloads;
};

class Analyzer {
public:
    explicit Analyzer(syntax::ModuleSyntax const& module) : module_(module) {}

    auto analyze() -> SemanticAnalysisResult {
        collect_async_callable_names();
        collect_unsafe_callable_names();
        collect_callable_return_types();
        collect_record_field_types();
        collect_concurrency_marker_implementations();
        collect_choice_variant_metadata();
        collect_constant_bindings();
        validate_constant_initializer_cycles();
        analyze_constants();

        for (auto const& function : module_.functions) {
            analyze_function(function);
        }

        for (auto const& implementation : module_.implementations) {
            auto receiver_type_name = render_type_name(implementation.receiver_type);
            for (auto const& method : implementation.methods) {
                analyze_function(method, receiver_type_name);
            }
        }

        for (auto const& extension : module_.extensions) {
            auto receiver_type_name = render_type_name(extension.receiver_type);
            for (auto const& method : extension.methods) {
                analyze_function(method, receiver_type_name);
            }
        }

        for (auto const& foreign_export : module_.foreign_exports) {
            analyze_function(foreign_export.function);
        }

        return SemanticAnalysisResult {
            .diagnostics = std::move(diagnostics_),
            .concurrency_captures = std::move(concurrency_captures),
        };
    }

private:
    struct Binding {
        std::string name;
        std::string type_name;
        ValueOriginKind value_origin = ValueOriginKind::none;
        bool mutable_binding = false;
        bool receiver_binding = false;
        bool parameter_binding = false;
        bool module_constant = false;
        std::size_t scope_depth = 0;
    };

    using ScopeSnapshot = std::vector<std::vector<Binding>>;

    struct ConstantReference {
        std::string name;
        std::size_t line = 0;
    };

    struct SwitchChoiceCoverage {
        std::optional<std::vector<std::string>> zero_payload_variants;
        std::unordered_set<std::string> zero_payload_variant_lookup;
        std::optional<std::unordered_set<std::string>> remaining_zero_payload_variants;
        std::optional<std::vector<std::string>> payload_bearing_variants;
        std::unordered_set<std::string> payload_bearing_variant_lookup;
        std::optional<std::unordered_set<std::string>> remaining_payload_bearing_variants;

        void set_zero_payload_variants(std::vector<std::string> const& variants) {
            zero_payload_variants = variants;
            zero_payload_variant_lookup = std::unordered_set<std::string>(variants.begin(), variants.end());
            remaining_zero_payload_variants = zero_payload_variant_lookup;
        }

        void set_payload_bearing_variants(std::vector<std::string> const& variants) {
            payload_bearing_variants = variants;
            payload_bearing_variant_lookup = std::unordered_set<std::string>(variants.begin(), variants.end());
            remaining_payload_bearing_variants = payload_bearing_variant_lookup;
        }

        auto is_zero_payload_variant(std::string const& variant_name) const -> bool {
            return zero_payload_variants.has_value() && zero_payload_variant_lookup.contains(variant_name);
        }

        auto tracks_payload_bearing_variant(std::string const& variant_name) const -> bool {
            return remaining_payload_bearing_variants.has_value() &&
                   payload_bearing_variant_lookup.contains(variant_name);
        }

        void cover_zero_payload_variant(std::string const& variant_name) {
            if (remaining_zero_payload_variants.has_value()) {
                remaining_zero_payload_variants->erase(variant_name);
            }
            cover_payload_bearing_variant(variant_name);
        }

        void cover_payload_bearing_variant(std::string const& variant_name) {
            if (tracks_payload_bearing_variant(variant_name)) {
                remaining_payload_bearing_variants->erase(variant_name);
            }
        }

        auto all_zero_payload_variants_covered() const -> bool {
            return remaining_zero_payload_variants.has_value() && remaining_zero_payload_variants->empty();
        }

        auto all_payload_bearing_variants_covered() const -> bool {
            return remaining_payload_bearing_variants.has_value() && remaining_payload_bearing_variants->empty();
        }

        auto first_missing_zero_payload_variant() const -> std::optional<std::string> {
            if (!zero_payload_variants.has_value() || !remaining_zero_payload_variants.has_value() ||
                remaining_zero_payload_variants->empty()) {
                return std::nullopt;
            }

            for (auto const& variant_name : *zero_payload_variants) {
                if (remaining_zero_payload_variants->contains(variant_name)) {
                    return variant_name;
                }
            }

            return std::nullopt;
        }

        auto first_missing_payload_bearing_variant() const -> std::optional<std::string> {
            if (!payload_bearing_variants.has_value() || !remaining_payload_bearing_variants.has_value() ||
                remaining_payload_bearing_variants->empty()) {
                return std::nullopt;
            }

            for (auto const& variant_name : *payload_bearing_variants) {
                if (remaining_payload_bearing_variants->contains(variant_name)) {
                    return variant_name;
                }
            }

            return std::nullopt;
        }
    };

    auto render_type_name(syntax::TypeSyntax const& type) const -> std::string {
        std::string rendered = type.name;
        if (type.generic_arguments.empty()) {
            return rendered;
        }

        rendered += "<";
        for (std::size_t index = 0; index < type.generic_arguments.size(); ++index) {
            if (index > 0) {
                rendered += ", ";
            }
            rendered += render_type_name(type.generic_arguments[index]);
        }
        rendered += ">";
        return rendered;
    }

    auto is_pointer_type(syntax::TypeSyntax const& type) const -> bool {
        return type.name == "Pointer";
    }

    auto is_address_type(syntax::TypeSyntax const& type) const -> bool {
        return type.name == "Address";
    }

    auto is_pointer_type_name(std::string const& type_name) const -> bool {
        return type_name == "Pointer" || type_name.rfind("Pointer<", 0) == 0;
    }

    auto is_address_type_name(std::string const& type_name) const -> bool {
        return type_name == "Address";
    }

    auto is_integer_type_name(std::string const& type_name) const -> bool {
        static constexpr char const* integer_types[] = {
            "Int8",    "Int16",   "Int32",   "Int64",   "Int128",  "IntSize",
            "UInt8",   "UInt16",  "UInt32",  "UInt64",  "UInt128", "UIntSize",
            "Byte",    "Char",
        };

        for (auto const* integer_type : integer_types) {
            if (type_name == integer_type) {
                return true;
            }
        }

        return false;
    }

    struct IntegerTypeInfo {
        int bit_width = 0;
        bool is_fixed_width = false;
    };

    auto integer_type_info(std::string const& type_name) const -> std::optional<IntegerTypeInfo> {
        if (type_name == "Int8" || type_name == "UInt8") {
            return IntegerTypeInfo {.bit_width = 8, .is_fixed_width = true};
        }
        if (type_name == "Int16" || type_name == "UInt16") {
            return IntegerTypeInfo {.bit_width = 16, .is_fixed_width = true};
        }
        if (type_name == "Int32" || type_name == "UInt32") {
            return IntegerTypeInfo {.bit_width = 32, .is_fixed_width = true};
        }
        if (type_name == "Int64" || type_name == "UInt64") {
            return IntegerTypeInfo {.bit_width = 64, .is_fixed_width = true};
        }
        if (type_name == "Int128" || type_name == "UInt128") {
            return IntegerTypeInfo {.bit_width = 128, .is_fixed_width = true};
        }

        if (type_name == "IntSize" || type_name == "UIntSize" || type_name == "Byte" || type_name == "Char") {
            return IntegerTypeInfo {.bit_width = 0, .is_fixed_width = false};
        }

        return std::nullopt;
    }

    auto is_integer_literal_compatible_with_pointee(
        std::string const& pointee_type_name,
        syntax::ExpressionSyntax const& value_expression
    ) const -> bool {
        return value_expression.kind == syntax::ExpressionKind::integer_literal &&
               is_integer_type_name(pointee_type_name);
    }

    auto is_integer_cast_compatible_with_pointee(
        std::string const& pointee_type_name,
        syntax::ExpressionSyntax const& value_expression
    ) const -> bool {
        if (value_expression.kind != syntax::ExpressionKind::cast || !value_expression.left ||
            !is_integer_type_name(pointee_type_name) || !is_integer_type_name(value_expression.text)) {
            return false;
        }

        auto source_type_name = infer_expression_type_name(*value_expression.left);
        if (source_type_name.empty() || !is_integer_type_name(source_type_name)) {
            return false;
        }

        if (value_expression.text == pointee_type_name) {
            return true;
        }

        auto cast_target_info = integer_type_info(value_expression.text);
        auto pointee_info = integer_type_info(pointee_type_name);
        return cast_target_info.has_value() && pointee_info.has_value() && cast_target_info->is_fixed_width &&
               pointee_info->is_fixed_width && cast_target_info->bit_width == pointee_info->bit_width;
    }

    auto are_low_level_write_types_compatible(
        std::string const& pointee_type_name,
        syntax::ExpressionSyntax const& value_expression
    ) const -> bool {
        auto value_type_name = infer_expression_type_name(value_expression);
        if (value_type_name.empty()) {
            return true;
        }

        if (are_low_level_read_types_compatible(value_type_name, pointee_type_name)) {
            return true;
        }

        return is_integer_literal_compatible_with_pointee(pointee_type_name, value_expression) ||
               is_integer_cast_compatible_with_pointee(pointee_type_name, value_expression);
    }

    auto are_low_level_read_types_compatible(
        std::string const& inferred_type_name,
        std::string const& expected_type_name
    ) const -> bool {
        if (inferred_type_name == expected_type_name) {
            return true;
        }

        auto inferred_info = integer_type_info(inferred_type_name);
        auto expected_info = integer_type_info(expected_type_name);
        return inferred_info.has_value() && expected_info.has_value() && inferred_info->is_fixed_width &&
               expected_info->is_fixed_width && inferred_info->bit_width == expected_info->bit_width;
    }

    auto merge_compatible_integer_type_names(
        std::string const& left_type_name,
        std::string const& right_type_name
    ) const -> std::string {
        if (left_type_name.empty() || right_type_name.empty() ||
            !is_integer_type_name(left_type_name) || !is_integer_type_name(right_type_name)) {
            return {};
        }

        if (left_type_name == "Int64" && right_type_name != "Int64") {
            return right_type_name;
        }

        if (right_type_name == "Int64" && left_type_name != "Int64") {
            return left_type_name;
        }

        if (are_low_level_read_types_compatible(left_type_name, right_type_name)) {
            return left_type_name;
        }

        return {};
    }

    auto pointer_pointee_type_name(std::string const& type_name) const -> std::string {
        if (!is_pointer_type_name(type_name)) {
            return {};
        }

        constexpr std::string_view prefix = "Pointer<";
        if (type_name.rfind(prefix, 0) != 0 || type_name.size() <= prefix.size() || type_name.back() != '>') {
            return {};
        }

        return type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1);
    }

    auto first_generic_argument_type_name(std::string const& type_name) const -> std::string {
        auto open_angle = type_name.find('<');
        if (open_angle == std::string::npos || type_name.empty() || type_name.back() != '>') {
            return {};
        }

        auto depth = 0;
        std::string argument;
        for (std::size_t index = open_angle + 1; index + 1 < type_name.size(); ++index) {
            auto const character = type_name[index];
            if (character == '<') {
                ++depth;
                argument += character;
                continue;
            }
            if (character == '>') {
                --depth;
                argument += character;
                continue;
            }
            if (character == ',' && depth == 0) {
                break;
            }
            argument += character;
        }

        while (!argument.empty() && argument.front() == ' ') {
            argument.erase(argument.begin());
        }
        while (!argument.empty() && argument.back() == ' ') {
            argument.pop_back();
        }
        return argument;
    }

    auto container_element_type_name(std::string const& type_name) const -> std::string {
        if (type_name.rfind("Array<", 0) == 0 || type_name.rfind("View<", 0) == 0 ||
            type_name.rfind("DynamicArray<", 0) == 0 || type_name.rfind("shared.View<", 0) == 0 ||
            type_name.rfind("exclusive.View<", 0) == 0 || type_name.rfind("shared.DynamicArray<", 0) == 0 ||
            type_name.rfind("exclusive.DynamicArray<", 0) == 0) {
            return first_generic_argument_type_name(type_name);
        }

        return {};
    }

    auto is_receiver_self_type_name(std::string const& type_name) const -> bool {
        return type_name == "This" || type_name == "shared.This" || type_name == "exclusive.This";
    }

    void validate_receiver_type_usage(syntax::TypeSyntax const& type, std::size_t line) {
        if (is_receiver_self_type_name(type.name) && !receiver_context_active_) {
            diagnostics_.error(line, "This type is only valid inside interface, implements, or extend methods");
        }

        for (auto const& argument : type.generic_arguments) {
            validate_receiver_type_usage(argument, line);
        }
    }

    auto is_obviously_safe_capture_type(std::string const& type_name) const -> bool {
        if (type_name.empty()) {
            return false;
        }

        if (type_name.rfind("shared.", 0) == 0) {
            return true;
        }

        static constexpr char const* safe_types[] = {
            "Int8",   "Int16",  "Int32",   "Int64",   "Int128", "IntSize", "UInt8",  "UInt16",
            "UInt32", "UInt64", "UInt128", "UIntSize", "Float32", "Float64", "Bool",   "Byte",
            "Char",   "Text",   "Address",
        };

        for (auto const* safe_type : safe_types) {
            if (type_name == safe_type) {
                return true;
            }
        }

        return false;
    }

    auto has_marker_type(
        std::vector<std::string> const& marker_types,
        std::string const& type_name
    ) const -> bool {
        for (auto const& constrained_type : marker_types) {
            if (constrained_type == type_name) {
                return true;
            }
        }
        return false;
    }

    auto has_async_callable_name(std::string const& name) const -> bool {
        for (auto const& async_callable_name : async_callable_names_) {
            if (async_callable_name == name) {
                return true;
            }
        }
        return false;
    }

    auto has_async_method_signature(
        std::string const& receiver_type,
        std::string const& method_name
    ) const -> bool {
        auto parsed_receiver_type = parse_rendered_type_name(receiver_type);
        if (!parsed_receiver_type.has_value()) {
            return false;
        }

        for (auto const& signature : async_method_signatures_) {
            if (signature.method_name == method_name &&
                receiver_matches_signature(signature.receiver_type, *parsed_receiver_type)) {
                return true;
            }
        }
        return false;
    }

    auto has_unsafe_callable_name(std::string const& name) const -> bool {
        for (auto const& unsafe_callable_name : unsafe_callable_names_) {
            if (unsafe_callable_name == name) {
                return true;
            }
        }
        return false;
    }

    auto has_unsafe_method_signature(
        std::string const& receiver_type,
        std::string const& method_name
    ) const -> bool {
        auto parsed_receiver_type = parse_rendered_type_name(receiver_type);
        if (!parsed_receiver_type.has_value()) {
            return false;
        }

        for (auto const& signature : unsafe_method_signatures_) {
            if (signature.method_name == method_name &&
                receiver_matches_signature(signature.receiver_type, *parsed_receiver_type)) {
                return true;
            }
        }
        return false;
    }

    auto parse_rendered_type_name(
        std::string const& type_name,
        std::size_t& index
    ) const -> std::optional<syntax::TypeSyntax> {
        while (index < type_name.size() && type_name[index] == ' ') {
            ++index;
        }

        if (index >= type_name.size()) {
            return std::nullopt;
        }

        syntax::TypeSyntax parsed_type;
        while (index < type_name.size() && type_name[index] != '<' && type_name[index] != '>' &&
               type_name[index] != ',') {
            parsed_type.name += type_name[index];
            ++index;
        }

        while (!parsed_type.name.empty() && parsed_type.name.back() == ' ') {
            parsed_type.name.pop_back();
        }

        if (parsed_type.name.empty()) {
            return std::nullopt;
        }

        if (index < type_name.size() && type_name[index] == '<') {
            ++index;
            while (index < type_name.size()) {
                auto argument = parse_rendered_type_name(type_name, index);
                if (!argument.has_value()) {
                    return std::nullopt;
                }
                parsed_type.generic_arguments.push_back(*argument);

                while (index < type_name.size() && type_name[index] == ' ') {
                    ++index;
                }

                if (index >= type_name.size()) {
                    return std::nullopt;
                }

                if (type_name[index] == ',') {
                    ++index;
                    continue;
                }

                if (type_name[index] == '>') {
                    ++index;
                    break;
                }

                return std::nullopt;
            }
        }

        return parsed_type;
    }

    auto parse_rendered_type_name(std::string const& type_name) const -> std::optional<syntax::TypeSyntax> {
        std::size_t index = 0;
        auto parsed_type = parse_rendered_type_name(type_name, index);
        if (!parsed_type.has_value()) {
            return std::nullopt;
        }

        while (index < type_name.size() && type_name[index] == ' ') {
            ++index;
        }

        return index == type_name.size() ? parsed_type : std::nullopt;
    }

    auto match_generic_type_pattern(
        syntax::TypeSyntax const& pattern,
        syntax::TypeSyntax const& actual,
        std::vector<std::string> const& generic_parameters,
        std::unordered_map<std::string, syntax::TypeSyntax>& bindings
    ) const -> bool {
        if (pattern.generic_arguments.empty()) {
            for (auto const& generic_parameter : generic_parameters) {
                if (pattern.name == generic_parameter) {
                    auto found = bindings.find(pattern.name);
                    if (found == bindings.end()) {
                        bindings.emplace(pattern.name, actual);
                        return true;
                    }

                    return render_type_name(found->second) == render_type_name(actual);
                }
            }
        }

        if (pattern.name != actual.name || pattern.generic_arguments.size() != actual.generic_arguments.size()) {
            return false;
        }

        for (std::size_t index = 0; index < pattern.generic_arguments.size(); ++index) {
            if (!match_generic_type_pattern(
                    pattern.generic_arguments[index],
                    actual.generic_arguments[index],
                    generic_parameters,
                    bindings
                )) {
                return false;
            }
        }

        return true;
    }

    auto substitute_generic_type_bindings(
        syntax::TypeSyntax const& type,
        std::unordered_map<std::string, syntax::TypeSyntax> const& bindings
    ) const -> syntax::TypeSyntax {
        auto found = bindings.find(type.name);
        if (type.generic_arguments.empty() && found != bindings.end()) {
            return found->second;
        }

        syntax::TypeSyntax substituted {.name = type.name};
        for (auto const& argument : type.generic_arguments) {
            substituted.generic_arguments.push_back(substitute_generic_type_bindings(argument, bindings));
        }
        return substituted;
    }

    void collect_receiver_generic_placeholder_names(
        syntax::TypeSyntax const& type,
        std::unordered_set<std::string>& placeholders
    ) const {
        for (auto const& argument : type.generic_arguments) {
            if (argument.generic_arguments.empty()) {
                placeholders.insert(argument.name);
            }

            collect_receiver_generic_placeholder_names(argument, placeholders);
        }
    }

    auto receiver_matches_signature(
        syntax::TypeSyntax const& receiver_pattern,
        syntax::TypeSyntax const& actual_receiver_type
    ) const -> bool {
        std::unordered_set<std::string> receiver_placeholder_set;
        collect_receiver_generic_placeholder_names(receiver_pattern, receiver_placeholder_set);
        std::vector<std::string> receiver_placeholders(
            receiver_placeholder_set.begin(),
            receiver_placeholder_set.end()
        );

        std::unordered_map<std::string, syntax::TypeSyntax> bindings;
        return match_generic_type_pattern(receiver_pattern, actual_receiver_type, receiver_placeholders, bindings);
    }

    auto infer_specialized_return_type_name(
        std::vector<std::string> const& generic_parameters,
        std::vector<syntax::ParameterSyntax> const& parameters,
        syntax::TypeSyntax const& return_type,
        std::vector<syntax::ExpressionSyntax> const& arguments
    ) const -> std::string {
        if (generic_parameters.empty()) {
            return render_type_name(return_type);
        }

        if (parameters.size() != arguments.size()) {
            return render_type_name(return_type);
        }

        std::unordered_map<std::string, syntax::TypeSyntax> bindings;
        for (std::size_t index = 0; index < parameters.size(); ++index) {
            auto argument_type_name = infer_expression_type_name(arguments[index]);
            if (argument_type_name.empty()) {
                return render_type_name(return_type);
            }

            auto parsed_argument_type = parse_rendered_type_name(argument_type_name);
            if (!parsed_argument_type.has_value() ||
                !match_generic_type_pattern(parameters[index].type, *parsed_argument_type, generic_parameters, bindings)) {
                return render_type_name(return_type);
            }
        }

        return render_type_name(substitute_generic_type_bindings(return_type, bindings));
    }

    auto infer_specialized_method_return_type_name(
        MethodReturnSignature const& signature,
        syntax::TypeSyntax const& actual_receiver_type,
        std::vector<syntax::ExpressionSyntax> const& arguments
    ) const -> std::string {
        std::unordered_set<std::string> receiver_placeholder_set;
        collect_receiver_generic_placeholder_names(signature.receiver_type, receiver_placeholder_set);
        std::vector<std::string> receiver_placeholders(
            receiver_placeholder_set.begin(),
            receiver_placeholder_set.end()
        );

        std::unordered_map<std::string, syntax::TypeSyntax> bindings;
        if (!match_generic_type_pattern(signature.receiver_type, actual_receiver_type, receiver_placeholders, bindings)) {
            return {};
        }

        if (signature.parameters.size() != arguments.size()) {
            return render_type_name(substitute_generic_type_bindings(signature.return_type, bindings));
        }

        auto all_generic_parameters = signature.generic_parameters;
        all_generic_parameters.insert(
            all_generic_parameters.end(),
            receiver_placeholders.begin(),
            receiver_placeholders.end()
        );

        for (std::size_t index = 0; index < signature.parameters.size(); ++index) {
            auto argument_type_name = infer_expression_type_name(arguments[index]);
            if (argument_type_name.empty()) {
                return render_type_name(substitute_generic_type_bindings(signature.return_type, bindings));
            }

            auto parsed_argument_type = parse_rendered_type_name(argument_type_name);
            if (!parsed_argument_type.has_value() ||
                !match_generic_type_pattern(
                    signature.parameters[index].type,
                    *parsed_argument_type,
                    all_generic_parameters,
                    bindings
                )) {
                return render_type_name(substitute_generic_type_bindings(signature.return_type, bindings));
            }
        }

        return render_type_name(substitute_generic_type_bindings(signature.return_type, bindings));
    }

    auto find_callable_return_type_name(
        std::string const& name,
        std::vector<syntax::ExpressionSyntax> const& arguments
    ) const -> std::string {
        for (auto const& signature : callable_signatures_) {
            if (signature.name == name) {
                return infer_specialized_return_type_name(
                    signature.generic_parameters,
                    signature.parameters,
                    signature.return_type,
                    arguments
                );
            }
        }

        return {};
    }

    auto find_method_return_type_name(
        std::string const& receiver_type,
        std::string const& method_name,
        std::vector<syntax::ExpressionSyntax> const& arguments
    ) const -> std::string {
        auto parsed_receiver_type = parse_rendered_type_name(receiver_type);
        if (!parsed_receiver_type.has_value()) {
            return {};
        }

        for (auto const& signature : method_return_signatures_) {
            if (signature.method_name == method_name) {
                auto specialized_return_type_name =
                    infer_specialized_method_return_type_name(signature, *parsed_receiver_type, arguments);
                if (!specialized_return_type_name.empty()) {
                    return specialized_return_type_name;
                }
            }
        }
        return {};
    }

    auto find_record_field_type_name(
        std::string const& record_type,
        std::string const& field_name
    ) const -> std::string {
        auto parsed_record_type = parse_rendered_type_name(record_type);
        if (!parsed_record_type.has_value()) {
            return {};
        }

        for (auto const& signature : record_field_signatures_) {
            if (signature.field_name != field_name) {
                continue;
            }

            std::unordered_map<std::string, syntax::TypeSyntax> bindings;
            if (match_generic_type_pattern(
                    signature.record_type,
                    *parsed_record_type,
                    signature.generic_parameters,
                    bindings
                )) {
                return render_type_name(substitute_generic_type_bindings(signature.field_type, bindings));
            }
        }
        return {};
    }

    auto find_choice_variant_signature(
        std::string const& variant_name,
        syntax::TypeSyntax const& choice_type
    ) const -> ChoiceVariantSignature const* {
        for (auto const& signature : choice_variant_signatures_) {
            if (signature.variant_name != variant_name) {
                continue;
            }

            std::unordered_map<std::string, syntax::TypeSyntax> bindings;
            if (match_generic_type_pattern(
                    signature.choice_type,
                    choice_type,
                    signature.generic_parameters,
                    bindings
                )) {
                return &signature;
            }
        }

        return nullptr;
    }

    auto find_choice_variant_payload_arity(
        std::string const& variant_name,
        syntax::TypeSyntax const& choice_type
    ) const -> std::optional<std::size_t> {
        auto const* signature = find_choice_variant_signature(variant_name, choice_type);
        if (signature == nullptr) {
            return std::nullopt;
        }

        return signature->payloads.size();
    }

    auto zero_payload_choice_variant_names_if_enum_like(
        syntax::TypeSyntax const& choice_type
    ) const -> std::optional<std::vector<std::string>> {
        std::vector<std::string> variants;
        auto matched_choice = false;
        for (auto const& signature : choice_variant_signatures_) {
            std::unordered_map<std::string, syntax::TypeSyntax> bindings;
            if (!match_generic_type_pattern(
                    signature.choice_type,
                    choice_type,
                    signature.generic_parameters,
                    bindings
                )) {
                continue;
            }

            matched_choice = true;
            if (!signature.payloads.empty()) {
                return std::nullopt;
            }
            variants.push_back(signature.variant_name);
        }

        if (!matched_choice || variants.empty()) {
            return std::nullopt;
        }

        return variants;
    }

    auto choice_variant_names_if_payload_bearing(
        syntax::TypeSyntax const& choice_type
    ) const -> std::optional<std::vector<std::string>> {
        std::vector<std::string> variants;
        auto matched_choice = false;
        auto has_payload_variant = false;
        for (auto const& signature : choice_variant_signatures_) {
            std::unordered_map<std::string, syntax::TypeSyntax> bindings;
            if (!match_generic_type_pattern(
                    signature.choice_type,
                    choice_type,
                    signature.generic_parameters,
                    bindings
                )) {
                continue;
            }

            matched_choice = true;
            has_payload_variant = has_payload_variant || !signature.payloads.empty();
            variants.push_back(signature.variant_name);
        }

        if (!matched_choice || !has_payload_variant || variants.empty()) {
            return std::nullopt;
        }

        return variants;
    }

    auto resolve_choice_pattern_subject_type(
        std::string const& variant_name,
        syntax::TypeSyntax const& subject_type
    ) const -> std::optional<syntax::TypeSyntax> {
        if (find_choice_variant_signature(variant_name, subject_type) != nullptr) {
            return subject_type;
        }

        if (subject_type.name == "Box" && subject_type.generic_arguments.size() == 1) {
            auto const& wrapped_type = subject_type.generic_arguments.front();
            if (find_choice_variant_signature(variant_name, wrapped_type) != nullptr) {
                return wrapped_type;
            }
        }

        return std::nullopt;
    }

    auto has_concurrency_marker_constraint(
        std::string const& type_name,
        ConcurrencyMarkerKind marker_kind
    ) const -> bool {
        if (marker_kind == ConcurrencyMarkerKind::transferable) {
            return has_marker_type(transferable_constraint_types_, type_name);
        }

        return has_marker_type(shareable_constraint_types_, type_name);
    }

    auto has_concrete_concurrency_marker(
        std::string const& type_name,
        ConcurrencyMarkerKind marker_kind
    ) const -> bool {
        if (marker_kind == ConcurrencyMarkerKind::transferable) {
            return has_marker_type(transferable_impl_types_, type_name);
        }

        return has_marker_type(shareable_impl_types_, type_name);
    }

    auto has_allowed_concurrency_marker(
        std::string const& type_name,
        ConcurrencyExpressionKind expression_kind
    ) const -> bool {
        if (has_concurrency_marker_constraint(type_name, ConcurrencyMarkerKind::transferable) ||
            has_concrete_concurrency_marker(type_name, ConcurrencyMarkerKind::transferable)) {
            return true;
        }

        if (expression_kind == ConcurrencyExpressionKind::task &&
            (has_concurrency_marker_constraint(type_name, ConcurrencyMarkerKind::shareable) ||
             has_concrete_concurrency_marker(type_name, ConcurrencyMarkerKind::shareable))) {
            return true;
        }

        return false;
    }

    auto validate_concurrency_value_type(
        std::size_t line,
        std::string const& type_name,
        ConcurrencyExpressionKind expression_kind,
        std::string_view value_role
    ) -> void {
        if (type_name.empty() || is_obviously_safe_capture_type(type_name) ||
            has_allowed_concurrency_marker(type_name, expression_kind)) {
            return;
        }

        if (expression_kind == ConcurrencyExpressionKind::thread) {
            diagnostics_.error(
                line,
                "thread " + std::string(value_role) + " type '" + type_name +
                    "' requires future Transferable analysis"
            );
            return;
        }

        diagnostics_.error(
            line,
            "task " + std::string(value_role) + " type '" + type_name +
                "' requires future Transferable/Shareable analysis"
        );
    }

    auto infer_expression_type_name(syntax::ExpressionSyntax const& expression) const -> std::string {
        switch (expression.kind) {
        case syntax::ExpressionKind::name: {
            auto const* binding = find_binding(expression.text);
            return binding == nullptr ? std::string {} : binding->type_name;
        }
        case syntax::ExpressionKind::unary:
            if (!expression.left) {
                return {};
            }
            if (expression.text == "-"
                || expression.text == "bit_not") {
                auto operand_type_name = infer_expression_type_name(*expression.left);
                return is_integer_type_name(operand_type_name) ? operand_type_name : std::string {};
            }
            if (expression.text == "not") {
                auto operand_type_name = infer_expression_type_name(*expression.left);
                return operand_type_name == "Bool" ? "Bool" : std::string {};
            }
            return {};
        case syntax::ExpressionKind::cast:
            return expression.text;
        case syntax::ExpressionKind::string_literal:
            return "Text";
        case syntax::ExpressionKind::boolean_literal:
            return "Bool";
        case syntax::ExpressionKind::integer_literal:
            return "Int64";
        case syntax::ExpressionKind::array_literal:
            if (expression.arguments.empty()) {
                return {};
            }
            {
                auto element_type_name = infer_expression_type_name(expression.arguments.front());
                if (element_type_name.empty()) {
                    return {};
                }
                for (std::size_t index = 1; index < expression.arguments.size(); ++index) {
                    element_type_name = merge_compatible_integer_type_names(
                        element_type_name,
                        infer_expression_type_name(expression.arguments[index])
                    );
                    if (element_type_name.empty()) {
                        return {};
                    }
                }
                return "Array<" + element_type_name + ", " + std::to_string(expression.arguments.size()) + ">";
            }
        case syntax::ExpressionKind::index_access:
            if (!expression.left) {
                return {};
            }
            {
                auto source_type_name = infer_expression_type_name(*expression.left);
                auto pointee_type_name = pointer_pointee_type_name(source_type_name);
                if (!pointee_type_name.empty()) {
                    return pointee_type_name;
                }
                return container_element_type_name(source_type_name);
            }
        case syntax::ExpressionKind::member_access:
        case syntax::ExpressionKind::null_safe_member_access:
            if (!expression.left) {
                return {};
            }
            return find_record_field_type_name(infer_expression_type_name(*expression.left), expression.text);
        case syntax::ExpressionKind::call:
            if (!expression.left) {
                return {};
            }
            if (expression.left->kind == syntax::ExpressionKind::member_access ||
                expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
                auto receiver_type_name = infer_receiver_type_name_for_member_call(*expression.left);
                if (receiver_type_name.empty()) {
                    return {};
                }

                return find_method_return_type_name(receiver_type_name, expression.left->text, expression.arguments);
            }
            if (expression.left->kind != syntax::ExpressionKind::name) {
                return {};
            }
            if (expression.left->text == "address_of") {
                return "Address";
            }
            if (expression.left->text == "Pointer") {
                if (!expected_pointer_type_name_.empty()) {
                    return expected_pointer_type_name_;
                }
                return "Pointer";
            }
            if (expression.left->text == "raw_offset" && !expression.arguments.empty()) {
                auto source_type_name = infer_expression_type_name(expression.arguments.front());
                if (is_pointer_type_name(source_type_name) || is_address_type_name(source_type_name)) {
                    if (source_type_name == "Pointer") {
                        auto recovered_pointer_type_name =
                            recover_explicit_pointer_type_name(expression.arguments.front());
                        if (!recovered_pointer_type_name.empty()) {
                            return recovered_pointer_type_name;
                        }
                    }
                    return source_type_name;
                }
                return {};
            }
            if ((expression.left->text == "raw_read" || expression.left->text == "volatile_read") &&
                !expression.arguments.empty()) {
                auto source_pointer_type_name = infer_expression_type_name(expression.arguments.front());
                auto pointee_type_name = pointer_pointee_type_name(source_pointer_type_name);
                if (!pointee_type_name.empty()) {
                    return pointee_type_name;
                }
                return explicit_pointer_source_pointee_type_name(expression.arguments.front());
            }
            return find_callable_return_type_name(expression.left->text, expression.arguments);
        case syntax::ExpressionKind::binary:
            if (!expression.left || !expression.right) {
                return {};
            }
            if (expression.text == "==" || expression.text == "!=" || expression.text == "<" ||
                expression.text == "<=" || expression.text == ">" || expression.text == ">=" ||
                expression.text == "and" || expression.text == "or") {
                return "Bool";
            }
            if (expression.text == "+" || expression.text == "-" || expression.text == "*" ||
                expression.text == "/" || expression.text == "%" || expression.text == "bit_and" ||
                expression.text == "bit_or" || expression.text == "bit_xor") {
                return merge_compatible_integer_type_names(
                    infer_expression_type_name(*expression.left),
                    infer_expression_type_name(*expression.right)
                );
            }
            if (expression.text == "shift_left" || expression.text == "shift_right") {
                auto left_type_name = infer_expression_type_name(*expression.left);
                auto right_type_name = infer_expression_type_name(*expression.right);
                return is_integer_type_name(left_type_name) && is_integer_type_name(right_type_name)
                           ? left_type_name
                           : std::string {};
            }
            return {};
        case syntax::ExpressionKind::ternary:
            if (!expression.right || !expression.alternate) {
                return {};
            }
            {
                auto consequence_type_name = infer_expression_type_name(*expression.right);
                auto alternate_type_name = infer_expression_type_name(*expression.alternate);
                if (consequence_type_name == alternate_type_name) {
                    return consequence_type_name;
                }

                return merge_compatible_integer_type_names(consequence_type_name, alternate_type_name);
            }
        default:
            return {};
        }
    }

    auto infer_receiver_type_name_for_member_call(syntax::ExpressionSyntax const& callee_expression) const
        -> std::string {
        if ((callee_expression.kind != syntax::ExpressionKind::member_access &&
             callee_expression.kind != syntax::ExpressionKind::null_safe_member_access) ||
            !callee_expression.left) {
            return {};
        }

        return infer_expression_type_name(*callee_expression.left);
    }

    auto merge_value_origins(ValueOriginKind left, ValueOriginKind right) const -> ValueOriginKind {
        if (left == right) {
            return left;
        }

        return ValueOriginKind::none;
    }

    auto snapshot_scope_stack() const -> ScopeSnapshot {
        return scope_stack_;
    }

    void restore_scope_stack(ScopeSnapshot snapshot) {
        scope_stack_ = std::move(snapshot);
    }

    auto merge_scope_snapshots(std::vector<ScopeSnapshot> const& snapshots) const -> ScopeSnapshot {
        if (snapshots.empty()) {
            return {};
        }

        auto merged = snapshots.front();
        for (std::size_t snapshot_index = 1; snapshot_index < snapshots.size(); ++snapshot_index) {
            auto const& snapshot = snapshots[snapshot_index];
            for (std::size_t scope_index = 0; scope_index < merged.size() && scope_index < snapshot.size(); ++scope_index) {
                for (std::size_t binding_index = 0;
                     binding_index < merged[scope_index].size() && binding_index < snapshot[scope_index].size();
                     ++binding_index) {
                    auto& merged_binding = merged[scope_index][binding_index];
                    auto const& snapshot_binding = snapshot[scope_index][binding_index];

                    merged_binding.value_origin =
                        merge_value_origins(merged_binding.value_origin, snapshot_binding.value_origin);
                    if (merged_binding.type_name != snapshot_binding.type_name) {
                        merged_binding.type_name.clear();
                    }
                }
            }
        }

        return merged;
    }

    auto infer_expression_value_origin(syntax::ExpressionSyntax const& expression) const -> ValueOriginKind {
        switch (expression.kind) {
        case syntax::ExpressionKind::task:
            return ValueOriginKind::task;
        case syntax::ExpressionKind::thread:
            return ValueOriginKind::thread;
        case syntax::ExpressionKind::unary:
            if (expression.text == "await") {
                return ValueOriginKind::none;
            }
            return expression.left ? infer_expression_value_origin(*expression.left) : ValueOriginKind::none;
        case syntax::ExpressionKind::call:
            if (expression.left && expression.left->kind == syntax::ExpressionKind::member_access &&
                expression.left->text == "join") {
                return ValueOriginKind::none;
            }
            return is_declared_async_call(expression) ? ValueOriginKind::async_call : ValueOriginKind::none;
        case syntax::ExpressionKind::name: {
            auto const* binding = find_binding(expression.text);
            return binding == nullptr ? ValueOriginKind::none : binding->value_origin;
        }
        case syntax::ExpressionKind::ternary:
            if (!expression.right || !expression.alternate) {
                return ValueOriginKind::none;
            }
            return merge_value_origins(
                infer_expression_value_origin(*expression.right),
                infer_expression_value_origin(*expression.alternate)
            );
        case syntax::ExpressionKind::cast:
            return expression.left ? infer_expression_value_origin(*expression.left) : ValueOriginKind::none;
        default:
            return ValueOriginKind::none;
        }
    }

    auto is_literal_switch_subpattern(syntax::ExpressionSyntax const& pattern) const -> bool {
        return pattern.kind == syntax::ExpressionKind::integer_literal ||
               pattern.kind == syntax::ExpressionKind::string_literal ||
               pattern.kind == syntax::ExpressionKind::boolean_literal;
    }

    auto normalized_integer_literal_text(std::string const& text) const -> std::string {
        std::string normalized;
        for (auto const character : text) {
            if (character != '_') {
                normalized += character;
            }
        }
        return normalized;
    }

    auto switch_integer_constant_text(syntax::ExpressionSyntax const& pattern) const -> std::optional<std::string> {
        if (pattern.kind == syntax::ExpressionKind::integer_literal) {
            return normalized_integer_literal_text(pattern.text);
        }

        if (pattern.kind == syntax::ExpressionKind::cast && pattern.left &&
            pattern.left->kind == syntax::ExpressionKind::integer_literal && is_integer_type_name(pattern.text)) {
            return normalized_integer_literal_text(pattern.left->text);
        }

        return std::nullopt;
    }

    auto switch_literal_pattern_key(syntax::ExpressionSyntax const& pattern) const -> std::optional<std::string> {
        auto integer_constant = switch_integer_constant_text(pattern);
        if (integer_constant.has_value()) {
            return "int:" + *integer_constant;
        }
        if (pattern.kind == syntax::ExpressionKind::boolean_literal) {
            return "bool:" + pattern.text;
        }
        if (pattern.kind == syntax::ExpressionKind::string_literal) {
            return "string:" + pattern.text;
        }
        return std::nullopt;
    }

    auto render_switch_literal_pattern(syntax::ExpressionSyntax const& pattern) const -> std::string {
        if (pattern.kind == syntax::ExpressionKind::string_literal) {
            if (pattern.text.size() >= 2 && pattern.text.front() == '"' && pattern.text.back() == '"') {
                return pattern.text;
            }
            return "\"" + pattern.text + "\"";
        }
        if (pattern.kind == syntax::ExpressionKind::cast && pattern.left &&
            pattern.left->kind == syntax::ExpressionKind::integer_literal) {
            return pattern.left->text + " as " + pattern.text;
        }
        return pattern.text;
    }

    auto is_switch_value_pattern_type_compatible(
        syntax::ExpressionSyntax const& pattern,
        std::string const& subject_type_name,
        std::string const& pattern_type_name
    ) const -> bool {
        if (subject_type_name.empty() || pattern_type_name.empty()) {
            return true;
        }

        if (subject_type_name == pattern_type_name) {
            return true;
        }

        if (pattern.kind == syntax::ExpressionKind::integer_literal && is_integer_type_name(subject_type_name)) {
            return true;
        }

        if (is_integer_type_name(subject_type_name) && is_integer_type_name(pattern_type_name) &&
            are_low_level_read_types_compatible(pattern_type_name, subject_type_name)) {
            return true;
        }

        return false;
    }

    void validate_switch_value_pattern_type(
        syntax::ExpressionSyntax const& pattern,
        std::optional<syntax::TypeSyntax> const& subject_type
    ) {
        if (!subject_type.has_value() || pattern.kind == syntax::ExpressionKind::name ||
            pattern.kind == syntax::ExpressionKind::call) {
            return;
        }

        auto subject_type_name = render_type_name(*subject_type);
        auto pattern_type_name = infer_expression_type_name(pattern);
        if (pattern_type_name.empty()) {
            return;
        }

        if (!is_switch_value_pattern_type_compatible(pattern, subject_type_name, pattern_type_name)) {
            diagnostics_.error(
                pattern.line,
                "switch value pattern type '" + pattern_type_name + "' does not match switched expression type '" +
                    subject_type_name + "'"
            );
        }
    }

    auto is_builtin_constructor_name(std::string const& name) const -> bool {
        return name == "Some" || name == "Empty" || name == "Ok" || name == "Error";
    }

    auto is_unsafe_intrinsic_name(std::string const& name) const -> bool {
        return name == "raw_read" || name == "raw_write" || name == "raw_offset" || name == "address_of" ||
               name == "volatile_read" || name == "volatile_write";
    }

    auto is_pointer_constructor_call(syntax::ExpressionSyntax const& expression) const -> bool {
        return expression.kind == syntax::ExpressionKind::call && expression.left &&
               expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Pointer";
    }

    auto read_intrinsic_name(syntax::ExpressionSyntax const& expression) const -> std::string {
        if (expression.kind != syntax::ExpressionKind::call || !expression.left ||
            expression.left->kind != syntax::ExpressionKind::name) {
            return {};
        }

        if (expression.left->text == "raw_read" || expression.left->text == "volatile_read") {
            return expression.left->text;
        }

        return {};
    }

    auto address_of_operand_type_name(syntax::ExpressionSyntax const& expression) const -> std::string {
        if (expression.kind != syntax::ExpressionKind::call || !expression.left ||
            expression.left->kind != syntax::ExpressionKind::name || expression.left->text != "address_of" ||
            expression.arguments.empty()) {
            return {};
        }

        return infer_expression_type_name(expression.arguments.front());
    }

    auto recover_explicit_pointer_type_name(syntax::ExpressionSyntax const& expression) const -> std::string {
        if (is_pointer_constructor_call(expression) && !expression.arguments.empty()) {
            auto source_type_name = address_of_operand_type_name(expression.arguments.front());
            if (!source_type_name.empty()) {
                return "Pointer<" + source_type_name + ">";
            }
            return {};
        }

        if (expression.kind == syntax::ExpressionKind::call && expression.left &&
            expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "raw_offset" &&
            !expression.arguments.empty()) {
            return recover_explicit_pointer_type_name(expression.arguments.front());
        }

        return {};
    }

    auto explicit_pointer_source_pointee_type_name(syntax::ExpressionSyntax const& expression) const
        -> std::string {
        auto inferred_type_name = infer_expression_type_name(expression);
        auto inferred_pointee_type_name = pointer_pointee_type_name(inferred_type_name);
        if (!inferred_pointee_type_name.empty()) {
            return inferred_pointee_type_name;
        }

        return pointer_pointee_type_name(recover_explicit_pointer_type_name(expression));
    }

    auto validate_read_result_type(
        syntax::ExpressionSyntax const& expression,
        std::string const& expected_type_name,
        std::size_t line,
        std::string_view context_description
    ) -> bool {
        auto intrinsic_name = read_intrinsic_name(expression);
        if (intrinsic_name.empty() || expected_type_name.empty()) {
            return false;
        }

        auto inferred_type_name = infer_expression_type_name(expression);
        if (!inferred_type_name.empty() &&
            !are_low_level_read_types_compatible(inferred_type_name, expected_type_name)) {
            diagnostics_.error(
                line,
                intrinsic_name + " result type '" + inferred_type_name + "' does not match " +
                    std::string(context_description) + " type '" + expected_type_name + "'"
            );
            return true;
        }

        return false;
    }

    void validate_pointer_constructor_operands(syntax::ExpressionSyntax const& expression) {
        if (!is_pointer_constructor_call(expression)) {
            return;
        }

        if (expression.arguments.size() != 1) {
            diagnostics_.error(
                expression.line,
                "Pointer construction currently requires exactly one source argument"
            );
            return;
        }

        auto expected_pointee_type_name = pointer_pointee_type_name(expected_pointer_type_name_);
        auto source_operand_type_name = address_of_operand_type_name(expression.arguments.front());
        auto source_type_name = infer_expression_type_name(expression.arguments.front());
        if (!source_type_name.empty()) {
            if (!is_address_type_name(source_type_name)) {
                diagnostics_.error(
                    expression.line,
                    "Pointer construction currently requires an address-like source argument"
                );
            }

            if (!expected_pointee_type_name.empty() && !source_operand_type_name.empty() &&
                !are_low_level_read_types_compatible(source_operand_type_name, expected_pointee_type_name)) {
                diagnostics_.error(
                    expression.line,
                    "Pointer construction source type '" + source_operand_type_name +
                        "' does not match expected pointer element type '" + expected_pointee_type_name + "'"
                );
            }
            return;
        }

        if (!is_structurally_address_like_expression(expression.arguments.front())) {
            diagnostics_.error(
                expression.line,
                "Pointer construction currently requires an address-like source argument"
            );
            return;
        }

        if (!expected_pointee_type_name.empty() && !source_operand_type_name.empty() &&
            !are_low_level_read_types_compatible(source_operand_type_name, expected_pointee_type_name)) {
            diagnostics_.error(
                expression.line,
                "Pointer construction source type '" + source_operand_type_name +
                    "' does not match expected pointer element type '" + expected_pointee_type_name + "'"
            );
        }
    }

    void validate_index_access_operands(syntax::ExpressionSyntax const& expression) {
        if (expression.kind != syntax::ExpressionKind::index_access || expression.arguments.empty()) {
            return;
        }

        auto index_type_name = infer_expression_type_name(expression.arguments.front());
        if (!index_type_name.empty() && !is_integer_type_name(index_type_name)) {
            diagnostics_.error(
                expression.line,
                "index access currently requires an integer index expression"
            );
        }
    }

    auto is_declared_unsafe_call(syntax::ExpressionSyntax const& expression) const -> bool {
        if (expression.kind != syntax::ExpressionKind::call || !expression.left) {
            return false;
        }

        if (expression.left->kind == syntax::ExpressionKind::name) {
            return has_unsafe_callable_name(expression.left->text);
        }

        if (expression.left->kind == syntax::ExpressionKind::member_access ||
            expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
            auto receiver_type = infer_receiver_type_name_for_member_call(*expression.left);
            if (receiver_type.empty()) {
                return false;
            }

            return has_unsafe_method_signature(receiver_type, expression.left->text);
        }

        return false;
    }

    auto is_addressable_storage_expression(syntax::ExpressionSyntax const& expression) const -> bool {
        switch (expression.kind) {
        case syntax::ExpressionKind::name:
            return true;
        case syntax::ExpressionKind::member_access:
        case syntax::ExpressionKind::index_access:
            return expression.left && is_addressable_storage_expression(*expression.left);
        default:
            return false;
        }
    }

    auto is_structurally_address_like_expression(syntax::ExpressionSyntax const& expression) const -> bool {
        if (expression.kind == syntax::ExpressionKind::name) {
            return true;
        }

        if (expression.kind == syntax::ExpressionKind::call && expression.left &&
            expression.left->kind == syntax::ExpressionKind::name) {
            return expression.left->text == "address_of" || expression.left->text == "raw_offset";
        }

        return false;
    }

    auto is_structurally_pointer_like_expression(syntax::ExpressionSyntax const& expression) const -> bool {
        if (expression.kind == syntax::ExpressionKind::name) {
            return true;
        }

        if (expression.kind == syntax::ExpressionKind::call && expression.left &&
            expression.left->kind == syntax::ExpressionKind::name) {
            return expression.left->text == "Pointer" || expression.left->text == "raw_offset";
        }

        return false;
    }

    void validate_pointer_typed_expression(
        syntax::ExpressionSyntax const& expression,
        std::size_t line,
        std::string_view context_description
    ) {
        auto inferred_type_name = infer_expression_type_name(expression);
        auto expected_pointee_type_name = pointer_pointee_type_name(expected_pointer_type_name_);
        if (!expected_pointee_type_name.empty() && expression.kind == syntax::ExpressionKind::call && expression.left &&
            expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "raw_offset") {
            auto source_pointee_type_name = explicit_pointer_source_pointee_type_name(expression.arguments.front());
            if (!source_pointee_type_name.empty() &&
                !are_low_level_read_types_compatible(source_pointee_type_name, expected_pointee_type_name)) {
                diagnostics_.error(
                    line,
                    "raw_offset source pointer element type '" + source_pointee_type_name +
                        "' does not match expected pointer element type '" + expected_pointee_type_name + "'"
                );
                return;
            }
        }

        if (!inferred_type_name.empty()) {
            if (!is_pointer_type_name(inferred_type_name)) {
                diagnostics_.error(
                    line,
                    std::string(context_description) + " currently requires a structurally pointer-like expression"
                );
                return;
            }

            if (!expected_pointee_type_name.empty()) {
                auto inferred_pointee_type_name = pointer_pointee_type_name(inferred_type_name);
                if (!inferred_pointee_type_name.empty() &&
                    !are_low_level_read_types_compatible(inferred_pointee_type_name, expected_pointee_type_name)) {
                    diagnostics_.error(
                        line,
                        std::string(context_description) + " pointer element type '" + inferred_pointee_type_name +
                            "' does not match expected pointer element type '" + expected_pointee_type_name + "'"
                    );
                }
            }
            return;
        }

        if (!is_structurally_pointer_like_expression(expression)) {
            diagnostics_.error(
                line,
                std::string(context_description) + " currently requires a structurally pointer-like expression"
            );
        }
    }

    void validate_address_typed_expression(
        syntax::ExpressionSyntax const& expression,
        std::size_t line,
        std::string_view context_description
    ) {
        if (expression.kind == syntax::ExpressionKind::integer_literal) {
            return;
        }

        auto inferred_type_name = infer_expression_type_name(expression);
        if (!inferred_type_name.empty()) {
            if (!is_address_type_name(inferred_type_name)) {
                diagnostics_.error(
                    line,
                    std::string(context_description) + " currently requires a structurally address-like expression"
                );
            }
            return;
        }

        if (!is_structurally_address_like_expression(expression)) {
            diagnostics_.error(
                line,
                std::string(context_description) + " currently requires a structurally address-like expression"
            );
        }
    }

    void validate_unsafe_intrinsic_operands(syntax::ExpressionSyntax const& expression) {
        if (expression.kind != syntax::ExpressionKind::call || !expression.left ||
            expression.left->kind != syntax::ExpressionKind::name ||
            !is_unsafe_intrinsic_name(expression.left->text)) {
            return;
        }

        auto const& intrinsic_name = expression.left->text;
        if (intrinsic_name == "address_of") {
            if (expression.arguments.empty() ||
                !is_addressable_storage_expression(expression.arguments.front())) {
                diagnostics_.error(
                    expression.line,
                    "address_of currently requires an addressable storage operand"
                );
            }
            return;
        }

        if (expression.arguments.empty()) {
            diagnostics_.error(
                expression.line,
                intrinsic_name + " currently requires an address-like first argument"
            );
            return;
        }

        auto first_argument_type_name = infer_expression_type_name(expression.arguments.front());
        if (first_argument_type_name.empty()) {
            if (!is_structurally_address_like_expression(expression.arguments.front()) &&
                !is_structurally_pointer_like_expression(expression.arguments.front())) {
                diagnostics_.error(
                    expression.line,
                    intrinsic_name + " currently requires an address-like first argument"
                );
                return;
            }
        } else if (!is_address_type_name(first_argument_type_name) &&
                   !is_pointer_type_name(first_argument_type_name)) {
            diagnostics_.error(
                expression.line,
                intrinsic_name + " currently requires an address-like first argument"
            );
            return;
        }

        if ((intrinsic_name == "raw_write" || intrinsic_name == "volatile_write") &&
            expression.arguments.size() >= 2) {
            auto pointee_type_name = pointer_pointee_type_name(infer_expression_type_name(expression.arguments.front()));
            auto value_type_name = infer_expression_type_name(expression.arguments[1]);
            if (!pointee_type_name.empty() && !value_type_name.empty() &&
                !are_low_level_write_types_compatible(pointee_type_name, expression.arguments[1])) {
                diagnostics_.error(
                    expression.line,
                    intrinsic_name + " value type '" + value_type_name +
                        "' does not match pointer element type '" + pointee_type_name + "'"
                );
            }
        }

        if (intrinsic_name == "raw_offset" && expression.arguments.size() >= 2) {
            auto offset_type_name = infer_expression_type_name(expression.arguments[1]);
            if (!offset_type_name.empty() && !is_integer_type_name(offset_type_name)) {
                diagnostics_.error(
                    expression.line,
                    "raw_offset currently requires an integer offset argument"
                );
            }
        }
    }

    auto classify_switch_pattern_kind(syntax::ExpressionSyntax const& pattern) const -> SwitchPatternKind {
        if (pattern.kind == syntax::ExpressionKind::call) {
            if (!pattern.left || pattern.left->kind != syntax::ExpressionKind::name) {
                return SwitchPatternKind::invalid;
            }

            if (choice_variant_arities_.contains(pattern.left->text) || is_builtin_constructor_name(pattern.left->text)) {
                return SwitchPatternKind::constructor;
            }

            return SwitchPatternKind::invalid;
        }

        if (pattern.kind == syntax::ExpressionKind::name) {
            if (choice_variant_arities_.contains(pattern.text) || is_builtin_constructor_name(pattern.text)) {
                return SwitchPatternKind::constructor;
            }

            return SwitchPatternKind::invalid;
        }

        return SwitchPatternKind::value;
    }

    auto switch_case_line(syntax::SwitchCaseSyntax const& switch_case) const -> std::size_t {
        if (switch_case.pattern.line != 0) {
            return switch_case.pattern.line;
        }

        if (!switch_case.statements.empty() && switch_case.statements.front()) {
            return switch_case.statements.front()->line;
        }

        return 0;
    }

    auto simple_payload_constructor_pattern_key(
        syntax::ExpressionSyntax const& pattern,
        std::optional<syntax::TypeSyntax> const& subject_type
    ) const -> std::optional<PayloadConstructorPatternKey> {
        if (!subject_type.has_value() || pattern.kind != syntax::ExpressionKind::call ||
            !pattern.left || pattern.left->kind != syntax::ExpressionKind::name || pattern.arguments.empty()) {
            return std::nullopt;
        }

        auto resolved_subject_type = resolve_choice_pattern_subject_type(pattern.left->text, *subject_type);
        if (!resolved_subject_type.has_value()) {
            return std::nullopt;
        }

        auto arity = find_choice_variant_payload_arity(pattern.left->text, *resolved_subject_type);
        if (!arity.has_value() || *arity != pattern.arguments.size()) {
            return std::nullopt;
        }

        auto const* variant_signature = find_choice_variant_signature(pattern.left->text, *resolved_subject_type);
        if (variant_signature == nullptr) {
            return std::nullopt;
        }

        std::unordered_map<std::string, syntax::TypeSyntax> bindings;
        if (!match_generic_type_pattern(
                variant_signature->choice_type,
                *resolved_subject_type,
                variant_signature->generic_parameters,
                bindings
            )) {
            return std::nullopt;
        }

        PayloadConstructorPatternKey key {
            .constructor_name = pattern.left->text,
        };
        for (std::size_t index = 0; index < pattern.arguments.size(); ++index) {
            auto const& argument = pattern.arguments[index];
            std::optional<syntax::TypeSyntax> payload_type;
            if (index < variant_signature->payloads.size()) {
                payload_type = substitute_generic_type_bindings(variant_signature->payloads[index].type, bindings);
            }

            if (argument.kind == syntax::ExpressionKind::name) {
                if (payload_type.has_value() &&
                    is_zero_payload_constructor_pattern(argument.text, *payload_type)) {
                    key.payloads.push_back(argument.text + "()");
                    continue;
                }

                key.payloads.push_back("*");
                continue;
            }

            auto literal_key = switch_literal_pattern_key(argument);
            if (literal_key.has_value()) {
                key.payloads.push_back(*literal_key);
                continue;
            }

            if (argument.kind == syntax::ExpressionKind::call && index < variant_signature->payloads.size()) {
                auto nested_key = simple_payload_constructor_pattern_key(argument, payload_type);
                if (nested_key.has_value()) {
                    key.payloads.push_back(render_payload_constructor_pattern_key(*nested_key));
                    continue;
                }
            }

            return std::nullopt;
        }

        return key;
    }

    auto fully_covering_choice_constructor_pattern_variant_name(
        syntax::ExpressionSyntax const& pattern,
        std::optional<syntax::TypeSyntax> const& subject_type
    ) const -> std::optional<std::string> {
        if (!subject_type.has_value() || pattern.kind != syntax::ExpressionKind::call ||
            !pattern.left || pattern.left->kind != syntax::ExpressionKind::name || pattern.arguments.empty()) {
            return std::nullopt;
        }

        auto resolved_subject_type = resolve_choice_pattern_subject_type(pattern.left->text, *subject_type);
        if (!resolved_subject_type.has_value()) {
            return std::nullopt;
        }

        auto const* variant_signature = find_choice_variant_signature(pattern.left->text, *resolved_subject_type);
        if (variant_signature == nullptr || variant_signature->payloads.empty() ||
            variant_signature->payloads.size() != pattern.arguments.size()) {
            return std::nullopt;
        }

        std::unordered_map<std::string, syntax::TypeSyntax> bindings;
        if (!match_generic_type_pattern(
                variant_signature->choice_type,
                *resolved_subject_type,
                variant_signature->generic_parameters,
                bindings
            )) {
            return std::nullopt;
        }

        for (std::size_t index = 0; index < pattern.arguments.size(); ++index) {
            auto const& argument = pattern.arguments[index];
            if (argument.kind != syntax::ExpressionKind::name) {
                return std::nullopt;
            }

            auto payload_type = substitute_generic_type_bindings(variant_signature->payloads[index].type, bindings);
            if (is_zero_payload_constructor_pattern(argument.text, payload_type)) {
                return std::nullopt;
            }
        }

        return pattern.left->text;
    }

    auto is_zero_payload_constructor_pattern(
        std::string const& constructor_name,
        syntax::TypeSyntax const& subject_type
    ) const -> bool {
        auto resolved_subject_type = resolve_choice_pattern_subject_type(constructor_name, subject_type);
        if (!resolved_subject_type.has_value()) {
            return false;
        }

        auto arity = find_choice_variant_payload_arity(constructor_name, *resolved_subject_type);
        return arity.has_value() && *arity == 0;
    }

    auto render_payload_constructor_pattern_key(PayloadConstructorPatternKey const& key) const -> std::string {
        std::string rendered = key.constructor_name + "(";
        for (std::size_t index = 0; index < key.payloads.size(); ++index) {
            if (index > 0) {
                rendered += ",";
            }
            rendered += key.payloads[index];
        }
        rendered += ")";
        return rendered;
    }

    auto parse_rendered_payload_constructor_pattern_key(std::string const& text) const
        -> std::optional<PayloadConstructorPatternKey> {
        auto const open = text.find('(');
        if (open == std::string::npos || open == 0 || text.back() != ')') {
            return std::nullopt;
        }

        PayloadConstructorPatternKey key {
            .constructor_name = text.substr(0, open),
        };

        std::size_t payload_start = open + 1;
        std::size_t depth = 0;
        for (std::size_t index = open + 1; index + 1 < text.size(); ++index) {
            if (text[index] == '(') {
                ++depth;
                continue;
            }
            if (text[index] == ')') {
                if (depth == 0) {
                    return std::nullopt;
                }
                --depth;
                continue;
            }
            if (text[index] == ',' && depth == 0) {
                key.payloads.push_back(text.substr(payload_start, index - payload_start));
                payload_start = index + 1;
            }
        }

        if (depth != 0) {
            return std::nullopt;
        }

        key.payloads.push_back(text.substr(payload_start, text.size() - payload_start - 1));
        return key;
    }

    auto payload_pattern_keys_overlap(std::string const& left, std::string const& right) const -> bool {
        if (left == "*" || right == "*" || left == right) {
            return true;
        }

        auto left_constructor = parse_rendered_payload_constructor_pattern_key(left);
        auto right_constructor = parse_rendered_payload_constructor_pattern_key(right);
        if (left_constructor.has_value() && right_constructor.has_value()) {
            return payload_constructor_patterns_overlap(*left_constructor, *right_constructor);
        }

        return false;
    }

    auto payload_constructor_patterns_overlap(
        PayloadConstructorPatternKey const& left,
        PayloadConstructorPatternKey const& right
    ) const -> bool {
        if (left.constructor_name != right.constructor_name || left.payloads.size() != right.payloads.size()) {
            return false;
        }

        for (std::size_t index = 0; index < left.payloads.size(); ++index) {
            if (!payload_pattern_keys_overlap(left.payloads[index], right.payloads[index])) {
                return false;
            }
        }

        return true;
    }

    auto validate_switch_pattern_arity(
        syntax::ExpressionSyntax const& pattern,
        std::optional<syntax::TypeSyntax> const& subject_type
    ) -> bool {
        if (pattern.kind == syntax::ExpressionKind::call) {
            if (!pattern.left || pattern.left->kind != syntax::ExpressionKind::name) {
                return true;
            }

            std::optional<std::size_t> expected_arity;
            if (subject_type.has_value()) {
                auto resolved_subject_type = resolve_choice_pattern_subject_type(pattern.left->text, *subject_type);
                if (resolved_subject_type.has_value()) {
                    expected_arity = find_choice_variant_payload_arity(pattern.left->text, *resolved_subject_type);
                }
            } else {
                auto variant = choice_variant_arities_.find(pattern.left->text);
                if (variant != choice_variant_arities_.end()) {
                    expected_arity = variant->second;
                }
            }

            if (expected_arity.has_value() && *expected_arity != pattern.arguments.size()) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.left->text + "' expects " +
                        std::to_string(*expected_arity) + " payload value" +
                        (*expected_arity == 1 ? "" : "s") + " but received " +
                        std::to_string(pattern.arguments.size())
                );
                return false;
            }
            return true;
        }

        if (pattern.kind == syntax::ExpressionKind::name) {
            std::optional<std::size_t> expected_arity;
            if (subject_type.has_value()) {
                auto resolved_subject_type = resolve_choice_pattern_subject_type(pattern.text, *subject_type);
                if (resolved_subject_type.has_value()) {
                    expected_arity = find_choice_variant_payload_arity(pattern.text, *resolved_subject_type);
                }
            } else {
                auto variant = choice_variant_arities_.find(pattern.text);
                if (variant != choice_variant_arities_.end()) {
                    expected_arity = variant->second;
                }
            }

            if (expected_arity.has_value() && *expected_arity != 0) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.text + "' expects " +
                        std::to_string(*expected_arity) + " payload value" +
                        (*expected_arity == 1 ? "" : "s") + " but received 0"
                );
                return false;
            }
        }

        return true;
    }

    auto validate_switch_pattern_head(
        syntax::ExpressionSyntax const& pattern,
        std::optional<syntax::TypeSyntax> const& subject_type
    ) -> bool {
        if (pattern.kind == syntax::ExpressionKind::call) {
            if (!pattern.left || pattern.left->kind != syntax::ExpressionKind::name) {
                return true;
            }

            auto resolved_subject_type = subject_type.has_value()
                                             ? resolve_choice_pattern_subject_type(pattern.left->text, *subject_type)
                                             : std::nullopt;
            auto matches_subject_variant = resolved_subject_type.has_value();
            if (!matches_subject_variant && !choice_variant_arities_.contains(pattern.left->text) &&
                !(subject_type.has_value() ? false : is_builtin_constructor_name(pattern.left->text))) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.left->text + "' does not match any declared choice variant"
                );
                return false;
            }

            if (!matches_subject_variant && subject_type.has_value() && choice_variant_arities_.contains(pattern.left->text)) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.left->text + "' does not belong to switched choice type '" +
                        render_type_name(*subject_type) + "'"
                );
                return false;
            }
            return true;
        }

        if (pattern.kind == syntax::ExpressionKind::name && !pattern.text.empty()) {
            auto resolved_subject_type = subject_type.has_value()
                                             ? resolve_choice_pattern_subject_type(pattern.text, *subject_type)
                                             : std::nullopt;
            auto matches_subject_variant = resolved_subject_type.has_value();
            if (!matches_subject_variant && !choice_variant_arities_.contains(pattern.text) &&
                !(subject_type.has_value() ? false : is_builtin_constructor_name(pattern.text)) &&
                pattern.text != "default") {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.text + "' does not match any declared choice variant"
                );
                return false;
            }

            if (!matches_subject_variant && subject_type.has_value() && choice_variant_arities_.contains(pattern.text) &&
                pattern.text != "default") {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.text + "' does not belong to switched choice type '" +
                        render_type_name(*subject_type) + "'"
                );
                return false;
            }
        }

        return true;
    }

    auto analyze_switch_pattern(
        syntax::ExpressionSyntax const& pattern,
        bool in_async_function,
        std::optional<syntax::TypeSyntax> const& subject_type = std::nullopt,
        bool in_constructor_payload = false
    ) -> bool {
        auto should_validate_constructor_head =
            !in_constructor_payload || (pattern.kind == syntax::ExpressionKind::call && subject_type.has_value());
        if (should_validate_constructor_head) {
            if (!validate_switch_pattern_head(pattern, subject_type)) {
                return false;
            }
            if (!validate_switch_pattern_arity(pattern, subject_type)) {
                return false;
            }
        }

        if (pattern.kind == syntax::ExpressionKind::call) {
            if (!pattern.left || pattern.left->kind != syntax::ExpressionKind::name) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern currently requires a constructor name"
                );
                return false;
            }

            auto valid = true;
            std::optional<ChoiceVariantSignature const*> variant_signature;
            std::unordered_map<std::string, syntax::TypeSyntax> bindings;
            if (subject_type.has_value()) {
                auto resolved_subject_type = resolve_choice_pattern_subject_type(pattern.left->text, *subject_type);
                auto const* matched_signature =
                    resolved_subject_type.has_value()
                        ? find_choice_variant_signature(pattern.left->text, *resolved_subject_type)
                        : nullptr;
                if (matched_signature != nullptr &&
                    match_generic_type_pattern(
                        matched_signature->choice_type,
                        *resolved_subject_type,
                        matched_signature->generic_parameters,
                        bindings
                    )) {
                    variant_signature = matched_signature;
                }
            }

            for (std::size_t index = 0; index < pattern.arguments.size(); ++index) {
                auto const& argument = pattern.arguments[index];
                std::optional<syntax::TypeSyntax> payload_type;
                if (variant_signature.has_value() && index < (*variant_signature)->payloads.size()) {
                    payload_type =
                        substitute_generic_type_bindings((*variant_signature)->payloads[index].type, bindings);
                }

                if (argument.kind == syntax::ExpressionKind::name || switch_literal_pattern_key(argument).has_value() ||
                    argument.kind == syntax::ExpressionKind::call) {
                    valid = analyze_switch_pattern(argument, in_async_function, payload_type, true) && valid;
                    continue;
                }

                diagnostics_.error(
                    argument.line,
                    "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
                );
                valid = false;
            }
            return valid;
        }

        if (pattern.kind == syntax::ExpressionKind::name) {
            return true;
        }

        if (switch_literal_pattern_key(pattern).has_value()) {
            if (!in_constructor_payload) {
                validate_switch_value_pattern_type(pattern, subject_type);
            }
            return true;
        }

        if (in_constructor_payload) {
            diagnostics_.error(
                pattern.line,
                "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
            );
            return false;
        }

        analyze_expression(pattern, in_async_function);
        validate_switch_value_pattern_type(pattern, subject_type);
        return true;
    }

    auto declare_switch_pattern_bindings(
        syntax::ExpressionSyntax const& pattern,
        std::optional<syntax::TypeSyntax> const& pattern_type,
        bool bind_name,
        std::unordered_set<std::string>& bound_names
    ) -> bool {
        if (pattern.kind == syntax::ExpressionKind::call) {
            if (!pattern.left || pattern.left->kind != syntax::ExpressionKind::name || !pattern_type.has_value()) {
                auto valid = true;
                for (auto const& argument : pattern.arguments) {
                    valid = declare_switch_pattern_bindings(argument, std::nullopt, true, bound_names) && valid;
                }
                return valid;
            }

            auto resolved_pattern_type = resolve_choice_pattern_subject_type(pattern.left->text, *pattern_type);
            auto const* variant_signature = resolved_pattern_type.has_value()
                                                ? find_choice_variant_signature(pattern.left->text, *resolved_pattern_type)
                                                : nullptr;
            if (variant_signature == nullptr) {
                auto valid = true;
                for (auto const& argument : pattern.arguments) {
                    valid = declare_switch_pattern_bindings(argument, std::nullopt, true, bound_names) && valid;
                }
                return valid;
            }

            std::unordered_map<std::string, syntax::TypeSyntax> bindings;
            if (!match_generic_type_pattern(
                    variant_signature->choice_type,
                    *resolved_pattern_type,
                    variant_signature->generic_parameters,
                    bindings
                )) {
                auto valid = true;
                for (auto const& argument : pattern.arguments) {
                    valid = declare_switch_pattern_bindings(argument, std::nullopt, true, bound_names) && valid;
                }
                return valid;
            }

            auto valid = true;
            for (std::size_t index = 0; index < pattern.arguments.size(); ++index) {
                std::optional<syntax::TypeSyntax> payload_type;
                if (index < variant_signature->payloads.size()) {
                    payload_type = substitute_generic_type_bindings(variant_signature->payloads[index].type, bindings);
                }
                valid =
                    declare_switch_pattern_bindings(pattern.arguments[index], payload_type, true, bound_names) && valid;
            }
            return valid;
        }

        if (bind_name && pattern.kind == syntax::ExpressionKind::name && !pattern.text.empty()) {
            if (pattern_type.has_value() && is_zero_payload_constructor_pattern(pattern.text, *pattern_type)) {
                return true;
            }

            if (!bound_names.insert(pattern.text).second) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern cannot bind '" + pattern.text + "' more than once"
                );
                return false;
            }

            declare_binding(
                pattern.text,
                pattern_type.has_value() ? render_type_name(*pattern_type) : std::string {},
                false
            );
        }
        return true;
    }

    auto is_value_return_statement(syntax::StatementSyntax const& statement) const -> bool {
        return statement.kind == syntax::StatementKind::return_statement &&
               (!statement.expression.text.empty() || statement.expression.left || statement.expression.right ||
                statement.expression.alternate || !statement.expression.arguments.empty() ||
                !statement.expression.nested_statements.empty());
    }

    auto expression_requires_value_boundary(syntax::ExpressionSyntax const& expression) const -> bool {
        return expression.kind == syntax::ExpressionKind::task || expression.kind == syntax::ExpressionKind::thread;
    }

    auto classify_concurrency_expression(syntax::ExpressionSyntax const& expression) const -> ConcurrencyExpressionKind {
        return expression.kind == syntax::ExpressionKind::thread ? ConcurrencyExpressionKind::thread
                                                                 : ConcurrencyExpressionKind::task;
    }

    auto is_structurally_awaitable_expression(syntax::ExpressionSyntax const& expression) const -> bool {
        auto origin = infer_expression_value_origin(expression);
        return origin == ValueOriginKind::task || origin == ValueOriginKind::async_call;
    }

    auto validate_await_operand(syntax::ExpressionSyntax const& operand, std::size_t line) -> void {
        auto origin = infer_expression_value_origin(operand);
        if (origin == ValueOriginKind::task || origin == ValueOriginKind::async_call) {
            return;
        }

        if (origin == ValueOriginKind::thread) {
            diagnostics_.error(line, "await cannot be used with thread values; use .join() instead");
            return;
        }

        diagnostics_.error(line, "await expression currently requires a task value or declared async call result");
    }

    auto is_declared_async_call(syntax::ExpressionSyntax const& expression) const -> bool {
        if (expression.kind != syntax::ExpressionKind::call || !expression.left) {
            return false;
        }

        if (expression.left->kind == syntax::ExpressionKind::name) {
            return has_async_callable_name(expression.left->text);
        }

        if (expression.left->kind == syntax::ExpressionKind::member_access ||
            expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
            auto receiver_type = infer_receiver_type_name_for_member_call(*expression.left);
            if (receiver_type.empty()) {
                return false;
            }

            return has_async_method_signature(receiver_type, expression.left->text);
        }

        return false;
    }

    auto is_structurally_thread_expression(syntax::ExpressionSyntax const& expression) const -> bool {
        return infer_expression_value_origin(expression) == ValueOriginKind::thread;
    }

    auto validate_join_receiver(syntax::ExpressionSyntax const& receiver_expression, std::size_t line) -> void {
        auto origin = infer_expression_value_origin(receiver_expression);
        if (origin == ValueOriginKind::thread) {
            return;
        }

        if (origin == ValueOriginKind::task) {
            diagnostics_.error(line, "join() cannot be used with task values; use await instead");
            return;
        }

        if (origin == ValueOriginKind::async_call) {
            diagnostics_.error(line, "join() cannot be used with declared async call results; use await instead");
            return;
        }

        diagnostics_.error(line, "join() currently requires a thread value receiver");
    }

    auto validate_return_expression(syntax::ExpressionSyntax const& expression, std::size_t line) -> void {
        auto origin = infer_expression_value_origin(expression);
        if (origin == ValueOriginKind::none) {
            return;
        }

        if (origin == ValueOriginKind::thread) {
            diagnostics_.error(line, "return cannot forward thread values; use .join() instead");
            return;
        }

        diagnostics_.error(line, "return cannot forward task or async-call values; use await instead");
    }

    auto infer_statement_value_type_name(syntax::StatementSyntax const& statement) const -> std::string {
        if (statement.kind != syntax::StatementKind::expression_statement &&
            statement.kind != syntax::StatementKind::return_statement) {
            return {};
        }

        return infer_expression_type_name(statement.expression);
    }

    void collect_concurrency_marker_implementations() {
        transferable_impl_types_.clear();
        shareable_impl_types_.clear();
        for (auto const& implementation : module_.implementations) {
            auto rendered_type = render_type_name(implementation.receiver_type);
            if (implementation.interface_type.name == "Transferable") {
                transferable_impl_types_.push_back(rendered_type);
            }
            if (implementation.interface_type.name == "Shareable") {
                shareable_impl_types_.push_back(rendered_type);
            }
        }
    }

    void collect_choice_variant_metadata() {
        choice_variant_arities_.clear();
        choice_variant_signatures_.clear();
        for (auto const& choice : module_.choices) {
            syntax::TypeSyntax choice_type;
            choice_type.name = choice.name;
            for (auto const& generic_parameter : choice.generic_parameters) {
                syntax::TypeSyntax generic_argument;
                generic_argument.name = generic_parameter;
                choice_type.generic_arguments.push_back(generic_argument);
            }

            for (auto const& variant : choice.variants) {
                choice_variant_arities_[variant.name] = variant.payloads.size();
                choice_variant_signatures_.push_back(ChoiceVariantSignature {
                    .choice_type = choice_type,
                    .generic_parameters = choice.generic_parameters,
                    .variant_name = variant.name,
                    .payloads = variant.payloads,
                });
            }
        }
    }

    void collect_async_callable_names() {
        async_callable_names_.clear();
        async_method_signatures_.clear();

        for (auto const& function : module_.functions) {
            if (function.is_async) {
                async_callable_names_.push_back(function.name);
            }
        }

        for (auto const& implementation : module_.implementations) {
            for (auto const& method : implementation.methods) {
                if (method.is_async) {
                    async_method_signatures_.push_back(AsyncMethodSignature {
                        .receiver_type = implementation.receiver_type,
                        .method_name = method.name,
                    });
                }
            }
        }

        for (auto const& extension : module_.extensions) {
            for (auto const& method : extension.methods) {
                if (method.is_async) {
                    async_method_signatures_.push_back(AsyncMethodSignature {
                        .receiver_type = extension.receiver_type,
                        .method_name = method.name,
                    });
                }
            }
        }

        for (auto const& foreign_export : module_.foreign_exports) {
            if (foreign_export.function.is_async) {
                async_callable_names_.push_back(foreign_export.function.name);
            }
        }
    }

    void collect_unsafe_callable_names() {
        unsafe_callable_names_.clear();
        unsafe_method_signatures_.clear();

        for (auto const& function : module_.functions) {
            if (function.is_unsafe) {
                unsafe_callable_names_.push_back(function.name);
            }
        }

        for (auto const& implementation : module_.implementations) {
            for (auto const& method : implementation.methods) {
                if (method.is_unsafe) {
                    unsafe_method_signatures_.push_back(UnsafeMethodSignature {
                        .receiver_type = implementation.receiver_type,
                        .method_name = method.name,
                    });
                }
            }
        }

        for (auto const& extension : module_.extensions) {
            for (auto const& method : extension.methods) {
                if (method.is_unsafe) {
                    unsafe_method_signatures_.push_back(UnsafeMethodSignature {
                        .receiver_type = extension.receiver_type,
                        .method_name = method.name,
                    });
                }
            }
        }

        for (auto const& foreign_export : module_.foreign_exports) {
            if (foreign_export.function.is_unsafe) {
                unsafe_callable_names_.push_back(foreign_export.function.name);
            }
        }
    }

    void collect_callable_return_types() {
        callable_signatures_.clear();
        method_return_signatures_.clear();

        for (auto const& function : module_.functions) {
            callable_signatures_.push_back(CallableSignature {
                .name = function.name,
                .generic_parameters = function.generic_parameters,
                .parameters = function.parameters,
                .return_type = function.return_type,
            });
        }

        for (auto const& implementation : module_.implementations) {
            for (auto const& method : implementation.methods) {
                method_return_signatures_.push_back(MethodReturnSignature {
                    .receiver_type = implementation.receiver_type,
                    .method_name = method.name,
                    .generic_parameters = method.generic_parameters,
                    .parameters = method.parameters,
                    .return_type = method.return_type,
                });
            }
        }

        for (auto const& extension : module_.extensions) {
            for (auto const& method : extension.methods) {
                method_return_signatures_.push_back(MethodReturnSignature {
                    .receiver_type = extension.receiver_type,
                    .method_name = method.name,
                    .generic_parameters = method.generic_parameters,
                    .parameters = method.parameters,
                    .return_type = method.return_type,
                });
            }
        }

        for (auto const& foreign_export : module_.foreign_exports) {
            callable_signatures_.push_back(CallableSignature {
                .name = foreign_export.function.name,
                .generic_parameters = foreign_export.function.generic_parameters,
                .parameters = foreign_export.function.parameters,
                .return_type = foreign_export.function.return_type,
            });
        }
    }

    void collect_record_field_types() {
        record_field_signatures_.clear();

        for (auto const& record : module_.records) {
            syntax::TypeSyntax record_type {.name = record.name};
            for (auto const& generic_parameter : record.generic_parameters) {
                record_type.generic_arguments.push_back(syntax::TypeSyntax {.name = generic_parameter});
            }

            for (auto const& field : record.fields) {
                record_field_signatures_.push_back(RecordFieldSignature {
                    .record_type = record_type,
                    .generic_parameters = record.generic_parameters,
                    .field_name = field.name,
                    .field_type = field.type,
                });
            }
        }
    }

    void collect_constant_bindings() {
        module_constant_bindings_.clear();
        std::unordered_set<std::string> seen_constant_names;

        for (auto const& constant : module_.constants) {
            if (!seen_constant_names.insert(constant.name).second) {
                diagnostics_.error(
                    constant.initializer.line,
                    "top-level constant '" + constant.name + "' is duplicated"
                );
                continue;
            }

            module_constant_bindings_.push_back(Binding {
                .name = constant.name,
                .type_name = render_type_name(constant.type),
                .module_constant = true,
            });
        }
    }

    auto is_constant_initializer_type_compatible(
        syntax::ExpressionSyntax const& initializer,
        std::string const& initializer_type_name,
        std::string const& declared_type_name
    ) const -> bool {
        if (initializer_type_name.empty() || declared_type_name.empty()) {
            return true;
        }

        if (initializer.kind == syntax::ExpressionKind::integer_literal &&
            (is_integer_type_name(declared_type_name) || is_address_type_name(declared_type_name))) {
            return true;
        }

        return are_low_level_read_types_compatible(initializer_type_name, declared_type_name);
    }

    template <typename Visitor>
    auto visit_constant_initializer_expression(
        syntax::ExpressionSyntax const& expression,
        Visitor& visitor
    ) const -> bool {
        if (visitor(expression)) {
            return true;
        }

        for (auto const& argument : expression.arguments) {
            if (visit_constant_initializer_expression(argument, visitor)) {
                return true;
            }
        }

        if (expression.left &&
            !(expression.kind == syntax::ExpressionKind::call &&
              expression.left->kind == syntax::ExpressionKind::name)) {
            if (visit_constant_initializer_expression(*expression.left, visitor)) {
                return true;
            }
        }

        if (expression.right && visit_constant_initializer_expression(*expression.right, visitor)) {
            return true;
        }

        if (expression.alternate && visit_constant_initializer_expression(*expression.alternate, visitor)) {
            return true;
        }

        return false;
    }

    void validate_constant_initializer_references(syntax::ExpressionSyntax const& expression) {
        auto validator = [&](syntax::ExpressionSyntax const& current) {
            if (current.kind != syntax::ExpressionKind::name || find_binding(current.text) != nullptr) {
                return false;
            }

            diagnostics_.error(
                current.line,
                "constant initializer references unknown name '" + current.text + "'"
            );
            return true;
        };
        visit_constant_initializer_expression(expression, validator);
    }

    void collect_constant_initializer_references(
        syntax::ExpressionSyntax const& expression,
        std::vector<ConstantReference>& references
    ) const {
        auto collector = [&](syntax::ExpressionSyntax const& current) {
            if (current.kind == syntax::ExpressionKind::name) {
                references.push_back(ConstantReference {
                    .name = current.text,
                    .line = current.line,
                });
            }
            return false;
        };
        visit_constant_initializer_expression(expression, collector);
    }

    void validate_constant_initializer_cycles() {
        std::unordered_map<std::string, syntax::ConstantSyntax const*> constants_by_name;
        for (auto const& constant : module_.constants) {
            constants_by_name.emplace(constant.name, &constant);
        }

        std::unordered_map<std::string, int> visit_state;
        auto visit = [&](auto&& self, std::string const& name) -> void {
            visit_state[name] = 1;

            auto constant = constants_by_name.find(name);
            if (constant == constants_by_name.end()) {
                visit_state[name] = 2;
                return;
            }

            std::vector<ConstantReference> references;
            collect_constant_initializer_references(constant->second->initializer, references);
            for (auto const& reference : references) {
                if (!constants_by_name.contains(reference.name)) {
                    continue;
                }

                auto state = visit_state.find(reference.name);
                if (state != visit_state.end() && state->second == 1) {
                    diagnostics_.error(
                        reference.line,
                        "constant initializer cycle includes '" + reference.name + "'"
                    );
                    continue;
                }
                if (state == visit_state.end()) {
                    self(self, reference.name);
                }
            }

            visit_state[name] = 2;
        };

        for (auto const& constant : module_.constants) {
            if (!visit_state.contains(constant.name)) {
                visit(visit, constant.name);
            }
        }
    }

    auto validate_constant_initializer_runtime_restrictions(syntax::ExpressionSyntax const& expression) -> bool {
        auto validator = [&](syntax::ExpressionSyntax const& current) {
            if (current.kind == syntax::ExpressionKind::task || current.kind == syntax::ExpressionKind::thread) {
                diagnostics_.error(
                    current.line,
                    "constant initializer cannot use " + current.text + " expression"
                );
                return true;
            }

            if (current.kind == syntax::ExpressionKind::unary && current.text == "await") {
                diagnostics_.error(current.line, "constant initializer cannot use await expression");
                return true;
            }

            if (current.kind == syntax::ExpressionKind::call && current.left &&
                current.left->kind == syntax::ExpressionKind::name &&
                is_unsafe_intrinsic_name(current.left->text)) {
                diagnostics_.error(
                    current.line,
                    "constant initializer cannot use unsafe intrinsic '" + current.left->text + "'"
                );
                return true;
            }

            if (is_pointer_constructor_call(current)) {
                diagnostics_.error(current.line, "constant initializer cannot use Pointer construction");
                return true;
            }

            if (is_declared_unsafe_call(current)) {
                auto diagnostic_subject = std::string {"unsafe function"};
                if (current.left->kind == syntax::ExpressionKind::member_access ||
                    current.left->kind == syntax::ExpressionKind::null_safe_member_access) {
                    diagnostic_subject = "unsafe method";
                }

                diagnostics_.error(
                    current.line,
                    "constant initializer cannot call " + diagnostic_subject + " '" + current.left->text + "'"
                );
                return true;
            }

            if (current.kind == syntax::ExpressionKind::call && current.left &&
                current.left->kind == syntax::ExpressionKind::name &&
                !find_callable_return_type_name(current.left->text, current.arguments).empty()) {
                diagnostics_.error(
                    current.line,
                    "constant initializer cannot call function '" + current.left->text + "'"
                );
                return true;
            }

            return false;
        };
        return visit_constant_initializer_expression(expression, validator);
    }

    void analyze_constants() {
        for (auto const& constant : module_.constants) {
            auto declared_type_name = render_type_name(constant.type);
            auto saved_expected_pointer_type_name = expected_pointer_type_name_;
            if (is_pointer_type(constant.type)) {
                expected_pointer_type_name_ = declared_type_name;
            }

            if (validate_constant_initializer_runtime_restrictions(constant.initializer)) {
                expected_pointer_type_name_ = saved_expected_pointer_type_name;
                continue;
            }

            analyze_expression(constant.initializer, false);
            validate_constant_initializer_references(constant.initializer);
            auto read_result_type_mismatch = validate_read_result_type(
                constant.initializer,
                declared_type_name,
                constant.initializer.line,
                "constant"
            );
            if (is_pointer_type(constant.type) && !read_result_type_mismatch) {
                validate_pointer_typed_expression(
                    constant.initializer,
                    constant.initializer.line,
                    "pointer-typed constant initializer"
                );
                expected_pointer_type_name_ = saved_expected_pointer_type_name;
                continue;
            }
            if (is_address_type(constant.type) && !read_result_type_mismatch) {
                validate_address_typed_expression(
                    constant.initializer,
                    constant.initializer.line,
                    "address-typed constant initializer"
                );
                expected_pointer_type_name_ = saved_expected_pointer_type_name;
                continue;
            }

            auto initializer_type_name = infer_expression_type_name(constant.initializer);
            if (!is_constant_initializer_type_compatible(
                    constant.initializer,
                    initializer_type_name,
                    declared_type_name
                )) {
                diagnostics_.error(
                    constant.initializer.line,
                    "constant initializer type '" + initializer_type_name +
                        "' does not match declared constant type '" + declared_type_name + "'"
                );
            }
            expected_pointer_type_name_ = saved_expected_pointer_type_name;
        }
    }

    void analyze_function(
        syntax::FunctionSyntax const& function,
        std::string receiver_type_name = {}
    ) {
        auto saved_receiver_context_active = receiver_context_active_;
        auto saved_unsafe_context_active = unsafe_context_active_;
        auto saved_current_function_returns_pointer = current_function_returns_pointer_;
        auto saved_current_function_returns_address = current_function_returns_address_;
        auto saved_current_function_return_type_name = current_function_return_type_name_;
        receiver_context_active_ = !receiver_type_name.empty();
        unsafe_context_active_ = function.is_unsafe;
        current_function_returns_pointer_ = is_pointer_type(function.return_type);
        current_function_returns_address_ = is_address_type(function.return_type);
        current_function_return_type_name_ = render_type_name(function.return_type);

        scope_stack_.clear();
        transferable_constraint_types_.clear();
        shareable_constraint_types_.clear();
        for (auto const& constraint : function.where_constraints) {
            for (auto const& requirement : constraint.requirements) {
                if (requirement.name == "Transferable") {
                    transferable_constraint_types_.push_back(constraint.parameter_name);
                }
                if (requirement.name == "Shareable") {
                    shareable_constraint_types_.push_back(constraint.parameter_name);
                }
            }
        }
        push_scope();
        for (auto const& parameter : function.parameters) {
            if (parameter.name == "this" && receiver_type_name.empty()) {
                diagnostics_.error(function.line, "receiver parameter 'this' is only valid in implements or extend methods");
            } else if (parameter.name == "this" && !is_receiver_self_type_name(parameter.type.name)) {
                diagnostics_.error(
                    function.line,
                    "receiver parameter 'this' must use This, shared This, or exclusive This"
                );
            }
            if (!(parameter.name == "this" && receiver_type_name.empty())) {
                validate_receiver_type_usage(parameter.type, function.line);
            }
            declare_binding(
                parameter.name,
                parameter.name == "this" && !receiver_type_name.empty() ? receiver_type_name
                                                                        : render_type_name(parameter.type),
                false,
                parameter.name == "this",
                true
            );
        }

        validate_receiver_type_usage(function.return_type, function.line);

        for (auto const& statement : function.body_statements) {
            analyze_statement(statement, function.is_async);
        }

        pop_scope();
        receiver_context_active_ = saved_receiver_context_active;
        unsafe_context_active_ = saved_unsafe_context_active;
        current_function_returns_pointer_ = saved_current_function_returns_pointer;
        current_function_returns_address_ = saved_current_function_returns_address;
        current_function_return_type_name_ = saved_current_function_return_type_name;
    }

    void analyze_statement(syntax::StatementSyntax const& statement, bool in_async_function) {
        if (statement.kind == syntax::StatementKind::let_binding || statement.kind == syntax::StatementKind::var_binding) {
            auto saved_expected_pointer_type_name = expected_pointer_type_name_;
            if (!statement.annotated_type.name.empty() && is_pointer_type(statement.annotated_type)) {
                expected_pointer_type_name_ = render_type_name(statement.annotated_type);
            }
            analyze_expression(statement.expression, in_async_function);
            auto type_name = !statement.annotated_type.name.empty()
                                 ? render_type_name(statement.annotated_type)
                                 : infer_expression_type_name(statement.expression);
            if (!statement.annotated_type.name.empty()) {
                validate_receiver_type_usage(statement.annotated_type, statement.line);
                auto read_result_type_mismatch = validate_read_result_type(
                    statement.expression,
                    render_type_name(statement.annotated_type),
                    statement.line,
                    "binding"
                );
                if (is_pointer_type(statement.annotated_type) && !read_result_type_mismatch) {
                    validate_pointer_typed_expression(
                        statement.expression,
                        statement.line,
                        "pointer-typed binding initializer"
                    );
                }
                if (is_address_type(statement.annotated_type) && !read_result_type_mismatch) {
                    validate_address_typed_expression(
                        statement.expression,
                        statement.line,
                        "address-typed binding initializer"
                    );
                }
            }
            declare_binding(
                statement.name,
                type_name,
                statement.kind == syntax::StatementKind::var_binding,
                false,
                false,
                infer_expression_value_origin(statement.expression)
            );
            expected_pointer_type_name_ = saved_expected_pointer_type_name;
            return;
        }

        if (statement.kind == syntax::StatementKind::assignment_statement) {
            analyze_expression(statement.assignment_target, in_async_function, true);
            analyze_expression(statement.expression, in_async_function);

            if (statement.assignment_target.kind == syntax::ExpressionKind::name) {
                update_binding(
                    statement.assignment_target.text,
                    infer_expression_type_name(statement.expression),
                    infer_expression_value_origin(statement.expression)
                );
            }
        }

        if (statement.kind == syntax::StatementKind::return_statement) {
            auto saved_expected_pointer_type_name = expected_pointer_type_name_;
            if (current_function_returns_pointer_) {
                expected_pointer_type_name_ = current_function_return_type_name_;
            }
            analyze_expression(statement.expression, in_async_function, true);
            validate_return_expression(statement.expression, statement.line);
            auto read_result_type_mismatch = validate_read_result_type(
                statement.expression,
                current_function_return_type_name_,
                statement.line,
                "function return"
            );
            if (current_function_returns_pointer_ && !read_result_type_mismatch) {
                validate_pointer_typed_expression(
                    statement.expression,
                    statement.line,
                    "pointer-returning function"
                );
            }
            if (current_function_returns_address_ && !read_result_type_mismatch) {
                validate_address_typed_expression(
                    statement.expression,
                    statement.line,
                    "address-returning function"
                );
            }
            expected_pointer_type_name_ = saved_expected_pointer_type_name;
        }

        if (statement.kind == syntax::StatementKind::break_statement) {
            if (loop_depth_ == 0) {
                diagnostics_.error(statement.line, "break statement is only valid inside loops");
            }
            return;
        }

        if (statement.kind == syntax::StatementKind::continue_statement) {
            if (loop_depth_ == 0) {
                diagnostics_.error(statement.line, "continue statement is only valid inside loops");
            }
            return;
        }

        if (statement.kind == syntax::StatementKind::for_statement) {
            analyze_expression(statement.expression, in_async_function);
            auto baseline_scope = snapshot_scope_stack();

            restore_scope_stack(baseline_scope);
            push_scope();
            declare_binding(statement.name, {}, true);
            ++loop_depth_;
            for (auto const& nested_statement : statement.nested_statements) {
                analyze_statement(nested_statement, in_async_function);
            }
            --loop_depth_;
            pop_scope();

            restore_scope_stack(merge_scope_snapshots({baseline_scope, snapshot_scope_stack()}));
            return;
        }

        if (statement.kind != syntax::StatementKind::assignment_statement &&
            statement.kind != syntax::StatementKind::return_statement) {
            analyze_expression(statement.assignment_target, in_async_function);
            analyze_expression(statement.expression, in_async_function);
        }

        if (statement.kind == syntax::StatementKind::if_statement) {
            auto baseline_scope = snapshot_scope_stack();
            std::vector<ScopeSnapshot> branch_results;

            restore_scope_stack(baseline_scope);
            push_scope();
            for (auto const& nested_statement : statement.nested_statements) {
                analyze_statement(nested_statement, in_async_function);
            }
            pop_scope();
            branch_results.push_back(snapshot_scope_stack());

            if (!statement.alternate_statements.empty()) {
                restore_scope_stack(baseline_scope);
                push_scope();
                for (auto const& alternate_statement : statement.alternate_statements) {
                    analyze_statement(alternate_statement, in_async_function);
                }
                pop_scope();
                branch_results.push_back(snapshot_scope_stack());
            } else {
                branch_results.push_back(baseline_scope);
            }

            restore_scope_stack(merge_scope_snapshots(branch_results));
            return;
        }

        if (statement.kind == syntax::StatementKind::guard_statement) {
            auto baseline_scope = snapshot_scope_stack();

            restore_scope_stack(baseline_scope);
            push_scope();
            for (auto const& nested_statement : statement.nested_statements) {
                analyze_statement(nested_statement, in_async_function);
            }
            pop_scope();

            restore_scope_stack(baseline_scope);
            return;
        }

        if (statement.kind == syntax::StatementKind::unsafe_statement) {
            auto saved_unsafe_context_active = unsafe_context_active_;
            unsafe_context_active_ = true;
            push_scope();
            for (auto const& nested_statement : statement.nested_statements) {
                analyze_statement(nested_statement, in_async_function);
            }
            pop_scope();
            unsafe_context_active_ = saved_unsafe_context_active;
            return;
        }

        if (statement.kind == syntax::StatementKind::switch_statement) {
            auto baseline_scope = snapshot_scope_stack();
            std::vector<ScopeSnapshot> case_results;
            auto has_default_case = false;
            auto saw_value_pattern = false;
            auto saw_constructor_pattern = false;
            auto saw_semantic_default = false;
            auto switch_patterns_valid = true;
            auto saw_true_value_pattern = false;
            auto saw_false_value_pattern = false;
            std::unordered_set<std::string> seen_literal_value_patterns;
            std::unordered_set<std::string> seen_zero_payload_choice_variants;
            std::vector<PayloadConstructorPatternKey> seen_simple_payload_choice_patterns;
            SwitchChoiceCoverage choice_coverage;
            std::optional<syntax::TypeSyntax> switch_subject_type;
            auto switch_subject_type_name = infer_expression_type_name(statement.expression);
            if (!switch_subject_type_name.empty()) {
                switch_subject_type = parse_rendered_type_name(switch_subject_type_name);
                if (switch_subject_type.has_value()) {
                    auto zero_payload_choice_variants =
                        zero_payload_choice_variant_names_if_enum_like(*switch_subject_type);
                    if (zero_payload_choice_variants.has_value()) {
                        choice_coverage.set_zero_payload_variants(*zero_payload_choice_variants);
                    }
                    auto payload_bearing_choice_variants = choice_variant_names_if_payload_bearing(*switch_subject_type);
                    if (payload_bearing_choice_variants.has_value()) {
                        choice_coverage.set_payload_bearing_variants(*payload_bearing_choice_variants);
                    }
                }
            }

            for (std::size_t case_index = 0; case_index < statement.switch_cases.size(); ++case_index) {
                auto const& switch_case = statement.switch_cases[case_index];
                auto valid_pattern = true;

                if (switch_case.is_default) {
                    if (saw_semantic_default) {
                        diagnostics_.error(
                            switch_case_line(switch_case),
                            "switch statement may only contain one default case"
                        );
                        valid_pattern = false;
                    }

                    if (case_index + 1 != statement.switch_cases.size()) {
                        diagnostics_.error(
                            switch_case_line(switch_case),
                            "switch default case must be the final case"
                        );
                        valid_pattern = false;
                    }

                    if (switch_patterns_valid && switch_subject_type_name == "Bool" && saw_true_value_pattern &&
                        saw_false_value_pattern) {
                        diagnostics_.error(
                            switch_case_line(switch_case),
                            "switch default case is redundant after true and false value patterns"
                        );
                        valid_pattern = false;
                    }

                    if (switch_patterns_valid && choice_coverage.all_zero_payload_variants_covered()) {
                        diagnostics_.error(
                            switch_case_line(switch_case),
                            "switch default case is redundant after all zero-payload choice variants are covered"
                        );
                        valid_pattern = false;
                    }

                    if (switch_patterns_valid && choice_coverage.all_payload_bearing_variants_covered()) {
                        diagnostics_.error(
                            switch_case_line(switch_case),
                            "switch default case is redundant after all choice variants are covered"
                        );
                        valid_pattern = false;
                    }

                    saw_semantic_default = true;
                }

                if (!switch_case.is_default) {
                    auto pattern_kind = classify_switch_pattern_kind(switch_case.pattern);
                    if (pattern_kind == SwitchPatternKind::value) {
                        saw_value_pattern = true;
                        if (switch_case.pattern.kind == syntax::ExpressionKind::boolean_literal) {
                            saw_true_value_pattern = saw_true_value_pattern || switch_case.pattern.text == "true";
                            saw_false_value_pattern = saw_false_value_pattern || switch_case.pattern.text == "false";
                        }
                        auto literal_key = switch_literal_pattern_key(switch_case.pattern);
                        if (literal_key.has_value() && !seen_literal_value_patterns.insert(*literal_key).second) {
                            diagnostics_.error(
                                switch_case.pattern.line,
                                "switch value pattern '" + render_switch_literal_pattern(switch_case.pattern) +
                                    "' is duplicated"
                            );
                            valid_pattern = false;
                        }
                    } else if (pattern_kind == SwitchPatternKind::constructor) {
                        saw_constructor_pattern = true;
                        if (switch_case.pattern.kind == syntax::ExpressionKind::name &&
                            choice_coverage.is_zero_payload_variant(switch_case.pattern.text)) {
                            if (!seen_zero_payload_choice_variants.insert(switch_case.pattern.text).second) {
                                diagnostics_.error(
                                    switch_case.pattern.line,
                                    "switch constructor pattern '" + switch_case.pattern.text + "' is duplicated"
                                );
                                valid_pattern = false;
                            }
                            choice_coverage.cover_zero_payload_variant(switch_case.pattern.text);
                        }
                        if (switch_case.pattern.kind == syntax::ExpressionKind::name &&
                            switch_subject_type.has_value() &&
                            choice_coverage.tracks_payload_bearing_variant(switch_case.pattern.text) &&
                            is_zero_payload_constructor_pattern(switch_case.pattern.text, *switch_subject_type)) {
                            choice_coverage.cover_payload_bearing_variant(switch_case.pattern.text);
                        }

                        auto constructor_key =
                            simple_payload_constructor_pattern_key(switch_case.pattern, switch_subject_type);
                        if (constructor_key.has_value()) {
                            for (auto const& seen_constructor_key : seen_simple_payload_choice_patterns) {
                                if (payload_constructor_patterns_overlap(seen_constructor_key, *constructor_key)) {
                                    diagnostics_.error(
                                        switch_case.pattern.line,
                                        "switch constructor pattern '" + switch_case.pattern.left->text +
                                            "(...)' is duplicated"
                                    );
                                    valid_pattern = false;
                                    break;
                                }
                            }
                            seen_simple_payload_choice_patterns.push_back(*constructor_key);
                        }

                        auto fully_covered_variant = fully_covering_choice_constructor_pattern_variant_name(
                            switch_case.pattern,
                            switch_subject_type
                        );
                        if (fully_covered_variant.has_value() &&
                            choice_coverage.tracks_payload_bearing_variant(*fully_covered_variant)) {
                            choice_coverage.cover_payload_bearing_variant(*fully_covered_variant);
                        }
                    }
                }

                if (!switch_case.is_default && saw_value_pattern && saw_constructor_pattern) {
                    diagnostics_.error(
                        switch_case.pattern.line,
                        "switch cannot mix value patterns with constructor patterns"
                    );
                    case_results.push_back(baseline_scope);
                    has_default_case = has_default_case || switch_case.is_default;
                    continue;
                }

                valid_pattern =
                    analyze_switch_pattern(switch_case.pattern, in_async_function, switch_subject_type) && valid_pattern;
                restore_scope_stack(baseline_scope);
                push_scope();
                if (!switch_case.is_default && valid_pattern) {
                    std::unordered_set<std::string> bound_names;
                    valid_pattern =
                        declare_switch_pattern_bindings(switch_case.pattern, switch_subject_type, false, bound_names) &&
                        valid_pattern;
                }
                switch_patterns_valid = switch_patterns_valid && valid_pattern;
                if (valid_pattern) {
                    for (auto const& consequence : switch_case.statements) {
                        analyze_statement(*consequence, in_async_function);
                    }
                }
                pop_scope();
                case_results.push_back(snapshot_scope_stack());
                has_default_case = has_default_case || switch_case.is_default;
            }

            if (switch_patterns_valid && !has_default_case && !saw_value_pattern) {
                auto missing_variant = choice_coverage.first_missing_zero_payload_variant();
                if (missing_variant.has_value()) {
                    diagnostics_.error(
                        statement.line,
                        "switch is missing zero-payload choice variant '" + *missing_variant + "'"
                    );
                }
            }

            if (switch_patterns_valid && !has_default_case && !saw_value_pattern) {
                auto missing_variant = choice_coverage.first_missing_payload_bearing_variant();
                if (missing_variant.has_value()) {
                    diagnostics_.error(statement.line, "switch is missing choice variant '" + *missing_variant + "'");
                }
            }

            if (switch_patterns_valid && !has_default_case && !saw_constructor_pattern &&
                switch_subject_type_name == "Bool" && saw_true_value_pattern != saw_false_value_pattern) {
                diagnostics_.error(
                    statement.line,
                    std::string("switch is missing boolean value pattern '") +
                        (saw_true_value_pattern ? "false" : "true") + "'"
                );
            }

            if (!has_default_case) {
                case_results.push_back(baseline_scope);
            }

            restore_scope_stack(merge_scope_snapshots(case_results));
            return;
        }

        if (statement.kind == syntax::StatementKind::while_statement) {
            auto baseline_scope = snapshot_scope_stack();

            restore_scope_stack(baseline_scope);
            push_scope();
            ++loop_depth_;
            for (auto const& nested_statement : statement.nested_statements) {
                analyze_statement(nested_statement, in_async_function);
            }
            --loop_depth_;
            pop_scope();
            auto body_result = snapshot_scope_stack();

            restore_scope_stack(merge_scope_snapshots({baseline_scope, body_result}));
            return;
        }

        if (statement.kind == syntax::StatementKind::repeat_statement) {
            auto baseline_scope = snapshot_scope_stack();

            restore_scope_stack(baseline_scope);
            push_scope();
            ++loop_depth_;
            for (auto const& nested_statement : statement.nested_statements) {
                analyze_statement(nested_statement, in_async_function);
            }
            --loop_depth_;
            pop_scope();
            auto body_result = snapshot_scope_stack();

            restore_scope_stack(body_result);
            return;
        }

        push_scope();
        for (auto const& nested_statement : statement.nested_statements) {
            analyze_statement(nested_statement, in_async_function);
        }
        pop_scope();

        push_scope();
        for (auto const& alternate_statement : statement.alternate_statements) {
            analyze_statement(alternate_statement, in_async_function);
        }
        pop_scope();
    }

    void analyze_expression(
        syntax::ExpressionSyntax const& expression,
        bool in_async_function,
        bool allow_thread_value_name = false
    ) {
        if (expression.kind == syntax::ExpressionKind::call && expression.left &&
            expression.left->kind == syntax::ExpressionKind::name &&
            is_unsafe_intrinsic_name(expression.left->text) && !unsafe_context_active_) {
            diagnostics_.error(
                expression.line,
                "unsafe intrinsic '" + expression.left->text +
                    "' is only valid inside unsafe functions or unsafe blocks"
            );
        }

        if (is_pointer_constructor_call(expression) && !unsafe_context_active_) {
            diagnostics_.error(
                expression.line,
                "Pointer construction is only valid inside unsafe functions or unsafe blocks"
            );
        }

        validate_pointer_constructor_operands(expression);
        validate_index_access_operands(expression);

        if (is_declared_unsafe_call(expression) && !unsafe_context_active_) {
            auto diagnostic_subject = std::string {"unsafe function"};
            auto callee_name = std::string {};
            if (expression.left->kind == syntax::ExpressionKind::name) {
                callee_name = expression.left->text;
            } else {
                diagnostic_subject = "unsafe method";
                callee_name = expression.left->text;
            }

            diagnostics_.error(
                expression.line,
                "call to " + diagnostic_subject + " '" + callee_name +
                    "' is only valid inside unsafe functions or unsafe blocks"
            );
        }

        validate_unsafe_intrinsic_operands(expression);

        if (expression.kind == syntax::ExpressionKind::name) {
            if (expression.text == "this" && !receiver_context_active_) {
                diagnostics_.error(expression.line, "receiver 'this' is only valid inside implements or extend methods");
            }
            auto const* binding = find_binding(expression.text);
            if (!allow_thread_value_name && binding != nullptr && binding->value_origin == ValueOriginKind::thread) {
                diagnostics_.error(expression.line, "thread value '" + expression.text + "' must be consumed with .join()");
            }
            record_concurrency_capture(expression);
        }

        if (expression.kind == syntax::ExpressionKind::unary && expression.text == "await" && !in_async_function) {
            diagnostics_.error(expression.line, "await expression is only valid inside async functions");
        }

        if (expression.kind == syntax::ExpressionKind::unary && expression.text == "await" && expression.left) {
            validate_await_operand(*expression.left, expression.line);
        }

        if (expression.kind == syntax::ExpressionKind::task && !in_async_function) {
            diagnostics_.error(expression.line, "task expression is only valid inside async functions");
        }

        if (expression.kind == syntax::ExpressionKind::call && expression.left &&
            expression.left->kind == syntax::ExpressionKind::member_access && expression.left->text == "join" &&
            expression.left->left) {
            validate_join_receiver(*expression.left->left, expression.line);
            analyze_expression(*expression.left->left, in_async_function, true);
            for (auto const& argument : expression.arguments) {
                analyze_expression(argument, in_async_function);
            }
            return;
        }

        if (expression_requires_value_boundary(expression)) {
            auto const* final_statement =
                expression.nested_statements.empty() ? nullptr : expression.nested_statements.back().get();
            auto expression_kind = classify_concurrency_expression(expression);
            if (final_statement == nullptr ||
                (final_statement->kind != syntax::StatementKind::expression_statement &&
                 !is_value_return_statement(*final_statement))) {
                diagnostics_.error(
                    final_statement != nullptr ? final_statement->line : expression.line,
                    expression.text + " expression body must end with an expression statement or value return"
                );
            }

            auto saved_capture_depth = capture_scope_depth_;
            auto saved_capture_expression_kind = current_capture_expression_kind_;
            capture_scope_depth_ = scope_stack_.size();
            current_capture_expression_kind_ = expression_kind;

            push_scope();
            for (auto const& nested_statement : expression.nested_statements) {
                analyze_statement(*nested_statement, in_async_function);
            }

            if (final_statement != nullptr &&
                (final_statement->kind == syntax::StatementKind::expression_statement ||
                 is_value_return_statement(*final_statement))) {
                validate_concurrency_value_type(
                    final_statement->line,
                    infer_statement_value_type_name(*final_statement),
                    expression_kind,
                    "result"
                );
            }
            pop_scope();

            capture_scope_depth_ = saved_capture_depth;
            current_capture_expression_kind_ = saved_capture_expression_kind;
            return;
        }

        for (auto const& argument : expression.arguments) {
            analyze_expression(argument, in_async_function);
        }

        if (expression.left) {
            auto left_allows_thread_value_name =
                allow_thread_value_name || (expression.kind == syntax::ExpressionKind::unary && expression.text == "await");
            analyze_expression(*expression.left, in_async_function, left_allows_thread_value_name);
        }

        if (expression.right) {
            auto right_allows_thread_value_name =
                expression.kind == syntax::ExpressionKind::ternary;
            analyze_expression(*expression.right, in_async_function, right_allows_thread_value_name);
        }

        if (expression.alternate) {
            auto alternate_allows_thread_value_name =
                expression.kind == syntax::ExpressionKind::ternary;
            analyze_expression(*expression.alternate, in_async_function, alternate_allows_thread_value_name);
        }
    }

    void push_scope() {
        scope_stack_.emplace_back();
    }

    void pop_scope() {
        if (!scope_stack_.empty()) {
            scope_stack_.pop_back();
        }
    }

    void declare_binding(
        std::string const& name,
        std::string type_name,
        bool mutable_binding,
        bool receiver_binding = false,
        bool parameter_binding = false,
        ValueOriginKind value_origin = ValueOriginKind::none
    ) {
        if (scope_stack_.empty()) {
            push_scope();
        }

        scope_stack_.back().push_back(Binding {
            .name = name,
            .type_name = std::move(type_name),
            .value_origin = value_origin,
            .mutable_binding = mutable_binding,
            .receiver_binding = receiver_binding,
            .parameter_binding = parameter_binding,
            .scope_depth = scope_stack_.size() - 1,
        });
    }

    auto find_binding(std::string const& name) const -> Binding const* {
        for (auto scope_index = scope_stack_.rbegin(); scope_index != scope_stack_.rend(); ++scope_index) {
            for (auto binding = scope_index->rbegin(); binding != scope_index->rend(); ++binding) {
                if (binding->name == name) {
                    return &*binding;
                }
            }
        }
        for (auto const& binding : module_constant_bindings_) {
            if (binding.name == name) {
                return &binding;
            }
        }
        return nullptr;
    }

    void update_binding(
        std::string const& name,
        std::string type_name,
        ValueOriginKind value_origin
    ) {
        for (auto scope_index = scope_stack_.rbegin(); scope_index != scope_stack_.rend(); ++scope_index) {
            for (auto binding = scope_index->rbegin(); binding != scope_index->rend(); ++binding) {
                if (binding->name == name) {
                    if (!type_name.empty()) {
                        binding->type_name = std::move(type_name);
                    }
                    binding->value_origin = value_origin;
                    return;
                }
            }
        }
    }

    void record_concurrency_capture(syntax::ExpressionSyntax const& expression) {
        if (capture_scope_depth_ == no_capture_scope_depth) {
            return;
        }

        auto const* binding = find_binding(expression.text);
        if (binding == nullptr || binding->scope_depth >= capture_scope_depth_) {
            return;
        }
        if (binding->module_constant) {
            return;
        }

        auto capture_kind = ConcurrencyCaptureKind::immutable_outer_local;
        if (binding->receiver_binding) {
            capture_kind = ConcurrencyCaptureKind::receiver;
        } else if (binding->parameter_binding) {
            capture_kind = ConcurrencyCaptureKind::parameter;
        } else if (binding->mutable_binding) {
            capture_kind = ConcurrencyCaptureKind::mutable_outer_local;
        }

        concurrency_captures.push_back(ConcurrencyCapture {
            .line = expression.line,
            .name = expression.text,
            .type_name = binding->type_name,
            .expression_kind = current_capture_expression_kind_,
            .capture_kind = capture_kind,
        });

        if (capture_kind == ConcurrencyCaptureKind::receiver) {
            diagnostics_.error(expression.line, "concurrency expression cannot capture receiver 'this'");
            return;
        }

        if (capture_kind == ConcurrencyCaptureKind::mutable_outer_local) {
            diagnostics_.error(
                expression.line,
                "concurrency expression cannot capture mutable outer local '" + expression.text + "'"
            );
            return;
        }

        if ((capture_kind == ConcurrencyCaptureKind::parameter ||
             capture_kind == ConcurrencyCaptureKind::immutable_outer_local) &&
            !binding->type_name.empty()) {
            if (current_capture_expression_kind_ == ConcurrencyExpressionKind::thread &&
                !is_obviously_safe_capture_type(binding->type_name) &&
                !has_allowed_concurrency_marker(binding->type_name, current_capture_expression_kind_)) {
                diagnostics_.error(
                    expression.line,
                    "thread capture '" + expression.text + "' of type '" + binding->type_name +
                        "' requires future Transferable analysis"
                );
                return;
            }

            if (current_capture_expression_kind_ == ConcurrencyExpressionKind::task &&
                !is_obviously_safe_capture_type(binding->type_name) &&
                !has_allowed_concurrency_marker(binding->type_name, current_capture_expression_kind_)) {
                diagnostics_.error(
                    expression.line,
                    "task capture '" + expression.text + "' of type '" + binding->type_name +
                        "' requires future Transferable/Shareable analysis"
                );
                return;
            }
        }
    }

    syntax::ModuleSyntax const& module_;
    diagnostics::DiagnosticBag diagnostics_;
    std::vector<ConcurrencyCapture> concurrency_captures;
    std::vector<std::string> async_callable_names_;
    std::vector<AsyncMethodSignature> async_method_signatures_;
    std::vector<std::string> unsafe_callable_names_;
    std::vector<UnsafeMethodSignature> unsafe_method_signatures_;
    std::vector<CallableSignature> callable_signatures_;
    std::vector<MethodReturnSignature> method_return_signatures_;
    std::vector<RecordFieldSignature> record_field_signatures_;
    std::unordered_map<std::string, std::size_t> choice_variant_arities_;
    std::vector<ChoiceVariantSignature> choice_variant_signatures_;
    std::vector<std::string> transferable_constraint_types_;
    std::vector<std::string> shareable_constraint_types_;
    std::vector<std::string> transferable_impl_types_;
    std::vector<std::string> shareable_impl_types_;
    std::vector<Binding> module_constant_bindings_;
    std::vector<std::vector<Binding>> scope_stack_;
    std::size_t loop_depth_ = 0;
    bool receiver_context_active_ = false;
    bool unsafe_context_active_ = false;
    bool current_function_returns_pointer_ = false;
    bool current_function_returns_address_ = false;
    std::string current_function_return_type_name_;
    std::string expected_pointer_type_name_;
    static constexpr std::size_t no_capture_scope_depth = static_cast<std::size_t>(-1);
    std::size_t capture_scope_depth_ = no_capture_scope_depth;
    ConcurrencyExpressionKind current_capture_expression_kind_ = ConcurrencyExpressionKind::task;
};

}  // namespace

auto ModuleSemanticAnalyzer::analyze(syntax::ModuleSyntax const& module) const -> SemanticAnalysisResult {
    return Analyzer(module).analyze();
}

}  // namespace orison::semantics
