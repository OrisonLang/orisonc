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
