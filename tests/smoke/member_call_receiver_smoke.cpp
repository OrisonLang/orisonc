#include "orison/lowering/member_call_receiver.hpp"

#include <cassert>
#include <memory>
#include <utility>

namespace {

auto member_call(
    orison::syntax::ExpressionKind member_kind = orison::syntax::ExpressionKind::member_access,
    orison::syntax::ExpressionSyntax receiver = [] {
        auto receiver = orison::syntax::ExpressionSyntax {};
        receiver.kind = orison::syntax::ExpressionKind::name;
        receiver.text = "device";
        return receiver;
    }()
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = member_kind;
    expression.left->text = "reset";
    expression.left->left = std::make_unique<orison::syntax::ExpressionSyntax>();
    *expression.left->left = std::move(receiver);
    return expression;
}

auto member_receiver() -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::member_access;
    expression.text = "device";
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = orison::syntax::ExpressionKind::name;
    expression.left->text = "slot";
    return expression;
}

auto indexed_member_receiver() -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::index_access;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = orison::syntax::ExpressionKind::member_access;
    expression.left->text = "devices";
    expression.left->left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->left->kind = orison::syntax::ExpressionKind::name;
    expression.left->left->text = "rack";
    auto index = orison::syntax::ExpressionSyntax {};
    index.kind = orison::syntax::ExpressionKind::integer_literal;
    index.text = "0";
    expression.arguments.push_back(std::move(index));
    return expression;
}

auto function_call_receiver() -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = orison::syntax::ExpressionKind::name;
    expression.left->text = "make_device";
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
    state.source_type_names["slot"] = "Slot";
    state.source_type_names["rack"] = "Rack";

    auto context = orison::lowering::LoweringContext {};
    context.records.emplace("Device", orison::lowering::LoweredRecordLayout {
        .name = "Device",
        .llvm_type_name = "%record.Device",
    });
    context.records.emplace("Slot", orison::lowering::LoweredRecordLayout {
        .name = "Slot",
        .llvm_type_name = "%record.Slot",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "device",
                .source_type_name = "Device",
                .llvm_type = "%record.Device",
                .index = 0,
            },
        },
    });
    context.records.emplace("Rack", orison::lowering::LoweredRecordLayout {
        .name = "Rack",
        .llvm_type_name = "%record.Rack",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "devices",
                .source_type_name = "Array<Device, 2>",
                .llvm_type = "[2 x %record.Device]",
                .index = 0,
            },
        },
    });
    context.functions.emplace("make_device", orison::lowering::LoweredFunctionSignature {
        .return_type = "%record.Device",
        .return_signedness = orison::lowering::IntegerSignedness::not_integer,
        .symbol_name = "make_device",
    });

    auto direct = orison::lowering::infer_member_call_receiver(member_call(), context, state);
    assert(direct.result == orison::lowering::MemberCallReceiverInferenceResult::found);
    assert(direct.receiver_type_name == "Device");
    assert(direct.method_name == "reset");

    auto null_safe = orison::lowering::infer_member_call_receiver(
        member_call(orison::syntax::ExpressionKind::null_safe_member_access),
        context,
        state
    );
    assert(null_safe.result == orison::lowering::MemberCallReceiverInferenceResult::found);
    assert(null_safe.receiver_type_name == "Device");
    assert(null_safe.method_name == "reset");

    auto member = orison::lowering::infer_member_call_receiver(
        member_call(orison::syntax::ExpressionKind::member_access, member_receiver()),
        context,
        state
    );
    assert(member.result == orison::lowering::MemberCallReceiverInferenceResult::found);
    assert(member.receiver_type_name == "Device");
    assert(member.method_name == "reset");

    auto indexed_member = orison::lowering::infer_member_call_receiver(
        member_call(orison::syntax::ExpressionKind::member_access, indexed_member_receiver()),
        context,
        state
    );
    assert(indexed_member.result == orison::lowering::MemberCallReceiverInferenceResult::found);
    assert(indexed_member.receiver_type_name == "Device");
    assert(indexed_member.method_name == "reset");

    auto function_call = orison::lowering::infer_member_call_receiver(
        member_call(orison::syntax::ExpressionKind::member_access, function_call_receiver()),
        context,
        state
    );
    assert(function_call.result == orison::lowering::MemberCallReceiverInferenceResult::found);
    assert(function_call.receiver_type_name == "Device");
    assert(function_call.method_name == "reset");

    auto missing = orison::lowering::infer_member_call_receiver(member_call(), context, {});
    assert(missing.result == orison::lowering::MemberCallReceiverInferenceResult::not_found);
    assert(missing.receiver_type_name.empty());
    assert(missing.method_name == "reset");

    auto unsupported = orison::syntax::ExpressionSyntax {};
    unsupported.kind = orison::syntax::ExpressionKind::call;
    unsupported.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    unsupported.left->kind = orison::syntax::ExpressionKind::name;
    unsupported.left->text = "reset";
    auto unsupported_result = orison::lowering::infer_member_call_receiver(unsupported, context, state);
    assert(unsupported_result.result == orison::lowering::MemberCallReceiverInferenceResult::unsupported_shape);
    assert(unsupported_result.method_name.empty());
    return 0;
}
