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

}  // namespace

auto main() -> int {
    test_plans_bound_dynamic_array_parameter_cleanups_in_name_order();
    test_suppresses_unauthorized_owned_element_cleanup();
    test_authorizes_owned_element_cleanup();
    return 0;
}
