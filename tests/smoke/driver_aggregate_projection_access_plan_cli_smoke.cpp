#include "orison/driver/compiler_app.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

void write_fixture(std::filesystem::path const& path) {
    auto output = std::ofstream(path);
    output <<
        "package smoke.aggregate_access_plan\n"
        "\n"
        "record Payload\n"
        "    public value: UInt32\n"
        "\n"
        "record Box\n"
        "    public payload: Payload\n"
        "    public count: UInt32\n"
        "\n"
        "function consume_payload(payload: Payload) -> UInt32\n"
        "    payload.value\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: Box = Box(Payload(13 as UInt32), 7 as UInt32)\n"
        "    let count: UInt32 = box.count\n"
        "    consume_payload(box.payload) + count\n";
}

auto run_single_file_command(
    orison::driver::CompilerApp const& app,
    std::string_view command,
    std::filesystem::path const& path
) -> orison::driver::CompileResult {
    auto path_text = path.string();
    std::array<char const*, 3> argv {
        "orisonc",
        command.data(),
        path_text.c_str(),
    };
    return app.run(std::span<char const* const>(argv.data(), argv.size()));
}

}  // namespace

auto main() -> int {
    auto smoke_temp_root = std::filesystem::current_path() /
        ("orison_driver_aggregate_projection_access_plan_" +
         std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto fixture_path = smoke_temp_root / "fixture.or";
    write_fixture(fixture_path);

    auto app = orison::driver::CompilerApp {};
    auto report = run_single_file_command(
        app,
        "--test-only-aggregate-projection-access-plans",
        fixture_path
    );

    assert(report.exit_code == 0);
    assert(report.stderr_text.empty());
    assert(
        report.stdout_text ==
        "function consume_payload aggregate projection access intent value_read status non_owned_projection "
        "binding payload.value source UInt32 receiver false\n"
        "function main aggregate projection access intent value_read status non_owned_projection binding box.count "
        "source UInt32 receiver false\n"
        "function main aggregate projection access intent explicit_transfer status allowed binding box.payload "
        "source Payload receiver false\n"
    );

    auto normal_emit = run_single_file_command(app, "--emit-llvm", fixture_path);
    assert(normal_emit.exit_code == 0);
    assert(normal_emit.stdout_text.find("aggregate projection access") == std::string::npos);

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
