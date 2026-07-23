#include "aggregate_field_cli_fixtures.hpp"

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_aggregate_field_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto aggregate_ownership_cli_lines = [](
        bool include_nested,
        bool use_switch,
        bool balanced_transfer,
        bool post_merge_reuse
    ) {
        auto lines = std::vector<std::string_view> {
            "package demo.cli",
            "record Payload",
            "    value: UInt32",
            "record Box",
            "    payload: Payload",
        };
        if (include_nested) {
            lines.push_back("record Nested");
            lines.push_back("    box: Box");
        }
        lines.push_back("function consume_payload(payload: Payload) -> UInt32");
        lines.push_back("    payload.value");
        if (post_merge_reuse) {
            if (include_nested) {
                lines.push_back("function consume_nested(nested: Nested) -> UInt32");
                lines.push_back("    nested.box.payload.value");
            } else {
                lines.push_back("function consume_box(box: Box) -> UInt32");
                lines.push_back("    box.payload.value");
            }
        }
        lines.push_back("function main(flag: Bool) -> UInt32");
        if (include_nested) {
            lines.push_back("    let nested: Nested = Nested(Box(Payload(1 as UInt32)))");
        } else {
            lines.push_back("    let box: Box = Box(Payload(1 as UInt32))");
        }
        if (use_switch) {
            lines.push_back("    switch flag");
            lines.push_back("        true =>");
            lines.push_back(include_nested
                ? "            let consumed: UInt32 = consume_payload(nested.box.payload)"
                : "            let consumed: UInt32 = consume_payload(box.payload)");
            lines.push_back("        false =>");
            lines.push_back(balanced_transfer
                ? (include_nested
                    ? "            let consumed: UInt32 = consume_payload(nested.box.payload)"
                    : "            let consumed: UInt32 = consume_payload(box.payload)")
                : "            let fallback: UInt32 = 0 as UInt32");
        } else {
            lines.push_back("    if flag");
            lines.push_back(include_nested
                ? "        let consumed: UInt32 = consume_payload(nested.box.payload)"
                : "        let consumed: UInt32 = consume_payload(box.payload)");
            lines.push_back("    else");
            lines.push_back(balanced_transfer
                ? (include_nested
                    ? "        let consumed: UInt32 = consume_payload(nested.box.payload)"
                    : "        let consumed: UInt32 = consume_payload(box.payload)")
                : "        let fallback: UInt32 = 0 as UInt32");
        }
        if (post_merge_reuse) {
            lines.push_back(include_nested ? "    consume_nested(nested)" : "    consume_box(box)");
        } else {
            lines.push_back("    0 as UInt32");
        }
        return lines;
    };
    auto scalar_nested_member_call_cli_lines = [] {
        return std::vector<std::string_view> {
            "package demo.cli",
            "record Payload",
            "    value: UInt32",
            "record Box",
            "    payload: Payload",
            "    count: UInt32",
            "record Nested",
            "    box: Box",
            "function consume_count(count: UInt32) -> UInt32",
            "    count",
            "function main() -> UInt32",
            "    let nested: Nested = Nested(Box(Payload(1 as UInt32), 7 as UInt32))",
            "    consume_count(nested.box.count)",
        };
    };
    auto scalar_nested_member_transfer_failure_cli_lines = [](std::string_view argument) {
        auto lines = std::vector<std::string> {
            "package demo.cli",
            "record Payload",
            "    value: UInt32",
            "record Box",
            "    payload: Payload",
            "    count: UInt32",
            "record Nested",
            "    box: Box",
            "function consume_payload(payload: Payload) -> UInt32",
            "    payload.value",
            "function main() -> UInt32",
            "    let nested: Nested = Nested(Box(Payload(1 as UInt32), 7 as UInt32))",
        };
        lines.push_back("    consume_payload(" + std::string(argument) + ")");
        return lines;
    };
    auto assert_cli_emit_llvm_failure_without_ownership_diagnostic = [](
        std::filesystem::path const& executable,
        std::filesystem::path const& path,
        auto const& lines,
        std::string_view expected_message
    ) {
        {
            std::ofstream output(path);
            for (auto line : lines) {
                output << line << '\n';
            }
        }

        auto command = executable.string() + " --emit-llvm " + path.string();
        auto output = read_failing_command_output(command);
        assert(output.find(expected_message) != std::string::npos);
        assert(output.find("ownership mismatch") == std::string::npos);
        assert(output.find("use after move") == std::string::npos);
    };

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
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_field_if_branch_ownership_mismatch.or",
        aggregate_ownership_cli_lines(false, false, false, false),
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_field_switch_case_ownership_mismatch.or",
        aggregate_ownership_cli_lines(false, true, false, false),
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_field_if_branch_ownership_mismatch.or",
        aggregate_ownership_cli_lines(true, false, false, false),
        "if branch ownership mismatch: owned transfers must match across all continuing branches"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_field_switch_case_ownership_mismatch.or",
        aggregate_ownership_cli_lines(true, true, false, false),
        "switch case ownership mismatch: owned transfers must match across all continuing cases"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_field_if_balanced_post_merge_reuse.or",
        aggregate_ownership_cli_lines(false, false, true, true),
        "use after move: box.payload"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_field_switch_balanced_post_merge_reuse.or",
        aggregate_ownership_cli_lines(false, true, true, true),
        "use after move: box.payload"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_field_if_balanced_post_merge_reuse.or",
        aggregate_ownership_cli_lines(true, false, true, true),
        "use after move: nested.box.payload"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_field_switch_balanced_post_merge_reuse.or",
        aggregate_ownership_cli_lines(true, true, true, true),
        "use after move: nested.box.payload"
    );
    auto nested_scalar_member_call_path =
        std::filesystem::temp_directory_path() / "orison_cli_nested_scalar_member_call_success.or";
    assert_cli_parse_success(
        executable,
        nested_scalar_member_call_path,
        scalar_nested_member_call_cli_lines()
    );
    auto nested_scalar_member_call_output =
        read_command_output(executable.string() + " --emit-llvm " + nested_scalar_member_call_path.string());
    assert(nested_scalar_member_call_output.find("call i32 @consume_count(i32") != std::string::npos);
    assert(nested_scalar_member_call_output.find("ownership mismatch") == std::string::npos);
    assert(nested_scalar_member_call_output.find("use after move") == std::string::npos);
    assert_cli_emit_llvm_failure_without_ownership_diagnostic(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_scalar_member_to_owned_parameter_failure.or",
        scalar_nested_member_transfer_failure_cli_lines("nested.box.count"),
        "function argument 'payload' type 'UInt32' does not match declared type 'Payload'"
    );
    assert_cli_emit_llvm_failure_without_ownership_diagnostic(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_scalar_member_continuation_failure.or",
        scalar_nested_member_transfer_failure_cli_lines("nested.box.count.payload"),
        "type 'UInt32' has no member 'payload'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_declared_record_unknown_member_failure.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Header",
            "    version: UInt16",
            "function main() -> UInt16",
            "    let header = Header(1 as UInt16)",
            "    return header.missing",
        },
        "type 'Header' has no member 'missing'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_concrete_generic_record_unknown_member_failure.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Box<T>",
            "    value: T",
            "function main() -> UInt32",
            "    let box: Box<UInt32> = Box(1 as UInt32)",
            "    return box.missing",
        },
        "type 'Box<UInt32>' has no member 'missing'"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_record_unknown_member_failure.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Registers",
            "    status: UInt32",
            "unsafe function demo(regs: Pointer<Registers>) -> UInt32",
            "    return regs.missing",
        },
        "type 'Pointer<Registers>' has no member 'missing'"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_record_scalar_member_continuation.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Registers",
            "    status: UInt32",
            "unsafe function demo(regs: Pointer<Registers>) -> UInt32",
            "    return regs.status.missing",
        },
        "type 'UInt32' has no member 'missing'"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() /
            "orison_cli_concrete_generic_pointer_record_scalar_member_continuation.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Box<T>",
            "    value: T",
            "unsafe function demo(box: Pointer<Box<UInt32>>) -> UInt32",
            "    return box.value.missing",
        },
        "type 'UInt32' has no member 'missing'"
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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
