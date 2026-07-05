#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

auto read_text(std::filesystem::path const& path) -> std::string {
    auto input = std::ifstream(path);
    auto buffer = std::ostringstream {};
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

auto main() -> int {
    auto smoke_dir = std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "smoke";
    auto failures = std::vector<std::filesystem::path> {};

    for (auto const& entry : std::filesystem::directory_iterator(smoke_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
            continue;
        }
        if (entry.path().filename() == "temp_isolation_guard_smoke.cpp") {
            continue;
        }

        auto text = read_text(entry.path());
        auto uses_temp_directory =
            text.find("std::filesystem::temp_directory_path()") != std::string::npos;
        auto sets_temp_root = text.find("setenv(\"TMPDIR\"") != std::string::npos;
        if (uses_temp_directory && !sets_temp_root) {
            failures.push_back(entry.path());
        }
    }

    if (!failures.empty()) {
        std::cerr << "smoke tests using std::filesystem::temp_directory_path() must set TMPDIR first:\n";
        for (auto const& failure : failures) {
            std::cerr << "  " << failure.string() << "\n";
        }
        return 1;
    }

    return 0;
}
