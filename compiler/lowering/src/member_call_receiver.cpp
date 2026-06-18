#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <cstddef>
#include <utility>

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
