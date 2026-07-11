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
            "function final_present_score() -> UInt32",
            "    let profile: Maybe<Profile> = Some(Profile(7 as UInt32))",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    switch rating",
            "        Some(value) => 0 as UInt32",
            "        Empty => 1 as UInt32",
            "function final_empty_score() -> UInt32",
            "    let user: Maybe<User> = Empty",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    switch rating",
            "        Some(value) => 1 as UInt32",
            "        Empty => 0 as UInt32",
            "function return_nested_absent_score() -> UInt32",
            "    let profile: Maybe<Profile> = Empty",
            "    let user: Maybe<User> = Some(User(profile))",
            "    let rating: Maybe<UInt32> = user?.profile?.rating",
            "    switch rating",
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
            "    return final_present_score() + final_empty_score() + return_nested_absent_score()",
        }
    );
    assert_run_success(run_program(app, run_null_safe_paths));

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
