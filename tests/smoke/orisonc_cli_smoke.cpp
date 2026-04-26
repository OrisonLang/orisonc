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
        output << "const UART0_BASE: Address = 0x4000_1000\n";
        output << "import\n";
        output << "    Logger as Log from diagnostics.logger\n";
        output << "package foreign \"c\" library \"m\"\n";
        output << "    function sin(x: Float64) -> Float64\n";
        output << "public foreign \"c\" as \"device_init\"\n";
        output << "function initialize_device() -> Int32\n";
        output << "    return 0\n";
        output << "public type Port = UInt16\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    return url\n";
        output << "unsafe function read_word(addr: Address) -> UInt32\n";
        output << "    return raw_read(addr)\n";
        output << "public interface Reader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "implements Reader for FileReader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "        return into.length()\n";
        output << "extend FileReader\n";
        output << "    public function reset(this: exclusive This) -> Unit\n";
        output << "        return input.length()\n";
        output << "package function main<R>(input: shared.View<Byte>, reader: exclusive R) -> Outcome<Int32, ParseError>\n";
        output << "where R: Reader\n";
        output << "    switch reader\n";
        output << "        Some(value) => return value.length()\n";
        output << "        Empty => return input.length()\n";
        output << "        default => return input.length()\n";
        output << "    return input.read(2)\n";
    }

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);

    assert(output.find("parsed ") != std::string::npos);
    assert(output.find("package demo.cli") != std::string::npos);
    assert(output.find("top-level declarations: 10") != std::string::npos);
    assert(output.find("imports: 1") != std::string::npos);
    assert(output.find("foreign imports: 1") != std::string::npos);
    assert(output.find("foreign exports: 1") != std::string::npos);
    assert(output.find("constants: 1") != std::string::npos);
    assert(output.find("type aliases: 1") != std::string::npos);
    assert(output.find("first import from: diagnostics.logger") != std::string::npos);
    assert(output.find("first foreign import abi: \"c\"") != std::string::npos);
    assert(output.find("first foreign import library: \"m\"") != std::string::npos);
    assert(output.find("first foreign import functions: 1") != std::string::npos);
    assert(output.find("first foreign export abi: \"c\"") != std::string::npos);
    assert(output.find("first foreign export symbol: \"device_init\"") != std::string::npos);
    assert(output.find("first foreign export function: initialize_device") != std::string::npos);
    assert(output.find("first constant type: Address") != std::string::npos);
    assert(output.find("first constant initializer: 0x4000_1000") != std::string::npos);
    assert(output.find("first type alias visibility: public") != std::string::npos);
    assert(output.find("first type alias target: UInt16") != std::string::npos);
    assert(output.find("records: 0") != std::string::npos);
    assert(output.find("choices: 0") != std::string::npos);
    assert(output.find("interfaces: 1") != std::string::npos);
    assert(output.find("implementations: 1") != std::string::npos);
    assert(output.find("extensions: 1") != std::string::npos);
    assert(output.find("first interface visibility: public") != std::string::npos);
    assert(output.find("first interface methods: 1") != std::string::npos);
    assert(output.find("first implementation interface: Reader") != std::string::npos);
    assert(output.find("first implementation receiver: FileReader") != std::string::npos);
    assert(output.find("first implementation methods: 1") != std::string::npos);
    assert(output.find("first extension receiver: FileReader") != std::string::npos);
    assert(output.find("first extension methods: 1") != std::string::npos);
    assert(output.find("first extension method visibility: public") != std::string::npos);
    assert(output.find("functions: 3") != std::string::npos);
    assert(output.find("first function visibility: package") != std::string::npos);
    assert(output.find("first function async: true") != std::string::npos);
    assert(output.find("first function unsafe: false") != std::string::npos);
    assert(output.find("function parameters: 1") != std::string::npos);
    assert(output.find("function return type: Outcome<Text, IOError>") != std::string::npos);
    assert(output.find("function where constraints: 0") != std::string::npos);
    assert(output.find("function body statements: 1") != std::string::npos);
    assert(output.find("first statement kind: return") != std::string::npos);
    assert(output.find("first statement expression: url") != std::string::npos);
    assert(output.find("first statement nested count: 0") != std::string::npos);
    assert(output.find("first statement alternate count: 0") != std::string::npos);
    assert(output.find("first statement switch cases: 0") != std::string::npos);
    return 0;
}
