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

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
