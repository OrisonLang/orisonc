#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
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
void write_lines(std::filesystem::path const& path, SourceLines const& lines) {
    std::ofstream output(path);
    for (auto line : lines) {
        output << line << '\n';
    }
}

template <typename SourceLines>
void assert_cli_parse_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    write_lines(path, lines);

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
    write_lines(path, lines);

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);
    assert(output.find("parsed ") != std::string::npos);
}

template <typename SourceLines>
void assert_cli_emit_llvm_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    write_lines(path, lines);

    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

void assert_cli_emit_llvm_existing_fixture_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    std::string_view expected_message
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

void assert_cli_run_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " run " + path.string();
    auto output = read_command_output(command);
    assert(output.empty());
}

void assert_cli_emit_llvm_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("define i64 @first__UInt64({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("call i32 @first__UInt32({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("call i64 @first__UInt64({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("getelementptr i32, ptr %values.dynamic_array_index") != std::string::npos);
    assert(output.find("getelementptr i64, ptr %values.dynamic_array_index") != std::string::npos);
}

void assert_cli_emit_llvm_call_result_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define { ptr, i64, i64 } @make_values()") != std::string::npos);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("%tmp0 = call { ptr, i64, i64 } @make_values()") != std::string::npos);
    assert(output.find("%tmp1 = call i32 @first__UInt32({ ptr, i64, i64 } %tmp0)") != std::string::npos);
    assert(output.find("ret { ptr, i64, i64 } %tmp1") != std::string::npos);
}

void assert_cli_emit_llvm_nested_call_result_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("define i32 @consume__UInt32(i32 %value)") != std::string::npos);
    assert(output.find("%tmp1 = call i32 @first__UInt32({ ptr, i64, i64 } %tmp0)") != std::string::npos);
    assert(output.find("%tmp2 = call i32 @consume__UInt32(i32 %tmp1)") != std::string::npos);
}

void assert_cli_emit_llvm_local_call_result_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @first__UInt32({ ptr, i64, i64 } %values)") != std::string::npos);
    assert(output.find("define i32 @consume__UInt32(i32 %value)") != std::string::npos);
    assert(output.find("%value = add i32 0, %tmp1") != std::string::npos);
    assert(output.find("call i32 @consume__UInt32(i32 %value)") != std::string::npos);
}

void assert_cli_emit_llvm_generic_method_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i32 @select_value__UInt64(i64 %value)") != std::string::npos);
    assert(output.find("define i32 @method.UInt32.select__UInt64(i32 %this, i64 %value)") != std::string::npos);
    assert(output.find("define i32 @method.Box_UInt32_.value__UInt32(%record.Box_UInt32_ %this)") != std::string::npos);
    assert(output.find("define i32 @method.Pair_UInt32__UInt64_.first__UInt32__UInt64(%record.Pair_UInt32__UInt64_ %this)") != std::string::npos);
    assert(output.find("define %record.Pair_UInt32__UInt64_ @method.Box_Pair_UInt32__UInt64__.value__Pair_UInt32__UInt64_(%record.Box_Pair_UInt32__UInt64__ %this)") != std::string::npos);
    assert(output.find("call i32 @select_value__UInt64(i64 11)") != std::string::npos);
    assert(output.find("call i32 @method.UInt32.select__UInt64(i32 %seed, i64 9)") != std::string::npos);
    assert(output.find("call i32 @method.Box_UInt32_.value__UInt32(%record.Box_UInt32_ %tmp") != std::string::npos);
    assert(output.find("call i32 @method.Pair_UInt32__UInt64_.first__UInt32__UInt64(%record.Pair_UInt32__UInt64_ %tmp") != std::string::npos);
    assert(output.find("call %record.Pair_UInt32__UInt64_ @method.Box_Pair_UInt32__UInt64__.value__Pair_UInt32__UInt64_(%record.Box_Pair_UInt32__UInt64__ %tmp") != std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_receiver_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define i64 @method.DynamicArray_UInt32_.count__UInt32({ ptr, i64, i64 } %this)") != std::string::npos);
    assert(output.find("call i64 @method.DynamicArray_UInt32_.count__UInt32({ ptr, i64, i64 } %tmp") != std::string::npos);
    assert(output.find("ret i64 %this.dynamic_array_length0.value") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_deallocate(ptr %this.") == std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_receiver_append_scalar_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define void @method.DynamicArray_UInt32_.append_value__UInt32(ptr %this, i32 %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_UInt32_.append_value__UInt32(ptr %values.addr, i32 3)") !=
        std::string::npos);
    assert(output.find("%this.dynamic_array_append") != std::string::npos);
    assert(output.find("call void @__orison_dynamic_array_grow") != std::string::npos);
}

void assert_cli_emit_llvm_dynamic_array_complete_contract_fixture_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path
) {
    auto command = executable.string() + " --emit-llvm " + path.string();
    auto output = read_command_output(command);
    assert(output.find("define void @method.DynamicArray_Payload_.append_value__Payload(ptr %this, %record.Payload %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_Payload_.append_value__Payload(ptr %values.addr, %record.Payload %tmp") !=
        std::string::npos);
    assert(output.find("define void @method.DynamicArray_UInt32_.replace_first__UInt32(ptr %this, i32 %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_UInt32_.replace_first__UInt32(ptr %values.addr, i32 7)") !=
        std::string::npos);
    assert(output.find("define i64 @method.DynamicArray_UInt32_.count_each__UInt32({ ptr, i64, i64 } %this)") !=
        std::string::npos);
    assert(output.find("call i64 @method.DynamicArray_UInt32_.count_each__UInt32({ ptr, i64, i64 } %tmp") !=
        std::string::npos);
    assert(output.find("%this.sequence_for") != std::string::npos);
    assert(output.find("define void @method.DynamicArray_Payload_.replace_first__Payload(ptr %this, %record.Payload %value)") !=
        std::string::npos);
    assert(output.find("call void @method.DynamicArray_Payload_.replace_first__Payload(ptr %values.addr, %record.Payload %tmp") !=
        std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %this.dynamic_array_assign") !=
        std::string::npos);
    assert(output.find("define i32 @method.DynamicArray_Payload_.first_value__Payload({ ptr, i64, i64 } %this)") !=
        std::string::npos);
    assert(output.find("call i32 @method.DynamicArray_Payload_.first_value__Payload({ ptr, i64, i64 } %tmp") !=
        std::string::npos);
    assert(output.find("%this.dynamic_array_element_path") != std::string::npos);
    assert(output.find("getelementptr %record.Payload, ptr %this.dynamic_array_element_path") !=
        std::string::npos);
    assert(output.find("define i64 @method.DynamicArray_Payload_.count_each__Payload({ ptr, i64, i64 } %this)") !=
        std::string::npos);
    assert(output.find("call i64 @method.DynamicArray_Payload_.count_each__Payload({ ptr, i64, i64 } %tmp") !=
        std::string::npos);
    assert(output.find("call void @__orison_drop.Payload(ptr %values.dynamic_array_cleanup") !=
        std::string::npos);
}

auto generic_method_lines() -> std::vector<std::string> {
    return {
        "package demo.cli",
        "record Box<T>",
        "    value: T",
        "record Pair<A, B>",
        "    first: A",
        "    second: B",
        "function select_value<T>(value: T) -> UInt32",
        "    return 0 as UInt32",
        "extend UInt32",
        "    function select<T>(this: shared This, value: T) -> UInt32",
        "        return 0 as UInt32",
        "extend Box<T>",
        "    function value(this: shared This) -> T",
        "        return this.value",
        "extend Pair<A, B>",
        "    function first(this: shared This) -> A",
        "        return this.first",
        "function main() -> UInt32",
        "    let seed: UInt32 = 7 as UInt32",
        "    let box: Box<UInt32> = Box(13 as UInt32)",
        "    let pair: Pair<UInt32, UInt64> = Pair(17 as UInt32, 19 as UInt64)",
        "    let nested_box: Box<Pair<UInt32, UInt64>> = Box(pair)",
        "    let selected: UInt32 = select_value(11 as UInt64) + seed.select(9 as UInt64) + box.value()",
        "    let first_value: UInt32 = pair.first()",
        "    let nested_pair: Pair<UInt32, UInt64> = nested_box.value()",
        "    0 as UInt32",
    };
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

auto generic_same_record_for_lines() -> std::vector<std::string> {
    return {
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
        "    var total: UInt16 = 0 as UInt16",
        "    for item in [Same(Header([1, 2], 1), OtherHeader([1, 2], 1))]",
        "        total = total + item.first.version",
        "    total",
    };
}

auto underconstrained_generic_record_for_lines() -> std::vector<std::string> {
    return {
        "package demo.cli",
        "record Tag<T>",
        "    code: UInt32",
        "function main() -> UInt32",
        "    var total: UInt32 = 0 as UInt32",
        "    for item in [Tag(7 as UInt32), Tag(9 as UInt32)]",
        "        total = total + item.code",
        "    total",
    };
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_generic_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto fixtures = std::filesystem::current_path().parent_path().parent_path() / "tests" / "fixtures";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_function_dependent_argument_type.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(OtherHeader([1, 2], 1), 1 as UInt16))"}
        ),
        "function argument 'pair' type 'Pair<OtherHeader, UInt16>' does not match declared type 'Pair<Header, UInt16>'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_repeated_binding_argument_type.or",
        generic_same_consumer_lines({"    return same(Header([1, 2], 1), OtherHeader([1, 2], 1))"}),
        "function argument 'right' type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_record_repeated_field_type.or",
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
        smoke_temp_root / "orison_cli_generic_record_array_literal_for_repeated_field_type.or",
        generic_same_record_for_lines(),
        "record constructor field 'second' type 'OtherHeader' does not match expected field type 'Header'"
    );
    assert_cli_emit_llvm_failure(
        executable,
        smoke_temp_root / "orison_cli_underconstrained_generic_record_array_literal_for_emit.or",
        underconstrained_generic_record_for_lines(),
        "generic parameter 'T' cannot be inferred for record 'Tag'"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_parameter.or"
    );
    assert_cli_emit_llvm_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_parameter.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_call_result_parameter.or"
    );
    assert_cli_emit_llvm_call_result_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_call_result_parameter.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_nested_call_result_parameter.or"
    );
    assert_cli_emit_llvm_nested_call_result_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_nested_call_result_parameter.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_local_call_result_parameter.or"
    );
    assert_cli_emit_llvm_local_call_result_fixture_success(
        executable,
        fixtures / "dynamic_array_generic_local_call_result_parameter.or"
    );
    auto generic_method_path = smoke_temp_root / "orison_cli_generic_method_specialization.or";
    write_lines(generic_method_path, generic_method_lines());
    assert_cli_run_fixture_success(
        executable,
        generic_method_path
    );
    assert_cli_emit_llvm_generic_method_fixture_success(
        executable,
        generic_method_path
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_length.or"
    );
    assert_cli_emit_llvm_dynamic_array_receiver_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_length.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_append_scalar.or"
    );
    assert_cli_emit_llvm_dynamic_array_receiver_append_scalar_fixture_success(
        executable,
        fixtures / "dynamic_array_receiver_append_scalar.or"
    );
    assert_cli_run_fixture_success(
        executable,
        fixtures / "dynamic_array_complete_contract.or"
    );
    assert_cli_emit_llvm_dynamic_array_complete_contract_fixture_success(
        executable,
        fixtures / "dynamic_array_complete_contract.or"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_receiver_owned_read_rejected.or",
        "shared DynamicArray receiver index read of owned element requires a non-owning projection"
    );
    assert_cli_emit_llvm_existing_fixture_failure(
        executable,
        fixtures / "dynamic_array_receiver_append_missing_drop.or",
        "lowering DynamicArray push to owned element requires authorized element drop"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_generic_function_dependent_same_width_integer.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(Header([1, 2], 1), 1 as Int16))"}
        )
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
