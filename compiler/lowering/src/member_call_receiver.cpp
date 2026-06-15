#include "orison/lowering/member_call_receiver.hpp"

#include <cstddef>

namespace orison::lowering {

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
    FunctionLoweringState const& state
) -> MemberCallReceiverInference {
    if (call_expression.kind != syntax::ExpressionKind::call || call_expression.left == nullptr ||
        (call_expression.left->kind != syntax::ExpressionKind::member_access &&
         call_expression.left->kind != syntax::ExpressionKind::null_safe_member_access) ||
        call_expression.left->left == nullptr ||
        call_expression.left->left->kind != syntax::ExpressionKind::name) {
        return {};
    }

    auto const& receiver_name = call_expression.left->left->text;
    auto receiver_type = state.source_type_names.find(receiver_name);
    if (receiver_type == state.source_type_names.end()) {
        return MemberCallReceiverInference {
            .result = MemberCallReceiverInferenceResult::not_found,
            .method_name = call_expression.left->text,
        };
    }

    return MemberCallReceiverInference {
        .result = MemberCallReceiverInferenceResult::found,
        .receiver_type_name = receiver_type->second,
        .method_name = call_expression.left->text,
    };
}

}  // namespace orison::lowering
