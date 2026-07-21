#include "orison/lowering/ownership_transfer.hpp"

#include <cassert>
#include <utility>

int main() {
    auto transfers = orison::lowering::OwnershipTransferState {};
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "items"));

    orison::lowering::mark_owned_binding_consumed(transfers, "items");
    assert(orison::lowering::is_owned_binding_consumed(transfers, "items"));
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "other"));

    auto matching = orison::lowering::merge_ownership_transfer_states({
        transfers,
        transfers,
    });
    assert(matching.has_value());
    assert(orison::lowering::is_owned_binding_consumed(*matching, "items"));

    auto different = orison::lowering::OwnershipTransferState {};
    orison::lowering::mark_owned_binding_consumed(different, "other");
    auto mismatched = orison::lowering::merge_ownership_transfer_states({
        std::move(transfers),
        std::move(different),
    });
    assert(!mismatched.has_value());

    auto empty = orison::lowering::merge_ownership_transfer_states({});
    assert(empty.has_value());
    assert(!orison::lowering::is_owned_binding_consumed(*empty, "items"));
    return 0;
}
