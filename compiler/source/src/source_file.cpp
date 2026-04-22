#include "orison/source/source_file.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <utility>

namespace orison::source {

std::optional<SourceFile> SourceFile::read(std::filesystem::path path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return SourceFile(std::move(path), buffer.str());
}

SourceFile::SourceFile(std::filesystem::path path, std::string content)
    : path_(std::move(path)), content_(std::move(content)) {}

auto SourceFile::path() const -> std::filesystem::path const& {
    return path_;
}

auto SourceFile::content() const -> std::string const& {
    return content_;
}

}  // namespace orison::source
