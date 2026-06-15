#include "orison/lowering/branch_binding_scope.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    auto state = orison::lowering::FunctionLoweringState {};
    state.immutable_bindings.emplace("outer", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%outer",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    state.mutable_bindings.emplace("outer_mutable", orison::lowering::MutableBinding {
        .type = {
            .type = "i32",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
        .storage = "%outer_mutable.addr",
    });
    state.source_type_names["outer"] = "Outer";
    state.source_type_names["outer_mutable"] = "OuterMutable";
    state.local_name_counts["outer"] = 1;
    state.next_temporary_index = 4;
    state.next_block_index = 2;
    state.current_block = "if.then.1";

    {
        auto scope = orison::lowering::BranchBindingScope(state);
        state.immutable_bindings.emplace("branch", orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "%branch",
        });
        state.mutable_bindings.emplace("branch_mutable", orison::lowering::MutableBinding {
            .type = {
                .type = "i1",
            },
            .storage = "%branch_mutable.addr",
        });
        state.source_type_names["branch"] = "Branch";
        state.source_type_names["branch_mutable"] = "BranchMutable";
        state.local_name_counts["branch"] = 1;
        state.next_temporary_index = 5;
        state.next_block_index = 3;
        state.current_block = "if.else.1";

        scope.reset();
        assert(state.immutable_bindings.size() == 1);
        assert(state.immutable_bindings.contains("outer"));
        assert(!state.immutable_bindings.contains("branch"));
        assert(state.mutable_bindings.size() == 1);
        assert(state.mutable_bindings.contains("outer_mutable"));
        assert(!state.mutable_bindings.contains("branch_mutable"));
        assert(state.source_type_names.size() == 2);
        assert(state.source_type_names.contains("outer"));
        assert(state.source_type_names.contains("outer_mutable"));
        assert(!state.source_type_names.contains("branch"));
        assert(!state.source_type_names.contains("branch_mutable"));
        assert(state.local_name_counts.contains("branch"));
        assert(state.next_temporary_index == 5);
        assert(state.next_block_index == 3);
        assert(state.current_block == "if.else.1");

        state.immutable_bindings.emplace("sibling", orison::lowering::LoweredExpression {
            .type = "i32",
            .value = "%sibling",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        });
        state.mutable_bindings.emplace("sibling_mutable", orison::lowering::MutableBinding {
            .type = {
                .type = "i32",
            },
            .storage = "%sibling_mutable.addr",
        });
        state.source_type_names["sibling"] = "Sibling";
        state.source_type_names["sibling_mutable"] = "SiblingMutable";
    }

    assert(state.immutable_bindings.size() == 1);
    assert(state.immutable_bindings.contains("outer"));
    assert(!state.immutable_bindings.contains("sibling"));
    assert(state.mutable_bindings.size() == 1);
    assert(state.mutable_bindings.contains("outer_mutable"));
    assert(!state.mutable_bindings.contains("sibling_mutable"));
    assert(state.source_type_names.size() == 2);
    assert(state.source_type_names.contains("outer"));
    assert(state.source_type_names.contains("outer_mutable"));
    assert(!state.source_type_names.contains("sibling"));
    assert(!state.source_type_names.contains("sibling_mutable"));
    assert(state.local_name_counts.contains("branch"));
    assert(state.next_temporary_index == 5);
    assert(state.next_block_index == 3);
    assert(state.current_block == "if.else.1");

    try {
        auto scope = orison::lowering::BranchBindingScope(state);
        state.immutable_bindings.emplace("unwound", orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "%unwound",
        });
        state.mutable_bindings.emplace("unwound_mutable", orison::lowering::MutableBinding {
            .type = {
                .type = "i1",
            },
            .storage = "%unwound_mutable.addr",
        });
        state.source_type_names["unwound"] = "Unwound";
        state.source_type_names["unwound_mutable"] = "UnwoundMutable";
        throw std::runtime_error("test unwind");
    } catch (std::runtime_error const&) {
    }
    assert(state.immutable_bindings.size() == 1);
    assert(state.immutable_bindings.contains("outer"));
    assert(!state.immutable_bindings.contains("unwound"));
    assert(state.mutable_bindings.size() == 1);
    assert(state.mutable_bindings.contains("outer_mutable"));
    assert(!state.mutable_bindings.contains("unwound_mutable"));
    assert(state.source_type_names.size() == 2);
    assert(state.source_type_names.contains("outer"));
    assert(state.source_type_names.contains("outer_mutable"));
    assert(!state.source_type_names.contains("unwound"));
    assert(!state.source_type_names.contains("unwound_mutable"));
    return 0;
}
