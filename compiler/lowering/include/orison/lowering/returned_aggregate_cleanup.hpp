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
    std::size_t source_line,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    if (descriptor_paths.size() <= 1) {
        return true;
    }

    for (auto const& descriptor_path : descriptor_paths) {
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

}  // namespace orison::lowering
