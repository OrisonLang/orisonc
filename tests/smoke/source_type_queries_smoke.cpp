#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/lowering_options.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

auto name(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::name;
    expression.text = std::move(text);
    return expression;
}

auto member(orison::syntax::ExpressionSyntax left, std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::member_access;
    expression.text = std::move(text);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    return expression;
}

auto null_safe_member(orison::syntax::ExpressionSyntax left, std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::null_safe_member_access;
    expression.text = std::move(text);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    return expression;
}

auto index(orison::syntax::ExpressionSyntax left) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::index_access;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    auto zero = orison::syntax::ExpressionSyntax {};
    zero.kind = orison::syntax::ExpressionKind::integer_literal;
    zero.text = "0";
    expression.arguments.push_back(std::move(zero));
    return expression;
}

auto call(std::string function_name) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(name(std::move(function_name)));
    return expression;
}

auto call(std::string function_name, orison::syntax::ExpressionSyntax argument)
    -> orison::syntax::ExpressionSyntax {
    auto expression = call(std::move(function_name));
    expression.arguments.push_back(std::move(argument));
    return expression;
}

auto call(
    std::string function_name,
    orison::syntax::ExpressionSyntax first_argument,
    orison::syntax::ExpressionSyntax second_argument
) -> orison::syntax::ExpressionSyntax {
    auto expression = call(std::move(function_name));
    expression.arguments.push_back(std::move(first_argument));
    expression.arguments.push_back(std::move(second_argument));
    return expression;
}

auto cast(orison::syntax::ExpressionSyntax operand, std::string type_name) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::cast;
    expression.text = std::move(type_name);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(operand));
    return expression;
}

auto integer_literal(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::integer_literal;
    expression.text = std::move(text);
    return expression;
}

auto record_constructor(std::string record_name) -> orison::syntax::ExpressionSyntax {
    auto expression = call(std::move(record_name));
    expression.arguments.push_back(cast(integer_literal("1"), "UInt32"));
    return expression;
}

auto array_literal(std::vector<orison::syntax::ExpressionSyntax> elements)
    -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::array_literal;
    expression.arguments = std::move(elements);
    return expression;
}

auto array_literal(
    orison::syntax::ExpressionSyntax first,
    orison::syntax::ExpressionSyntax second
) -> orison::syntax::ExpressionSyntax {
    auto elements = std::vector<orison::syntax::ExpressionSyntax> {};
    elements.push_back(std::move(first));
    elements.push_back(std::move(second));
    return array_literal(std::move(elements));
}

auto expression_statement(orison::syntax::ExpressionSyntax expression) -> orison::syntax::StatementSyntax {
    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::expression_statement;
    statement.expression = std::move(expression);
    return statement;
}

auto let_statement(std::string name, orison::syntax::ExpressionSyntax expression)
    -> orison::syntax::StatementSyntax {
    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::let_binding;
    statement.name = std::move(name);
    statement.expression = std::move(expression);
    return statement;
}

auto two_statement_block(
    orison::syntax::StatementSyntax first,
    orison::syntax::StatementSyntax second
) -> std::vector<orison::syntax::StatementSyntax> {
    auto statements = std::vector<orison::syntax::StatementSyntax> {};
    statements.push_back(std::move(first));
    statements.push_back(std::move(second));
    return statements;
}

auto one_statement_block(orison::syntax::StatementSyntax statement)
    -> std::vector<orison::syntax::StatementSyntax> {
    auto statements = std::vector<orison::syntax::StatementSyntax> {};
    statements.push_back(std::move(statement));
    return statements;
}

auto if_statement(
    std::vector<orison::syntax::StatementSyntax> then_statements,
    std::vector<orison::syntax::StatementSyntax> else_statements
) -> orison::syntax::StatementSyntax {
    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::if_statement;
    statement.expression = name("flag");
    statement.nested_statements = std::move(then_statements);
    statement.alternate_statements = std::move(else_statements);
    return statement;
}

auto switch_statement(
    std::vector<orison::syntax::StatementSyntax> first_case,
    std::vector<orison::syntax::StatementSyntax> second_case
) -> orison::syntax::StatementSyntax {
    auto first_case_pointers = std::vector<std::unique_ptr<orison::syntax::StatementSyntax>> {};
    first_case_pointers.reserve(first_case.size());
    for (auto& case_statement : first_case) {
        first_case_pointers.push_back(
            std::make_unique<orison::syntax::StatementSyntax>(std::move(case_statement))
        );
    }

    auto second_case_pointers = std::vector<std::unique_ptr<orison::syntax::StatementSyntax>> {};
    second_case_pointers.reserve(second_case.size());
    for (auto& case_statement : second_case) {
        second_case_pointers.push_back(
            std::make_unique<orison::syntax::StatementSyntax>(std::move(case_statement))
        );
    }

    auto statement = orison::syntax::StatementSyntax {};
    statement.kind = orison::syntax::StatementKind::switch_statement;
    statement.expression = name("selector");
    statement.switch_cases.push_back(orison::syntax::SwitchCaseSyntax {
        .is_default = false,
        .pattern = integer_literal("0"),
        .statements = std::move(first_case_pointers),
    });
    statement.switch_cases.push_back(orison::syntax::SwitchCaseSyntax {
        .is_default = true,
        .pattern = {},
        .statements = std::move(second_case_pointers),
    });
    return statement;
}

auto method_call(orison::syntax::ExpressionSyntax receiver, std::string method_name)
    -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = orison::syntax::ExpressionKind::member_access;
    expression.left->text = std::move(method_name);
    expression.left->left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(receiver));
    return expression;
}

auto null_safe_method_call(
    orison::syntax::ExpressionSyntax receiver,
    std::string method_name,
    orison::syntax::ExpressionSyntax argument
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    expression.left->kind = orison::syntax::ExpressionKind::null_safe_member_access;
    expression.left->text = std::move(method_name);
    expression.left->left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(receiver));
    expression.arguments.push_back(std::move(argument));
    return expression;
}

auto ternary(
    orison::syntax::ExpressionSyntax condition,
    orison::syntax::ExpressionSyntax then_expression,
    orison::syntax::ExpressionSyntax else_expression
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::ternary;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(condition));
    expression.right = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(then_expression));
    expression.alternate = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(else_expression));
    return expression;
}

}  // namespace

int main() {
    auto computed_cleanup_insertion_options = orison::lowering::LlvmIrEmissionOptions {};
    assert(orison::lowering::computed_dynamic_array_cleanup_call_insertion_capability(
        computed_cleanup_insertion_options
    ).enabled);
    computed_cleanup_insertion_options.enable_computed_dynamic_array_local_cleanup_call_insertion = false;
    assert(!orison::lowering::computed_dynamic_array_cleanup_call_insertion_capability(
        computed_cleanup_insertion_options
    ).enabled);
    computed_cleanup_insertion_options.fixture_authorize_computed_dynamic_array_cleanup_calls = true;
    assert(!orison::lowering::computed_dynamic_array_cleanup_call_insertion_capability(
        computed_cleanup_insertion_options
    ).enabled);
    computed_cleanup_insertion_options.fixture_insert_computed_dynamic_array_cleanup_calls = true;
    auto const computed_cleanup_insertion_capability =
        orison::lowering::computed_dynamic_array_cleanup_call_insertion_capability(
            computed_cleanup_insertion_options
        );
    assert(computed_cleanup_insertion_capability.cleanup_call_authorization_enabled);
    assert(computed_cleanup_insertion_capability.cleanup_call_insertion_enabled);
    assert(computed_cleanup_insertion_capability.enabled);

    auto context = orison::lowering::LoweringContext {};
    context.records.emplace("Bucket", orison::lowering::LoweredRecordLayout {
        .name = "Bucket",
        .llvm_type_name = "%record.Bucket",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "values",
                .source_type_name = "Array<UInt32, 3>",
                .llvm_type = "[3 x i32]",
                .index = 0,
            },
        },
    });
    context.records.emplace("Wrapper", orison::lowering::LoweredRecordLayout {
        .name = "Wrapper",
        .llvm_type_name = "%record.Wrapper",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "bucket",
                .source_type_name = "Bucket",
                .llvm_type = "%record.Bucket",
                .index = 0,
            },
        },
    });
    context.functions.emplace("make_bucket", orison::lowering::LoweredFunctionSignature {
        .return_type = "%record.Bucket",
        .return_signedness = orison::lowering::IntegerSignedness::not_integer,
        .symbol_name = "make_bucket",
    });
    context.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Bucket",
        .method_name = "view",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "[3 x i32]",
            .return_signedness = orison::lowering::IntegerSignedness::not_integer,
            .parameter_types = {"%record.Bucket"},
            .parameter_signedness = {orison::lowering::IntegerSignedness::not_integer},
            .symbol_name = "method.Bucket.view",
        },
    });
    context.records.emplace("Box<UInt32>", orison::lowering::LoweredRecordLayout {
        .name = "Box<UInt32>",
        .llvm_type_name = "%record.Box_UInt32_",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "value",
                .source_type_name = "UInt32",
                .llvm_type = "i32",
                .index = 0,
            },
        },
    });
    context.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Box<UInt32>",
        .method_name = "bump",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "%record.Box_UInt32_",
            .source_return_type_name = "Box<UInt32>",
            .return_signedness = orison::lowering::IntegerSignedness::not_integer,
            .parameter_types = {"%record.Box_UInt32_", "i32"},
            .parameter_source_type_names = {"shared.Box<UInt32>", "UInt32"},
            .parameter_signedness = {
                orison::lowering::IntegerSignedness::not_integer,
                orison::lowering::IntegerSignedness::unsigned_integer,
            },
            .symbol_name = "method.Box_UInt32_.bump",
        },
    });
    context.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Box<UInt32>",
        .method_name = "pair",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "[2 x %record.Box_UInt32_]",
            .source_return_type_name = "Array<Box<UInt32>, 2>",
            .return_signedness = orison::lowering::IntegerSignedness::not_integer,
            .parameter_types = {"%record.Box_UInt32_", "i32"},
            .parameter_source_type_names = {"shared.Box<UInt32>", "UInt32"},
            .parameter_signedness = {
                orison::lowering::IntegerSignedness::not_integer,
                orison::lowering::IntegerSignedness::unsigned_integer,
            },
            .symbol_name = "method.Box_UInt32_.pair",
        },
    });

    auto state = orison::lowering::FunctionLoweringState {};
    state.source_type_names["wrapper"] = "Wrapper";
    state.source_type_names["buckets"] = "Array<Bucket, 2>";
    state.source_type_names["bucket_array_pointer"] = "Pointer<Array<Bucket, 2>>";
    state.source_type_names["wrapper_pointer"] = "Pointer<Wrapper>";
    state.source_type_names["left_values"] = "Array<UInt32, 3>";
    state.source_type_names["right_values"] = "Array<UInt32, 3>";
    state.source_type_names["box"] = "Maybe<Box<UInt32>>";

    assert(orison::lowering::split_top_level_generic_arguments("Bucket, Array<UInt32, 3>").size() == 2);
    assert(orison::lowering::array_element_source_type_name("Array<Bucket, 2>") == "Bucket");
    assert(orison::lowering::dynamic_array_element_source_type_name("DynamicArray<Array<UInt32, 3>>") == "Array<UInt32, 3>");
    assert(!orison::lowering::dynamic_array_element_source_type_name("Array<UInt32, 3>").has_value());
    assert(orison::lowering::view_element_source_type_name("View<UInt32>") == "UInt32");
    assert(orison::lowering::view_element_source_type_name("shared.View<Array<UInt32, 3>>") == "Array<UInt32, 3>");
    assert(orison::lowering::view_element_source_type_name("exclusive.View<Bucket>") == "Bucket");
    assert(orison::lowering::pointer_pointee_source_type_name("Pointer<UInt32>") == "UInt32");
    assert(orison::lowering::maybe_payload_source_type_name("Maybe<Array<UInt32, 3>>") == "Array<UInt32, 3>");
    assert(!orison::lowering::maybe_payload_source_type_name("Array<UInt32, 3>").has_value());

    auto dynamic_array_sequence = orison::lowering::dynamic_sequence_source_type("DynamicArray<UInt32>");
    assert(dynamic_array_sequence.has_value());
    assert(dynamic_array_sequence->kind == orison::lowering::DynamicSequenceKind::dynamic_array);
    assert(dynamic_array_sequence->element_source_type_name == "UInt32");
    assert(dynamic_array_sequence->owns_storage);
    assert(dynamic_array_sequence->permits_element_mutation);
    assert(orison::lowering::dynamic_array_descriptor_llvm_type() == "{ ptr, i64, i64 }");
    auto dynamic_array_invariants = orison::lowering::dynamic_array_lowering_invariants();
    assert(dynamic_array_invariants.descriptor_llvm_type == orison::lowering::dynamic_array_descriptor_llvm_type());
    assert(dynamic_array_invariants.unique_owner_required);
    assert(dynamic_array_invariants.allocator_required);
    assert(dynamic_array_invariants.length_capacity_invariant_required);
    assert(dynamic_array_invariants.element_drop_walk_required);
    assert(!dynamic_array_invariants.lowered_signatures_enabled);

    auto shared_view_sequence = orison::lowering::dynamic_sequence_source_type("shared.View<Byte>");
    assert(shared_view_sequence.has_value());
    assert(shared_view_sequence->kind == orison::lowering::DynamicSequenceKind::shared_view);
    assert(shared_view_sequence->element_source_type_name == "Byte");
    assert(!shared_view_sequence->owns_storage);
    assert(!shared_view_sequence->permits_element_mutation);

    auto plain_view_sequence = orison::lowering::dynamic_sequence_source_type("View<Byte>");
    assert(plain_view_sequence.has_value());
    assert(plain_view_sequence->kind == orison::lowering::DynamicSequenceKind::view);
    assert(plain_view_sequence->element_source_type_name == "Byte");
    assert(!plain_view_sequence->owns_storage);
    assert(!plain_view_sequence->permits_element_mutation);

    auto exclusive_view_sequence = orison::lowering::dynamic_sequence_source_type("exclusive.View<Bucket>");
    assert(exclusive_view_sequence.has_value());
    assert(exclusive_view_sequence->kind == orison::lowering::DynamicSequenceKind::exclusive_view);
    assert(exclusive_view_sequence->element_source_type_name == "Bucket");
    assert(!exclusive_view_sequence->owns_storage);
    assert(exclusive_view_sequence->permits_element_mutation);
    assert(!orison::lowering::dynamic_sequence_source_type("Array<UInt32, 3>").has_value());

    state.source_type_names["items"] = "DynamicArray<UInt32>";
    state.source_type_names["predicted_items"] = "DynamicArray<UInt32>";
    state.source_type_names["computed_left"] = "DynamicArray<UInt32>";
    state.source_type_names["computed_right"] = "DynamicArray<UInt32>";
    state.addressable_bindings["items"] = orison::lowering::AddressableBinding {
        .type = orison::lowering::LoweredType {
            .type = std::string {orison::lowering::dynamic_array_descriptor_llvm_type()},
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        .storage = "%items.addr",
    };
    state.addressable_bindings["predicted_items"] = orison::lowering::AddressableBinding {
        .type = orison::lowering::LoweredType {
            .type = std::string {orison::lowering::dynamic_array_descriptor_llvm_type()},
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        .storage = "%predicted_items.addr",
    };
    state.dynamic_array_local_cleanup_plans.push_back(orison::lowering::DynamicArrayDescriptorCleanupPlan {
        .owner_name = "items",
        .source_type_name = "DynamicArray<UInt32>",
        .element_source_type_name = "UInt32",
        .element_llvm_type = "i32",
        .descriptor_storage_name = "%items.addr",
        .descriptor_storage_status =
            orison::lowering::DynamicArrayDescriptorStorageStatus::lowered_local_descriptor,
        .element_size_bytes = 4,
    });
    state.dynamic_array_local_cleanup_plans.push_back(orison::lowering::DynamicArrayDescriptorCleanupPlan {
        .owner_name = "predicted_items",
        .source_type_name = "DynamicArray<UInt32>",
        .element_source_type_name = "UInt32",
        .element_llvm_type = "i32",
        .descriptor_storage_name = "%predicted_items.addr",
        .descriptor_storage_status =
            orison::lowering::DynamicArrayDescriptorStorageStatus::predicted_owner_local,
        .element_size_bytes = 4,
    });

    auto named_dynamic_array_plan =
        orison::lowering::plan_dynamic_array_iterable_descriptor(name("items"), context, state);
    assert(
        named_dynamic_array_plan.kind ==
        orison::lowering::DynamicArrayIterableDescriptorPlanKind::named_descriptor_owner
    );
    assert(named_dynamic_array_plan.source_type_name == "DynamicArray<UInt32>");
    assert(named_dynamic_array_plan.element_source_type_name == "UInt32");
    assert(named_dynamic_array_plan.owner_name == "items");
    assert(named_dynamic_array_plan.descriptor_storage == "%items.addr");
    assert(named_dynamic_array_plan.can_lower_now);
    assert(named_dynamic_array_plan.cleanup_owner_proven);
    assert(
        named_dynamic_array_plan.cleanup_owner_proof_status ==
        orison::lowering::DynamicArrayIterableCleanupOwnerProofStatus::proven_lowered_local_descriptor
    );
    assert(
        orison::lowering::dynamic_array_iterable_descriptor_plan_report(named_dynamic_array_plan)
            .find("cleanup owner proven from lowered local descriptor") != std::string::npos
    );

    auto predicted_dynamic_array_plan =
        orison::lowering::plan_dynamic_array_iterable_descriptor(name("predicted_items"), context, state);
    assert(
        predicted_dynamic_array_plan.kind ==
        orison::lowering::DynamicArrayIterableDescriptorPlanKind::named_descriptor_owner
    );
    assert(predicted_dynamic_array_plan.can_lower_now);
    assert(!predicted_dynamic_array_plan.cleanup_owner_proven);
    assert(
        predicted_dynamic_array_plan.cleanup_owner_proof_status ==
        orison::lowering::DynamicArrayIterableCleanupOwnerProofStatus::predicted_owner_local
    );
    assert(
        orison::lowering::dynamic_array_iterable_descriptor_plan_report(predicted_dynamic_array_plan)
            .find("cleanup owner predicted from semantic descriptor origin") != std::string::npos
    );

    auto missing_dynamic_array_plan =
        orison::lowering::plan_dynamic_array_iterable_descriptor(name("computed_left"), context, state);
    assert(
        missing_dynamic_array_plan.kind ==
        orison::lowering::DynamicArrayIterableDescriptorPlanKind::missing_named_descriptor_storage
    );
    assert(!missing_dynamic_array_plan.can_lower_now);
    assert(!missing_dynamic_array_plan.cleanup_owner_proven);
    assert(
        missing_dynamic_array_plan.cleanup_owner_proof_status ==
        orison::lowering::DynamicArrayIterableCleanupOwnerProofStatus::missing_cleanup_plan
    );
    assert(
        orison::lowering::dynamic_array_iterable_descriptor_plan_report(missing_dynamic_array_plan)
            .find("has no bound descriptor storage") != std::string::npos
    );

    auto computed_dynamic_array_plan = orison::lowering::plan_dynamic_array_iterable_descriptor(
        ternary(name("flag"), name("computed_left"), name("computed_right")),
        context,
        state
    );
    assert(
        computed_dynamic_array_plan.kind ==
        orison::lowering::DynamicArrayIterableDescriptorPlanKind::computed_owner_unproven
    );
    assert(computed_dynamic_array_plan.source_type_name == "DynamicArray<UInt32>");
    assert(computed_dynamic_array_plan.element_source_type_name == "UInt32");
    assert(!computed_dynamic_array_plan.can_lower_now);
    assert(!computed_dynamic_array_plan.cleanup_owner_proven);
    assert(
        computed_dynamic_array_plan.cleanup_owner_proof_status ==
        orison::lowering::DynamicArrayIterableCleanupOwnerProofStatus::missing_cleanup_plan
    );
    assert(
        orison::lowering::dynamic_array_iterable_descriptor_plan_report(computed_dynamic_array_plan)
            .find("requires a proven single descriptor owner") != std::string::npos
    );

    auto mismatched_computed_ownership_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_ownership_transfer(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_ownership_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableOwnershipPlanKind::ternary_branch_owner_mismatch
    );
    assert(mismatched_computed_ownership_plan.source_type_name == "DynamicArray<UInt32>");
    assert(mismatched_computed_ownership_plan.element_source_type_name == "UInt32");
    assert(mismatched_computed_ownership_plan.branch_owner_names.size() == 2);
    assert(mismatched_computed_ownership_plan.branch_owner_names[0] == "computed_left");
    assert(mismatched_computed_ownership_plan.branch_owner_names[1] == "computed_right");
    assert(!mismatched_computed_ownership_plan.ownership_join_matches);
    assert(!mismatched_computed_ownership_plan.cleanup_owner_proven);
    assert(
        orison::lowering::computed_dynamic_array_iterable_ownership_plan_report(
            mismatched_computed_ownership_plan
        ).find("ternary branch owner mismatch") != std::string::npos
    );
    auto mismatched_computed_handoff_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_descriptor_handoff(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_handoff_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableDescriptorHandoffPlanKind::ownership_join_blocked
    );
    assert(mismatched_computed_handoff_plan.source_type_name == "DynamicArray<UInt32>");
    assert(!mismatched_computed_handoff_plan.descriptor_storage_available);
    assert(!mismatched_computed_handoff_plan.cleanup_owner_proven);
    assert(!mismatched_computed_handoff_plan.lowering_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_descriptor_handoff_plan_report(
            mismatched_computed_handoff_plan
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_cleanup_sequence =
        orison::lowering::plan_computed_dynamic_array_iterable_cleanup_sequence(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_cleanup_sequence.kind ==
        orison::lowering::ComputedDynamicArrayIterableCleanupSequencePlanKind::ownership_join_blocked
    );
    assert(!mismatched_computed_cleanup_sequence.loop_body_has_cleanup_responsibility);
    assert(!mismatched_computed_cleanup_sequence.function_cleanup_resumes_after_loop);
    assert(!mismatched_computed_cleanup_sequence.cleanup_sequence_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_cleanup_sequence_plan_report(
            mismatched_computed_cleanup_sequence
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_descriptor_render =
        orison::lowering::plan_computed_dynamic_array_iterable_descriptor_render(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_descriptor_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableDescriptorRenderPlanKind::ownership_join_blocked
    );
    assert(mismatched_computed_descriptor_render.rendered_ir.empty());
    assert(!mismatched_computed_descriptor_render.descriptor_load_planned);
    assert(!mismatched_computed_descriptor_render.data_projection_planned);
    assert(!mismatched_computed_descriptor_render.length_projection_planned);
    assert(!mismatched_computed_descriptor_render.capacity_projection_planned);
    assert(!mismatched_computed_descriptor_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_descriptor_render_plan_report(
            mismatched_computed_descriptor_render
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_loop_control_render =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_control_render(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_loop_control_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopControlRenderPlanKind::ownership_join_blocked
    );
    assert(mismatched_computed_loop_control_render.rendered_ir.empty());
    assert(!mismatched_computed_loop_control_render.entry_branch_planned);
    assert(!mismatched_computed_loop_control_render.index_phi_planned);
    assert(!mismatched_computed_loop_control_render.bounds_check_planned);
    assert(!mismatched_computed_loop_control_render.conditional_branch_planned);
    assert(!mismatched_computed_loop_control_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_control_render_plan_report(
            mismatched_computed_loop_control_render
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_element_address_render =
        orison::lowering::plan_computed_dynamic_array_iterable_element_address_render(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_element_address_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableElementAddressRenderPlanKind::ownership_join_blocked
    );
    assert(mismatched_computed_element_address_render.rendered_ir.empty());
    assert(!mismatched_computed_element_address_render.data_pointer_available);
    assert(!mismatched_computed_element_address_render.index_available);
    assert(!mismatched_computed_element_address_render.element_address_planned);
    assert(!mismatched_computed_element_address_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_element_address_render_plan_report(
            mismatched_computed_element_address_render
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_element_load_render =
        orison::lowering::plan_computed_dynamic_array_iterable_element_load_render(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_element_load_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableElementLoadRenderPlanKind::ownership_join_blocked
    );
    assert(mismatched_computed_element_load_render.rendered_ir.empty());
    assert(!mismatched_computed_element_load_render.element_address_available);
    assert(!mismatched_computed_element_load_render.item_value_planned);
    assert(!mismatched_computed_element_load_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_element_load_render_plan_report(
            mismatched_computed_element_load_render
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_loop_continue_render =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_continue_render(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_loop_continue_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopContinueRenderPlanKind::ownership_join_blocked
    );
    assert(mismatched_computed_loop_continue_render.rendered_ir.empty());
    assert(!mismatched_computed_loop_continue_render.continue_block_planned);
    assert(!mismatched_computed_loop_continue_render.next_index_planned);
    assert(!mismatched_computed_loop_continue_render.backedge_branch_planned);
    assert(!mismatched_computed_loop_continue_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_continue_render_plan_report(
            mismatched_computed_loop_continue_render
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_loop_render_sequence =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_render_sequence(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_loop_render_sequence.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopRenderSequencePlanKind::
            ownership_join_blocked
    );
    assert(mismatched_computed_loop_render_sequence.rendered_ir.empty());
    assert(!mismatched_computed_loop_render_sequence.descriptor_render_planned);
    assert(!mismatched_computed_loop_render_sequence.loop_control_render_planned);
    assert(!mismatched_computed_loop_render_sequence.body_block_planned);
    assert(!mismatched_computed_loop_render_sequence.element_address_render_planned);
    assert(!mismatched_computed_loop_render_sequence.element_load_render_planned);
    assert(!mismatched_computed_loop_render_sequence.loop_continue_render_planned);
    assert(!mismatched_computed_loop_render_sequence.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_render_sequence_plan_report(
            mismatched_computed_loop_render_sequence
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_loop_exit_cleanup =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_exit_cleanup(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_loop_exit_cleanup.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopExitCleanupPlanKind::
            ownership_join_blocked
    );
    assert(mismatched_computed_loop_exit_cleanup.rendered_ir.empty());
    assert(!mismatched_computed_loop_exit_cleanup.exit_block_planned);
    assert(!mismatched_computed_loop_exit_cleanup.cleanup_resumption_planned);
    assert(!mismatched_computed_loop_exit_cleanup.cleanup_sequence_enabled);
    assert(!mismatched_computed_loop_exit_cleanup.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_exit_cleanup_plan_report(
            mismatched_computed_loop_exit_cleanup
        ).find("ownership join blocked") != std::string::npos
    );
    auto mismatched_computed_production_emission_gate =
        orison::lowering::plan_computed_dynamic_array_iterable_production_emission_gate(
            ternary(name("flag"), name("computed_left"), name("computed_right")),
            context,
            state
        );
    assert(
        mismatched_computed_production_emission_gate.kind ==
        orison::lowering::ComputedDynamicArrayIterableProductionEmissionGatePlanKind::
            ownership_join_blocked
    );
    assert(!mismatched_computed_production_emission_gate.ownership_ready);
    assert(!mismatched_computed_production_emission_gate.loop_render_ready);
    assert(!mismatched_computed_production_emission_gate.exit_cleanup_ready);
    assert(mismatched_computed_production_emission_gate.rendered_ir.empty());
    assert(!mismatched_computed_production_emission_gate.production_sequence_render_planned);
    assert(!mismatched_computed_production_emission_gate.production_emission_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_production_emission_gate_plan_report(
            mismatched_computed_production_emission_gate
        ).find("ownership join blocked") != std::string::npos
    );

    auto proven_computed_ownership_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_ownership_transfer(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_ownership_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_proven
    );
    assert(proven_computed_ownership_plan.ownership_join_matches);
    assert(proven_computed_ownership_plan.cleanup_owner_proven);
    assert(orison::lowering::is_owned_binding_consumed(proven_computed_ownership_plan.merged_transfers, "items"));
    assert(
        orison::lowering::computed_dynamic_array_iterable_ownership_plan_report(
            proven_computed_ownership_plan
        ).find("ternary single owner proven") != std::string::npos
    );
    auto proven_nested_computed_ownership_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_ownership_transfer(
            ternary(
                name("flag"),
                name("items"),
                ternary(name("other_flag"), name("items"), name("items"))
            ),
            context,
            state
        );
    assert(
        proven_nested_computed_ownership_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_proven
    );
    assert(proven_nested_computed_ownership_plan.branch_owner_names.size() == 3);
    assert(proven_nested_computed_ownership_plan.branch_owner_names[0] == "items");
    assert(proven_nested_computed_ownership_plan.branch_owner_names[1] == "items");
    assert(proven_nested_computed_ownership_plan.branch_owner_names[2] == "items");
    assert(proven_nested_computed_ownership_plan.ownership_join_matches);
    assert(proven_nested_computed_ownership_plan.cleanup_owner_proven);
    assert(
        orison::lowering::is_owned_binding_consumed(
            proven_nested_computed_ownership_plan.merged_transfers,
            "items"
        )
    );
    auto nested_mismatched_computed_ownership_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_ownership_transfer(
            ternary(
                name("flag"),
                name("items"),
                ternary(name("other_flag"), name("items"), name("computed_right"))
            ),
            context,
            state
        );
    assert(
        nested_mismatched_computed_ownership_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableOwnershipPlanKind::ternary_branch_owner_mismatch
    );
    assert(nested_mismatched_computed_ownership_plan.branch_owner_names.size() == 3);
    assert(nested_mismatched_computed_ownership_plan.branch_owner_names[0] == "items");
    assert(nested_mismatched_computed_ownership_plan.branch_owner_names[1] == "items");
    assert(nested_mismatched_computed_ownership_plan.branch_owner_names[2] == "computed_right");
    assert(!nested_mismatched_computed_ownership_plan.ownership_join_matches);
    assert(!nested_mismatched_computed_ownership_plan.cleanup_owner_proven);
    assert(
        orison::lowering::computed_dynamic_array_iterable_ownership_plan_report(
            nested_mismatched_computed_ownership_plan
        ).find("owners items items computed_right [ownership join blocked]") != std::string::npos
    );
    auto proven_computed_handoff_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_descriptor_handoff(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_handoff_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableDescriptorHandoffPlanKind::
            single_cleanup_owner_handoff_planned
    );
    assert(proven_computed_handoff_plan.source_type_name == "DynamicArray<UInt32>");
    assert(proven_computed_handoff_plan.element_source_type_name == "UInt32");
    assert(proven_computed_handoff_plan.source_owner_name == "items");
    assert(proven_computed_handoff_plan.handoff_owner_name == "items");
    assert(proven_computed_handoff_plan.descriptor_storage_name == "%items.addr");
    assert(proven_computed_handoff_plan.descriptor_storage_available);
    assert(proven_computed_handoff_plan.cleanup_owner_proven);
    assert(!proven_computed_handoff_plan.lowering_enabled);
    auto proven_computed_handoff_report =
        orison::lowering::computed_dynamic_array_iterable_descriptor_handoff_plan_report(
            proven_computed_handoff_plan
        );
    assert(proven_computed_handoff_report.find("single cleanup owner handoff planned") != std::string::npos);
    assert(proven_computed_handoff_report.find("[lowering disabled]") != std::string::npos);
    auto proven_computed_cleanup_sequence =
        orison::lowering::plan_computed_dynamic_array_iterable_cleanup_sequence(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_cleanup_sequence.kind ==
        orison::lowering::ComputedDynamicArrayIterableCleanupSequencePlanKind::loop_cleanup_sequence_planned
    );
    assert(proven_computed_cleanup_sequence.cleanup_owner_name == "items");
    assert(proven_computed_cleanup_sequence.descriptor_storage_name == "%items.addr");
    assert(proven_computed_cleanup_sequence.loop_entry_cleanup_owner_name == "items.loop.entry");
    assert(proven_computed_cleanup_sequence.loop_exit_cleanup_owner_name == "items");
    assert(proven_computed_cleanup_sequence.loop_entry_cleanup_operation_name == "items.computed_for.cleanup.acquire");
    assert(proven_computed_cleanup_sequence.loop_body_has_cleanup_responsibility);
    assert(proven_computed_cleanup_sequence.function_cleanup_resumes_after_loop);
    assert(!proven_computed_cleanup_sequence.cleanup_sequence_enabled);
    auto proven_computed_cleanup_sequence_report =
        orison::lowering::computed_dynamic_array_iterable_cleanup_sequence_plan_report(
            proven_computed_cleanup_sequence
        );
    assert(proven_computed_cleanup_sequence_report.find("loop cleanup sequence planned") != std::string::npos);
    assert(
        proven_computed_cleanup_sequence_report.find("operation items.computed_for.cleanup.acquire") !=
        std::string::npos
    );
    assert(proven_computed_cleanup_sequence_report.find("[cleanup sequence disabled]") != std::string::npos);
    auto proven_computed_descriptor_render =
        orison::lowering::plan_computed_dynamic_array_iterable_descriptor_render(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_descriptor_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableDescriptorRenderPlanKind::descriptor_render_planned
    );
    assert(proven_computed_descriptor_render.cleanup_owner_name == "items");
    assert(proven_computed_descriptor_render.descriptor_storage_name == "%items.addr");
    assert(proven_computed_descriptor_render.descriptor_value_name == "%items.computed_for.descriptor");
    assert(proven_computed_descriptor_render.data_pointer_name == "%items.computed_for.data");
    assert(proven_computed_descriptor_render.length_name == "%items.computed_for.length");
    assert(proven_computed_descriptor_render.capacity_name == "%items.computed_for.capacity");
    assert(proven_computed_descriptor_render.rendered_ir.size() == 4);
    assert(
        proven_computed_descriptor_render.rendered_ir[0] ==
        "  %items.computed_for.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert(
        proven_computed_descriptor_render.rendered_ir[1] ==
        "  %items.computed_for.data = extractvalue { ptr, i64, i64 } %items.computed_for.descriptor, 0\n"
    );
    assert(
        proven_computed_descriptor_render.rendered_ir[2] ==
        "  %items.computed_for.length = extractvalue { ptr, i64, i64 } %items.computed_for.descriptor, 1\n"
    );
    assert(
        proven_computed_descriptor_render.rendered_ir[3] ==
        "  %items.computed_for.capacity = extractvalue { ptr, i64, i64 } %items.computed_for.descriptor, 2\n"
    );
    assert(proven_computed_descriptor_render.descriptor_load_planned);
    assert(proven_computed_descriptor_render.data_projection_planned);
    assert(proven_computed_descriptor_render.length_projection_planned);
    assert(proven_computed_descriptor_render.capacity_projection_planned);
    assert(!proven_computed_descriptor_render.render_enabled);
    auto proven_computed_descriptor_render_report =
        orison::lowering::computed_dynamic_array_iterable_descriptor_render_plan_report(
            proven_computed_descriptor_render
        );
    assert(
        proven_computed_descriptor_render_report.find("descriptor load projection planned") !=
        std::string::npos
    );
    assert(proven_computed_descriptor_render_report.find("[render disabled]") != std::string::npos);
    auto proven_computed_loop_control_render =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_control_render(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_loop_control_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopControlRenderPlanKind::
            loop_control_render_planned
    );
    assert(proven_computed_loop_control_render.cleanup_owner_name == "items");
    assert(proven_computed_loop_control_render.condition_block_name == "items.computed_for.condition");
    assert(proven_computed_loop_control_render.body_block_name == "items.computed_for.body");
    assert(proven_computed_loop_control_render.continue_block_name == "items.computed_for.continue");
    assert(proven_computed_loop_control_render.exit_block_name == "items.computed_for.exit");
    assert(proven_computed_loop_control_render.index_name == "%items.computed_for.index");
    assert(proven_computed_loop_control_render.next_index_name == "%items.computed_for.next.index");
    assert(proven_computed_loop_control_render.bounds_check_name == "%items.computed_for.more");
    assert(proven_computed_loop_control_render.rendered_ir.size() == 5);
    assert(proven_computed_loop_control_render.rendered_ir[0] == "  br label %items.computed_for.condition\n");
    assert(proven_computed_loop_control_render.rendered_ir[1] == "items.computed_for.condition:\n");
    assert(
        proven_computed_loop_control_render.rendered_ir[2] ==
        "  %items.computed_for.index = phi i64 [ 0, %entry ], [ %items.computed_for.next.index, "
        "%items.computed_for.continue ]\n"
    );
    assert(
        proven_computed_loop_control_render.rendered_ir[3] ==
        "  %items.computed_for.more = icmp ult i64 %items.computed_for.index, %items.computed_for.length\n"
    );
    assert(
        proven_computed_loop_control_render.rendered_ir[4] ==
        "  br i1 %items.computed_for.more, label %items.computed_for.body, label %items.computed_for.exit\n"
    );
    assert(proven_computed_loop_control_render.entry_branch_planned);
    assert(proven_computed_loop_control_render.index_phi_planned);
    assert(proven_computed_loop_control_render.bounds_check_planned);
    assert(proven_computed_loop_control_render.conditional_branch_planned);
    assert(!proven_computed_loop_control_render.render_enabled);
    auto proven_computed_loop_control_render_report =
        orison::lowering::computed_dynamic_array_iterable_loop_control_render_plan_report(
            proven_computed_loop_control_render
        );
    assert(
        proven_computed_loop_control_render_report.find("loop control render planned") !=
        std::string::npos
    );
    assert(proven_computed_loop_control_render_report.find("[render disabled]") != std::string::npos);
    auto proven_computed_element_address_render =
        orison::lowering::plan_computed_dynamic_array_iterable_element_address_render(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_element_address_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableElementAddressRenderPlanKind::
            element_address_render_planned
    );
    assert(proven_computed_element_address_render.cleanup_owner_name == "items");
    assert(proven_computed_element_address_render.element_source_type_name == "UInt32");
    assert(proven_computed_element_address_render.element_llvm_type_name == "i32");
    assert(proven_computed_element_address_render.data_pointer_name == "%items.computed_for.data");
    assert(proven_computed_element_address_render.index_name == "%items.computed_for.index");
    assert(proven_computed_element_address_render.element_address_name == "%items.computed_for.element.addr");
    assert(proven_computed_element_address_render.rendered_ir.size() == 1);
    assert(
        proven_computed_element_address_render.rendered_ir[0] ==
        "  %items.computed_for.element.addr = getelementptr i32, ptr %items.computed_for.data, "
        "i64 %items.computed_for.index\n"
    );
    assert(proven_computed_element_address_render.data_pointer_available);
    assert(proven_computed_element_address_render.index_available);
    assert(proven_computed_element_address_render.element_address_planned);
    assert(!proven_computed_element_address_render.render_enabled);
    auto proven_computed_element_address_render_report =
        orison::lowering::computed_dynamic_array_iterable_element_address_render_plan_report(
            proven_computed_element_address_render
        );
    assert(
        proven_computed_element_address_render_report.find("element address render planned") !=
        std::string::npos
    );
    assert(
        proven_computed_element_address_render_report.find("lowers-to i32") !=
        std::string::npos
    );
    assert(proven_computed_element_address_render_report.find("[render disabled]") != std::string::npos);
    auto proven_computed_element_load_render =
        orison::lowering::plan_computed_dynamic_array_iterable_element_load_render(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_element_load_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableElementLoadRenderPlanKind::element_load_render_planned
    );
    assert(proven_computed_element_load_render.cleanup_owner_name == "items");
    assert(proven_computed_element_load_render.element_source_type_name == "UInt32");
    assert(proven_computed_element_load_render.element_llvm_type_name == "i32");
    assert(proven_computed_element_load_render.element_address_name == "%items.computed_for.element.addr");
    assert(proven_computed_element_load_render.item_value_name == "%items.computed_for.item");
    assert(proven_computed_element_load_render.rendered_ir.size() == 1);
    assert(
        proven_computed_element_load_render.rendered_ir[0] ==
        "  %items.computed_for.item = load i32, ptr %items.computed_for.element.addr\n"
    );
    assert(proven_computed_element_load_render.element_address_available);
    assert(proven_computed_element_load_render.item_value_planned);
    assert(!proven_computed_element_load_render.render_enabled);
    auto proven_computed_element_load_render_report =
        orison::lowering::computed_dynamic_array_iterable_element_load_render_plan_report(
            proven_computed_element_load_render
        );
    assert(
        proven_computed_element_load_render_report.find("element load render planned") !=
        std::string::npos
    );
    assert(
        proven_computed_element_load_render_report.find("item %items.computed_for.item") !=
        std::string::npos
    );
    assert(proven_computed_element_load_render_report.find("[render disabled]") != std::string::npos);
    auto proven_computed_loop_continue_render =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_continue_render(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_loop_continue_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopContinueRenderPlanKind::
            loop_continue_render_planned
    );
    assert(proven_computed_loop_continue_render.cleanup_owner_name == "items");
    assert(proven_computed_loop_continue_render.continue_block_name == "items.computed_for.continue");
    assert(proven_computed_loop_continue_render.condition_block_name == "items.computed_for.condition");
    assert(proven_computed_loop_continue_render.index_name == "%items.computed_for.index");
    assert(proven_computed_loop_continue_render.next_index_name == "%items.computed_for.next.index");
    assert(proven_computed_loop_continue_render.rendered_ir.size() == 3);
    assert(proven_computed_loop_continue_render.rendered_ir[0] == "items.computed_for.continue:\n");
    assert(
        proven_computed_loop_continue_render.rendered_ir[1] ==
        "  %items.computed_for.next.index = add i64 %items.computed_for.index, 1\n"
    );
    assert(
        proven_computed_loop_continue_render.rendered_ir[2] ==
        "  br label %items.computed_for.condition\n"
    );
    assert(proven_computed_loop_continue_render.continue_block_planned);
    assert(proven_computed_loop_continue_render.next_index_planned);
    assert(proven_computed_loop_continue_render.backedge_branch_planned);
    assert(!proven_computed_loop_continue_render.render_enabled);
    auto proven_computed_loop_continue_render_report =
        orison::lowering::computed_dynamic_array_iterable_loop_continue_render_plan_report(
            proven_computed_loop_continue_render
        );
    assert(
        proven_computed_loop_continue_render_report.find("loop continue render planned") !=
        std::string::npos
    );
    assert(
        proven_computed_loop_continue_render_report.find("next-index %items.computed_for.next.index") !=
        std::string::npos
    );
    assert(proven_computed_loop_continue_render_report.find("[render disabled]") != std::string::npos);
    auto proven_computed_loop_render_sequence =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_render_sequence(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_loop_render_sequence.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopRenderSequencePlanKind::
            loop_render_sequence_planned
    );
    assert(proven_computed_loop_render_sequence.cleanup_owner_name == "items");
    assert(proven_computed_loop_render_sequence.source_type_name == "DynamicArray<UInt32>");
    assert(proven_computed_loop_render_sequence.element_source_type_name == "UInt32");
    assert(proven_computed_loop_render_sequence.body_block_name == "items.computed_for.body");
    assert(proven_computed_loop_render_sequence.rendered_ir.size() == 15);
    assert(
        proven_computed_loop_render_sequence.rendered_ir[0] ==
        "  %items.computed_for.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert(proven_computed_loop_render_sequence.rendered_ir[4] == "  br label %items.computed_for.condition\n");
    assert(proven_computed_loop_render_sequence.rendered_ir[9] == "items.computed_for.body:\n");
    assert(
        proven_computed_loop_render_sequence.rendered_ir[10] ==
        "  %items.computed_for.element.addr = getelementptr i32, ptr %items.computed_for.data, "
        "i64 %items.computed_for.index\n"
    );
    assert(
        proven_computed_loop_render_sequence.rendered_ir[11] ==
        "  %items.computed_for.item = load i32, ptr %items.computed_for.element.addr\n"
    );
    assert(proven_computed_loop_render_sequence.rendered_ir[12] == "items.computed_for.continue:\n");
    assert(
        proven_computed_loop_render_sequence.rendered_ir[14] ==
        "  br label %items.computed_for.condition\n"
    );
    assert(proven_computed_loop_render_sequence.descriptor_render_planned);
    assert(proven_computed_loop_render_sequence.loop_control_render_planned);
    assert(proven_computed_loop_render_sequence.body_block_planned);
    assert(proven_computed_loop_render_sequence.element_address_render_planned);
    assert(proven_computed_loop_render_sequence.element_load_render_planned);
    assert(proven_computed_loop_render_sequence.loop_continue_render_planned);
    assert(!proven_computed_loop_render_sequence.render_enabled);
    auto proven_computed_loop_render_sequence_report =
        orison::lowering::computed_dynamic_array_iterable_loop_render_sequence_plan_report(
            proven_computed_loop_render_sequence
        );
    assert(
        proven_computed_loop_render_sequence_report.find("loop render sequence planned") !=
        std::string::npos
    );
    assert(
        proven_computed_loop_render_sequence_report.find("body items.computed_for.body") !=
        std::string::npos
    );
    assert(proven_computed_loop_render_sequence_report.find("[render disabled]") != std::string::npos);
    auto proven_computed_loop_exit_cleanup =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_exit_cleanup(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_loop_exit_cleanup.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopExitCleanupPlanKind::
            loop_exit_cleanup_planned
    );
    assert(proven_computed_loop_exit_cleanup.cleanup_owner_name == "items");
    assert(proven_computed_loop_exit_cleanup.source_type_name == "DynamicArray<UInt32>");
    assert(proven_computed_loop_exit_cleanup.element_source_type_name == "UInt32");
    assert(proven_computed_loop_exit_cleanup.exit_block_name == "items.computed_for.exit");
    assert(proven_computed_loop_exit_cleanup.loop_entry_cleanup_owner_name == "items.loop.entry");
    assert(proven_computed_loop_exit_cleanup.loop_exit_cleanup_owner_name == "items");
    assert(proven_computed_loop_exit_cleanup.cleanup_resumption_operation_name == "items.computed_for.cleanup.resume");
    assert(proven_computed_loop_exit_cleanup.rendered_ir.size() == 2);
    assert(proven_computed_loop_exit_cleanup.rendered_ir[0] == "items.computed_for.exit:\n");
    assert(
        proven_computed_loop_exit_cleanup.rendered_ir[1] ==
        "  ; cleanup state handoff resume operation items.computed_for.cleanup.resume "
        "from items.loop.entry to items [cleanup calls disabled]\n"
    );
    assert(proven_computed_loop_exit_cleanup.exit_block_planned);
    assert(proven_computed_loop_exit_cleanup.cleanup_resumption_planned);
    assert(!proven_computed_loop_exit_cleanup.cleanup_sequence_enabled);
    assert(!proven_computed_loop_exit_cleanup.render_enabled);
    auto proven_computed_loop_exit_cleanup_report =
        orison::lowering::computed_dynamic_array_iterable_loop_exit_cleanup_plan_report(
            proven_computed_loop_exit_cleanup
        );
    assert(
        proven_computed_loop_exit_cleanup_report.find("loop exit cleanup planned") !=
        std::string::npos
    );
    assert(
        proven_computed_loop_exit_cleanup_report.find("exit items.computed_for.exit") !=
        std::string::npos
    );
    assert(proven_computed_loop_exit_cleanup_report.find("[cleanup sequence disabled]") != std::string::npos);
    auto proven_computed_production_emission_gate =
        orison::lowering::plan_computed_dynamic_array_iterable_production_emission_gate(
            ternary(name("flag"), name("items"), name("items")),
            context,
            state
        );
    assert(
        proven_computed_production_emission_gate.kind ==
        orison::lowering::ComputedDynamicArrayIterableProductionEmissionGatePlanKind::
            production_emission_gate_planned
    );
    assert(proven_computed_production_emission_gate.cleanup_owner_name == "items");
    assert(proven_computed_production_emission_gate.source_type_name == "DynamicArray<UInt32>");
    assert(proven_computed_production_emission_gate.element_source_type_name == "UInt32");
    assert(proven_computed_production_emission_gate.ownership_ready);
    assert(proven_computed_production_emission_gate.loop_render_ready);
    assert(proven_computed_production_emission_gate.loop_cleanup_ownership_ready);
    assert(proven_computed_production_emission_gate.function_cleanup_resumption_ready);
    assert(proven_computed_production_emission_gate.exit_cleanup_ready);
    assert(proven_computed_production_emission_gate.rendered_ir.size() == 17);
    assert(
        proven_computed_production_emission_gate.rendered_ir[0] ==
        "  %items.computed_for.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert(proven_computed_production_emission_gate.rendered_ir[9] == "items.computed_for.body:\n");
    assert(proven_computed_production_emission_gate.rendered_ir[15] == "items.computed_for.exit:\n");
    assert(
        proven_computed_production_emission_gate.rendered_ir[16] ==
        "  ; cleanup state handoff resume operation items.computed_for.cleanup.resume "
        "from items.loop.entry to items [cleanup calls disabled]\n"
    );
    assert(proven_computed_production_emission_gate.production_sequence_render_planned);
    assert(!proven_computed_production_emission_gate.production_emission_enabled);
    auto proven_computed_production_emission_gate_report =
        orison::lowering::computed_dynamic_array_iterable_production_emission_gate_plan_report(
            proven_computed_production_emission_gate
        );
    assert(
        proven_computed_production_emission_gate_report.find("production emission gate planned") !=
        std::string::npos
    );
    assert(
        proven_computed_production_emission_gate_report.find("[production emission disabled]") !=
        std::string::npos
    );
    assert(
        proven_computed_production_emission_gate_report.find("[production sequence planned]") !=
        std::string::npos
    );
    assert(
        proven_computed_production_emission_gate_report.find("[loop cleanup ownership ready]") !=
        std::string::npos
    );
    assert(
        proven_computed_production_emission_gate_report.find("[function cleanup resumption ready]") !=
        std::string::npos
    );

    auto unproven_computed_ownership_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_ownership_transfer(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_ownership_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_unproven
    );
    assert(unproven_computed_ownership_plan.ownership_join_matches);
    assert(!unproven_computed_ownership_plan.cleanup_owner_proven);
    assert(
        orison::lowering::computed_dynamic_array_iterable_ownership_plan_report(
            unproven_computed_ownership_plan
        ).find("ternary single owner unproven") != std::string::npos
    );
    auto unproven_computed_handoff_plan =
        orison::lowering::plan_computed_dynamic_array_iterable_descriptor_handoff(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_handoff_plan.kind ==
        orison::lowering::ComputedDynamicArrayIterableDescriptorHandoffPlanKind::cleanup_owner_unproven
    );
    assert(unproven_computed_handoff_plan.source_owner_name == "predicted_items");
    assert(unproven_computed_handoff_plan.handoff_owner_name == "predicted_items");
    assert(!unproven_computed_handoff_plan.descriptor_storage_available);
    assert(!unproven_computed_handoff_plan.cleanup_owner_proven);
    assert(!unproven_computed_handoff_plan.lowering_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_descriptor_handoff_plan_report(
            unproven_computed_handoff_plan
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_cleanup_sequence =
        orison::lowering::plan_computed_dynamic_array_iterable_cleanup_sequence(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_cleanup_sequence.kind ==
        orison::lowering::ComputedDynamicArrayIterableCleanupSequencePlanKind::cleanup_owner_unproven
    );
    assert(unproven_computed_cleanup_sequence.cleanup_owner_name == "predicted_items");
    assert(!unproven_computed_cleanup_sequence.loop_body_has_cleanup_responsibility);
    assert(!unproven_computed_cleanup_sequence.function_cleanup_resumes_after_loop);
    assert(!unproven_computed_cleanup_sequence.cleanup_sequence_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_cleanup_sequence_plan_report(
            unproven_computed_cleanup_sequence
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_descriptor_render =
        orison::lowering::plan_computed_dynamic_array_iterable_descriptor_render(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_descriptor_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableDescriptorRenderPlanKind::cleanup_owner_unproven
    );
    assert(unproven_computed_descriptor_render.cleanup_owner_name == "predicted_items");
    assert(unproven_computed_descriptor_render.rendered_ir.empty());
    assert(!unproven_computed_descriptor_render.descriptor_load_planned);
    assert(!unproven_computed_descriptor_render.data_projection_planned);
    assert(!unproven_computed_descriptor_render.length_projection_planned);
    assert(!unproven_computed_descriptor_render.capacity_projection_planned);
    assert(!unproven_computed_descriptor_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_descriptor_render_plan_report(
            unproven_computed_descriptor_render
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_loop_control_render =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_control_render(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_loop_control_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopControlRenderPlanKind::cleanup_owner_unproven
    );
    assert(unproven_computed_loop_control_render.cleanup_owner_name == "predicted_items");
    assert(unproven_computed_loop_control_render.rendered_ir.empty());
    assert(!unproven_computed_loop_control_render.entry_branch_planned);
    assert(!unproven_computed_loop_control_render.index_phi_planned);
    assert(!unproven_computed_loop_control_render.bounds_check_planned);
    assert(!unproven_computed_loop_control_render.conditional_branch_planned);
    assert(!unproven_computed_loop_control_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_control_render_plan_report(
            unproven_computed_loop_control_render
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_element_address_render =
        orison::lowering::plan_computed_dynamic_array_iterable_element_address_render(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_element_address_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableElementAddressRenderPlanKind::cleanup_owner_unproven
    );
    assert(unproven_computed_element_address_render.cleanup_owner_name == "predicted_items");
    assert(unproven_computed_element_address_render.rendered_ir.empty());
    assert(!unproven_computed_element_address_render.data_pointer_available);
    assert(!unproven_computed_element_address_render.index_available);
    assert(!unproven_computed_element_address_render.element_address_planned);
    assert(!unproven_computed_element_address_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_element_address_render_plan_report(
            unproven_computed_element_address_render
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_element_load_render =
        orison::lowering::plan_computed_dynamic_array_iterable_element_load_render(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_element_load_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableElementLoadRenderPlanKind::cleanup_owner_unproven
    );
    assert(unproven_computed_element_load_render.cleanup_owner_name == "predicted_items");
    assert(unproven_computed_element_load_render.rendered_ir.empty());
    assert(!unproven_computed_element_load_render.element_address_available);
    assert(!unproven_computed_element_load_render.item_value_planned);
    assert(!unproven_computed_element_load_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_element_load_render_plan_report(
            unproven_computed_element_load_render
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_loop_continue_render =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_continue_render(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_loop_continue_render.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopContinueRenderPlanKind::cleanup_owner_unproven
    );
    assert(unproven_computed_loop_continue_render.cleanup_owner_name == "predicted_items");
    assert(unproven_computed_loop_continue_render.rendered_ir.empty());
    assert(!unproven_computed_loop_continue_render.continue_block_planned);
    assert(!unproven_computed_loop_continue_render.next_index_planned);
    assert(!unproven_computed_loop_continue_render.backedge_branch_planned);
    assert(!unproven_computed_loop_continue_render.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_continue_render_plan_report(
            unproven_computed_loop_continue_render
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_loop_render_sequence =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_render_sequence(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_loop_render_sequence.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopRenderSequencePlanKind::
            cleanup_owner_unproven
    );
    assert(unproven_computed_loop_render_sequence.cleanup_owner_name == "predicted_items");
    assert(unproven_computed_loop_render_sequence.rendered_ir.empty());
    assert(!unproven_computed_loop_render_sequence.descriptor_render_planned);
    assert(!unproven_computed_loop_render_sequence.loop_control_render_planned);
    assert(!unproven_computed_loop_render_sequence.body_block_planned);
    assert(!unproven_computed_loop_render_sequence.element_address_render_planned);
    assert(!unproven_computed_loop_render_sequence.element_load_render_planned);
    assert(!unproven_computed_loop_render_sequence.loop_continue_render_planned);
    assert(!unproven_computed_loop_render_sequence.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_render_sequence_plan_report(
            unproven_computed_loop_render_sequence
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_loop_exit_cleanup =
        orison::lowering::plan_computed_dynamic_array_iterable_loop_exit_cleanup(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_loop_exit_cleanup.kind ==
        orison::lowering::ComputedDynamicArrayIterableLoopExitCleanupPlanKind::
            cleanup_owner_unproven
    );
    assert(unproven_computed_loop_exit_cleanup.cleanup_owner_name == "predicted_items");
    assert(unproven_computed_loop_exit_cleanup.rendered_ir.empty());
    assert(!unproven_computed_loop_exit_cleanup.exit_block_planned);
    assert(!unproven_computed_loop_exit_cleanup.cleanup_resumption_planned);
    assert(!unproven_computed_loop_exit_cleanup.cleanup_sequence_enabled);
    assert(!unproven_computed_loop_exit_cleanup.render_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_loop_exit_cleanup_plan_report(
            unproven_computed_loop_exit_cleanup
        ).find("cleanup owner unproven") != std::string::npos
    );
    auto unproven_computed_production_emission_gate =
        orison::lowering::plan_computed_dynamic_array_iterable_production_emission_gate(
            ternary(name("flag"), name("predicted_items"), name("predicted_items")),
            context,
            state
        );
    assert(
        unproven_computed_production_emission_gate.kind ==
        orison::lowering::ComputedDynamicArrayIterableProductionEmissionGatePlanKind::
            cleanup_owner_unproven
    );
    assert(unproven_computed_production_emission_gate.cleanup_owner_name == "predicted_items");
    assert(!unproven_computed_production_emission_gate.ownership_ready);
    assert(!unproven_computed_production_emission_gate.loop_render_ready);
    assert(!unproven_computed_production_emission_gate.exit_cleanup_ready);
    assert(unproven_computed_production_emission_gate.rendered_ir.empty());
    assert(!unproven_computed_production_emission_gate.production_sequence_render_planned);
    assert(!unproven_computed_production_emission_gate.production_emission_enabled);
    assert(
        orison::lowering::computed_dynamic_array_iterable_production_emission_gate_plan_report(
            unproven_computed_production_emission_gate
        ).find("cleanup owner unproven") != std::string::npos
    );

    auto not_dynamic_array_plan =
        orison::lowering::plan_dynamic_array_iterable_descriptor(name("left_values"), context, state);
    assert(
        not_dynamic_array_plan.kind ==
        orison::lowering::DynamicArrayIterableDescriptorPlanKind::not_dynamic_array
    );
    assert(!not_dynamic_array_plan.can_lower_now);
    assert(!not_dynamic_array_plan.cleanup_owner_proven);
    assert(
        not_dynamic_array_plan.cleanup_owner_proof_status ==
        orison::lowering::DynamicArrayIterableCleanupOwnerProofStatus::not_dynamic_array
    );

    auto array = orison::lowering::parse_llvm_array_type("[3 x i32]");
    assert(array.has_value());
    assert(array->element_type == "i32");
    assert(array->length == 3);

    assert(orison::lowering::source_type_name_for_llvm_type("%record.Bucket", context) == "Bucket");
    assert(orison::lowering::source_type_name_for_llvm_type("[3 x i32]", context) == "Array<UInt32, 3>");
    assert(orison::lowering::find_record_field(context.records.at("Wrapper"), "bucket") != nullptr);

    auto bucket_type = orison::lowering::lowered_type_for_source_type_name("Bucket", context);
    assert(bucket_type.has_value());
    assert(bucket_type->type == "%record.Bucket");

    auto array_type = orison::lowering::llvm_type_for_source_type_name("Array<Bucket, 2>", context);
    assert(array_type.has_value());
    assert(*array_type == "[2 x %record.Bucket]");

    auto maybe_scalar_type = orison::lowering::llvm_type_for_source_type_name("Maybe<UInt32>", context);
    assert(maybe_scalar_type.has_value());
    assert(*maybe_scalar_type == "{ i1, i32 }");

    auto maybe_record_type = orison::lowering::llvm_type_for_source_type_name("Maybe<Bucket>", context);
    assert(maybe_record_type.has_value());
    assert(*maybe_record_type == "{ i1, %record.Bucket }");

    auto maybe_array_type =
        orison::lowering::llvm_type_for_source_type_name("Maybe<Array<UInt32, 3>>", context);
    assert(maybe_array_type.has_value());
    assert(*maybe_array_type == "{ i1, [3 x i32] }");

    auto view_type = orison::lowering::llvm_type_for_source_type_name("shared.View<UInt32>", context);
    assert(view_type.has_value());
    assert(*view_type == std::string {orison::lowering::view_descriptor_llvm_type()});
    auto dynamic_array_type =
        orison::lowering::llvm_type_for_source_type_name("DynamicArray<UInt32>", context);
    assert(dynamic_array_type.has_value());
    assert(*dynamic_array_type == std::string {orison::lowering::dynamic_array_descriptor_llvm_type()});
    auto owned_dynamic_array_type =
        orison::lowering::llvm_type_for_source_type_name("DynamicArray<Bucket>", context);
    assert(owned_dynamic_array_type.has_value());
    assert(*owned_dynamic_array_type == std::string {orison::lowering::dynamic_array_descriptor_llvm_type()});

    assert(!orison::lowering::llvm_type_for_source_type_name("Maybe<Unit>", context).has_value());

    assert(orison::lowering::source_type_name_for_expression(name("wrapper"), context, state) == "Wrapper");
    assert(
        orison::lowering::source_type_name_for_expression(
            member(name("wrapper"), "bucket"),
            context,
            state
        ) == "Bucket"
    );
    assert(orison::lowering::source_type_name_for_expression(index(name("buckets")), context, state) == "Bucket");
    assert(orison::lowering::source_type_name_for_expression(index(name("bucket_array_pointer")), context, state) == "Bucket");
    assert(orison::lowering::source_type_name_for_expression(call("make_bucket"), context, state) == "Bucket");
    assert(
        orison::lowering::source_type_name_for_expression(
            null_safe_method_call(name("box"), "bump", cast(integer_literal("5"), "UInt32")),
            context,
            state
        ) == "Maybe<Box<UInt32>>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            null_safe_member(
                null_safe_method_call(name("box"), "bump", cast(integer_literal("5"), "UInt32")),
                "value"
            ),
            context,
            state
        ) == "Maybe<UInt32>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            null_safe_method_call(name("box"), "pair", cast(integer_literal("7"), "UInt32")),
            context,
            state
        ) == "Maybe<Array<Box<UInt32>, 2>>"
    );
    assert(
        !orison::lowering::source_type_name_for_expression(
            index(null_safe_method_call(name("box"), "pair", cast(integer_literal("7"), "UInt32"))),
            context,
            state
        ).has_value()
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            cast(integer_literal("1"), "UInt32"),
            context,
            state
        ) == "UInt32"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            array_literal(cast(integer_literal("1"), "UInt32"), cast(integer_literal("2"), "UInt32")),
            context,
            state
        ) == "Array<UInt32, 2>"
    );
    assert(orison::lowering::source_type_name_for_expression(record_constructor("Bucket"), context, state) == "Bucket");
    assert(
        orison::lowering::source_type_name_for_expression(
            call("Pointer", call("address_of", member(name("wrapper"), "bucket"))),
            context,
            state
        ) == "Pointer<Bucket>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            call("Pointer", call("address_of", member(member(name("wrapper_pointer"), "bucket"), "values"))),
            context,
            state
        ) == "Pointer<Array<UInt32, 3>>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            call(
                "raw_offset",
                call("Pointer", call("address_of", member(name("wrapper"), "bucket"))),
                cast(integer_literal("1"), "UInt64")
            ),
            context,
            state
        ) == "Pointer<Bucket>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            array_literal(record_constructor("Bucket"), record_constructor("Bucket")),
            context,
            state
        ) == "Array<Bucket, 2>"
    );
    assert(
        orison::lowering::source_type_name_for_initializer(
            name("wrapper"),
            context,
            state,
            "%record.Wrapper"
        ) == "Wrapper"
    );
    assert(
        orison::lowering::source_type_name_for_initializer(
            cast(integer_literal("1"), "UInt32"),
            context,
            state,
            "i32"
        ) == "UInt32"
    );
    assert(
        orison::lowering::source_type_name_for_initializer(
            integer_literal("1"),
            context,
            state,
            "i32"
        ) == "UInt32"
    );
    assert(
        orison::lowering::source_type_name_for_value_statement(
            if_statement(
                two_statement_block(
                    let_statement("bucket", record_constructor("Bucket")),
                    expression_statement(member(name("bucket"), "values"))
                ),
                two_statement_block(
                    let_statement("bucket", record_constructor("Bucket")),
                    expression_statement(member(name("bucket"), "values"))
                )
            ),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        orison::lowering::source_type_name_for_value_statement(
            switch_statement(
                one_statement_block(expression_statement(name("left_values"))),
                one_statement_block(expression_statement(name("right_values")))
            ),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        !orison::lowering::source_type_name_for_value_statement(
            if_statement(
                one_statement_block(expression_statement(name("wrapper"))),
                one_statement_block(expression_statement(name("right_values")))
            ),
            context,
            state
        ).has_value()
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            method_call(member(name("wrapper"), "bucket"), "view"),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            ternary(name("flag"), name("left_values"), name("right_values")),
            context,
            state
        ) == "Array<UInt32, 3>"
    );
    assert(
        orison::lowering::source_type_name_for_expression(
            ternary(
                name("flag"),
                array_literal(cast(integer_literal("1"), "UInt32"), cast(integer_literal("2"), "UInt32")),
                array_literal(cast(integer_literal("3"), "UInt32"), cast(integer_literal("4"), "UInt32"))
            ),
            context,
            state
        ) == "Array<UInt32, 2>"
    );
    assert(
        !orison::lowering::source_type_name_for_expression(
            array_literal(cast(integer_literal("1"), "UInt32"), cast(integer_literal("2"), "UInt64")),
            context,
            state
        ).has_value()
    );
    assert(
        !orison::lowering::source_type_name_for_expression(
            ternary(name("flag"), name("wrapper"), name("right_values")),
            context,
            state
        ).has_value()
    );

    return 0;
}
