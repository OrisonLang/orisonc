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

auto ordinary_final_if_lines(std::string_view true_branch, std::string_view false_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function demo(flag: Bool) -> UInt32",
            "    if flag",
            "        " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto ordinary_final_switch_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function demo(flag: Bool) -> UInt32",
            "    switch flag",
            "        true => " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
}

auto ordinary_final_ternary_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function demo(flag: Bool) -> UInt32",
            "    " + std::string(expression),
        }
    );
}

auto choice_final_if_lines(std::string_view true_branch, std::string_view false_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    if flag",
            "        " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto choice_final_switch_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    switch flag",
            "        true => " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
}

auto choice_final_ternary_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    " + std::string(expression),
        }
    );
}

auto choice_final_unsafe_block_ternary_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    unsafe",
            "        " + std::string(expression),
        }
    );
}

auto choice_final_if_ternary_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return choice_final_if_lines(
        "flag ? " + std::string(true_branch) + " : " + std::string(false_branch),
        false_branch
    );
}

auto choice_final_switch_ternary_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return choice_final_switch_lines(
        "flag ? " + std::string(true_branch) + " : " + std::string(false_branch),
        false_branch
    );
}

auto box_maybe_items_cli_lines(
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Box<T>, 1>"
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "choice Wrap<T>",
        "    Items(value: " + std::string(payload_type) + ")",
        "record Box<T>",
        "    value: Maybe<T>",
    };
    if (include_holder) {
        lines.push_back("record Holder<T>");
        lines.push_back("    wrap: Wrap<T>");
    }
    lines.push_back("function demo(flag: Bool) -> UInt32");
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return cli_module_lines(lines);
}

auto slot_pointer_items_cli_lines(
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "record Slot<T>",
        "    ptr: Pointer<T>",
        "choice Wrap<T>",
        "    Items(value: " + std::string(payload_type) + ")",
    };
    if (include_holder) {
        lines.push_back("record Holder<T>");
        lines.push_back("    wrap: Wrap<T>");
    }
    lines.push_back(
        "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32"
    );
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return cli_module_lines(lines);
}

auto slot_pointer_items_success_cli_lines(
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
)
    -> std::vector<std::string> {
    auto lines = slot_pointer_items_cli_lines(binding_line, include_holder, payload_type);
    auto const function_index = include_holder ? std::size_t {7} : std::size_t {5};
    lines[function_index] =
        "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    var item: Wrap<UInt32> = Items([[Box(Some(1 as UInt32))]])",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    var item: Wrap<UInt32> = Items([[Slot(raw_offset(other, 1))]])",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_items_assignment_success_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    auto lines = slot_pointer_items_assignment_cli_lines(assignment_line);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_holder_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "function demo(flag: Bool) -> UInt32",
            "    var holder: Holder<UInt32> = Holder(Items([[Box(Some(1 as UInt32))]]))",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_holder_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    var holder: Holder<UInt32> = Holder(Items([[Slot(raw_offset(other, 1))]]))",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_holder_items_assignment_success_cli_lines(std::string_view assignment_line)
    -> std::vector<std::string> {
    auto lines = slot_pointer_holder_items_assignment_cli_lines(assignment_line);
    lines[7] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> Wrap<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Wrap<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_items_return_success_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    auto lines = slot_pointer_items_return_cli_lines(return_line);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Wrap<UInt32>";
    return lines;
}

auto box_maybe_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> Wrap<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Items([[Box(Some(1 as UInt32))]])",
        }
    );
}

auto slot_pointer_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Wrap<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Items([[Slot(raw_offset(other, 1))]])",
        }
    );
}

auto slot_pointer_items_final_if_success_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    auto lines = slot_pointer_items_final_if_cli_lines(true_branch);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Wrap<UInt32>";
    return lines;
}

auto box_maybe_holder_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "function demo(flag: Bool) -> Holder<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_holder_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Holder<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_holder_items_return_success_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    auto lines = slot_pointer_holder_items_return_cli_lines(return_line);
    lines[7] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Holder<UInt32>";
    return lines;
}

auto box_maybe_holder_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "function demo(flag: Bool) -> Holder<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Holder(Items([[Box(Some(1 as UInt32))]]))",
        }
    );
}

auto slot_pointer_holder_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Holder<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Holder(Items([[Slot(raw_offset(other, 1))]]))",
        }
    );
}

auto slot_pointer_holder_items_final_if_success_cli_lines(std::string_view true_branch)
    -> std::vector<std::string> {
    auto lines = slot_pointer_holder_items_final_if_cli_lines(true_branch);
    lines[7] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Holder<UInt32>";
    return lines;
}

void assert_cli_choice_payload_items_choice_ternary_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Box<T>, 1>"
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        box_maybe_items_cli_lines(binding_line, include_holder, payload_type),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
}

void assert_cli_choice_payload_items_choice_ternary_success(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Box<T>, 1>"
) {
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        box_maybe_items_cli_lines(binding_line, include_holder, payload_type)
    );
}

void assert_cli_choice_payload_items_pointer_ternary_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        slot_pointer_items_cli_lines(binding_line, include_holder, payload_type),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
}

void assert_cli_choice_payload_items_pointer_ternary_success(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) {
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        slot_pointer_items_success_cli_lines(binding_line, include_holder, payload_type)
    );
}

auto box_maybe_record_cli_lines(std::string_view binding_line, bool include_outer) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Box<T>",
        "    value: Maybe<T>",
    };
    if (include_outer) {
        lines.push_back("record Outer<T>");
        lines.push_back("    box: Box<T>");
    }
    lines.push_back("function demo(flag: Bool) -> UInt32");
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return cli_module_lines(lines);
}

auto slot_pointer_record_cli_lines(std::string_view binding_line, bool include_wrapper) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "record Slot<T>",
        "    ptr: Pointer<T>",
    };
    if (include_wrapper) {
        lines.push_back("record Wrapper<T>");
        lines.push_back("    slot: Slot<T>");
    }
    lines.push_back(
        "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32"
    );
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return cli_module_lines(lines);
}

auto slot_pointer_record_success_cli_lines(std::string_view binding_line, bool include_wrapper)
    -> std::vector<std::string> {
    auto lines = slot_pointer_record_cli_lines(binding_line, include_wrapper);
    auto const function_index = include_wrapper ? std::size_t {5} : std::size_t {3};
    lines[function_index] =
        "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_nested_array_field_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "record Shelf<T>",
            "    boxes: Array<Array<Box<T>, 1>, 1>",
            "function demo(flag: Bool) -> UInt32",
            std::string(binding_line),
            "    return 1",
        }
    );
}

auto slot_pointer_nested_array_field_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "record Rack<T>",
            "    slots: Array<Array<Slot<T>, 1>, 1>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            std::string(binding_line),
            "    return 1",
        }
    );
}

auto slot_pointer_nested_array_field_success_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    auto lines = slot_pointer_nested_array_field_cli_lines(binding_line);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto maybe_nested_array_field_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record ChoiceShelf<T>",
            "    values: Array<Array<Maybe<T>, 1>, 1>",
            "function demo(flag: Bool) -> UInt32",
            std::string(binding_line),
            "    return 1",
        }
    );
}

auto box_maybe_nested_array_argument_cli_lines(
    std::string_view call_line,
    bool method_call = false
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Box<T>",
        "    value: Maybe<T>",
    };
    if (method_call) {
        lines.push_back("record Consumer");
        lines.push_back("    id: UInt32");
        lines.push_back("extend Consumer");
        lines.push_back("    function consume(this: shared This, values: Array<Array<Box<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("        return 1");
        lines.push_back("function demo(consumer: Consumer, flag: Bool) -> UInt32");
    } else {
        lines.push_back("function consume(values: Array<Array<Box<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("    return 1");
        lines.push_back("function demo(flag: Bool) -> UInt32");
    }
    lines.push_back(std::string(call_line));
    return cli_module_lines(lines);
}

auto maybe_nested_array_argument_cli_lines(
    std::string_view call_line,
    bool method_call = false
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
    };
    if (method_call) {
        lines.push_back("record Consumer");
        lines.push_back("    id: UInt32");
        lines.push_back("extend Consumer");
        lines.push_back("    function consume(this: shared This, values: Array<Array<Maybe<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("        return 1");
        lines.push_back("function demo(consumer: Consumer, flag: Bool) -> UInt32");
    } else {
        lines.push_back("function consume(values: Array<Array<Maybe<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("    return 1");
        lines.push_back("function demo(flag: Bool) -> UInt32");
    }
    lines.push_back(std::string(call_line));
    return cli_module_lines(lines);
}

auto box_maybe_nested_array_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    var values: Array<Array<Box<UInt32>, 1>, 1> = [[Box(Some(1 as UInt32))]]",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto maybe_nested_array_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var values: Array<Array<Maybe<UInt32>, 1>, 1> = [[Some(1 as UInt32)]]",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_nested_array_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    var slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(raw_offset(other, 1))]]",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_nested_array_assignment_success_cli_lines(std::string_view assignment_line)
    -> std::vector<std::string> {
    auto lines = slot_pointer_nested_array_assignment_cli_lines(assignment_line);
    lines[3] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

template <typename SourceLines>
void assert_cli_record_field_nested_array_choice_context_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    SourceLines const& lines
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        lines,
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
}

template <typename SourceLines>
void assert_cli_record_field_nested_array_pointer_context_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    SourceLines const& lines
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        lines,
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
}

template <typename SourceLines>
void assert_cli_record_field_nested_array_context_success(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    SourceLines const& lines
) {
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        lines
    );
}

auto low_level_read_call(std::string_view intrinsic, std::string_view operand) -> std::string {
    return std::string(intrinsic) + "(" + std::string(operand) + ")";
}

auto low_level_final_if_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    if flag",
            "        " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto low_level_final_switch_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    switch flag",
            "        true => " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
}

auto low_level_unsafe_final_if_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    unsafe",
            "        if flag",
            "            " + std::string(true_branch),
            "        else",
            "            " + std::string(false_branch),
        }
    );
}

auto low_level_unsafe_final_switch_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    unsafe",
            "        switch flag",
            "            true => " + std::string(true_branch),
            "            false => " + std::string(false_branch),
        }
    );
}

auto low_level_final_if_unsafe_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    if flag",
            "        unsafe",
            "            " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto low_level_final_switch_unsafe_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    switch flag",
            "        true =>",
            "            unsafe",
            "                " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
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
    return low_level_final_if_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_if_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_switch_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_switch_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_unsafe_block_if_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_if_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_unsafe_block_if_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_if_lines(
        "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_unsafe_block_switch_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_switch_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_unsafe_block_switch_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_switch_lines(
        "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_if_unsafe_block_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_unsafe_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_if_unsafe_block_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    if flag",
            "        unsafe",
            "            " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        unsafe",
            "            " + low_level_read_call(intrinsic, "right"),
        }
    );
}

auto low_level_final_switch_unsafe_block_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_unsafe_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_switch_unsafe_block_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    switch flag",
            "        true =>",
            "            unsafe",
            "                " + low_level_read_call(intrinsic, "left"),
            "        false =>",
            "            unsafe",
            "                " + low_level_read_call(intrinsic, "right"),
        }
    );
}

auto low_level_final_unsafe_block_ternary_read_mismatch_lines(std::string_view intrinsic)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    unsafe",
            "        flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        }
    );
}

auto low_level_final_unsafe_block_ternary_read_success_lines(std::string_view intrinsic)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    unsafe",
            "        flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                low_level_read_call(intrinsic, "right"),
        }
    );
}

auto low_level_final_if_ternary_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        "1 as UInt32"
    );
}

auto low_level_final_if_ternary_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : " +
            low_level_read_call(intrinsic, "right"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_switch_ternary_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        "1 as UInt32"
    );
}

auto low_level_final_switch_ternary_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : " +
            low_level_read_call(intrinsic, "right"),
        low_level_read_call(intrinsic, "right")
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

void assert_cli_nested_final_container_parse_cases(
    std::filesystem::path const& executable,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_if_expression_type.or"),
        low_level_final_unsafe_block_if_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_if_expression_success.or"),
        low_level_final_unsafe_block_if_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_switch_expression_type.or"),
        low_level_final_unsafe_block_switch_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_switch_expression_success.or"),
        low_level_final_unsafe_block_switch_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_unsafe_block_expression_type.or"),
        low_level_final_if_unsafe_block_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_unsafe_block_expression_success.or"),
        low_level_final_if_unsafe_block_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_unsafe_block_expression_type.or"),
        low_level_final_switch_unsafe_block_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_unsafe_block_expression_success.or"),
        low_level_final_switch_unsafe_block_read_success_lines(intrinsic)
    );
}

void assert_cli_final_container_return_parse_cases(
    std::filesystem::path const& executable,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_return_type.or"),
        low_level_final_if_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_return_success.or"),
        low_level_final_if_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_return_type.or"),
        low_level_final_switch_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_return_success.or"),
        low_level_final_switch_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
}

void assert_cli_final_ternary_low_level_parse_cases(
    std::filesystem::path const& executable,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_expression_type.or"),
        cli_module_lines(
            {
                "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
                "    flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
            }
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_expression_success.or"),
        cli_module_lines(
            {
                "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
                "    flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                    low_level_read_call(intrinsic, "right"),
            }
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_return_type.or"),
        cli_module_lines(
            {
                "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
                "    return flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
            }
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_return_success.or"),
        cli_module_lines(
            {
                "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
                "    return flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                    low_level_read_call(intrinsic, "right"),
            }
        )
    );
}

void assert_cli_nested_final_ternary_low_level_parse_cases(
    std::filesystem::path const& executable,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_ternary_type.or"),
        low_level_final_unsafe_block_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_ternary_success.or"),
        low_level_final_unsafe_block_ternary_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_ternary_type.or"),
        low_level_final_if_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_ternary_success.or"),
        low_level_final_if_ternary_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_ternary_type.or"),
        low_level_final_switch_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_ternary_success.or"),
        low_level_final_switch_ternary_read_success_lines(intrinsic)
    );
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
        ordinary_final_if_lines("true", "1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_expression_type.or",
        ordinary_final_switch_lines("true", "1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_expression_success.or",
        ordinary_final_if_lines("1 as UInt32", "2 as UInt32")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_expression_success.or",
        ordinary_final_switch_lines("1 as UInt32", "2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_return_type.or",
        ordinary_final_if_lines("return true", "return 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_return_type.or",
        ordinary_final_switch_lines("return true", "return 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_return_success.or",
        ordinary_final_if_lines("return 1 as UInt32", "return 2 as UInt32")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_return_success.or",
        ordinary_final_switch_lines("return 1 as UInt32", "return 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_expression_type.or",
        ordinary_final_ternary_lines("flag ? true : 1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_expression_success.or",
        ordinary_final_ternary_lines("flag ? 1 as UInt32 : 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_return_type.or",
        ordinary_final_ternary_lines("return flag ? true : 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_return_success.or",
        ordinary_final_ternary_lines("return flag ? 1 as UInt32 : 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_without_else_value.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    if flag",
            "        1 as UInt32",
        },
        "function body must end with an expression statement, value return, or total final-expression container"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_unsafe_without_value.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> UInt32",
            "    unsafe",
            "        let value = 1 as UInt32",
        },
        "function body must end with an expression statement, value return, or total final-expression container"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_non_value_case.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    switch flag",
            "        true =>",
            "            let value = 1 as UInt32",
            "        false => 2 as UInt32",
        },
        "function body must end with an expression statement, value return, or total final-expression container"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_non_unit_empty_return.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> UInt32",
            "    return",
        },
        "return statement must return a value for declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_empty_return.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> Unit",
            "    return",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_final_if_without_else.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> Unit",
            "    if flag",
            "        let value = 1 as UInt32",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_final_unsafe_without_value.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> Unit",
            "    unsafe",
            "        let value = 1 as UInt32",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_final_switch_non_value_case.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> Unit",
            "    switch flag",
            "        true =>",
            "            let value = 1 as UInt32",
            "        false =>",
            "            let value = 2 as UInt32",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_payload_type.or",
        choice_final_if_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_payload_type.or",
        choice_final_switch_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_return_payload_type.or",
        choice_final_if_lines("return Some(true)", "return Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_return_payload_type.or",
        choice_final_switch_lines("return Some(true)", "return Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_success.or",
        choice_final_if_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_success.or",
        choice_final_switch_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_return_success.or",
        choice_final_if_lines("return Some(1 as UInt32)", "return Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_return_success.or",
        choice_final_switch_lines("return Some(1 as UInt32)", "return Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_payload_type.or",
        choice_final_ternary_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_return_payload_type.or",
        choice_final_ternary_lines("return flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_success.or",
        choice_final_ternary_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_return_success.or",
        choice_final_ternary_lines("return flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unsafe_final_ternary_payload_type.or",
        choice_final_unsafe_block_ternary_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unsafe_final_ternary_success.or",
        choice_final_unsafe_block_ternary_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_ternary_payload_type.or",
        choice_final_if_ternary_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_ternary_success.or",
        choice_final_if_ternary_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_ternary_payload_type.or",
        choice_final_switch_ternary_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_ternary_success.or",
        choice_final_switch_ternary_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_binding_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    let value: Maybe<UInt32> = flag ? Some(true) : Empty",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_binding_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    let value: Maybe<UInt32> = flag ? Some(1 as UInt32) : Empty",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_assignment_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = flag ? Some(true) : Empty",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_assignment_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = flag ? Some(1 as UInt32) : Empty",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_call_argument_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo(flag: Bool) -> UInt32",
            "    return consume(flag ? Some(true) : Empty)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_call_argument_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo(flag: Bool) -> UInt32",
            "    return consume(flag ? Some(1 as UInt32) : Empty)",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_return_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_binding_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_binding_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_assignment_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    var p: Pointer<UInt32> = raw_offset(other, 1)",
            "    p = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_assignment_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    var p: Pointer<Byte> = raw_offset(other, 1)",
            "    p = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_call_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function consume(ptr: Pointer<UInt32>) -> UInt32",
            "    return 1",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    return consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_call_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function consume(ptr: Pointer<Byte>) -> UInt32",
            "    return 1",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> UInt32",
            "    return consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_method_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Device",
            "    status: UInt32",
            "extend Device",
            "    function consume(this: shared This, ptr: Pointer<UInt32>) -> UInt32",
            "        return 1",
            "unsafe function demo(device: Device, flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    return device.consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_method_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Device",
            "    status: UInt32",
            "extend Device",
            "    function consume(this: shared This, ptr: Pointer<Byte>) -> UInt32",
            "        return 1",
            "unsafe function demo(device: Device, flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> UInt32",
            "    return device.consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_if_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    if flag",
            "        flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    else",
            "        raw_offset(other, 1)",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_ternary_pointer_source_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(flag: Bool, buf: Buffer, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? Pointer(address_of(buf.data[0])) : raw_offset(other, 1)",
        },
        "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'"
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
        std::filesystem::temp_directory_path() / "orison_cli_address_return_ternary_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    return flag ? address_of(buf.data[0]) : \"text\"",
        },
        "address-returning function currently requires a structurally address-like expression"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_unsafe_final_ternary_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    unsafe",
            "        flag ? address_of(buf.data[0]) : address_of(buf.data[1])",
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
    assert_cli_nested_final_container_parse_cases(executable, "raw_read");
    assert_cli_final_container_return_parse_cases(executable, "raw_read");
    assert_cli_final_ternary_low_level_parse_cases(executable, "raw_read");
    assert_cli_nested_final_ternary_low_level_parse_cases(executable, "raw_read");
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
    assert_cli_nested_final_container_parse_cases(executable, "volatile_read");
    assert_cli_final_container_return_parse_cases(executable, "volatile_read");
    assert_cli_final_ternary_low_level_parse_cases(executable, "volatile_read");
    assert_cli_nested_final_ternary_low_level_parse_cases(executable, "volatile_read");
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
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_choice_ternary_field_type.or",
        box_maybe_record_cli_lines("    let box: Box<UInt32> = Box(flag ? Some(true) : Empty)", false),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_choice_ternary_field_success.or",
        box_maybe_record_cli_lines("    let box: Box<UInt32> = Box(flag ? Some(1 as UInt32) : Empty)", false)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_pointer_ternary_field_type.or",
        slot_pointer_record_cli_lines(
            "    let slot: Slot<UInt32> = Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
            false
        ),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_pointer_ternary_field_success.or",
        slot_pointer_record_success_cli_lines(
            "    let slot: Slot<UInt32> = Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
            false
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_choice_ternary_field_type.or",
        box_maybe_record_cli_lines(
            "    let outer: Outer<UInt32> = Outer(Box(flag ? Some(true) : Empty))",
            true
        ),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_choice_ternary_field_success.or",
        box_maybe_record_cli_lines(
            "    let outer: Outer<UInt32> = Outer(Box(flag ? Some(1 as UInt32) : Empty))",
            true
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_pointer_ternary_field_type.or",
        slot_pointer_record_cli_lines(
            "    let wrapper: Wrapper<UInt32> = Wrapper(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            true
        ),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_pointer_ternary_field_success.or",
        slot_pointer_record_success_cli_lines(
            "    let wrapper: Wrapper<UInt32> = Wrapper(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            true
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_choice_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Item(value: Box<T>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Box(flag ? Some(true) : Empty))",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_choice_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Item(value: Box<T>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Box(flag ? Some(1 as UInt32) : Empty))",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_choice_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let boxes: Array<Box<UInt32>, 1> = [Box(flag ? Some(true) : Empty)]",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_choice_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let boxes: Array<Box<UInt32>, 1> = [Box(flag ? Some(1 as UInt32) : Empty)]",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_pointer_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Item(value: Slot<T>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            "    return 1",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_pointer_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Item(value: Slot<T>)",
            "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_pointer_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    let slots: Array<Slot<UInt32>, 1> = [Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]",
            "    return 1",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_pointer_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32",
            "    let slots: Array<Slot<UInt32>, 1> = [Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_choice_ternary_field_type.or",
        box_maybe_record_cli_lines(
            "    let boxes: Array<Array<Box<UInt32>, 1>, 1> = [[Box(flag ? Some(true) : Empty)]]",
            false
        ),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_choice_ternary_field_success.or",
        box_maybe_record_cli_lines(
            "    let boxes: Array<Array<Box<UInt32>, 1>, 1> = [[Box(flag ? Some(1 as UInt32) : Empty)]]",
            false
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_record_cli_lines(
            "    let slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]",
            false
        ),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_record_success_cli_lines(
            "    let slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]",
            false
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_record_field_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_field_cli_lines(
            "    let shelf: Shelf<UInt32> = Shelf([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_record_field_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_field_cli_lines(
            "    let shelf: Shelf<UInt32> = Shelf([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_record_field_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_nested_array_field_cli_lines(
            "    let rack: Rack<UInt32> = Rack([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_record_field_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_nested_array_field_success_cli_lines(
            "    let rack: Rack<UInt32> = Rack([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_record_field_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_field_cli_lines(
            "    let shelf: ChoiceShelf<UInt32> = ChoiceShelf([[flag ? Some(true) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_record_field_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_field_cli_lines(
            "    let shelf: ChoiceShelf<UInt32> = ChoiceShelf([[flag ? Some(1 as UInt32) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_call_argument_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consume([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_call_argument_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consume([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_call_argument_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_argument_cli_lines(
            "    return consume([[flag ? Some(true) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_call_argument_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_argument_cli_lines(
            "    return consume([[flag ? Some(1 as UInt32) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_method_argument_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[Box(flag ? Some(true) : Empty)]])",
            true
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_method_argument_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[Box(flag ? Some(1 as UInt32) : Empty)]])",
            true
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_method_argument_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[flag ? Some(true) : Empty]])",
            true
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_method_argument_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[flag ? Some(1 as UInt32) : Empty]])",
            true
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_assignment_cli_lines(
            "    values = [[Box(flag ? Some(true) : Empty)]]"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_assignment_cli_lines(
            "    values = [[Box(flag ? Some(1 as UInt32) : Empty)]]"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_assignment_cli_lines(
            "    values = [[flag ? Some(true) : Empty]]"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_assignment_cli_lines(
            "    values = [[flag ? Some(1 as UInt32) : Empty]]"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_assignment_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_nested_array_assignment_cli_lines(
            "    slots = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_nested_array_assignment_success_cli_lines(
            "    slots = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]"
        )
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_choice_payload_array_record_choice_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([Box(flag ? Some(true) : Empty)])",
        false
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_choice_payload_array_record_choice_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([Box(flag ? Some(1 as UInt32) : Empty)])",
        false
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_choice_payload_array_record_pointer_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))])",
        false
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_choice_payload_array_record_pointer_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))])",
        false
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_choice_payload_nested_array_record_choice_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([[Box(flag ? Some(true) : Empty)]])",
        false,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_choice_payload_nested_array_record_choice_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([[Box(flag ? Some(1 as UInt32) : Empty)]])",
        false,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])",
        false,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])",
        false,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_assignment_cli_lines(
            "    item = Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_assignment_cli_lines(
            "    item = Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_assignment_cli_lines(
            "    item = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_assignment_success_cli_lines(
            "    item = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_return_cli_lines(
            "    return Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_return_cli_lines(
            "    return Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_return_cli_lines(
            "    Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_return_cli_lines(
            "    Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_return_cli_lines(
            "    return Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_return_success_cli_lines(
            "    return Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_return_cli_lines(
            "    Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_return_success_cli_lines(
            "    Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_final_if_cli_lines(
            "        Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_final_if_cli_lines(
            "        Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_final_if_cli_lines(
            "        Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_final_if_success_cli_lines(
            "        Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_array_record_choice_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([Box(flag ? Some(true) : Empty)]))",
        true
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_array_record_choice_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([Box(flag ? Some(1 as UInt32) : Empty)]))",
        true
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_array_record_pointer_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]))",
        true
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_array_record_pointer_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]))",
        true
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_choice_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Box(flag ? Some(true) : Empty)]]))",
        true,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_choice_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))",
        true,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))",
        true,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))",
        true,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_assignment_cli_lines(
            "    holder = Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_assignment_cli_lines(
            "    holder = Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_assignment_cli_lines(
            "    holder = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_assignment_success_cli_lines(
            "    holder = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_return_cli_lines(
            "    return Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_return_cli_lines(
            "    return Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_return_cli_lines(
            "    Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_return_cli_lines(
            "    Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_return_cli_lines(
            "    return Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_return_success_cli_lines(
            "    return Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_return_cli_lines(
            "    Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_return_success_cli_lines(
            "    Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_final_if_cli_lines(
            "        Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_final_if_cli_lines(
            "        Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_final_if_cli_lines(
            "        Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_final_if_success_cli_lines(
            "        Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
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
