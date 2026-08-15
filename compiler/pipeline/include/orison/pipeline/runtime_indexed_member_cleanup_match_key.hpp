#pragma once

#include <algorithm>
#include <cstddef>
#include <sstream>
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

inline auto runtime_indexed_member_cleanup_dotted_path(
    std::vector<std::string> const& path
) -> std::string {
    if (path.empty()) {
        return "none";
    }

    auto text = std::ostringstream {};
    for (auto index = std::size_t {0}; index < path.size(); ++index) {
        if (index > 0) {
            text << '.';
        }
        text << path[index];
    }
    return text.str();
}

template <typename Record>
auto find_runtime_indexed_member_cleanup_record(
    RuntimeIndexedMemberCleanupMatchKey const& key,
    std::vector<Record> const& records
) -> Record const* {
    auto const found = std::find_if(
        records.begin(),
        records.end(),
        [&](Record const& record) {
            return runtime_indexed_member_cleanup_match_key(record) == key;
        }
    );
    if (found == records.end()) {
        return nullptr;
    }
    return &*found;
}

template <typename KeyRecord, typename Record>
auto find_runtime_indexed_member_cleanup_record(
    KeyRecord const& key_record,
    std::vector<Record> const& records
) -> Record const* {
    return find_runtime_indexed_member_cleanup_record(
        runtime_indexed_member_cleanup_match_key(key_record),
        records
    );
}

template <typename KeyRecord, typename Record>
auto count_runtime_indexed_member_cleanup_records(
    KeyRecord const& key_record,
    std::vector<Record> const& records
) -> std::size_t {
    return static_cast<std::size_t>(std::count_if(
        records.begin(),
        records.end(),
        [&](Record const& record) {
            return same_runtime_indexed_member_cleanup_key(key_record, record);
        }
    ));
}

template <typename Record, typename Render>
void append_runtime_indexed_member_cleanup_record_line(
    std::vector<std::string>& lines,
    RuntimeIndexedMemberCleanupMatchKey const& key,
    std::vector<Record> const& records,
    Render render
) {
    if (auto const* record = find_runtime_indexed_member_cleanup_record(key, records)) {
        lines.push_back(render(*record));
    }
}

template <typename Record, typename Render, typename Diagnostics>
void append_runtime_indexed_member_cleanup_record_line_with_diagnostics(
    std::vector<std::string>& lines,
    RuntimeIndexedMemberCleanupMatchKey const& key,
    std::vector<Record> const& records,
    Render render,
    Diagnostics diagnostics
) {
    if (auto const* record = find_runtime_indexed_member_cleanup_record(key, records)) {
        lines.push_back(render(*record));
        auto diagnostic_lines = diagnostics(*record);
        lines.insert(lines.end(), diagnostic_lines.begin(), diagnostic_lines.end());
    }
}

template <typename Record, typename Render>
void append_runtime_indexed_member_cleanup_record_lines(
    std::vector<std::string>& lines,
    std::vector<Record> const& records,
    Render render
) {
    for (auto const& record : records) {
        lines.push_back(render(record));
    }
}

template <typename Record, typename Render, typename Diagnostics>
void append_runtime_indexed_member_cleanup_record_lines_with_diagnostics(
    std::vector<std::string>& lines,
    std::vector<Record> const& records,
    Render render,
    Diagnostics diagnostics
) {
    for (auto const& record : records) {
        lines.push_back(render(record));
        auto diagnostic_lines = diagnostics(record);
        lines.insert(lines.end(), diagnostic_lines.begin(), diagnostic_lines.end());
    }
}

}  // namespace orison::pipeline
