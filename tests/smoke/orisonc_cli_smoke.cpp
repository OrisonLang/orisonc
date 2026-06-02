#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
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

void assert_cli_parse_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    std::vector<std::string_view> const& lines,
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

auto status_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Status",
        "    Ready(code: UInt32)",
        "    Empty",
        initializer,
    };
}

auto maybe_choice_constant_lines(
    std::string_view initializer,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string_view> {
    std::vector<std::string_view> lines {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        initializer,
    };
    lines.insert(lines.end(), trailing_lines.begin(), trailing_lines.end());
    return lines;
}

auto boxed_maybe_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "choice Boxed<T>",
        "    Wrap(inner: T)",
        initializer,
    };
}

auto maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "choice Result<T>",
        "    Ok(value: T)",
        "    Error(message: Text)",
        initializer,
    };
}

auto boxed_maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "choice Boxed<T>",
        "    Wrap(inner: T)",
        "choice Result<T>",
        "    Ok(value: T)",
        "    Error(message: Text)",
        initializer,
    };
}

auto boxed_maybe_array_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "choice Boxed<T>",
        "    Wrap(inner: T)",
        initializer,
    };
}

auto scalar_array_constant_lines(
    std::string_view initializer,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string_view> {
    std::vector<std::string_view> lines {
        "package demo.cli",
        initializer,
    };
    lines.insert(lines.end(), trailing_lines.begin(), trailing_lines.end());
    return lines;
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

auto nested_array_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        initializer,
    };
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
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_element.or",
        scalar_array_constant_lines("const MAGIC: Array<UInt32, 1> = [true]"),
        "constant array initializer element type 'Bool' does not match declared element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_length.or",
        scalar_array_constant_lines("const MAGIC: Array<UInt32, 2> = [1, 2, 3]"),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_constant_element.or",
        nested_array_constant_lines("const MATRIX: Array<Array<UInt32, 2>, 1> = [[1, true]]"),
        "constant array initializer element type 'Bool' does not match declared element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_constant_length.or",
        nested_array_constant_lines("const MATRIX: Array<Array<UInt32, 2>, 1> = [[1, 2, 3]]"),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_unknown_reference.or",
        scalar_array_constant_lines("const MAGIC: Array<UInt32, 1> = [STATUS_LOW]"),
        "constant initializer references unknown name 'STATUS_LOW'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_direct_cycle.or",
        scalar_array_constant_lines("const MAGIC: Array<UInt32, 1> = [MAGIC[0]]"),
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
            "const MAGIC: Array<UInt32, 1> = [mask(0xFFFF)]",
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
            "const MAGIC: Array<UInt32, 1> = [raw_read(UART_STATUS)]",
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
            "const MAGIC: Array<UInt32, 1> = [await STATUS_MASK]",
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
