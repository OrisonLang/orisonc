#include "orison/lowering/addressable_binding.hpp"

#include <cassert>
#include <optional>
#include <sstream>
#include <string>

int main() {
    auto state = orison::lowering::FunctionLoweringState {};
    auto failures = orison::lowering::LoweringFailures {};
    auto session = orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };
    auto output = std::ostringstream {};

    assert(!orison::lowering::is_aggregate_llvm_type("i32"));
    assert(orison::lowering::is_aggregate_llvm_type("%record.Point"));
    assert(orison::lowering::is_aggregate_llvm_type("[2 x i32]"));

    orison::lowering::bind_addressable_aggregate_value(
        "scalar",
        orison::lowering::LoweredExpression {
            .type = "i32",
            .value = "%scalar",
            .signedness = orison::lowering::IntegerSignedness::signed_integer,
        },
        session,
        output
    );
    assert(output.str().empty());
    assert(state.addressable_bindings.empty());

    orison::lowering::bind_addressable_aggregate_value(
        "point",
        orison::lowering::LoweredExpression {
            .type = "%record.Point",
            .value = "%point",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        session,
        output
    );
    assert(output.str() ==
           "  %point.addr = alloca %record.Point\n"
           "  store %record.Point %point, ptr %point.addr\n");
    assert(state.addressable_bindings.size() == 1);
    assert(state.addressable_bindings.at("point").type.type == "%record.Point");
    assert(state.addressable_bindings.at("point").storage == "%point.addr");
    assert(orison::lowering::aggregate_storage_for_name("missing", state) == std::nullopt);
    assert(orison::lowering::named_aggregate_storage_for_name("missing", state) == std::nullopt);
    auto point_storage = orison::lowering::aggregate_storage_for_name("point", state);
    assert(point_storage.has_value());
    assert(*point_storage == "%point.addr");
    auto point_without_source = orison::lowering::named_aggregate_storage_for_name("point", state);
    assert(point_without_source.has_value());
    assert(point_without_source->storage == "%point.addr");
    assert(!point_without_source->source_type_name.has_value());

    state.source_type_names["point"] = "Point";
    auto point_with_source = orison::lowering::named_aggregate_storage_for_name("point", state);
    assert(point_with_source.has_value());
    assert(point_with_source->storage == "%point.addr");
    assert(point_with_source->source_type_name == "Point");

    state.mutable_bindings["point"] = orison::lowering::MutableBinding {
        .type = orison::lowering::LoweredType {
            .type = "%record.Point",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        .storage = "%point.mutable.addr",
    };
    auto mutable_point_storage = orison::lowering::aggregate_storage_for_name("point", state);
    assert(mutable_point_storage.has_value());
    assert(*mutable_point_storage == "%point.mutable.addr");
    auto mutable_point_with_source = orison::lowering::named_aggregate_storage_for_name("point", state);
    assert(mutable_point_with_source.has_value());
    assert(mutable_point_with_source->storage == "%point.mutable.addr");
    assert(mutable_point_with_source->source_type_name == "Point");

    output.str({});
    output.clear();
    auto scalar_spill = orison::lowering::spill_aggregate_value_to_temporary_storage(
        orison::lowering::LoweredExpression {
            .type = "i32",
            .value = "%scalar",
            .signedness = orison::lowering::IntegerSignedness::signed_integer,
        },
        session,
        output
    );
    assert(!scalar_spill.has_value());
    assert(output.str().empty());

    auto array_spill = orison::lowering::spill_aggregate_value_to_temporary_storage(
        orison::lowering::LoweredExpression {
            .type = "[2 x i32]",
            .value = "%values",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        session,
        output
    );
    assert(array_spill.has_value());
    assert(*array_spill == "%tmp0");
    assert(output.str() ==
           "  %tmp0 = alloca [2 x i32]\n"
           "  store [2 x i32] %values, ptr %tmp0\n");
    assert(state.addressable_bindings.size() == 1);
    assert(!state.addressable_bindings.contains("values"));
    assert(state.next_temporary_index == 1);

    return 0;
}
