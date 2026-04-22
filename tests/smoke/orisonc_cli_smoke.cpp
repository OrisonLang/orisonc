#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

auto read_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status == 0);
    return output;
}

}  // namespace

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_cli_smoke.or";
    {
        std::ofstream output(path);
        output << "package demo.cli\n";
        output << "record User\n";
        output << "    name: Text\n";
        output << "function main() -> Int32\n";
        output << "    0\n";
    }

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);

    assert(output.find("parsed ") != std::string::npos);
    assert(output.find("package demo.cli") != std::string::npos);
    assert(output.find("top-level declarations: 2") != std::string::npos);
    assert(output.find("records: 1") != std::string::npos);
    assert(output.find("functions: 1") != std::string::npos);
    assert(output.find("record fields: 1") != std::string::npos);
    assert(output.find("function body statements: 1") != std::string::npos);
    return 0;
}
