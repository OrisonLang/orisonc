#include "orison/lowering/member_call_receiver.hpp"

#include <cassert>
#include <memory>

namespace {

auto member_call(
    orison::syntax::ExpressionKind member_kind = orison::syntax::ExpressionKind::member_access
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = member_kind;
    expression.left->text = "reset";
    expression.left->left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->left->kind = orison::syntax::ExpressionKind::name;
    expression.left->left->text = "device";
    return expression;
}

}  // namespace

int main() {
    auto type = orison::syntax::TypeSyntax {
        .name = "Box",
        .generic_arguments = {orison::syntax::TypeSyntax {.name = "UInt32"}},
    };
    assert(orison::lowering::render_source_type_name(type) == "Box<UInt32>");

    auto state = orison::lowering::FunctionLoweringState {};
    state.source_type_names["device"] = "Device";

    auto direct = orison::lowering::infer_member_call_receiver(member_call(), state);
    assert(direct.result == orison::lowering::MemberCallReceiverInferenceResult::found);
    assert(direct.receiver_type_name == "Device");
    assert(direct.method_name == "reset");

    auto null_safe = orison::lowering::infer_member_call_receiver(
        member_call(orison::syntax::ExpressionKind::null_safe_member_access),
        state
    );
    assert(null_safe.result == orison::lowering::MemberCallReceiverInferenceResult::found);
    assert(null_safe.receiver_type_name == "Device");
    assert(null_safe.method_name == "reset");

    auto missing = orison::lowering::infer_member_call_receiver(member_call(), {});
    assert(missing.result == orison::lowering::MemberCallReceiverInferenceResult::not_found);
    assert(missing.receiver_type_name.empty());
    assert(missing.method_name == "reset");

    auto unsupported = orison::syntax::ExpressionSyntax {};
    unsupported.kind = orison::syntax::ExpressionKind::call;
    unsupported.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    unsupported.left->kind = orison::syntax::ExpressionKind::name;
    unsupported.left->text = "reset";
    auto unsupported_result = orison::lowering::infer_member_call_receiver(unsupported, state);
    assert(unsupported_result.result == orison::lowering::MemberCallReceiverInferenceResult::unsupported_shape);
    assert(unsupported_result.method_name.empty());
    return 0;
}
