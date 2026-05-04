#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace orison::source {

class SourceFile {
public:
    static std::optional<SourceFile> read(std::filesystem::path path);

    SourceFile(std::filesystem::path path, std::string content);

    [[nodiscard]] auto path() const -> std::filesystem::path const&;
    [[nodiscard]] auto content() const -> std::string const&;

private:
    std::filesystem::path path_;
    std::string content_;
};

}  // namespace orison::source
