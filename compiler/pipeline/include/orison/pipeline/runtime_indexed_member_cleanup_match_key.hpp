#pragma once

#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedMemberCleanupMatchKey {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;

    auto operator==(RuntimeIndexedMemberCleanupMatchKey const&) const -> bool = default;
};

template <typename Record>
auto runtime_indexed_member_cleanup_match_key(
    Record const& record
) -> RuntimeIndexedMemberCleanupMatchKey {
    return RuntimeIndexedMemberCleanupMatchKey {
        .owner_name = record.owner_name,
        .index_expression_text = record.index_expression_text,
        .element_source_type_name = record.element_source_type_name,
        .moved_source_type_name = record.moved_source_type_name,
        .moved_member_path = record.moved_member_path,
    };
}

template <typename LeftRecord, typename RightRecord>
auto same_runtime_indexed_member_cleanup_key(
    LeftRecord const& left,
    RightRecord const& right
) -> bool {
    return runtime_indexed_member_cleanup_match_key(left) ==
        runtime_indexed_member_cleanup_match_key(right);
}

}  // namespace orison::pipeline
