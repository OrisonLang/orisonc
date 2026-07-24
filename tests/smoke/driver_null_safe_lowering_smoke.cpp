#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

void write_fixture(
    std::filesystem::path const& path,
    std::string_view package_name,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package " << package_name << "\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

auto run_emit_llvm(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "--emit-llvm",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

auto run_program(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "run",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_success_with_stdout_contains(
    orison::driver::CompileResult const& result,
    std::initializer_list<std::string_view> expected_fragments
) {
    if (result.exit_code != 0 || !result.stderr_text.empty()) {
        std::cerr << result.stderr_text << result.stdout_text;
    }
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
    for (auto expected_fragment : expected_fragments) {
        if (result.stdout_text.find(expected_fragment) == std::string::npos) {
            std::cerr << "missing expected fragment: " << expected_fragment << "\n";
            std::cerr << result.stdout_text;
        }
        assert(result.stdout_text.find(expected_fragment) != std::string::npos);
    }
}

void assert_run_success(orison::driver::CompileResult const& result) {
    if (result.exit_code != 0 || !result.stderr_text.empty()) {
        std::cerr << result.stderr_text << result.stdout_text;
    }
    assert(result.exit_code == 0);
    assert(result.stderr_text.empty());
}

void assert_failure_with_stderr_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_fragment
) {
    if (result.exit_code == 0 || result.stderr_text.find(expected_fragment) == std::string::npos ||
        !result.stdout_text.empty()) {
        std::cerr << result.stderr_text << result.stdout_text;
    }
    assert(result.exit_code != 0);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_fragment) != std::string::npos);
}

void assert_negative_uint32_cast_failure(orison::driver::CompileResult const& result) {
    assert_failure_with_stderr_contains(result, "unsupported cast: negative value to UInt32");
}

void assert_negative_int32_null_safe_member_call_emits(orison::driver::CompileResult const& result) {
    assert_success_with_stdout_contains(
        result,
        {
            "%record.SignedProfile = type { i32 }",
            "define i32 @method.SignedProfile.shifted(%record.SignedProfile %this, i32 %delta)",
            "sub i32 0, 27",
            "call i32 @method.SignedProfile.shifted(%record.SignedProfile",
            "i32 %tmp",
            "insertvalue { i1, i32 } undef, i1 true, 0",
            "phi { i1, i32 }",
            "ret i32 0",
        }
    );
}

void assert_negative_int32_ternary_null_safe_member_call_emits(orison::driver::CompileResult const& result) {
    assert_success_with_stdout_contains(
        result,
        {
            "%record.SignedProfile = type { i32 }",
            "define i32 @method.SignedProfile.shifted(%record.SignedProfile %this, i32 %delta)",
            "ternary.then.",
            "sub i32 0, 27",
            "ternary.else.",
            "ternary.merge.",
            "phi i32",
            "call i32 @method.SignedProfile.shifted(%record.SignedProfile",
            "i32 %tmp",
            "insertvalue { i1, i32 } undef, i1 true, 0",
            "phi { i1, i32 }",
            "ret i32 0",
        }
    );
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_null_safe_lowering_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto emit_null_safe_field_path = std::filesystem::temp_directory_path() / "emit_null_safe_field.or";
    write_fixture(
        emit_null_safe_field_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "record User",
            "    profile: Maybe<Profile>",
            "function main() -> UInt32",
            "    let user: Maybe<User> = Empty",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    return 0 as UInt32",
        }
    );
    auto emit_null_safe_field = run_emit_llvm(app, emit_null_safe_field_path);
    assert_success_with_stdout_contains(
        emit_null_safe_field,
        {
            "%record.Profile = type { i32 }",
            "%record.User = type { { i1, %record.Profile } }",
            "define i32 @main()",
            "nullsafe.empty.",
            "nullsafe.some.",
            "nullsafe.merge.",
            "phi { i1, i32 }",
            "ret i32 0",
        }
    );

    auto reject_null_safe_unknown_field_path =
        std::filesystem::temp_directory_path() / "reject_null_safe_unknown_field.or";
    write_fixture(
        reject_null_safe_unknown_field_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "record User",
            "    profile: Maybe<Profile>",
            "function demo() -> Maybe<UInt32>",
            "    let user: Maybe<User> = Empty",
            "    return user?.profile?.missing",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_unknown_field_path),
        "type 'Profile' has no member 'missing'"
    );

    auto reject_null_safe_scalar_continuation_path =
        std::filesystem::temp_directory_path() / "reject_null_safe_scalar_continuation.or";
    write_fixture(
        reject_null_safe_scalar_continuation_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "record User",
            "    profile: Maybe<Profile>",
            "function demo() -> Maybe<UInt32>",
            "    let user: Maybe<User> = Empty",
            "    return user?.profile?.rating?.missing",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_scalar_continuation_path),
        "type 'UInt32' has no member 'missing'"
    );

    auto reject_null_safe_concrete_generic_scalar_continuation_path =
        std::filesystem::temp_directory_path() /
        "reject_null_safe_concrete_generic_scalar_continuation.or";
    write_fixture(
        reject_null_safe_concrete_generic_scalar_continuation_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "function demo() -> Maybe<UInt32>",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.value?.missing",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_scalar_continuation_path),
        "type 'UInt32' has no member 'missing'"
    );

    auto reject_null_safe_concrete_generic_unknown_method_path =
        std::filesystem::temp_directory_path() / "reject_null_safe_concrete_generic_unknown_method.or";
    write_fixture(
        reject_null_safe_concrete_generic_unknown_method_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "function demo() -> Maybe<UInt32>",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.missing()",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_unknown_method_path),
        "type 'Box<UInt32>' has no method 'missing'"
    );

    auto reject_null_safe_concrete_generic_scalar_unknown_method_path =
        std::filesystem::temp_directory_path() / "reject_null_safe_concrete_generic_scalar_unknown_method.or";
    write_fixture(
        reject_null_safe_concrete_generic_scalar_unknown_method_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "function demo() -> Maybe<UInt32>",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.value?.missing()",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_scalar_unknown_method_path),
        "type 'UInt32' has no method 'missing'"
    );

    auto reject_null_safe_concrete_generic_method_argument_path =
        std::filesystem::temp_directory_path() / "reject_null_safe_concrete_generic_method_argument.or";
    write_fixture(
        reject_null_safe_concrete_generic_method_argument_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "extend Box<UInt32>",
            "    function scale(this: shared This, delta: UInt32) -> UInt32",
            "        return this.value + delta",
            "function demo() -> Maybe<UInt32>",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.scale(true)",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_method_argument_path),
        "method argument 'delta' type 'Bool' does not match declared type 'UInt32'"
    );

    auto reject_null_safe_concrete_generic_method_missing_argument_path =
        std::filesystem::temp_directory_path() /
        "reject_null_safe_concrete_generic_method_missing_argument.or";
    write_fixture(
        reject_null_safe_concrete_generic_method_missing_argument_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "extend Box<UInt32>",
            "    function scale(this: shared This, delta: UInt32) -> UInt32",
            "        return this.value + delta",
            "function demo() -> Maybe<UInt32>",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.scale()",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_method_missing_argument_path),
        "method 'scale' expects 1 argument but received 0"
    );

    auto reject_null_safe_concrete_generic_method_extra_argument_path =
        std::filesystem::temp_directory_path() / "reject_null_safe_concrete_generic_method_extra_argument.or";
    write_fixture(
        reject_null_safe_concrete_generic_method_extra_argument_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "extend Box<UInt32>",
            "    function scale(this: shared This, delta: UInt32) -> UInt32",
            "        return this.value + delta",
            "function demo() -> Maybe<UInt32>",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.scale(1 as UInt32, 2 as UInt32)",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_method_extra_argument_path),
        "method 'scale' expects 1 argument but received 2"
    );

    auto emit_null_safe_concrete_generic_member_call_path =
        std::filesystem::temp_directory_path() / "emit_null_safe_concrete_generic_member_call.or";
    write_fixture(
        emit_null_safe_concrete_generic_member_call_path,
        "demo.generic_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "extend Box<UInt32>",
            "    function scale(this: shared This, delta: UInt32) -> UInt32",
            "        return this.value + delta",
            "function main() -> UInt32",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    let scaled: Maybe<UInt32> = box?.scale(5 as UInt32)",
            "    box?.scale(6 as UInt32)",
            "    return 0 as UInt32",
        }
    );
    auto emit_null_safe_concrete_generic_member_call =
        run_emit_llvm(app, emit_null_safe_concrete_generic_member_call_path);
    assert_success_with_stdout_contains(
        emit_null_safe_concrete_generic_member_call,
        {
            "%record.Box_UInt32_ = type { i32 }",
            "define i32 @method.Box_UInt32_.scale(%record.Box_UInt32_ %this, i32 %delta)",
            "define i32 @main()",
            "insertvalue { i1, %record.Box_UInt32_ } undef, i1 false, 0",
            "nullsafe.call.empty.",
            "nullsafe.call.some.",
            "nullsafe.call.merge.",
            "call i32 @method.Box_UInt32_.scale(%record.Box_UInt32_",
            "call i32 @method.Box_UInt32_.scale(%record.Box_UInt32_ %tmp",
            "i32 6)",
            "insertvalue { i1, i32 } undef, i1 true, 0",
            "phi { i1, i32 }",
            "ret i32 0",
        }
    );

    auto emit_null_safe_concrete_generic_aggregate_member_call_path =
        std::filesystem::temp_directory_path() / "emit_null_safe_concrete_generic_aggregate_member_call.or";
    write_fixture(
        emit_null_safe_concrete_generic_aggregate_member_call_path,
        "demo.generic_aggregate_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "extend Box<UInt32>",
            "    function bump(this: shared This, delta: UInt32) -> Box<UInt32>",
            "        return Box(this.value + delta)",
            "    function pair(this: shared This, delta: UInt32) -> Array<Box<UInt32>, 2>",
            "        return [Box(this.value), Box(this.value + delta)]",
            "function main() -> UInt32",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    let bumped: Maybe<Box<UInt32>> = box?.bump(5 as UInt32)",
            "    let bumped_value: Maybe<UInt32> = box?.bump(5 as UInt32)?.value",
            "    let pair: Maybe<Array<Box<UInt32>, 2>> = box?.pair(7 as UInt32)",
            "    box?.bump(6 as UInt32)",
            "    box?.pair(8 as UInt32)",
            "    return 0 as UInt32",
        }
    );
    auto emit_null_safe_concrete_generic_aggregate_member_call =
        run_emit_llvm(app, emit_null_safe_concrete_generic_aggregate_member_call_path);
    assert_success_with_stdout_contains(
        emit_null_safe_concrete_generic_aggregate_member_call,
        {
            "%record.Box_UInt32_ = type { i32 }",
            "define %record.Box_UInt32_ @method.Box_UInt32_.bump(%record.Box_UInt32_ %this, i32 %delta)",
            "define [2 x %record.Box_UInt32_] @method.Box_UInt32_.pair(%record.Box_UInt32_ %this, i32 %delta)",
            "define i32 @main()",
            "nullsafe.call.empty.",
            "nullsafe.call.some.",
            "nullsafe.call.merge.",
            "call %record.Box_UInt32_ @method.Box_UInt32_.bump(%record.Box_UInt32_",
            "call %record.Box_UInt32_ @method.Box_UInt32_.bump(%record.Box_UInt32_ %tmp",
            "i32 6)",
            "call [2 x %record.Box_UInt32_] @method.Box_UInt32_.pair(%record.Box_UInt32_",
            "i32 8)",
            "insertvalue { i1, %record.Box_UInt32_ } undef, i1 true, 0",
            "insertvalue { i1, [2 x %record.Box_UInt32_] } undef, i1 true, 0",
            "insertvalue { i1, i32 } undef, i1 true, 0",
            "phi { i1, %record.Box_UInt32_ }",
            "phi { i1, [2 x %record.Box_UInt32_] }",
            "phi { i1, i32 }",
            "ret i32 0",
        }
    );

    auto reject_null_safe_concrete_generic_array_method_direct_index_path =
        std::filesystem::temp_directory_path() /
        "reject_null_safe_concrete_generic_array_method_direct_index.or";
    write_fixture(
        reject_null_safe_concrete_generic_array_method_direct_index_path,
        "demo.generic_array_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "extend Box<UInt32>",
            "    function pair(this: shared This, delta: UInt32) -> Array<Box<UInt32>, 2>",
            "        return [Box(this.value), Box(this.value + delta)]",
            "function demo() -> Box<UInt32>",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.pair(7 as UInt32)[0 as UInt64]",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_array_method_direct_index_path),
        "index access requires Array, View, DynamicArray, or Pointer base: Maybe<Array<Box<UInt32>, 2>>"
    );

    auto reject_null_safe_concrete_generic_aggregate_method_ordinary_field_path =
        std::filesystem::temp_directory_path() /
        "reject_null_safe_concrete_generic_aggregate_method_ordinary_field.or";
    write_fixture(
        reject_null_safe_concrete_generic_aggregate_method_ordinary_field_path,
        "demo.generic_aggregate_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: T",
            "extend Box<UInt32>",
            "    function bump(this: shared This, delta: UInt32) -> Box<UInt32>",
            "        return Box(this.value + delta)",
            "function demo() -> UInt32",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    return box?.bump(5 as UInt32).value",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_concrete_generic_aggregate_method_ordinary_field_path),
        "type 'Maybe<Box<UInt32>>' has no member 'value'"
    );

    auto reject_null_safe_non_maybe_receiver_path =
        std::filesystem::temp_directory_path() / "reject_null_safe_non_maybe_receiver.or";
    write_fixture(
        reject_null_safe_non_maybe_receiver_path,
        "demo.emit",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "function demo() -> Maybe<UInt32>",
            "    let profile: Profile = Profile(1 as UInt32)",
            "    return profile?.rating",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_null_safe_non_maybe_receiver_path),
        "null-safe access requires Maybe base: Profile"
    );

    auto emit_null_safe_present_path = std::filesystem::temp_directory_path() /
        "emit_null_safe_present_path.or";
    write_fixture(
        emit_null_safe_present_path,
        "demo.present",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "record User",
            "    profile: Maybe<Profile>",
            "function main() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    return 0 as UInt32",
        }
    );
    auto emit_present_path = run_emit_llvm(app, emit_null_safe_present_path);
    assert_success_with_stdout_contains(
        emit_present_path,
        {
            "%record.Profile = type { i32 }",
            "%record.User = type { { i1, %record.Profile } }",
            "define i32 @main()",
            "insertvalue %record.Profile undef, i32 7, 0",
            "insertvalue { i1, %record.Profile } undef, i1 true, 0",
            "insertvalue %record.User undef, { i1, %record.Profile }",
            "insertvalue { i1, %record.User } undef, i1 true, 0",
            "nullsafe.merge.",
            "phi { i1, i32 }",
            "ret i32 0",
        }
    );

    auto emit_null_safe_nested_absent_path = std::filesystem::temp_directory_path() /
        "emit_null_safe_nested_absent_path.or";
    write_fixture(
        emit_null_safe_nested_absent_path,
        "demo.nested_absent",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "record User",
            "    profile: Maybe<Profile>",
            "function main() -> UInt32",
            "    let profile: Maybe<Profile> = Empty",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    return 0 as UInt32",
        }
    );
    auto emit_nested_absent_path = run_emit_llvm(app, emit_null_safe_nested_absent_path);
    assert_success_with_stdout_contains(
        emit_nested_absent_path,
        {
            "%record.Profile = type { i32 }",
            "%record.User = type { { i1, %record.Profile } }",
            "define i32 @main()",
            "insertvalue { i1, %record.Profile } undef, i1 false, 0",
            "insertvalue %record.User undef, { i1, %record.Profile }",
            "insertvalue { i1, %record.User } undef, i1 true, 0",
            "nullsafe.empty.2:",
            "nullsafe.some.2:",
            "phi { i1, i32 }",
            "ret i32 0",
        }
    );

    auto emit_maybe_record_switch_payload_path = std::filesystem::temp_directory_path() /
        "emit_maybe_record_switch_payload.or";
    write_fixture(
        emit_maybe_record_switch_payload_path,
        "demo.record_payload",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "function main() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(9 as UInt32))",
            "    switch profile",
            "        Some(value) => value.rating",
            "        Empty => 0 as UInt32",
        }
    );
    auto emit_maybe_record_switch_payload = run_emit_llvm(app, emit_maybe_record_switch_payload_path);
    assert_success_with_stdout_contains(
        emit_maybe_record_switch_payload,
        {
            "%record.Profile = type { i32 }",
            "extractvalue { i1, %record.Profile }",
            "%value.addr = alloca %record.Profile",
            "store %record.Profile",
            "getelementptr %record.Profile, ptr %value.addr, i32 0, i32 0",
            "load i32",
            "ret i32",
        }
    );

    auto emit_null_safe_member_call_path = std::filesystem::temp_directory_path() /
        "emit_null_safe_member_call.or";
    write_fixture(
        emit_null_safe_member_call_path,
        "demo.member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "extend Profile",
            "    function scaled(this: shared This, delta: UInt32) -> UInt32",
            "        return this.rating + delta",
            "    function adjusted(this: shared This, delta: UInt32) -> Profile",
            "        return Profile(this.rating + delta)",
            "    function pair(this: shared This, delta: UInt32) -> Array<UInt32, 2>",
            "        return [this.rating, this.rating + delta]",
            "record Profile",
            "    rating: UInt32",
            "record User",
            "    profile: Maybe<Profile>",
            "function main() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let scaled: Maybe<UInt32> = user?.profile?.scaled(5 as UInt32)",
            "    let adjusted: Maybe<Profile> = user?.profile?.adjusted(3 as UInt32)",
            "    let adjusted_rating: Maybe<UInt32> = user?.profile?.adjusted(3 as UInt32)?.rating",
            "    let pair: Maybe<Array<UInt32, 2>> = user?.profile?.pair(4 as UInt32)",
            "    user?.profile?.scaled(6 as UInt32)",
            "    user?.profile?.pair(9 as UInt32)",
            "    return 0 as UInt32",
        }
    );
    auto emit_null_safe_member_call = run_emit_llvm(app, emit_null_safe_member_call_path);
    assert_success_with_stdout_contains(
        emit_null_safe_member_call,
        {
            "%record.Profile = type { i32 }",
            "%record.User = type { { i1, %record.Profile } }",
            "define i32 @method.Profile.scaled(%record.Profile %this, i32 %delta)",
            "define %record.Profile @method.Profile.adjusted(%record.Profile %this, i32 %delta)",
            "define [2 x i32] @method.Profile.pair(%record.Profile %this, i32 %delta)",
            "define i32 @main()",
            "nullsafe.empty.",
            "nullsafe.some.",
            "nullsafe.merge.",
            "nullsafe.call.empty.",
            "nullsafe.call.some.",
            "nullsafe.call.merge.",
            "call i32 @method.Profile.scaled(%record.Profile",
            "call i32 @method.Profile.scaled(%record.Profile %tmp",
            "i32 6)",
            "call %record.Profile @method.Profile.adjusted(%record.Profile",
            "call [2 x i32] @method.Profile.pair(%record.Profile",
            "i32 9)",
            "insertvalue { i1, %record.Profile } undef, i1 true, 0",
            "insertvalue { i1, [2 x i32] } undef, i1 true, 0",
            "insertvalue { i1, i32 } undef, i1 true, 0",
            "phi { i1, i32 }",
            "phi { i1, %record.Profile }",
            "phi { i1, [2 x i32] }",
            "ret i32 0",
        }
    );

    auto emit_negative_null_safe_member_call_path = std::filesystem::temp_directory_path() /
        "emit_negative_null_safe_member_call.or";
    write_fixture(
        emit_negative_null_safe_member_call_path,
        "demo.negative_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "extend SignedProfile",
            "    function shifted(this: shared This, delta: Int32) -> Int32",
            "        return this.rating + delta",
            "record SignedProfile",
            "    rating: Int32",
            "function main() -> UInt32",
            "    let profile: Maybe<SignedProfile> = Some(SignedProfile(7 as Int32))",
            "    let shifted: Maybe<Int32> = profile?.shifted(-27 as Int32)",
            "    return 0 as UInt32",
        }
    );
    auto emit_negative_null_safe_member_call =
        run_emit_llvm(app, emit_negative_null_safe_member_call_path);
    assert_negative_int32_null_safe_member_call_emits(emit_negative_null_safe_member_call);

    auto emit_negative_ternary_null_safe_member_call_path = std::filesystem::temp_directory_path() /
        "emit_negative_ternary_null_safe_member_call.or";
    write_fixture(
        emit_negative_ternary_null_safe_member_call_path,
        "demo.negative_ternary_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "extend SignedProfile",
            "    function shifted(this: shared This, delta: Int32) -> Int32",
            "        return this.rating + delta",
            "record SignedProfile",
            "    rating: Int32",
            "function main(flag: Bool) -> UInt32",
            "    let profile: Maybe<SignedProfile> = Some(SignedProfile(7 as Int32))",
            "    let shifted: Maybe<Int32> = profile?.shifted(flag ? -27 as Int32 : 4 as Int32)",
            "    return 0 as UInt32",
        }
    );
    auto emit_negative_ternary_null_safe_member_call =
        run_emit_llvm(app, emit_negative_ternary_null_safe_member_call_path);
    assert_negative_int32_ternary_null_safe_member_call_emits(emit_negative_ternary_null_safe_member_call);

    auto reject_negative_uint32_null_safe_member_call_path = std::filesystem::temp_directory_path() /
        "reject_negative_uint32_null_safe_member_call.or";
    write_fixture(
        reject_negative_uint32_null_safe_member_call_path,
        "demo.reject_negative_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "extend Profile",
            "    function scaled(this: shared This, delta: UInt32) -> UInt32",
            "        return this.rating + delta",
            "record Profile",
            "    rating: UInt32",
            "function main() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let scaled: Maybe<UInt32> = profile?.scaled(-1 as UInt32)",
            "    return 0 as UInt32",
        }
    );
    assert_negative_uint32_cast_failure(run_emit_llvm(app, reject_negative_uint32_null_safe_member_call_path));

    auto reject_negative_uint32_ternary_null_safe_member_call_path = std::filesystem::temp_directory_path() /
        "reject_negative_uint32_ternary_null_safe_member_call.or";
    write_fixture(
        reject_negative_uint32_ternary_null_safe_member_call_path,
        "demo.reject_negative_ternary_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "extend Profile",
            "    function scaled(this: shared This, delta: UInt32) -> UInt32",
            "        return this.rating + delta",
            "record Profile",
            "    rating: UInt32",
            "function main(flag: Bool) -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let scaled: Maybe<UInt32> = profile?.scaled(flag ? -1 as UInt32 : 4 as UInt32)",
            "    return 0 as UInt32",
        }
    );
    assert_negative_uint32_cast_failure(
        run_emit_llvm(app, reject_negative_uint32_ternary_null_safe_member_call_path)
    );

    auto reject_void_null_safe_member_call_path = std::filesystem::temp_directory_path() /
        "reject_void_null_safe_member_call.or";
    write_fixture(
        reject_void_null_safe_member_call_path,
        "demo.reject_void_member_call",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "extend Profile",
            "    function reset(this: shared This, value: UInt32) -> Unit",
            "        return",
            "record Profile",
            "    rating: UInt32",
            "function main() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    profile?.reset(9 as UInt32)",
            "    return 0 as UInt32",
        }
    );
    assert_failure_with_stderr_contains(
        run_emit_llvm(app, reject_void_null_safe_member_call_path),
        "lowering void null-safe member call statements requires an accepted Maybe<Unit> ABI: Profile.reset"
    );

    auto run_null_safe_paths = std::filesystem::temp_directory_path() /
        "run_null_safe_paths.or";
    write_fixture(
        run_null_safe_paths,
        "demo.run",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Profile",
            "    rating: UInt32",
            "record User",
            "    profile: Maybe<Profile>",
            "record Box<T>",
            "    value: T",
            "function final_present_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    switch rating",
            "        Some(value) => value",
            "        Empty => 0 as UInt32",
            "function final_empty_score() -> UInt32",
            "    let user: Maybe<User> = Empty",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    switch rating",
            "        Some(value) => 1 as UInt32",
            "        Empty => 0 as UInt32",
            "function return_present_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    switch rating",
            "        Some(value) => return value",
            "        Empty => return 0 as UInt32",
            "function return_nested_absent_score() -> UInt32",
            "    let profile: Maybe<Profile> = Empty",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    switch rating",
            "        Some(value) => return 1 as UInt32",
            "        Empty => return 0 as UInt32",
            "function final_record_payload_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(9 as UInt32))",
            "    switch profile",
            "        Some(value) => value.rating",
            "        Empty => 0 as UInt32",
            "function return_record_payload_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(11 as UInt32))",
            "    switch profile",
            "        Some(value) => return value.rating",
            "        Empty => return 0 as UInt32",
            "extend Profile",
            "    function scaled(this: shared This, delta: UInt32) -> UInt32",
            "        return this.rating + delta",
            "    function adjusted(this: shared This, delta: UInt32) -> Profile",
            "        return Profile(this.rating + delta)",
            "    function pair(this: shared This, delta: UInt32) -> Array<UInt32, 2>",
            "        return [this.rating, this.rating + delta]",
            "extend Box<UInt32>",
            "    function bump(this: shared This, delta: UInt32) -> Box<UInt32>",
            "        return Box(this.value + delta)",
            "function final_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let scaled: Maybe<UInt32> = profile?.scaled(5 as UInt32)",
            "    switch scaled",
            "        Some(value) => value",
            "        Empty => 0 as UInt32",
            "function final_nested_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let scaled: Maybe<UInt32> = user?.profile?.scaled(5 as UInt32)",
            "    switch scaled",
            "        Some(value) => value",
            "        Empty => 0 as UInt32",
            "function final_nested_absent_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Empty",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let scaled: Maybe<UInt32> = user?.profile?.scaled(5 as UInt32)",
            "    switch scaled",
            "        Some(value) => 1 as UInt32",
            "        Empty => 0 as UInt32",
            "function final_record_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let adjusted: Maybe<Profile> = profile?.adjusted(3 as UInt32)",
            "    switch adjusted",
            "        Some(value) => value.rating",
            "        Empty => 0 as UInt32",
            "function final_record_member_call_field_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let rating: Maybe<UInt32> = profile?.adjusted(3 as UInt32)?.rating",
            "    switch rating",
            "        Some(value) => value",
            "        Empty => 0 as UInt32",
            "function final_empty_record_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Empty",
            "    let adjusted: Maybe<Profile> = profile?.adjusted(3 as UInt32)",
            "    switch adjusted",
            "        Some(value) => 1 as UInt32",
            "        Empty => 0 as UInt32",
            "function final_array_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let pair: Maybe<Array<UInt32, 2>> = profile?.pair(4 as UInt32)",
            "    switch pair",
            "        Some(value) => value[0 as UInt64] + value[1 as UInt64]",
            "        Empty => 0 as UInt32",
            "function final_empty_array_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Empty",
            "    let pair: Maybe<Array<UInt32, 2>> = profile?.pair(4 as UInt32)",
            "    switch pair",
            "        Some(value) => 1 as UInt32",
            "        Empty => 0 as UInt32",
            "function final_generic_record_member_call_field_score() -> UInt32",
            "    let box: Maybe<Box<UInt32>> = Some(Box(7 as UInt32))",
            "    let bumped: Maybe<UInt32> = box?.bump(5 as UInt32)?.value",
            "    switch bumped",
            "        Some(value) => value",
            "        Empty => 0 as UInt32",
            "function final_empty_generic_record_member_call_field_score() -> UInt32",
            "    let box: Maybe<Box<UInt32>> = Empty",
            "    let bumped: Maybe<UInt32> = box?.bump(5 as UInt32)?.value",
            "    switch bumped",
            "        Some(value) => 1 as UInt32",
            "        Empty => 0 as UInt32",
            "function if_member_call_score(flag: Bool) -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    if flag",
            "        let scaled: Maybe<UInt32> = profile?.scaled(5 as UInt32)",
            "        return 12 as UInt32",
            "    return 0 as UInt32",
            "function guard_member_call_score(flag: Bool) -> UInt32",
            "    guard flag else",
            "        return 0 as UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let scaled: Maybe<UInt32> = profile?.scaled(5 as UInt32)",
            "    return 12 as UInt32",
            "function while_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    var index = 0 as UInt32",
            "    while index < 1 as UInt32",
            "        let scaled: Maybe<UInt32> = profile?.scaled(5 as UInt32)",
            "        index = index + 1 as UInt32",
            "    return 0 as UInt32",
            "function return_empty_member_call_score() -> UInt32",
            "    let profile: Maybe<Profile> = Empty",
            "    let scaled: Maybe<UInt32> = profile?.scaled(5 as UInt32)",
            "    switch scaled",
            "        Some(value) => return 1 as UInt32",
            "        Empty => return 0 as UInt32",
            "function main() -> UInt32",
            "    let empty_user: Maybe<User> = Empty",
            "    let empty_rating: Maybe<UInt32> = empty_user?.profile?.rating",
            "    let present_profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let present_user: Maybe<User> = Some(User(present_profile))",
            "    let present_rating: Maybe<UInt32> = present_user?.profile?.rating",
            "    let absent_profile: Maybe<Profile> = Empty",
            "    let nested_absent_user: Maybe<User> = Some(User(absent_profile))",
            "    let nested_absent_rating: Maybe<UInt32> = nested_absent_user?.profile?.rating",
            "    return final_present_score() + return_present_score() + final_empty_score() + return_nested_absent_score() + final_record_payload_score() + return_record_payload_score() + final_member_call_score() + final_nested_member_call_score() + final_nested_absent_member_call_score() + final_record_member_call_score() + final_record_member_call_field_score() + final_empty_record_member_call_score() + final_array_member_call_score() + final_empty_array_member_call_score() + final_generic_record_member_call_field_score() + final_empty_generic_record_member_call_field_score() + if_member_call_score(true) + guard_member_call_score(true) + while_member_call_score() + return_empty_member_call_score() - 132 as UInt32",
        }
    );
    assert_run_success(run_program(app, run_null_safe_paths));

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
