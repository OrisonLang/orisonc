#include "orison/link/host_runner.hpp"

#include "orison/link/host_linker.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <spawn.h>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

extern char** environ;

namespace orison::link {
namespace {

auto temporary_executable_path() -> std::filesystem::path {
    static auto next_id = std::atomic<std::uint64_t> {0};
    return std::filesystem::temp_directory_path() /
           ("orison-run-" + std::to_string(getpid()) + "-" + std::to_string(next_id++));
}

class TemporaryExecutable {
public:
    explicit TemporaryExecutable(std::filesystem::path path) : path_(std::move(path)) {}

    ~TemporaryExecutable() {
        auto error = std::error_code {};
        std::filesystem::remove(path_, error);
    }

    auto path() const -> std::filesystem::path const& {
        return path_;
    }

private:
    std::filesystem::path path_;
};

}  // namespace

auto HostRunResult::has_errors() const -> bool {
    return diagnostics.has_errors();
}

auto HostRunner::run(std::string_view object_bytes) const -> HostRunResult {
    auto result = HostRunResult {};
    auto executable = TemporaryExecutable(temporary_executable_path());

    HostLinker linker;
    auto link_result = linker.link(object_bytes, executable.path());
    if (link_result.has_errors()) {
        result.diagnostics = std::move(link_result.diagnostics);
        return result;
    }

    auto executable_path = executable.path().string();
    char* arguments[] {
        executable_path.data(),
        nullptr,
    };

    auto process_id = pid_t {};
    auto spawn_error = posix_spawn(&process_id, executable_path.c_str(), nullptr, nullptr, arguments, environ);
    if (spawn_error != 0) {
        result.diagnostics.error(
            1,
            "unable to start compiled program: " +
                std::error_code(spawn_error, std::generic_category()).message()
        );
        return result;
    }

    auto status = int {};
    while (waitpid(process_id, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        result.diagnostics.error(
            1,
            "unable to wait for compiled program: " +
                std::error_code(errno, std::generic_category()).message()
        );
        return result;
    }

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        return result;
    }
    if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
        return result;
    }

    result.diagnostics.error(1, "compiled program ended with an unsupported process status");
    return result;
}

}  // namespace orison::link
