#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

auto read_failing_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status != 0);
    return output;
}

template <typename SourceLines>
void assert_cli_parse_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

template <typename SourceLines>
void assert_cli_parse_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines
) {
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);
    assert(output.find("parsed ") != std::string::npos);
}

auto status_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Status",
        "    Ready(code: UInt32)",
        "    Empty",
        initializer,
    };
}

auto cli_module_lines(std::vector<std::string> body_lines) -> std::vector<std::string> {
    std::vector<std::string> lines {"package demo.cli"};
    lines.insert(lines.end(), body_lines.begin(), body_lines.end());
    return lines;
}

auto low_level_read_call(std::string_view intrinsic, std::string_view operand) -> std::string {
    return std::string(intrinsic) + "(" + std::string(operand) + ")";
}

auto low_level_final_read_direct_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_direct_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_rebound_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value = " + low_level_read_call(intrinsic, "p"),
            "    value",
        }
    );
}

auto low_level_final_read_rebound_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value = " + low_level_read_call(intrinsic, "p"),
            "    value",
        }
    );
}

auto low_level_final_read_unsafe_block_direct_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    unsafe",
            "        " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_unsafe_block_direct_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(p: Pointer<Byte>) -> Byte",
            "    unsafe",
            "        " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_unsafe_block_rebound_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_word(p: Pointer<Byte>) -> UInt32",
            "    unsafe",
            "        let value = " + low_level_read_call(intrinsic, "p"),
            "        value",
        }
    );
}

auto low_level_final_read_unsafe_block_rebound_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(p: Pointer<Byte>) -> Byte",
            "    unsafe",
            "        let value = " + low_level_read_call(intrinsic, "p"),
            "        value",
        }
    );
}

auto low_level_final_read_branch_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    if flag",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
}

auto low_level_final_read_branch_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    if flag",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
}

auto low_level_final_read_switch_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(selector: UInt32, left: Pointer<Byte>, right: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    switch selector",
            "        0 => value = " + low_level_read_call(intrinsic, "left"),
            "        default => value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
}

auto low_level_final_read_switch_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(selector: UInt32, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    switch selector",
            "        0 => value = " + low_level_read_call(intrinsic, "left"),
            "        default => value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
}

auto low_level_final_if_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    if flag",
            "        " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        1 as UInt32",
        }
    );
}

auto low_level_final_if_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    if flag",
            "        " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        " + low_level_read_call(intrinsic, "right"),
        }
    );
}

auto low_level_final_switch_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    switch flag",
            "        true => " + low_level_read_call(intrinsic, "left"),
            "        false => 1 as UInt32",
        }
    );
}

auto low_level_final_switch_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    switch flag",
            "        true => " + low_level_read_call(intrinsic, "left"),
            "        false => " + low_level_read_call(intrinsic, "right"),
        }
    );
}

auto low_level_final_read_guard_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<UInt32>) -> UInt32",
            "    var value = 1 as UInt32",
            "    guard flag else",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_guard_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    guard flag else",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_while_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<UInt32>) -> UInt32",
            "    var value = 1 as UInt32",
            "    while flag",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_for_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(items: shared View<Int64>, left: Pointer<UInt32>) -> UInt32",
            "    var value = 1 as UInt32",
            "    for item in items",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_for_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(items: shared View<Int64>, left: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    for item in items",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_repeat_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    repeat",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    while flag",
            "    value",
        }
    );
}

auto maybe_choice_constant_lines_with_declarations(
    std::string_view initializer,
    std::initializer_list<std::string_view> declarations,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string_view> {
    std::vector<std::string_view> lines {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
    };
    lines.insert(lines.end(), declarations.begin(), declarations.end());
    lines.push_back(initializer);
    lines.insert(lines.end(), trailing_lines.begin(), trailing_lines.end());
    return lines;
}

auto maybe_choice_constant_lines(
    std::string_view initializer,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(initializer, {}, trailing_lines);
}

auto boxed_maybe_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {"choice Boxed<T>", "    Wrap(inner: T)"}
    );
}

auto maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {"choice Result<T>", "    Ok(value: T)", "    Error(message: Text)"}
    );
}

auto boxed_maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {
            "choice Boxed<T>",
            "    Wrap(inner: T)",
            "choice Result<T>",
            "    Ok(value: T)",
            "    Error(message: Text)",
        }
    );
}

auto boxed_maybe_array_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return boxed_maybe_choice_constant_lines(initializer);
}

auto scalar_array_constant_lines(
    std::string initializer,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        std::move(initializer),
    };
    for (auto line : trailing_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto scalar_array_constant_initializer(std::string_view elements, std::size_t length = 1) -> std::string {
    return "const MAGIC: Array<UInt32, " + std::to_string(length) + "> = [" + std::string(elements) + "]";
}

auto nested_array_constant_initializer(std::string_view elements, std::size_t length = 1) -> std::string {
    return "const MATRIX: Array<Array<UInt32, 2>, " + std::to_string(length) + "> = [" + std::string(elements) + "]";
}

auto scalar_array_block_runtime_constant_lines(std::string_view construct) -> std::vector<std::string_view> {
    if (construct == "task") {
        return {
            "package demo.cli",
            "const MAGIC: Array<UInt32, 1> = [task",
            "    1",
            "]",
        };
    }

    return {
        "package demo.cli",
        "const MAGIC: Array<UInt32, 1> = [thread",
        "    1",
        "]",
    };
}

auto maybe_array_payload_block_runtime_constant_lines(std::string_view construct) -> std::vector<std::string_view> {
    if (construct == "task") {
        return {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([task",
            "    1",
            "])",
        };
    }

    return {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([thread",
        "    1",
        "])",
    };
}

auto nested_array_constant_lines(std::string initializer) -> std::vector<std::string> {
    return {
        "package demo.cli",
        std::move(initializer),
    };
}

auto header_record_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        initializer,
    };
}

auto header_record_function_lines(std::initializer_list<std::string_view> body_lines) -> std::vector<std::string_view> {
    std::vector<std::string_view> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "function default_header() -> Header",
    };
    lines.insert(lines.end(), body_lines.begin(), body_lines.end());
    return lines;
}

auto two_header_record_function_lines(
    std::string_view return_type,
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
    };
    lines.push_back("function default_header() -> " + std::string(return_type));
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto two_header_record_consumer_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "function consume_header(header: Header) -> UInt16",
        "    return header.version",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_pair_consumer_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record Pair<A, B>",
        "    first: A",
        "    second: B",
        "function consume_pair<T>(left: T, pair: Pair<T, UInt16>) -> UInt16",
        "    return pair.second",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_consumer_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "function same<T>(left: T, right: T) -> UInt16",
        "    return 1",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_record_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record Same<T>",
        "    first: T",
        "    second: T",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto low_level_array_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        initializer,
        "const UART0_DATA: Address = 0x4000_1000",
    };
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
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return await request_task\n";
        output << "public function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        sum(data)\n";
        output << "    return worker.join()\n";
        output << "unsafe function read_word(addr: Address) -> UInt32\n";
        output << "    return raw_read(addr)\n";
        output << "choice MaybeText\n";
        output << "    Some(value: Text)\n";
        output << "    Empty\n";
        output << "public interface Reader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "implements Reader for FileReader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "        return into.length()\n";
        output << "extend FileReader\n";
        output << "    public function reset(this: exclusive This) -> Unit\n";
        output << "        return input.length()\n";
        output << "package function main<R>(input: shared.View<Byte>, state: MaybeText, reader: exclusive R) -> Outcome<Int32, ParseError>\n";
        output << "where R: Reader\n";
        output << "    switch state\n";
        output << "        Some(value) => return value.length()\n";
        output << "        Empty => return input.length()\n";
        output << "    return input.read(2)\n";
    }

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);

    assert(output.find("parsed ") != std::string::npos);
    assert(output.find("package demo.cli") != std::string::npos);
    assert(output.find("top-level declarations: 12") != std::string::npos);
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
    assert(output.find("choices: 1") != std::string::npos);
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
    assert(output.find("functions: 4") != std::string::npos);
    assert(output.find("first function visibility: package") != std::string::npos);
    assert(output.find("first function async: true") != std::string::npos);
    assert(output.find("first function unsafe: false") != std::string::npos);
    assert(output.find("function parameters: 1") != std::string::npos);
    assert(output.find("function return type: Outcome<Text, IOError>") != std::string::npos);
    assert(output.find("function where constraints: 0") != std::string::npos);
    assert(output.find("function body statements: 2") != std::string::npos);
    assert(output.find("first statement kind: let") != std::string::npos);
    assert(output.find("first statement expression: task { request(url) }") != std::string::npos);
    assert(output.find("first statement nested count: 0") != std::string::npos);
    assert(output.find("first statement alternate count: 0") != std::string::npos);
    assert(output.find("first statement switch cases: 0") != std::string::npos);

    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unknown_choice_constant.or",
        maybe_choice_constant_lines("const DEFAULT_VALUE: Maybe<UInt32> = Missing(1)"),
        "choice constructor 'Missing' does not match any declared choice variant for constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_wrong_choice_constant.or",
        maybe_result_choice_constant_lines("const DEFAULT_VALUE: Maybe<UInt32> = Error(\"missing\")"),
        "choice constructor 'Error' does not belong to declared constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_arity.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Ready()"),
        "choice constructor 'Ready' expects 1 payload value but received 0"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_zero_payload_choice_constant_arity.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Empty(1)"),
        "choice constructor 'Empty' expects 0 payload values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_zero_payload_choice_constant_arity.or",
        boxed_maybe_choice_constant_lines("const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Empty(1))"),
        "choice constructor 'Empty' expects 0 payload values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_wrong_choice_constant.or",
        boxed_maybe_result_choice_constant_lines(
            "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Error(\"missing\"))"
        ),
        "choice constructor 'Error' does not belong to declared constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_unknown_choice_constant.or",
        boxed_maybe_choice_constant_lines("const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Missing(1))"),
        "choice constructor 'Missing' does not match any declared choice variant for constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_payload.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Ready(true)"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_repeated_payload_type.or",
        maybe_choice_constant_lines_with_declarations(
            "const DEFAULT_VALUE: Both<Header> = Pair(Header([1, 2], 1), OtherHeader([1, 2], 1))",
            {
                "record Header",
                "    magic: Array<UInt32, 2>",
                "    version: UInt16",
                "record OtherHeader",
                "    magic: Array<UInt32, 2>",
                "    version: UInt16",
                "choice Both<T>",
                "    Pair(left: T, right: T)",
            }
        ),
        "choice constructor payload type 'OtherHeader' does not match expected payload type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_return_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    return Some(true)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_expression_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    Some(true)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    if flag",
            "        true",
            "    else",
            "        1 as UInt32",
        },
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    switch flag",
            "        true => true",
            "        false => 1 as UInt32",
        },
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    if flag",
            "        1 as UInt32",
            "    else",
            "        2 as UInt32",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    switch flag",
            "        true => 1 as UInt32",
            "        false => 2 as UInt32",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    if flag",
            "        Some(true)",
            "    else",
            "        Empty",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    switch flag",
            "        true => Some(true)",
            "        false => Empty",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    if flag",
            "        Some(1 as UInt32)",
            "    else",
            "        Empty",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    switch flag",
            "        true => Some(1 as UInt32)",
            "        false => Empty",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_zero_payload_final_expression.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    Empty",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unannotated_binding_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    let value = Some(1 as UInt32)",
            "    return consume(value)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unannotated_zero_payload.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> UInt32",
            "    let value = Empty",
            "    return 1",
        },
        "choice constructor 'Empty' requires an expected choice type"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unannotated_ambiguous_name.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice LocalStatus",
            "    Ready(value: UInt32)",
            "choice RemoteStatus",
            "    Ready(value: UInt32)",
            "function demo() -> UInt32",
            "    let value = Ready(1 as UInt32)",
            "    return 1",
        },
        "choice constructor 'Ready' requires an expected choice type"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_zero_payload_call_argument.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    return consume(Empty)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_element.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("true")),
        "constant array initializer element type 'Bool' does not match declared element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_length.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("1, 2, 3", 2)),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_constant_element.or",
        nested_array_constant_lines(nested_array_constant_initializer("[1, true]")),
        "constant array initializer element type 'Bool' does not match declared element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_constant_length.or",
        nested_array_constant_lines(nested_array_constant_initializer("[1, 2, 3]")),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_indexed_array_constant_element_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "const FLAGS: Array<Bool, 1> = [true]",
            "const FIRST_FLAG: UInt32 = FLAGS[0]",
        },
        "constant initializer type 'Bool' does not match declared constant type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_constant_field_type.or",
        header_record_constant_lines("const DEFAULT_HEADER: Header = Header([1, 2], true)"),
        "record constructor field 'version' type 'Bool' does not match expected field type 'UInt16'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_return_arity.or",
        header_record_function_lines({"    return Header([1, 2])"}),
        "record constructor 'Header' expects 2 field values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_return_type.or",
        two_header_record_function_lines("OtherHeader", {"    return Header([1, 2], 1)"}),
        "return expression type 'Header' does not match declared type 'OtherHeader'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_assignment_type.or",
        two_header_record_function_lines(
            "Header",
            {
                "    var header = Header([1, 2], 1)",
                "    header = OtherHeader([1, 2], 1)",
                "    return header",
            }
        ),
        "assignment value type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_function_argument_type.or",
        two_header_record_consumer_lines({"    return consume_header(OtherHeader([1, 2], 1))"}),
        "function argument 'header' type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    \"text\"",
        },
        "pointer-returning function currently requires a structurally pointer-like expression"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    raw_offset(base, 1)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_final_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function base() -> Address",
            "    \"text\"",
        },
        "address-returning function currently requires a structurally address-like expression"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_final_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function base(buf: exclusive Buffer) -> Address",
            "    address_of(buf.data[0])",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_expression_type.or",
        low_level_final_read_direct_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_expression_success.or",
        low_level_final_read_direct_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_rebound_final_expression_type.or",
        low_level_final_read_rebound_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_rebound_final_expression_success.or",
        low_level_final_read_rebound_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_final_expression_type.or",
        low_level_final_read_unsafe_block_direct_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_final_expression_success.or",
        low_level_final_read_unsafe_block_direct_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_rebound_final_expression_type.or",
        low_level_final_read_unsafe_block_rebound_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_rebound_final_expression_success.or",
        low_level_final_read_unsafe_block_rebound_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_branch_final_expression_type.or",
        low_level_final_read_branch_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_branch_final_expression_success.or",
        low_level_final_read_branch_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_switch_final_expression_type.or",
        low_level_final_read_switch_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_switch_final_expression_success.or",
        low_level_final_read_switch_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_if_expression_type.or",
        low_level_final_if_read_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_if_expression_success.or",
        low_level_final_if_read_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_switch_expression_type.or",
        low_level_final_switch_read_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_switch_expression_success.or",
        low_level_final_switch_read_success_lines("raw_read")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_guard_final_expression_success.or",
        low_level_final_read_guard_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_guard_final_expression_type.or",
        low_level_final_read_guard_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_while_final_expression_success.or",
        low_level_final_read_while_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_repeat_final_expression_type.or",
        low_level_final_read_repeat_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_for_final_expression_success.or",
        low_level_final_read_for_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_for_final_expression_type.or",
        low_level_final_read_for_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_expression_type.or",
        low_level_final_read_direct_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_expression_success.or",
        low_level_final_read_direct_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_rebound_final_expression_type.or",
        low_level_final_read_rebound_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_rebound_final_expression_success.or",
        low_level_final_read_rebound_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_unsafe_block_final_expression_type.or",
        low_level_final_read_unsafe_block_direct_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_unsafe_block_final_expression_success.or",
        low_level_final_read_unsafe_block_direct_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() /
            "orison_cli_volatile_read_unsafe_block_rebound_final_expression_type.or",
        low_level_final_read_unsafe_block_rebound_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() /
            "orison_cli_volatile_read_unsafe_block_rebound_final_expression_success.or",
        low_level_final_read_unsafe_block_rebound_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_branch_final_expression_type.or",
        low_level_final_read_branch_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_branch_final_expression_success.or",
        low_level_final_read_branch_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_switch_final_expression_type.or",
        low_level_final_read_switch_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_switch_final_expression_success.or",
        low_level_final_read_switch_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_if_expression_type.or",
        low_level_final_if_read_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_if_expression_success.or",
        low_level_final_if_read_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_switch_expression_type.or",
        low_level_final_switch_read_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_switch_expression_success.or",
        low_level_final_switch_read_success_lines("volatile_read")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_guard_final_expression_success.or",
        low_level_final_read_guard_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_guard_final_expression_type.or",
        low_level_final_read_guard_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_while_final_expression_success.or",
        low_level_final_read_while_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_repeat_final_expression_type.or",
        low_level_final_read_repeat_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_for_final_expression_success.or",
        low_level_final_read_for_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_for_final_expression_type.or",
        low_level_final_read_for_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_function_dependent_argument_type.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(OtherHeader([1, 2], 1), 1 as UInt16))"}
        ),
        "function argument 'pair' type 'Pair<OtherHeader, UInt16>' does not match declared type 'Pair<Header, UInt16>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_repeated_binding_argument_type.or",
        generic_same_consumer_lines({"    return same(Header([1, 2], 1), OtherHeader([1, 2], 1))"}),
        "function argument 'right' type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_record_repeated_field_type.or",
        generic_same_record_lines(
            {
                "    let same = Same(Header([1, 2], 1), OtherHeader([1, 2], 1))",
                "    return same.first.version",
            }
        ),
        "record constructor field 'second' type 'OtherHeader' does not match expected field type 'Header'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_function_dependent_same_width_integer.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(Header([1, 2], 1), 1 as Int16))"}
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_unknown_reference.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("STATUS_LOW")),
        "constant initializer references unknown name 'STATUS_LOW'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_direct_cycle.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("MAGIC[0]")),
        "constant initializer cycle includes 'MAGIC'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_array_constant_element.or",
        scalar_array_constant_lines("const UART_REGISTERS: Array<Address, 1> = [\"uart\"]"),
        "constant array initializer element type 'Text' does not match declared element type 'Address'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_array_constant_construction.or",
        low_level_array_constant_lines("const UART_POINTERS: Array<Pointer<UInt32>, 1> = [Pointer(UART0_DATA)]"),
        "constant initializer cannot use Pointer construction"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_choice_array_constant_payload.or",
        boxed_maybe_array_choice_constant_lines(
            "const DEFAULT_VALUES: Array<Boxed<Maybe<UInt32>>, 1> = [Wrap(Some(true))]"
        ),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_array_payload_length.or",
        maybe_choice_constant_lines("const DEFAULT_VALUE: Maybe<Array<UInt32, 2>> = Some([1, 2, 3])"),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_function_call.or",
        scalar_array_constant_lines(
            scalar_array_constant_initializer("mask(0xFFFF)"),
            {"function mask(value: UInt32) -> UInt32", "    return value bit_and 0xFF"}
        ),
        "constant initializer cannot call function 'mask'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_function_call.or",
        maybe_choice_constant_lines(
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([mask(0xFFFF)])",
            {"function mask(value: UInt32) -> UInt32", "    return value bit_and 0xFF"}
        ),
        "constant initializer cannot call function 'mask'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_unsafe_intrinsic.or",
        scalar_array_constant_lines(
            scalar_array_constant_initializer("raw_read(UART_STATUS)"),
            {"const UART_STATUS: Address = 0x4000_1000"}
        ),
        "constant initializer cannot use unsafe intrinsic 'raw_read'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_unsafe_intrinsic.or",
        maybe_choice_constant_lines(
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([raw_read(UART_STATUS)])",
            {"const UART_STATUS: Address = 0x4000_1000"}
        ),
        "constant initializer cannot use unsafe intrinsic 'raw_read'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_await.or",
        scalar_array_constant_lines(
            scalar_array_constant_initializer("await STATUS_MASK"),
            {"const STATUS_MASK: UInt32 = 0xFF"}
        ),
        "constant initializer cannot use await expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_await.or",
        maybe_choice_constant_lines(
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([await STATUS_MASK])",
            {"const STATUS_MASK: UInt32 = 0xFF"}
        ),
        "constant initializer cannot use await expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_task.or",
        scalar_array_block_runtime_constant_lines("task"),
        "constant initializer cannot use task expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_task.or",
        maybe_array_payload_block_runtime_constant_lines("task"),
        "constant initializer cannot use task expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_thread.or",
        scalar_array_block_runtime_constant_lines("thread"),
        "constant initializer cannot use thread expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_thread.or",
        maybe_array_payload_block_runtime_constant_lines("thread"),
        "constant initializer cannot use thread expression"
    );
    return 0;
}
