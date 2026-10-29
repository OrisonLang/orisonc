#include "orison/link/host_linker.hpp"

#include <atomic>
#include <array>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <spawn.h>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

extern char** environ;

namespace orison::link {
namespace {

auto temporary_object_path() -> std::filesystem::path {
    static auto next_id = std::atomic<std::uint64_t> {0};
    return std::filesystem::temp_directory_path() /
           ("orison-link-" + std::to_string(getpid()) + "-" + std::to_string(next_id++) + ".o");
}

class TemporaryObjects {
public:
    ~TemporaryObjects() {
        for (auto const& path : paths_) {
            auto error = std::error_code {};
            std::filesystem::remove(path, error);
        }
    }

    void add(std::filesystem::path path) {
        paths_.push_back(std::move(path));
    }

    auto paths() const -> std::vector<std::filesystem::path> const& {
        return paths_;
    }

private:
    std::vector<std::filesystem::path> paths_;
};

}  // namespace

auto HostLinkResult::has_errors() const -> bool {
    return diagnostics.has_errors();
}

auto HostLinker::link(
    std::string_view object_bytes,
    std::filesystem::path const& output_path,
    std::vector<std::string> const& libraries
) const -> HostLinkResult {
    auto object_images = std::array<std::string_view, 1> {object_bytes};
    return link(object_images, output_path, libraries);
}

auto HostLinker::link(
    std::span<std::string_view const> object_images,
    std::filesystem::path const& output_path,
    std::vector<std::string> const& libraries
) const -> HostLinkResult {
    auto result = HostLinkResult {};
    if (object_images.empty()) {
        result.diagnostics.error(1, "host linker requires at least one object image");
        return result;
    }
    auto output_parent = output_path.parent_path();
    if (!output_parent.empty()) {
        auto status_error = std::error_code {};
        auto parent_exists = std::filesystem::exists(output_parent, status_error);
        if (status_error) {
            result.diagnostics.error(
                1,
                "unable to inspect output directory '" + output_parent.string() + "': " + status_error.message()
            );
            return result;
        }
        if (!parent_exists) {
            result.diagnostics.error(1, "output directory does not exist: " + output_parent.string());
            return result;
        }
        auto directory_error = std::error_code {};
        if (!std::filesystem::is_directory(output_parent, directory_error)) {
            if (directory_error) {
                result.diagnostics.error(
                    1,
                    "unable to inspect output directory '" + output_parent.string() + "': " + directory_error.message()
                );
            } else {
                result.diagnostics.error(1, "output path parent is not a directory: " + output_parent.string());
            }
            return result;
        }
    }
    auto output_status_error = std::error_code {};
    auto output_exists = std::filesystem::exists(output_path, output_status_error);
    if (output_status_error) {
        result.diagnostics.error(
            1,
            "unable to inspect output path '" + output_path.string() + "': " + output_status_error.message()
        );
        return result;
    }
    if (output_exists) {
        auto output_directory_error = std::error_code {};
        if (std::filesystem::is_directory(output_path, output_directory_error)) {
            result.diagnostics.error(1, "output path is a directory: " + output_path.string());
            return result;
        }
        if (output_directory_error) {
            result.diagnostics.error(
                1,
                "unable to inspect output path '" + output_path.string() + "': " + output_directory_error.message()
            );
            return result;
        }
    }

    auto temporary_objects = TemporaryObjects {};
    for (auto object_bytes : object_images) {
        auto object_path = temporary_object_path();
        temporary_objects.add(object_path);
        auto object_output = std::ofstream(
            object_path,
            std::ios::out | std::ios::binary | std::ios::trunc
        );
        if (!object_output) {
            result.diagnostics.error(1, "unable to create temporary object file for linking");
            return result;
        }
        object_output.write(object_bytes.data(), static_cast<std::streamsize>(object_bytes.size()));
        object_output.close();
        if (!object_output) {
            result.diagnostics.error(1, "unable to write temporary object file for linking");
            return result;
        }
    }

    auto driver = std::string(ORISON_HOST_LINK_DRIVER);
    auto output_path_text = output_path.string();
    if (output_exists) {
        auto remove_error = std::error_code {};
        std::filesystem::remove(output_path, remove_error);
        if (remove_error) {
            result.diagnostics.error(
                1,
                "unable to replace output path '" + output_path.string() + "': " + remove_error.message()
            );
            return result;
        }
    }
    auto argument_storage = std::vector<std::string> {};
    argument_storage.reserve(5 + temporary_objects.paths().size() + libraries.size());
    argument_storage.push_back(driver);
    for (auto const& object_path : temporary_objects.paths()) {
        argument_storage.push_back(object_path.string());
    }
#ifdef ORISON_CONCURRENCY_RUNTIME_ARCHIVE
    argument_storage.push_back(ORISON_CONCURRENCY_RUNTIME_ARCHIVE);
    argument_storage.push_back("-pthread");
#endif
    for (auto const& library : libraries) {
        argument_storage.push_back("-l" + library);
    }
    argument_storage.push_back("-o");
    argument_storage.push_back(output_path_text);

    auto arguments = std::vector<char*> {};
    arguments.reserve(argument_storage.size() + 1);
    for (auto& argument : argument_storage) {
        arguments.push_back(argument.data());
    }
    arguments.push_back(nullptr);

    auto process_id = pid_t {};
    auto spawn_error = posix_spawn(&process_id, driver.c_str(), nullptr, nullptr, arguments.data(), environ);
    if (spawn_error != 0) {
        result.diagnostics.error(
            1,
            "unable to start host linker '" + driver + "': " + std::error_code(spawn_error, std::generic_category()).message()
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
            "unable to wait for host linker: " + std::error_code(errno, std::generic_category()).message()
        );
        return result;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        auto error = std::error_code {};
        std::filesystem::remove(output_path, error);
        result.diagnostics.error(1, "host linker failed to produce an executable");
    }
    return result;
}

}  // namespace orison::link
