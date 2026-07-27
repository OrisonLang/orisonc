#pragma once

#include <array>
#include <string_view>

namespace orison::tests::smoke {

inline constexpr std::string_view computed_dynamic_array_descriptor_render_report =
    "computed DynamicArray for descriptor render function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 owner items descriptor %items.addr value %items.computed_for.descriptor "
    "data %items.computed_for.data length %items.computed_for.length snippets 3 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_control_render_report =
    "computed DynamicArray for loop control render function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 owner items condition items.computed_for.condition body items.computed_for.body "
    "continue items.computed_for.continue exit items.computed_for.exit index %items.computed_for.index "
    "next %items.computed_for.next.index bounds %items.computed_for.more snippets 5 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_element_address_render_report =
    "computed DynamicArray for element address render function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 lowers-to i32 owner items data %items.computed_for.data "
    "index %items.computed_for.index address %items.computed_for.element.addr snippets 1 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_element_load_render_report =
    "computed DynamicArray for element load render function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 lowers-to i32 owner items address %items.computed_for.element.addr "
    "item %items.computed_for.item snippets 1 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_continue_render_report =
    "computed DynamicArray for loop continue render function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 owner items continue items.computed_for.continue "
    "condition items.computed_for.condition index %items.computed_for.index "
    "next %items.computed_for.next.index snippets 3 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_render_sequence_report =
    "computed DynamicArray for loop render sequence function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 owner items body items.computed_for.body snippets 14 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_exit_cleanup_report =
    "computed DynamicArray for loop exit cleanup function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 owner items exit items.computed_for.exit from items.loop.entry to items "
    "operation items.computed_for.cleanup.resume snippets 2 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_cleanup_transition_report =
    "computed DynamicArray for cleanup transition function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 owner items acquire-from items acquire-to items.loop.entry "
    "acquire-operation items.computed_for.cleanup.acquire resume-from items.loop.entry "
    "resume-to items resume-operation items.computed_for.cleanup.resume (metadata only)";

inline constexpr std::string_view computed_dynamic_array_inserted_cleanup_transition_report =
    "computed DynamicArray for inserted cleanup transition acquire-from items acquire-to items.loop.entry "
    "acquire-operation items.computed_for.0.cleanup.acquire resume-from items.loop.entry "
    "resume-to items resume-operation items.computed_for.0.cleanup.resume (inserted IR)";

inline constexpr std::string_view computed_dynamic_array_inserted_cleanup_state_verification_report =
    "computed DynamicArray for inserted cleanup state verification "
    "acquire-operation items.computed_for.0.cleanup.acquire "
    "resume-operation items.computed_for.0.cleanup.resume acquire-from items acquire-to items.loop.entry "
    "resume-from items.loop.entry resume-to items [handoff paired] [cleanup calls disabled] (inserted IR)";

inline constexpr std::string_view computed_dynamic_array_cleanup_call_emission_gate_report =
    "computed DynamicArray for cleanup call emission gate blocked "
    "acquire-operation items.computed_for.0.cleanup.acquire "
    "resume-operation items.computed_for.0.cleanup.resume [inserted state verified] "
    "[cleanup calls disabled] [cleanup call emission blocked] (inserted IR)";

inline constexpr std::string_view computed_dynamic_array_cleanup_call_plan_report =
    "computed DynamicArray for cleanup call plan planned "
    "cleanup-operation items.computed_for.0.cleanup.resume.call "
    "after-resume-operation items.computed_for.0.cleanup.resume owner items data %items.computed_for.0.data "
    "element-size 4 [inserted state verified] [cleanup calls disabled] [data operand proven] "
    "[element-size operand proven] [capacity operand pending] [cleanup call disabled] snippets 1 (inserted IR)";

inline constexpr std::string_view computed_dynamic_array_production_emission_gate_report =
    "computed DynamicArray for production emission gate function sum_words line 6 "
    "source DynamicArray<UInt32> element UInt32 owner items [ownership ready] [loop render ready] "
    "[loop cleanup ownership ready] [function cleanup resumption ready] [exit cleanup ready] "
    "[production sequence planned] [production emission disabled] "
    "snippets 16 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_production_sequence_report =
    "computed DynamicArray for production sequence function sum_words line 6 source DynamicArray<UInt32> "
    "element UInt32 owner items snippets 16 (metadata only)";

inline constexpr std::string_view computed_dynamic_array_descriptor_render_plan =
    "computed DynamicArray descriptor render plan descriptor load projection planned source "
    "DynamicArray<UInt32> element UInt32 owner items descriptor %items.addr "
    "value %items.computed_for.descriptor data %items.computed_for.data "
    "length %items.computed_for.length [descriptor load planned] [data projection planned] "
    "[length projection planned] [render disabled] (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_control_render_plan =
    "computed DynamicArray loop control render plan loop control render planned source "
    "DynamicArray<UInt32> element UInt32 owner items condition items.computed_for.condition "
    "body items.computed_for.body continue items.computed_for.continue exit items.computed_for.exit "
    "index %items.computed_for.index bounds %items.computed_for.more [entry branch planned] "
    "[index phi planned] [bounds check planned] [conditional branch planned] "
    "[render disabled] (metadata only)";

inline constexpr std::string_view computed_dynamic_array_element_address_render_plan =
    "computed DynamicArray element address render plan element address render planned source "
    "DynamicArray<UInt32> element UInt32 lowers-to i32 owner items data %items.computed_for.data "
    "index %items.computed_for.index address %items.computed_for.element.addr "
    "[data pointer available] [index available] [element address planned] "
    "[render disabled] (metadata only)";

inline constexpr std::string_view computed_dynamic_array_element_load_render_plan =
    "computed DynamicArray element load render plan element load render planned source "
    "DynamicArray<UInt32> element UInt32 lowers-to i32 owner items "
    "address %items.computed_for.element.addr item %items.computed_for.item "
    "[element address available] [item value planned] [render disabled] (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_continue_render_plan =
    "computed DynamicArray loop continue render plan loop continue render planned source "
    "DynamicArray<UInt32> element UInt32 owner items continue items.computed_for.continue "
    "condition items.computed_for.condition index %items.computed_for.index "
    "next-index %items.computed_for.next.index [continue block planned] "
    "[next index planned] [backedge branch planned] [render disabled] (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_render_sequence_plan =
    "computed DynamicArray loop render sequence plan loop render sequence planned source "
    "DynamicArray<UInt32> element UInt32 owner items body items.computed_for.body "
    "[descriptor render planned] [loop control planned] [body block planned] "
    "[element address planned] [element load planned] [loop continue planned] "
    "[render disabled] (metadata only)";

inline constexpr std::string_view computed_dynamic_array_loop_exit_cleanup_plan =
    "computed DynamicArray loop exit cleanup plan loop exit cleanup planned source "
    "DynamicArray<UInt32> element UInt32 owner items exit items.computed_for.exit "
    "from items.loop.entry to items operation items.computed_for.cleanup.resume "
    "[exit block planned] [cleanup resumes] [cleanup sequence disabled] "
    "[render disabled] (metadata only)";

inline constexpr std::string_view computed_dynamic_array_production_emission_gate_plan =
    "computed DynamicArray production emission gate plan production emission gate planned source "
    "DynamicArray<UInt32> element UInt32 owner items [ownership ready] [loop render ready] "
    "[loop cleanup ownership ready] [function cleanup resumption ready] [exit cleanup ready] "
    "[production sequence planned] [production emission disabled] (metadata only)";

inline constexpr std::array<std::string_view, 10> computed_dynamic_array_local_same_owner_audit_reports {
    computed_dynamic_array_descriptor_render_report,
    computed_dynamic_array_loop_control_render_report,
    computed_dynamic_array_element_address_render_report,
    computed_dynamic_array_element_load_render_report,
    computed_dynamic_array_loop_continue_render_report,
    computed_dynamic_array_loop_render_sequence_report,
    computed_dynamic_array_loop_exit_cleanup_report,
    computed_dynamic_array_cleanup_transition_report,
    computed_dynamic_array_production_emission_gate_report,
    computed_dynamic_array_production_sequence_report,
};

}  // namespace orison::tests::smoke
