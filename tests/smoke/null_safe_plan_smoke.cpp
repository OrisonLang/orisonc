#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/null_safe_plan.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_null_safe_plan_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto path = std::filesystem::temp_directory_path() / "orison_null_safe_plan_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.null_safe_plan\n"
                  "\n"
                  "choice Maybe<T>\n"
                  "    Some(value: T)\n"
                  "    Empty\n"
                  "\n"
                  "record AddressInfo\n"
                  "    city: Text\n"
                  "\n"
                  "record Profile\n"
                  "    address: Maybe<AddressInfo>\n"
                  "\n"
                  "record User\n"
                  "    profile: Maybe<Profile>\n"
                  "\n"
                  "record Box<T>\n"
                  "    value: T\n"
                  "\n"
                  "extend Box<UInt32>\n"
                  "    function bump(this: shared This, delta: UInt32) -> Box<UInt32>\n"
                  "        return Box(this.value + delta)\n"
                  "\n"
                  "function city_name(user: Maybe<User>) -> Maybe<Text>\n"
                  "    user?.profile?.address?.city\n"
                  "\n"
                  "function missing_zipcode(user: Maybe<User>) -> Maybe<Text>\n"
                  "    user?.profile?.address?.zipcode\n"
                  "\n"
                  "function bumped_value(box: Maybe<Box<UInt32>>) -> Maybe<UInt32>\n"
                  "    box?.bump(5 as UInt32)?.value\n";
    }

    auto source = orison::source::SourceFile::read(path);
    assert(source.has_value());
    auto parser = orison::syntax::ModuleParser {};
    auto parse_result = parser.parse(*source);
    assert(!parse_result.diagnostics.has_errors());

    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    auto context = orison::lowering::build_lowering_context(parse_result.module, diagnostics);
    assert(!diagnostics.has_errors());

    auto state = orison::lowering::FunctionLoweringState {};
    state.source_type_names["user"] = "Maybe<User>";

    auto const& expression = parse_result.module.functions.front().body_statements.front().expression;
    auto result = orison::lowering::plan_null_safe_member_access(expression, context, state);
    assert(result.plan.has_value());
    assert(result.plan->base_expression != nullptr);
    assert(result.plan->base_maybe_type_name == "Maybe<User>");
    assert(result.plan->segments.size() == 3);
    assert(result.plan->segments[0].receiver_type_name == "User");
    assert(result.plan->segments[0].field_name == "profile");
    assert(result.plan->segments[0].field_type_name == "Maybe<Profile>");
    assert(result.plan->segments[1].receiver_type_name == "Profile");
    assert(result.plan->segments[1].field_name == "address");
    assert(result.plan->segments[1].field_type_name == "Maybe<AddressInfo>");
    assert(result.plan->segments[2].receiver_type_name == "AddressInfo");
    assert(result.plan->segments[2].field_name == "city");
    assert(result.plan->segments[2].field_type_name == "Text");
    assert(result.plan->result_payload_type_name == "Text");
    assert(result.plan->result_maybe_type_name == "Maybe<Text>");
    assert(orison::lowering::render_null_safe_plan_failure(result.failure).empty());

    auto missing_base_state = orison::lowering::FunctionLoweringState {};
    auto missing_base = orison::lowering::plan_null_safe_member_access(expression, context, missing_base_state);
    assert(!missing_base.plan.has_value());
    assert(
        orison::lowering::render_null_safe_plan_failure(missing_base.failure) ==
        "unknown null-safe base type: base expression"
    );

    auto non_maybe_state = orison::lowering::FunctionLoweringState {};
    non_maybe_state.source_type_names["user"] = "User";
    auto non_maybe = orison::lowering::plan_null_safe_member_access(expression, context, non_maybe_state);
    assert(!non_maybe.plan.has_value());
    assert(
        orison::lowering::render_null_safe_plan_failure(non_maybe.failure) ==
        "null-safe access requires Maybe base: User"
    );

    auto const& missing_field_expression = parse_result.module.functions[1].body_statements.front().expression;
    auto missing_field_result =
        orison::lowering::plan_null_safe_member_access(missing_field_expression, context, state);
    assert(!missing_field_result.plan.has_value());
    assert(
        orison::lowering::render_null_safe_plan_failure(missing_field_result.failure) ==
        "unknown null-safe field: AddressInfo.zipcode"
    );

    auto generic_state = orison::lowering::FunctionLoweringState {};
    generic_state.source_type_names["box"] = "Maybe<Box<UInt32>>";
    auto const& generic_expression = parse_result.module.functions[2].body_statements.front().expression;
    auto generic_result = orison::lowering::plan_null_safe_member_access(generic_expression, context, generic_state);
    assert(generic_result.plan.has_value());
    assert(generic_result.plan->base_expression != nullptr);
    assert(generic_result.plan->base_maybe_type_name == "Maybe<Box<UInt32>>");
    assert(generic_result.plan->segments.size() == 1);
    assert(generic_result.plan->segments[0].receiver_type_name == "Box<UInt32>");
    assert(generic_result.plan->segments[0].field_name == "value");
    assert(generic_result.plan->segments[0].field_type_name == "UInt32");
    assert(generic_result.plan->result_payload_type_name == "UInt32");
    assert(generic_result.plan->result_maybe_type_name == "Maybe<UInt32>");
    assert(orison::lowering::render_null_safe_plan_failure(generic_result.failure).empty());

    assert(orison::lowering::maybe_payload_source_type_name("Maybe<Array<UInt32, 3>>") == "Array<UInt32, 3>");
    assert(!orison::lowering::maybe_payload_source_type_name("Array<UInt32, 3>").has_value());

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
