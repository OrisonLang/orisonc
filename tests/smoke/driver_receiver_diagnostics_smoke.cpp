#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

void write_receiver_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package demo.receiver\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

auto run_parse(orison::driver::CompilerApp const& app, std::filesystem::path const& path)
    -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        "--parse",
        path_text.c_str()
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

void assert_parse_failure_contains(
    orison::driver::CompileResult const& result,
    std::string_view expected_message
) {
    assert(result.exit_code == 1);
    assert(result.stdout_text.empty());
    assert(result.stderr_text.find(expected_message) != std::string_view::npos);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_receiver_diagnostics_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto app = orison::driver::CompilerApp {};

    auto this_outside_method_failure_path = std::filesystem::temp_directory_path() / "this_outside_method_failure.or";
    write_receiver_fixture(this_outside_method_failure_path, {"function current() -> Int64", "    return this"});
    assert_parse_failure_contains(
        run_parse(app, this_outside_method_failure_path),
        "receiver 'this' is only valid inside implements or extend methods"
    );

    auto this_type_signature_failure_path = std::filesystem::temp_directory_path() / "this_type_signature_failure.or";
    write_receiver_fixture(
        this_type_signature_failure_path,
        {"function project(value: shared This) -> shared This", "    return value"}
    );
    assert_parse_failure_contains(
        run_parse(app, this_type_signature_failure_path),
        "This type is only valid inside interface, implements, or extend methods"
    );

    auto receiver_parameter_nonself_type_failure_path =
        std::filesystem::temp_directory_path() / "receiver_parameter_nonself_type_failure.or";
    write_receiver_fixture(
        receiver_parameter_nonself_type_failure_path,
        {"extend Buffer", "    function reset(this: Int64) -> Unit", "        return"}
    );
    assert_parse_failure_contains(
        run_parse(app, receiver_parameter_nonself_type_failure_path),
        "receiver parameter 'this' must use This, shared This, or exclusive This"
    );

    return 0;
}
