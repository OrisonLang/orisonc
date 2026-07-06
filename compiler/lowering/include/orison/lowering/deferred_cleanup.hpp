#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowering_emission_context.hpp"

#include <cstddef>
#include <sstream>

namespace orison::lowering {

class DeferredCleanupScope {
public:
    explicit DeferredCleanupScope(FunctionLoweringState& state);
    ~DeferredCleanupScope() noexcept;

    DeferredCleanupScope(DeferredCleanupScope const&) = delete;
    auto operator=(DeferredCleanupScope const&) -> DeferredCleanupScope& = delete;
    DeferredCleanupScope(DeferredCleanupScope&&) = delete;
    auto operator=(DeferredCleanupScope&&) -> DeferredCleanupScope& = delete;

    auto cleanup_depth() const -> std::size_t;

private:
    FunctionLoweringState& state_;
    std::size_t cleanup_depth_ = 0;
};

auto emit_deferred_cleanup_to_depth(
    std::size_t target_depth,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

}  // namespace orison::lowering
