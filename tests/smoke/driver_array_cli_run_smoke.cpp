#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void assert_run_success(std::filesystem::path const& executable, std::filesystem::path const& source_path) {
    auto status = std::system((executable.string() + " run " + source_path.string()).c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_array_cli_run_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto examples = std::filesystem::path(ORISON_SOURCE_DIR) / "examples";
    constexpr auto run_examples = std::array<std::string_view, 11> {
        "local_array_for.or",
        "local_ternary_array_for.or",
        "local_ternary_array_literal_for.or",
        "local_ternary_record_array_literal_for.or",
        "local_record_array_for.or",
        "local_record_index_for.or",
        "local_record_index_field_for.or",
        "local_helper_array_for.or",
        "local_method_array_for.or",
        "local_member_receiver_method_array_for.or",
        "local_record_method_array_for.or",
    };

    for (auto name : run_examples) {
        assert_run_success(executable, examples / name);
    }

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
