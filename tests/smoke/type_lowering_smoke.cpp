#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/target_layout.hpp"
#include "orison/lowering/lowering_context.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    using orison::lowering::IntegerSignedness;
    using orison::syntax::ParameterSyntax;
    using orison::syntax::TypeSyntax;

    auto int32_type = TypeSyntax {.name = "Int32"};
    assert(orison::lowering::llvm_type_for(int32_type) == "i32");
    assert(orison::lowering::integer_signedness_for(int32_type) == IntegerSignedness::signed_integer);

    auto uint64_type = TypeSyntax {.name = "UInt64"};
    assert(orison::lowering::llvm_type_for(uint64_type) == "i64");
    assert(orison::lowering::integer_signedness_for(uint64_type) == IntegerSignedness::unsigned_integer);

    auto float32_type = TypeSyntax {.name = "Float32"};
    assert(orison::lowering::llvm_type_for(float32_type) == "float");
    assert(orison::lowering::integer_signedness_for(float32_type) == IntegerSignedness::not_integer);

    auto pointer_type = TypeSyntax {
        .name = "Pointer",
        .generic_arguments = {TypeSyntax {.name = "Byte"}},
    };
    assert(orison::lowering::llvm_type_for(pointer_type) == "ptr");

    auto unsupported_generic = TypeSyntax {
        .name = "View",
        .generic_arguments = {TypeSyntax {.name = "Byte"}},
    };
    assert(!orison::lowering::llvm_type_for(unsupported_generic).has_value());

    auto parameters = std::vector<ParameterSyntax> {
        ParameterSyntax {.name = "text", .type = pointer_type},
        ParameterSyntax {.name = "value", .type = int32_type},
    };
    auto parameter_types = orison::lowering::llvm_parameter_types_for(parameters);
    assert(parameter_types.has_value());
    assert(*parameter_types == std::vector<std::string>({"ptr", "i32"}));

    auto parameter_signedness = orison::lowering::parameter_signedness_for(parameters);
    assert(parameter_signedness.size() == 2);
    assert(parameter_signedness[0] == IntegerSignedness::not_integer);
    assert(parameter_signedness[1] == IntegerSignedness::signed_integer);

    auto unit_parameter = std::vector<ParameterSyntax> {
        ParameterSyntax {.name = "invalid", .type = TypeSyntax {.name = "Unit"}},
    };
    assert(!orison::lowering::llvm_parameter_types_for(unit_parameter).has_value());

    auto pointer_size = orison::lowering::lowered_type_size_bytes("ptr");
    assert(pointer_size.has_value());
    assert(*pointer_size == 8);

    auto array_size = orison::lowering::lowered_type_size_bytes("[4 x i16]");
    assert(array_size.has_value());
    assert(*array_size == 8);

    auto padded_struct_size = orison::lowering::lowered_struct_size_bytes({"i8", "i64"});
    assert(padded_struct_size.has_value());
    assert(*padded_struct_size == 16);

    assert(!orison::lowering::lowered_type_size_bytes("%Unknown").has_value());

    auto context = orison::lowering::LoweringContext {};
    context.records.emplace("Payload", orison::lowering::LoweredRecordLayout {
        .name = "Payload",
        .llvm_type_name = "%record.Payload",
        .fields = {
            orison::lowering::LoweredRecordField {
                .name = "tag",
                .source_type_name = "UInt8",
                .llvm_type = "i8",
                .index = 0,
            },
            orison::lowering::LoweredRecordField {
                .name = "value",
                .source_type_name = "Int64",
                .llvm_type = "i64",
                .index = 1,
            },
        },
    });
    auto record_size = orison::lowering::lowered_type_size_bytes("%record.Payload", context);
    assert(record_size.has_value());
    assert(*record_size == 16);

    auto record_struct_size = orison::lowering::lowered_struct_size_bytes({"i8", "%record.Payload"}, context);
    assert(record_struct_size.has_value());
    assert(*record_struct_size == 24);

    assert(!orison::lowering::lowered_type_size_bytes("%record.Payload").has_value());
    return 0;
}
