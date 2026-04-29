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
    std::string receiver_type;
    std::string method_name;
};

struct UnsafeMethodSignature {
    std::string receiver_type;
    std::string method_name;
};

class Analyzer {
public:
    explicit Analyzer(syntax::ModuleSyntax const& module) : module_(module) {}

    auto analyze() -> SemanticAnalysisResult {
        collect_async_callable_names();
        collect_unsafe_callable_names();
        collect_concurrency_marker_implementations();
        collect_choice_variant_arities();

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
        std::size_t scope_depth = 0;
    };

    using ScopeSnapshot = std::vector<std::vector<Binding>>;

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
        for (auto const& signature : async_method_signatures_) {
            if (signature.receiver_type == receiver_type && signature.method_name == method_name) {
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
        for (auto const& signature : unsafe_method_signatures_) {
            if (signature.receiver_type == receiver_type && signature.method_name == method_name) {
                return true;
            }
        }
        return false;
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
        case syntax::ExpressionKind::string_literal:
            return "Text";
        case syntax::ExpressionKind::boolean_literal:
            return "Bool";
        case syntax::ExpressionKind::integer_literal:
            return "Int64";
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

    void validate_pointer_constructor_operands(syntax::ExpressionSyntax const& expression) {
        if (!is_pointer_constructor_call(expression)) {
            return;
        }

        if (expression.arguments.size() != 1) {
            diagnostics_.error(
                expression.line,
                "Pointer construction currently requires exactly one source argument"
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

        if (expression.arguments.empty() ||
            !is_structurally_address_like_expression(expression.arguments.front())) {
            diagnostics_.error(
                expression.line,
                intrinsic_name + " currently requires an address-like first argument"
            );
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

    void validate_switch_pattern_arity(syntax::ExpressionSyntax const& pattern) {
        if (pattern.kind == syntax::ExpressionKind::call) {
            if (!pattern.left || pattern.left->kind != syntax::ExpressionKind::name) {
                return;
            }

            auto variant = choice_variant_arities_.find(pattern.left->text);
            if (variant != choice_variant_arities_.end() && variant->second != pattern.arguments.size()) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.left->text + "' expects " +
                        std::to_string(variant->second) + " payload value" +
                        (variant->second == 1 ? "" : "s") + " but received " +
                        std::to_string(pattern.arguments.size())
                );
            }
            return;
        }

        if (pattern.kind == syntax::ExpressionKind::name) {
            auto variant = choice_variant_arities_.find(pattern.text);
            if (variant != choice_variant_arities_.end() && variant->second != 0) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.text + "' expects " +
                        std::to_string(variant->second) + " payload value" +
                        (variant->second == 1 ? "" : "s") + " but received 0"
                );
            }
        }
    }

    auto validate_switch_pattern_head(syntax::ExpressionSyntax const& pattern) -> bool {
        if (pattern.kind == syntax::ExpressionKind::call) {
            if (!pattern.left || pattern.left->kind != syntax::ExpressionKind::name) {
                return true;
            }

            if (!choice_variant_arities_.contains(pattern.left->text) &&
                !is_builtin_constructor_name(pattern.left->text)) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.left->text + "' does not match any declared choice variant"
                );
                return false;
            }
            return true;
        }

        if (pattern.kind == syntax::ExpressionKind::name && !pattern.text.empty()) {
            if (!choice_variant_arities_.contains(pattern.text) &&
                !is_builtin_constructor_name(pattern.text) &&
                pattern.text != "default") {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern '" + pattern.text + "' does not match any declared choice variant"
                );
                return false;
            }
        }

        return true;
    }

    auto analyze_switch_pattern(
        syntax::ExpressionSyntax const& pattern,
        bool in_async_function,
        bool in_constructor_payload = false
    ) -> bool {
        if (!in_constructor_payload) {
            if (!validate_switch_pattern_head(pattern)) {
                return false;
            }
            validate_switch_pattern_arity(pattern);
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
            for (auto const& argument : pattern.arguments) {
                if (argument.kind == syntax::ExpressionKind::name || is_literal_switch_subpattern(argument) ||
                    argument.kind == syntax::ExpressionKind::call) {
                    valid = analyze_switch_pattern(argument, in_async_function, true) && valid;
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

        if (is_literal_switch_subpattern(pattern)) {
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
        return true;
    }

    void declare_switch_pattern_bindings(
        syntax::ExpressionSyntax const& pattern,
        bool bind_name,
        std::unordered_set<std::string>& bound_names
    ) {
        if (pattern.kind == syntax::ExpressionKind::call) {
            for (auto const& argument : pattern.arguments) {
                declare_switch_pattern_bindings(argument, true, bound_names);
            }
            return;
        }

        if (bind_name && pattern.kind == syntax::ExpressionKind::name && !pattern.text.empty()) {
            if (!bound_names.insert(pattern.text).second) {
                diagnostics_.error(
                    pattern.line,
                    "switch constructor pattern cannot bind '" + pattern.text + "' more than once"
                );
                return;
            }
            declare_binding(pattern.text, {}, false);
        }
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

    void collect_choice_variant_arities() {
        choice_variant_arities_.clear();
        for (auto const& choice : module_.choices) {
            for (auto const& variant : choice.variants) {
                choice_variant_arities_[variant.name] = variant.payloads.size();
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
            auto receiver_type_name = render_type_name(implementation.receiver_type);
            for (auto const& method : implementation.methods) {
                if (method.is_async) {
                    async_method_signatures_.push_back(AsyncMethodSignature {
                        .receiver_type = receiver_type_name,
                        .method_name = method.name,
                    });
                }
            }
        }

        for (auto const& extension : module_.extensions) {
            auto receiver_type_name = render_type_name(extension.receiver_type);
            for (auto const& method : extension.methods) {
                if (method.is_async) {
                    async_method_signatures_.push_back(AsyncMethodSignature {
                        .receiver_type = receiver_type_name,
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
            auto receiver_type_name = render_type_name(implementation.receiver_type);
            for (auto const& method : implementation.methods) {
                if (method.is_unsafe) {
                    unsafe_method_signatures_.push_back(UnsafeMethodSignature {
                        .receiver_type = receiver_type_name,
                        .method_name = method.name,
                    });
                }
            }
        }

        for (auto const& extension : module_.extensions) {
            auto receiver_type_name = render_type_name(extension.receiver_type);
            for (auto const& method : extension.methods) {
                if (method.is_unsafe) {
                    unsafe_method_signatures_.push_back(UnsafeMethodSignature {
                        .receiver_type = receiver_type_name,
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

    void analyze_function(
        syntax::FunctionSyntax const& function,
        std::string receiver_type_name = {}
    ) {
        auto saved_receiver_context_active = receiver_context_active_;
        auto saved_unsafe_context_active = unsafe_context_active_;
        receiver_context_active_ = !receiver_type_name.empty();
        unsafe_context_active_ = function.is_unsafe;

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
    }

    void analyze_statement(syntax::StatementSyntax const& statement, bool in_async_function) {
        if (statement.kind == syntax::StatementKind::let_binding || statement.kind == syntax::StatementKind::var_binding) {
            analyze_expression(statement.expression, in_async_function);
            auto type_name = !statement.annotated_type.name.empty()
                                 ? render_type_name(statement.annotated_type)
                                 : infer_expression_type_name(statement.expression);
            if (!statement.annotated_type.name.empty()) {
                validate_receiver_type_usage(statement.annotated_type, statement.line);
            }
            declare_binding(
                statement.name,
                type_name,
                statement.kind == syntax::StatementKind::var_binding,
                false,
                false,
                infer_expression_value_origin(statement.expression)
            );
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
            analyze_expression(statement.expression, in_async_function, true);
            validate_return_expression(statement.expression, statement.line);
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

                    saw_semantic_default = true;
                }

                if (!switch_case.is_default) {
                    auto pattern_kind = classify_switch_pattern_kind(switch_case.pattern);
                    if (pattern_kind == SwitchPatternKind::value) {
                        saw_value_pattern = true;
                    } else if (pattern_kind == SwitchPatternKind::constructor) {
                        saw_constructor_pattern = true;
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

                valid_pattern = analyze_switch_pattern(switch_case.pattern, in_async_function) && valid_pattern;
                restore_scope_stack(baseline_scope);
                push_scope();
                if (!switch_case.is_default && valid_pattern) {
                    std::unordered_set<std::string> bound_names;
                    declare_switch_pattern_bindings(switch_case.pattern, false, bound_names);
                }
                if (valid_pattern) {
                    for (auto const& consequence : switch_case.statements) {
                        analyze_statement(*consequence, in_async_function);
                    }
                }
                pop_scope();
                case_results.push_back(snapshot_scope_stack());
                has_default_case = has_default_case || switch_case.is_default;
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
    std::unordered_map<std::string, std::size_t> choice_variant_arities_;
    std::vector<std::string> transferable_constraint_types_;
    std::vector<std::string> shareable_constraint_types_;
    std::vector<std::string> transferable_impl_types_;
    std::vector<std::string> shareable_impl_types_;
    std::vector<std::vector<Binding>> scope_stack_;
    std::size_t loop_depth_ = 0;
    bool receiver_context_active_ = false;
    bool unsafe_context_active_ = false;
    static constexpr std::size_t no_capture_scope_depth = static_cast<std::size_t>(-1);
    std::size_t capture_scope_depth_ = no_capture_scope_depth;
    ConcurrencyExpressionKind current_capture_expression_kind_ = ConcurrencyExpressionKind::task;
};

}  // namespace

auto ModuleSemanticAnalyzer::analyze(syntax::ModuleSyntax const& module) const -> SemanticAnalysisResult {
    return Analyzer(module).analyze();
}

}  // namespace orison::semantics
