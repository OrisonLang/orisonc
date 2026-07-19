#include "orison/lowering/dynamic_array_cleanup_plan.hpp"

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_failures.hpp"
#include "orison/lowering/string_constants.hpp"

#include <cassert>
#include <sstream>
#include <string>

namespace {

auto test_context() -> orison::lowering::LoweringContext {
    auto context = orison::lowering::LoweringContext {};
    context.records.emplace("Payload", orison::lowering::LoweredRecordLayout {
        .name = "Payload",
        .llvm_type_name = "%record.Payload",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "value",
                .source_type_name = "Int64",
                .llvm_type = "i64",
                .index = 0,
            },
        },
    });
    return context;
}

auto test_session(orison::lowering::FunctionLoweringState& state, orison::lowering::LoweringFailures& failures)
    -> orison::lowering::FunctionLoweringSession {
    return orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };
}

void bind_dynamic_array_parameter(
    orison::lowering::FunctionLoweringState& state,
    std::string name,
    std::string source_type_name
) {
    state.source_type_names.emplace(name, source_type_name);
    state.addressable_bindings.emplace(name, orison::lowering::AddressableBinding {
        .type = orison::lowering::LoweredType {
            .type = "{ ptr, i64, i64 }",
        },
        .storage = "%" + name + ".addr",
    });
}

void test_plans_bound_dynamic_array_parameter_cleanups_in_name_order() {
    auto lowering = test_context();
    auto strings = orison::lowering::StringConstantTable {};
    auto state = orison::lowering::FunctionLoweringState {};
    auto failures = orison::lowering::LoweringFailures {};
    bind_dynamic_array_parameter(state, "z_items", "DynamicArray<UInt32>");
    bind_dynamic_array_parameter(state, "a_items", "DynamicArray<UInt32>");
    auto session = test_session(state, failures);
    auto context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = strings,
        .options = orison::lowering::LlvmIrEmissionOptions {
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        },
    };

    auto plans = orison::lowering::plan_bound_dynamic_array_parameter_cleanups(context, session);
    assert(plans.has_value());
    assert(plans->size() == 2);
    assert((*plans)[0].descriptor_cleanup.owner_name == "a_items");
    assert((*plans)[0].descriptor_cleanup.descriptor_storage_name == "%a_items.addr");
    assert(!(*plans)[0].element_drop_symbol_name.has_value());
    assert((*plans)[1].descriptor_cleanup.owner_name == "z_items");
    assert((*plans)[1].descriptor_cleanup.descriptor_storage_name == "%z_items.addr");
    assert(!(*plans)[1].element_drop_symbol_name.has_value());

    auto output = std::ostringstream {};
    assert(orison::lowering::emit_bound_dynamic_array_parameter_cleanups(context, session, output));
    auto ir = output.str();
    auto a_cleanup = ir.find("%a_items.dynamic_array_cleanup0.descriptor");
    auto z_cleanup = ir.find("%z_items.dynamic_array_cleanup1.descriptor");
    assert(a_cleanup != std::string::npos);
    assert(z_cleanup != std::string::npos);
    assert(a_cleanup < z_cleanup);
    assert(ir.find("call void @__orison_drop.Payload") == std::string::npos);
    assert(ir.find("call void @__orison_dynamic_array_deallocate") != std::string::npos);
    assert(state.next_temporary_index == 2);
}

void test_suppresses_unauthorized_owned_element_cleanup() {
    auto lowering = test_context();
    auto strings = orison::lowering::StringConstantTable {};
    auto state = orison::lowering::FunctionLoweringState {};
    auto failures = orison::lowering::LoweringFailures {};
    bind_dynamic_array_parameter(state, "items", "DynamicArray<Payload>");
    auto session = test_session(state, failures);
    auto context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = strings,
        .options = orison::lowering::LlvmIrEmissionOptions {
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
        },
    };

    auto plans = orison::lowering::plan_bound_dynamic_array_parameter_cleanups(context, session);
    assert(plans.has_value());
    assert(plans->empty());

    auto output = std::ostringstream {};
    assert(orison::lowering::emit_bound_dynamic_array_parameter_cleanups(context, session, output));
    assert(output.str().empty());
    assert(state.next_temporary_index == 0);
}

void test_authorizes_owned_element_cleanup() {
    auto lowering = test_context();
    auto strings = orison::lowering::StringConstantTable {};
    auto state = orison::lowering::FunctionLoweringState {};
    auto failures = orison::lowering::LoweringFailures {};
    bind_dynamic_array_parameter(state, "items", "DynamicArray<Payload>");
    auto session = test_session(state, failures);
    auto context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = strings,
        .options = orison::lowering::LlvmIrEmissionOptions {
            .test_only_enable_dynamic_array_parameter_descriptors = true,
            .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        },
    };

    auto plans = orison::lowering::plan_bound_dynamic_array_parameter_cleanups(context, session);
    assert(plans.has_value());
    assert(plans->size() == 1);
    assert(plans->front().descriptor_cleanup.owner_name == "items");
    assert(plans->front().descriptor_cleanup.element_source_type_name == "Payload");
    assert(plans->front().element_drop_symbol_name == "__orison_drop.Payload");

    auto output = std::ostringstream {};
    assert(orison::lowering::emit_bound_dynamic_array_parameter_cleanups(context, session, output));
    auto ir = output.str();
    auto drop_call = ir.find("call void @__orison_drop.Payload");
    auto deallocate_call = ir.find("call void @__orison_dynamic_array_deallocate");
    assert(drop_call != std::string::npos);
    assert(deallocate_call != std::string::npos);
    assert(drop_call < deallocate_call);
    assert(state.next_temporary_index == 1);
}

void test_plans_descriptor_cleanup_obligations() {
    auto lowering = test_context();
    auto scalar_plan = orison::lowering::plan_dynamic_array_descriptor_cleanup(
        "numbers",
        "DynamicArray<UInt32>",
        lowering
    );
    auto owned_plan = orison::lowering::plan_dynamic_array_descriptor_cleanup(
        "items",
        "DynamicArray<Payload>",
        lowering
    );
    assert(scalar_plan.has_value());
    assert(owned_plan.has_value());
    owned_plan->source_line = 22;

    auto obligations = orison::lowering::plan_dynamic_array_descriptor_cleanup_obligations(
        {*scalar_plan, *owned_plan},
        3
    );
    assert(obligations.size() == 2);
    assert(obligations[0].cleanup_symbol_name == "__orison_dynamic_array_cleanup.3");
    assert(obligations[0].descriptor_cleanup.owner_name == "numbers");
    assert(obligations[0].actions.empty());
    assert(obligations[0].requires_descriptor_deallocation);
    assert(obligations[1].cleanup_symbol_name == "__orison_dynamic_array_cleanup.4");
    assert(obligations[1].descriptor_cleanup.owner_name == "items");
    assert(obligations[1].actions.size() == 1);
    assert(obligations[1].actions.front().capture_name == "items.element");
    assert(obligations[1].actions.front().symbol_name == "__orison_drop.Payload");
    assert(obligations[1].actions.front().discovery_line == 22);

    auto cleanup = orison::lowering::drop_cleanup_for_dynamic_array_cleanup_obligation(obligations[1]);
    assert(cleanup.cleanup_symbol_name == "__orison_dynamic_array_cleanup.4");
    assert(cleanup.actions.size() == 1);
    assert(cleanup.requires_semantic_authorization);
    assert(cleanup.requires_descriptor_deallocation);

    auto sequence_plans = orison::lowering::plan_dynamic_array_cleanup_sequences(obligations);
    assert(sequence_plans.size() == 2);
    assert(sequence_plans[0].phases.size() == 2);
    assert(sequence_plans[0].phases[0] == "load descriptor");
    assert(sequence_plans[0].phases[1] == "deallocate descriptor storage");
    assert(sequence_plans[1].phases.size() == 3);
    assert(sequence_plans[1].phases[0] == "load descriptor");
    assert(sequence_plans[1].phases[1] == "drop initialized elements");
    assert(sequence_plans[1].phases[2] == "deallocate descriptor storage");

    auto report = orison::lowering::format_dynamic_array_cleanup_obligation_report(obligations);
    assert(report.size() == 2);
    assert(
        report[0] ==
        "dynamic array cleanup obligation __orison_dynamic_array_cleanup.3 owner numbers "
        "source DynamicArray<UInt32> element UInt32 descriptor %numbers.addr actions 0 "
        "descriptor deallocation required (metadata only)"
    );
    assert(
        report[1] ==
        "dynamic array cleanup obligation __orison_dynamic_array_cleanup.4 owner items "
        "source DynamicArray<Payload> element Payload descriptor %items.addr origin line 22 actions 1 "
        "descriptor deallocation required (metadata only)"
    );

    auto sequence_report = orison::lowering::format_dynamic_array_cleanup_sequence_plan_report(sequence_plans);
    assert(sequence_report.size() == 2);
    assert(
        sequence_report[0] ==
        "dynamic array cleanup sequence __orison_dynamic_array_cleanup.3 owner numbers phases "
        "[load descriptor] [deallocate descriptor storage] (metadata only)"
    );
    assert(
        sequence_report[1] ==
        "dynamic array cleanup sequence __orison_dynamic_array_cleanup.4 owner items phases "
        "[load descriptor] [drop initialized elements] [deallocate descriptor storage] (metadata only)"
    );

    auto verifications = orison::lowering::verify_dynamic_array_cleanup_sequence_plans(sequence_plans);
    assert(verifications.size() == 2);
    assert(orison::lowering::dynamic_array_cleanup_sequence_verification_report_passed(verifications));
    auto verification_report =
        orison::lowering::format_dynamic_array_cleanup_sequence_verification_report(verifications);
    assert(verification_report.size() == 2);
    assert(
        verification_report[0] ==
        "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.3 passed (metadata only)"
    );
    assert(
        verification_report[1] ==
        "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.4 passed (metadata only)"
    );

    auto malformed_owned = sequence_plans[1];
    malformed_owned.phases = {"load descriptor", "deallocate descriptor storage"};
    auto malformed_owned_verification =
        orison::lowering::verify_dynamic_array_cleanup_sequence_plan(malformed_owned);
    assert(!orison::lowering::dynamic_array_cleanup_sequence_verification_passed(malformed_owned_verification));
    assert(malformed_owned_verification.errors.size() == 1);
    assert(
        malformed_owned_verification.errors.front() ==
        "owned element cleanup requires initialized-element drop phase"
    );

    auto malformed_scalar = sequence_plans[0];
    malformed_scalar.phases = {"drop initialized elements", "deallocate descriptor storage"};
    auto malformed_scalar_report = orison::lowering::format_dynamic_array_cleanup_sequence_verification(
        orison::lowering::verify_dynamic_array_cleanup_sequence_plan(malformed_scalar)
    );
    assert(
        malformed_scalar_report ==
        "dynamic array cleanup sequence verification __orison_dynamic_array_cleanup.3 failed "
        "[first phase must load descriptor] [scalar or non-owning cleanup must not drop initialized elements] "
        "[initialized-element drop phase must follow descriptor load] (metadata only)"
    );
}

}  // namespace

auto main() -> int {
    test_plans_bound_dynamic_array_parameter_cleanups_in_name_order();
    test_suppresses_unauthorized_owned_element_cleanup();
    test_authorizes_owned_element_cleanup();
    test_plans_descriptor_cleanup_obligations();
    return 0;
}
