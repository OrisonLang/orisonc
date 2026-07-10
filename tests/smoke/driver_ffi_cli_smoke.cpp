#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

auto read_command_output_with_exit_code(std::string const& command, int expected_exit_code) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == expected_exit_code);
    return output;
}

void assert_successful_command(std::string const& command) {
    auto status = std::system(command.c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_ffi_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto examples = std::filesystem::path(ORISON_SOURCE_DIR) / "examples";

    auto printf_demo_path = examples / "tour_09_ffi_printf.or";
    auto printf_output = read_command_output_with_exit_code(
        executable.string() + " run " + printf_demo_path.string(),
        25
    );
    assert(printf_output == "Hello world from Orison!\n");

    auto fixed_ffi_path = examples / "ffi_fixed_parameters.or";
    assert_successful_command(executable.string() + " run " + fixed_ffi_path.string());

    auto aggregate_scalar_ffi_path = examples / "ffi_aggregate_scalar_parameters.or";
    assert_successful_command(executable.string() + " run " + aggregate_scalar_ffi_path.string() + " >/dev/null");

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
