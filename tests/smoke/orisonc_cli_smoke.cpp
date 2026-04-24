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
        output << "    values: DynamicArray<Maybe<Int32>>\n";
        output << "function main(input: shared.View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "    switch input.length() >= 1\n";
        output << "        0 =>\n";
        output << "            let fallback = input.read(0)\n";
        output << "            return fallback\n";
        output << "        default => return input.read(2)\n";
        output << "    return input.read(2)\n";
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
    assert(output.find("first field type: DynamicArray<Maybe<Int32>>") != std::string::npos);
    assert(output.find("function parameters: 1") != std::string::npos);
    assert(output.find("function return type: Outcome<Int32, ParseError>") != std::string::npos);
    assert(output.find("function body statements: 2") != std::string::npos);
    assert(output.find("first statement kind: switch") != std::string::npos);
    assert(output.find("first statement expression: (input.length() >= 1)") != std::string::npos);
    assert(output.find("first statement nested count: 0") != std::string::npos);
    assert(output.find("first statement alternate count: 0") != std::string::npos);
    assert(output.find("first statement switch cases: 2") != std::string::npos);
    assert(output.find("first switch case statements: 2") != std::string::npos);
    return 0;
}
