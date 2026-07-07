#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include <algorithm>
#include <sstream>

namespace orison::pipeline {
namespace {

auto same_planned_drop_action(
    lowering::PlannedDropAction const& left,
    lowering::PlannedDropAction const& right
) -> bool {
    return left.symbol_name == right.symbol_name &&
           left.source_type_name == right.source_type_name &&
           left.capture_name == right.capture_name &&
           left.field_index == right.field_index &&
           left.discovery_line == right.discovery_line;
}

void append_unique_action(
    std::vector<lowering::PlannedDropAction>& actions,
    lowering::PlannedDropAction const& action
) {
    auto const existing = std::find_if(
        actions.begin(),
        actions.end(),
        [&action](lowering::PlannedDropAction const& candidate) {
            return same_planned_drop_action(candidate, action);
        }
    );
    if (existing == actions.end()) {
        actions.push_back(action);
    }
}

auto find_semantic_authorization(
    lowering::PlannedDropAction const& action,
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> semantics::DropLoweringAuthorization const* {
    auto exact = std::find_if(
        authorizations.begin(),
        authorizations.end(),
        [&action](semantics::DropLoweringAuthorization const& authorization) {
            return authorization.site.abi_symbol_name == action.symbol_name &&
                   authorization.site.source_type_name == action.source_type_name &&
                   authorization.site.owner_name == action.capture_name;
        }
    );
    if (exact != authorizations.end()) {
        return &*exact;
    }

    auto same_type = std::find_if(
        authorizations.begin(),
        authorizations.end(),
        [&action](semantics::DropLoweringAuthorization const& authorization) {
            return authorization.site.abi_symbol_name == action.symbol_name &&
                   authorization.site.source_type_name == action.source_type_name;
        }
    );
    return same_type == authorizations.end() ? nullptr : &*same_type;
}

auto has_emitted_declaration(
    lowering::PlannedDropAction const& action,
    std::vector<lowering::PlannedDropDeclaration> const& declarations
) -> bool {
    return std::find_if(
        declarations.begin(),
        declarations.end(),
        [&action](lowering::PlannedDropDeclaration const& declaration) {
            return declaration.symbol_name == action.symbol_name &&
                   declaration.source_type_name == action.source_type_name &&
                   declaration.emit_declaration;
        }
    ) != declarations.end();
}

}  // namespace

auto format_drop_readiness_source_correlation_report(
    lowering::DropReadinessSnapshot const& snapshot
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto correlated_actions = std::vector<lowering::PlannedDropAction> {};
    for (auto const& cleanup : snapshot.cleanup_authorizations) {
        for (auto const& action : cleanup.authorization.semantic_lowering_blockers) {
            append_unique_action(correlated_actions, action);
        }
        for (auto const& action : cleanup.authorization.missing_declarations) {
            append_unique_action(correlated_actions, action);
        }
    }

    auto header = std::ostringstream {};
    header << "drop readiness source correlations actions " << correlated_actions.size()
           << " semantic sites " << snapshot.semantic_authorizations.size();
    lines.push_back(header.str());

    for (auto const& cleanup : snapshot.cleanup_authorizations) {
        auto cleanup_actions = std::vector<lowering::PlannedDropAction> {};
        for (auto const& action : cleanup.authorization.semantic_lowering_blockers) {
            append_unique_action(cleanup_actions, action);
        }
        for (auto const& action : cleanup.authorization.missing_declarations) {
            append_unique_action(cleanup_actions, action);
        }

        for (auto const& action : cleanup_actions) {
            auto const* semantic = find_semantic_authorization(action, snapshot.semantic_authorizations);
            auto line = std::ostringstream {};
            line << "drop readiness source correlation";
            if (!cleanup.cleanup_symbol_name.empty()) {
                line << " " << cleanup.cleanup_symbol_name;
            }
            line << " " << action.symbol_name;
            if (!action.source_type_name.empty()) {
                line << " for " << action.source_type_name;
            }
            line << " capture " << action.capture_name << " field " << action.field_index;
            if (action.discovery_line != 0) {
                line << " action line " << action.discovery_line;
            }
            if (semantic != nullptr) {
                if (!semantic->site.owner_name.empty()) {
                    line << " semantic owner " << semantic->site.owner_name;
                }
                if (semantic->site.site_line != 0) {
                    line << " site line " << semantic->site.site_line;
                }
                line << (semantic->semantic_resolved ? " semantic resolved" : " semantic unresolved");
                line << (semantic->source_drop_lowering_enabled
                    ? " source lowering accepted"
                    : " source lowering not accepted");
            } else {
                line << " semantic absent source lowering absent";
            }
            line << (has_emitted_declaration(action, snapshot.emitted_declarations)
                ? " declaration emitted"
                : " declaration missing");
            lines.push_back(line.str());
        }
    }

    return lines;
}

}  // namespace orison::pipeline
