#pragma once

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {

enum class ReturnedAggregateDescriptorProjectionStepKind {
    field,
    array_element,
};

struct ReturnedAggregateDescriptorProjectionStep {
    ReturnedAggregateDescriptorProjectionStepKind kind = ReturnedAggregateDescriptorProjectionStepKind::field;
    std::string aggregate_llvm_type;
    std::string index_value;
};

struct ReturnedAggregateDescriptorProjectionPath {
    std::string owner_name;
    std::string source_type_name;
    std::vector<ReturnedAggregateDescriptorProjectionStep> steps;
};

struct ReturnedAggregateSiblingProjectionPaths {
    std::vector<ReturnedAggregateDescriptorProjectionPath> descriptor_paths;
    std::vector<ReturnedAggregateDescriptorProjectionPath> maybe_owner_paths;
    std::vector<ReturnedAggregateDescriptorProjectionPath> choice_owner_paths;
};

struct ReturnedAggregateSelectedDescriptorProjection {
    std::string pointer;
    std::string source_type_name;
    std::vector<ReturnedAggregateDescriptorProjectionStep> static_descriptor_steps;
    bool complete_static_descriptor_path = true;
};

inline auto returned_aggregate_selected_static_descriptor_path(
    ReturnedAggregateSelectedDescriptorProjection const& selected_path
) -> std::vector<ReturnedAggregateDescriptorProjectionStep> const* {
    return selected_path.complete_static_descriptor_path ? &selected_path.static_descriptor_steps : nullptr;
}

inline auto returned_aggregate_register_without_static_selected_path(
    ReturnedAggregateSelectedDescriptorProjection const& selected_path
) -> bool {
    return !selected_path.complete_static_descriptor_path;
}

inline auto append_returned_aggregate_selected_field_step(
    std::vector<ReturnedAggregateDescriptorProjectionStep>& selected_steps,
    std::string aggregate_llvm_type,
    std::size_t field_index
) -> void {
    selected_steps.push_back(ReturnedAggregateDescriptorProjectionStep {
        .kind = ReturnedAggregateDescriptorProjectionStepKind::field,
        .aggregate_llvm_type = std::move(aggregate_llvm_type),
        .index_value = std::to_string(field_index),
    });
}

inline auto append_returned_aggregate_selected_array_element_step(
    std::vector<ReturnedAggregateDescriptorProjectionStep>& selected_steps,
    std::string aggregate_llvm_type,
    std::string index_value
) -> void {
    selected_steps.push_back(ReturnedAggregateDescriptorProjectionStep {
        .kind = ReturnedAggregateDescriptorProjectionStepKind::array_element,
        .aggregate_llvm_type = std::move(aggregate_llvm_type),
        .index_value = std::move(index_value),
    });
}

inline auto same_returned_aggregate_descriptor_projection_step(
    ReturnedAggregateDescriptorProjectionStep const& left,
    ReturnedAggregateDescriptorProjectionStep const& right
) -> bool {
    return left.kind == right.kind &&
        left.aggregate_llvm_type == right.aggregate_llvm_type &&
        left.index_value == right.index_value;
}

inline auto same_returned_aggregate_descriptor_projection_path(
    std::vector<ReturnedAggregateDescriptorProjectionStep> const& left,
    std::vector<ReturnedAggregateDescriptorProjectionStep> const& right
) -> bool {
    return left.size() == right.size() &&
        std::equal(
            left.begin(),
            left.end(),
            right.begin(),
            same_returned_aggregate_descriptor_projection_step
        );
}

enum class ReturnedAggregateProjectionCollectionKind {
    dynamic_array_descriptor,
    maybe_owner,
    choice_owner,
};

inline auto returned_aggregate_dynamic_array_descriptor_count(
    std::string_view source_type_name,
    LoweringContext const& context,
    std::size_t depth = 0
) -> std::size_t {
    if (depth > 16) {
        return 2;
    }
    if (dynamic_array_element_source_type_name(source_type_name).has_value()) {
        return 1;
    }
    if (auto array_element_type = array_element_source_type_name(source_type_name)) {
        auto element_count = returned_aggregate_dynamic_array_descriptor_count(
            *array_element_type,
            context,
            depth + 1
        );
        return element_count == 0 ? 0 : 2;
    }
    if (auto maybe_payload_type = maybe_payload_source_type_name(source_type_name)) {
        return returned_aggregate_dynamic_array_descriptor_count(*maybe_payload_type, context, depth + 1);
    }
    auto choice = context.choices.find(std::string {source_type_name});
    if (choice != context.choices.end()) {
        auto count = std::size_t {0};
        for (auto const& variant : choice->second.variants) {
            for (auto const& payload : variant.payloads) {
                count += returned_aggregate_dynamic_array_descriptor_count(
                    payload.source_type_name,
                    context,
                    depth + 1
                );
                if (count > 1) {
                    return count;
                }
            }
        }
        return count;
    }
    auto record = context.records.find(std::string {source_type_name});
    if (record == context.records.end()) {
        return 0;
    }
    auto count = std::size_t {0};
    for (auto const& field : record->second.fields) {
        count += returned_aggregate_dynamic_array_descriptor_count(field.source_type_name, context, depth + 1);
        if (count > 1) {
            return count;
        }
    }
    return count;
}

inline auto returned_aggregate_projection_collection_target(
    ReturnedAggregateProjectionCollectionKind kind,
    std::string_view source_type_name,
    LoweringContext const& context
) -> bool {
    switch (kind) {
    case ReturnedAggregateProjectionCollectionKind::dynamic_array_descriptor:
        return dynamic_array_element_source_type_name(source_type_name).has_value();
    case ReturnedAggregateProjectionCollectionKind::maybe_owner:
        return maybe_payload_source_type_name(source_type_name).has_value() &&
            returned_aggregate_dynamic_array_descriptor_count(source_type_name, context) > 0;
    case ReturnedAggregateProjectionCollectionKind::choice_owner:
        return context.choices.contains(std::string {source_type_name}) &&
            returned_aggregate_dynamic_array_descriptor_count(source_type_name, context) > 0;
    }
    return false;
}

inline auto collect_returned_aggregate_projection_paths(
    std::string owner_name,
    std::string_view source_type_name,
    std::string_view llvm_type,
    LoweringContext const& context,
    ReturnedAggregateProjectionCollectionKind kind,
    std::vector<ReturnedAggregateDescriptorProjectionStep> steps = {}
) -> std::optional<std::vector<ReturnedAggregateDescriptorProjectionPath>> {
    if (returned_aggregate_projection_collection_target(kind, source_type_name, context)) {
        return std::vector<ReturnedAggregateDescriptorProjectionPath> {
            ReturnedAggregateDescriptorProjectionPath {
                .owner_name = std::move(owner_name),
                .source_type_name = std::string {source_type_name},
                .steps = std::move(steps),
            },
        };
    }

    if (auto array_element_type = array_element_source_type_name(source_type_name)) {
        auto array_type = parse_llvm_array_type(llvm_type);
        if (!array_type.has_value()) {
            return std::nullopt;
        }

        auto paths = std::vector<ReturnedAggregateDescriptorProjectionPath> {};
        for (auto index = std::size_t {0}; index < array_type->length; ++index) {
            auto element_steps = steps;
            element_steps.push_back(ReturnedAggregateDescriptorProjectionStep {
                .kind = ReturnedAggregateDescriptorProjectionStepKind::array_element,
                .aggregate_llvm_type = std::string {llvm_type},
                .index_value = std::to_string(index),
            });
            auto nested = collect_returned_aggregate_projection_paths(
                owner_name + ".element" + std::to_string(index),
                *array_element_type,
                array_type->element_type,
                context,
                kind,
                std::move(element_steps)
            );
            if (!nested.has_value()) {
                return std::nullopt;
            }
            paths.insert(
                paths.end(),
                std::make_move_iterator(nested->begin()),
                std::make_move_iterator(nested->end())
            );
        }
        return paths;
    }

    auto record = context.records.find(std::string {source_type_name});
    if (record == context.records.end()) {
        return std::vector<ReturnedAggregateDescriptorProjectionPath> {};
    }

    auto paths = std::vector<ReturnedAggregateDescriptorProjectionPath> {};
    for (auto const& field : record->second.fields) {
        auto field_steps = steps;
        field_steps.push_back(ReturnedAggregateDescriptorProjectionStep {
            .kind = ReturnedAggregateDescriptorProjectionStepKind::field,
            .aggregate_llvm_type = std::string {llvm_type},
            .index_value = std::to_string(field.index),
        });
        auto nested = collect_returned_aggregate_projection_paths(
            owner_name + "." + field.name,
            field.source_type_name,
            field.llvm_type,
            context,
            kind,
            std::move(field_steps)
        );
        if (!nested.has_value()) {
            return std::nullopt;
        }
        paths.insert(
            paths.end(),
            std::make_move_iterator(nested->begin()),
            std::make_move_iterator(nested->end())
        );
    }
    return paths;
}

inline auto collect_returned_aggregate_descriptor_projection_paths(
    std::string owner_name,
    std::string_view source_type_name,
    std::string_view llvm_type,
    LoweringContext const& context,
    std::vector<ReturnedAggregateDescriptorProjectionStep> steps = {}
) -> std::optional<std::vector<ReturnedAggregateDescriptorProjectionPath>> {
    return collect_returned_aggregate_projection_paths(
        std::move(owner_name),
        source_type_name,
        llvm_type,
        context,
        ReturnedAggregateProjectionCollectionKind::dynamic_array_descriptor,
        std::move(steps)
    );
}

inline auto collect_returned_aggregate_maybe_projection_paths(
    std::string owner_name,
    std::string_view source_type_name,
    std::string_view llvm_type,
    LoweringContext const& context,
    std::vector<ReturnedAggregateDescriptorProjectionStep> steps = {}
) -> std::optional<std::vector<ReturnedAggregateDescriptorProjectionPath>> {
    return collect_returned_aggregate_projection_paths(
        std::move(owner_name),
        source_type_name,
        llvm_type,
        context,
        ReturnedAggregateProjectionCollectionKind::maybe_owner,
        std::move(steps)
    );
}

inline auto collect_returned_aggregate_choice_projection_paths(
    std::string owner_name,
    std::string_view source_type_name,
    std::string_view llvm_type,
    LoweringContext const& context,
    std::vector<ReturnedAggregateDescriptorProjectionStep> steps = {}
) -> std::optional<std::vector<ReturnedAggregateDescriptorProjectionPath>> {
    return collect_returned_aggregate_projection_paths(
        std::move(owner_name),
        source_type_name,
        llvm_type,
        context,
        ReturnedAggregateProjectionCollectionKind::choice_owner,
        std::move(steps)
    );
}

inline auto collect_returned_aggregate_sibling_projection_paths(
    std::string owner_name,
    std::string_view source_type_name,
    std::string_view llvm_type,
    LoweringContext const& context
) -> std::optional<ReturnedAggregateSiblingProjectionPaths> {
    auto descriptor_paths = collect_returned_aggregate_descriptor_projection_paths(
        owner_name,
        source_type_name,
        llvm_type,
        context
    );
    if (!descriptor_paths.has_value()) {
        return std::nullopt;
    }
    auto maybe_paths = collect_returned_aggregate_maybe_projection_paths(
        owner_name,
        source_type_name,
        llvm_type,
        context
    );
    if (!maybe_paths.has_value()) {
        return std::nullopt;
    }
    auto choice_paths = collect_returned_aggregate_choice_projection_paths(
        owner_name,
        source_type_name,
        llvm_type,
        context
    );
    if (!choice_paths.has_value()) {
        return std::nullopt;
    }
    return ReturnedAggregateSiblingProjectionPaths {
        .descriptor_paths = std::move(*descriptor_paths),
        .maybe_owner_paths = std::move(*maybe_paths),
        .choice_owner_paths = std::move(*choice_paths),
    };
}

inline auto emit_returned_aggregate_descriptor_projection_pointer(
    std::string_view root_storage,
    ReturnedAggregateDescriptorProjectionPath const& path,
    std::string_view pointer_prefix,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::string {
    auto pointer = std::string {root_storage};
    for (auto const& step : path.steps) {
        auto next_pointer = std::string {pointer_prefix} + ".path" +
            std::to_string(session.state.next_temporary_index++);
        output << "  " << next_pointer << " = getelementptr " << step.aggregate_llvm_type
               << ", ptr " << pointer;
        if (step.kind == ReturnedAggregateDescriptorProjectionStepKind::field) {
            output << ", i32 0, i32 " << step.index_value << "\n";
        } else {
            output << ", i64 0, i64 " << step.index_value << "\n";
        }
        pointer = std::move(next_pointer);
    }
    return pointer;
}

inline auto register_returned_aggregate_descriptor_projection_cleanups(
    std::string_view aggregate_storage,
    std::vector<ReturnedAggregateDescriptorProjectionPath> const& descriptor_paths,
    std::vector<ReturnedAggregateDescriptorProjectionStep> const* selected_descriptor_path,
    bool register_without_static_selected_path,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    if (selected_descriptor_path == nullptr &&
        !register_without_static_selected_path &&
        descriptor_paths.size() <= 1) {
        return true;
    }

    for (auto const& descriptor_path : descriptor_paths) {
        if (selected_descriptor_path != nullptr &&
            same_returned_aggregate_descriptor_projection_path(descriptor_path.steps, *selected_descriptor_path)) {
            continue;
        }
        auto descriptor_pointer = emit_returned_aggregate_descriptor_projection_pointer(
            aggregate_storage,
            descriptor_path,
            "%" + descriptor_path.owner_name,
            session,
            output
        );
        auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
            descriptor_path.owner_name,
            descriptor_path.source_type_name,
            context.lowering
        );
        if (!cleanup_plan.has_value()) {
            return false;
        }
        cleanup_plan->descriptor_storage_name = std::move(descriptor_pointer);
        cleanup_plan->descriptor_storage_status =
            DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
        cleanup_plan->source_line = source_line;
        session.state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));
    }
    return true;
}

inline auto emit_returned_aggregate_materialized_storage(
    std::string_view owner_name,
    std::string_view aggregate_llvm_type,
    std::string_view aggregate_value,
    std::ostringstream& output
) -> std::string {
    auto aggregate_storage = "%" + llvm_identifier_fragment(owner_name) + ".aggregate.addr";
    output << "  " << aggregate_storage << " = alloca " << aggregate_llvm_type << "\n";
    output << "  store " << aggregate_llvm_type << " " << aggregate_value;
    output << ", ptr " << aggregate_storage << "\n";
    return aggregate_storage;
}

inline auto move_returned_aggregate_selected_descriptor_to_cleanup_storage(
    std::string_view owner_name,
    std::string_view expected_source_type_name,
    std::string_view projected_descriptor_storage,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::string> {
    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        std::string {owner_name},
        expected_source_type_name,
        context.lowering
    );
    if (!cleanup_plan.has_value() || cleanup_plan->descriptor_storage_name.empty()) {
        return std::nullopt;
    }

    output << "  " << cleanup_plan->descriptor_storage_name << " = alloca "
           << dynamic_array_descriptor_llvm_type() << "\n";
    auto descriptor_value = "%" + llvm_identifier_fragment(owner_name) + ".descriptor";
    output << "  " << descriptor_value << " = load " << dynamic_array_descriptor_llvm_type()
           << ", ptr " << projected_descriptor_storage << "\n";
    output << "  store " << dynamic_array_descriptor_llvm_type() << " " << descriptor_value
           << ", ptr " << cleanup_plan->descriptor_storage_name << "\n";
    output << "  store " << dynamic_array_descriptor_llvm_type()
           << " zeroinitializer, ptr " << projected_descriptor_storage << "\n";

    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    cleanup_plan->source_line = source_line;
    auto descriptor_storage_name = cleanup_plan->descriptor_storage_name;
    session.state.source_type_names[std::string {owner_name}] = std::string {expected_source_type_name};
    session.state.addressable_bindings[std::string {owner_name}] = AddressableBinding {
        .type = LoweredType {
            .type = std::string {dynamic_array_descriptor_llvm_type()},
            .signedness = IntegerSignedness::not_integer,
        },
        .storage = descriptor_storage_name,
    };
    return descriptor_storage_name;
}

inline auto register_returned_aggregate_owner_bindings(
    std::string_view aggregate_storage,
    std::vector<ReturnedAggregateDescriptorProjectionPath> const& owner_paths,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    for (auto const& owner_path : owner_paths) {
        auto pointer = emit_returned_aggregate_descriptor_projection_pointer(
            aggregate_storage,
            owner_path,
            "%" + owner_path.owner_name,
            session,
            output
        );
        auto lowered_type = llvm_type_for_source_type_name(owner_path.source_type_name, context.lowering);
        if (!lowered_type.has_value() || *lowered_type == "void") {
            return false;
        }
        session.state.source_type_names[owner_path.owner_name] = owner_path.source_type_name;
        session.state.addressable_bindings[owner_path.owner_name] = AddressableBinding {
            .type = LoweredType {
                .type = *lowered_type,
                .signedness = IntegerSignedness::not_integer,
            },
            .storage = std::move(pointer),
        };
    }
    return true;
}

inline auto register_returned_aggregate_sibling_cleanups_and_bindings(
    std::string_view aggregate_storage,
    ReturnedAggregateSiblingProjectionPaths const& sibling_paths,
    std::vector<ReturnedAggregateDescriptorProjectionStep> const* selected_descriptor_path,
    bool register_without_static_selected_path,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    return register_returned_aggregate_descriptor_projection_cleanups(
            aggregate_storage,
            sibling_paths.descriptor_paths,
            selected_descriptor_path,
            register_without_static_selected_path,
            source_line,
            context,
            session,
            output) &&
        register_returned_aggregate_owner_bindings(
            aggregate_storage,
            sibling_paths.maybe_owner_paths,
            context,
            session,
            output) &&
        register_returned_aggregate_owner_bindings(
            aggregate_storage,
            sibling_paths.choice_owner_paths,
            context,
            session,
            output);
}

inline auto register_returned_aggregate_sibling_cleanups_and_bindings(
    std::string_view aggregate_storage,
    ReturnedAggregateSiblingProjectionPaths const& sibling_paths,
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    return register_returned_aggregate_sibling_cleanups_and_bindings(
        aggregate_storage,
        sibling_paths,
        nullptr,
        false,
        source_line,
        context,
        session,
        output
    );
}

}  // namespace orison::lowering
