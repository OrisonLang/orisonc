#include "orison/lowering/immutable_binding_scope.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    auto state = orison::lowering::FunctionLoweringState {};
    state.immutable_bindings.emplace("outer", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%outer",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    state.local_name_counts["outer"] = 1;
    state.next_temporary_index = 4;
    state.next_block_index = 2;
    state.current_block = "if.then.1";

    {
        auto scope = orison::lowering::ImmutableBindingScope(state);
        state.immutable_bindings.emplace("branch", orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "%branch",
        });
        state.local_name_counts["branch"] = 1;
        state.next_temporary_index = 5;
        state.next_block_index = 3;
        state.current_block = "if.else.1";

        scope.reset();
        assert(state.immutable_bindings.size() == 1);
        assert(state.immutable_bindings.contains("outer"));
        assert(!state.immutable_bindings.contains("branch"));
        assert(state.local_name_counts.contains("branch"));
        assert(state.next_temporary_index == 5);
        assert(state.next_block_index == 3);
        assert(state.current_block == "if.else.1");

        state.immutable_bindings.emplace("sibling", orison::lowering::LoweredExpression {
            .type = "i32",
            .value = "%sibling",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        });
    }

    assert(state.immutable_bindings.size() == 1);
    assert(state.immutable_bindings.contains("outer"));
    assert(!state.immutable_bindings.contains("sibling"));
    assert(state.local_name_counts.contains("branch"));
    assert(state.next_temporary_index == 5);
    assert(state.next_block_index == 3);
    assert(state.current_block == "if.else.1");

    try {
        auto scope = orison::lowering::ImmutableBindingScope(state);
        state.immutable_bindings.emplace("unwound", orison::lowering::LoweredExpression {
            .type = "i1",
            .value = "%unwound",
        });
        throw std::runtime_error("test unwind");
    } catch (std::runtime_error const&) {
    }
    assert(state.immutable_bindings.size() == 1);
    assert(state.immutable_bindings.contains("outer"));
    assert(!state.immutable_bindings.contains("unwound"));
    return 0;
}
