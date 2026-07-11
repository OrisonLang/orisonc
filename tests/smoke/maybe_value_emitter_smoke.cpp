#include "orison/lowering/maybe_value_emitter.hpp"

#include <cassert>
#include <sstream>

int main() {
    auto context = orison::lowering::LoweringContext {};
    context.records.emplace("Bucket", orison::lowering::LoweredRecordLayout {
        .name = "Bucket",
        .llvm_type_name = "%record.Bucket",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "value",
                .source_type_name = "UInt32",
                .llvm_type = "i32",
                .index = 0,
            },
        },
    });

    auto scalar_abi = orison::lowering::maybe_value_abi_for_source_type("Maybe<UInt32>", context);
    assert(scalar_abi.has_value());
    assert(scalar_abi->source_type_name == "Maybe<UInt32>");
    assert(scalar_abi->payload_source_type_name == "UInt32");
    assert(scalar_abi->llvm_type == "{ i1, i32 }");
    assert(scalar_abi->payload_llvm_type == "i32");

    auto record_abi = orison::lowering::maybe_value_abi_for_source_type("Maybe<Bucket>", context);
    assert(record_abi.has_value());
    assert(record_abi->llvm_type == "{ i1, %record.Bucket }");
    assert(record_abi->payload_llvm_type == "%record.Bucket");

    auto array_abi = orison::lowering::maybe_value_abi_for_source_type(
        "Maybe<Array<UInt32, 3>>",
        context
    );
    assert(array_abi.has_value());
    assert(array_abi->llvm_type == "{ i1, [3 x i32] }");
    assert(array_abi->payload_llvm_type == "[3 x i32]");

    auto scalar_llvm_abi = orison::lowering::maybe_value_abi_for_llvm_type("{ i1, i32 }", context);
    assert(scalar_llvm_abi.has_value());
    assert(scalar_llvm_abi->source_type_name == "Maybe<UInt32>");
    assert(scalar_llvm_abi->payload_source_type_name == "UInt32");
    assert(scalar_llvm_abi->llvm_type == "{ i1, i32 }");
    assert(scalar_llvm_abi->payload_llvm_type == "i32");

    auto record_llvm_abi = orison::lowering::maybe_value_abi_for_llvm_type("{ i1, %record.Bucket }", context);
    assert(record_llvm_abi.has_value());
    assert(record_llvm_abi->source_type_name == "Maybe<Bucket>");
    assert(record_llvm_abi->payload_llvm_type == "%record.Bucket");

    assert(!orison::lowering::maybe_value_abi_for_llvm_type("i32", context).has_value());

    auto next_temporary_index = std::size_t {0};
    auto empty_output = std::ostringstream {};
    auto empty = orison::lowering::emit_empty_maybe_value(
        "Maybe<UInt32>",
        context,
        next_temporary_index,
        empty_output
    );
    assert(empty.has_value());
    assert(empty->type == "{ i1, i32 }");
    assert(empty->value == "%tmp0");
    assert(next_temporary_index == 1);
    assert(empty_output.str() == "  %tmp0 = insertvalue { i1, i32 } undef, i1 false, 0\n");

    auto some_output = std::ostringstream {};
    auto some = orison::lowering::emit_some_maybe_value(
        "Maybe<UInt32>",
        orison::lowering::LoweredExpression {
            .type = "i32",
            .value = "%value",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
        context,
        next_temporary_index,
        some_output
    );
    assert(some.has_value());
    assert(some->type == "{ i1, i32 }");
    assert(some->value == "%tmp2");
    assert(next_temporary_index == 3);
    assert(
        some_output.str() ==
        "  %tmp1 = insertvalue { i1, i32 } undef, i1 true, 0\n"
        "  %tmp2 = insertvalue { i1, i32 } %tmp1, i32 %value, 1\n"
    );

    assert(!orison::lowering::maybe_value_abi_for_source_type("UInt32", context).has_value());
    assert(!orison::lowering::maybe_value_abi_for_source_type("Maybe<Unit>", context).has_value());

    auto mismatch_output = std::ostringstream {};
    auto mismatch = orison::lowering::emit_some_maybe_value(
        "Maybe<UInt32>",
        orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "true",
        },
        context,
        next_temporary_index,
        mismatch_output
    );
    assert(!mismatch.has_value());
    assert(mismatch_output.str().empty());
    assert(next_temporary_index == 3);

    return 0;
}
