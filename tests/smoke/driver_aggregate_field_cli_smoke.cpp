#define main orisonc_cli_smoke_original_main
#include "orisonc_cli_smoke.cpp"
#undef main

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_aggregate_field_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
