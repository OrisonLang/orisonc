#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <sstream>
#include <vector>

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

using DeferredCleanupBlockLowerer = StatementFlow (*)(
    std::vector<syntax::StatementSyntax const*> const& statements,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
);

auto emit_deferred_cleanup_to_depth_with_block_lowerer(
    std::size_t target_depth,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> bool;

}  // namespace orison::lowering
