#include "orison/lowering/merge_emitter.hpp"

#include <cassert>
#include <sstream>

int main() {
    auto state = orison::lowering::FunctionLoweringState {
        .next_temporary_index = 4,
        .current_block = "case.exit",
    };
    auto output = std::ostringstream {};
    auto value = orison::lowering::emit_branch_merge_value(
        "switch.merge.2",
        {
            .result_type = {
                .type = "i32",
                .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
            },
            .incoming = {
                {
                    .value = "%left",
                    .block = "switch.case.2.0",
                },
                {
                    .value = "%right",
                    .block = "switch.default.2",
                },
            },
        },
        state,
        output
    );

    assert(value.type == "i32");
    assert(value.value == "%tmp4");
    assert(value.signedness == orison::lowering::IntegerSignedness::unsigned_integer);
    assert(state.current_block == "switch.merge.2");
    assert(state.next_temporary_index == 5);
    assert(
        output.str() ==
        "switch.merge.2:\n"
        "  %tmp4 = phi i32 [%left, %switch.case.2.0], [%right, %switch.default.2]\n"
    );
    return 0;
}
