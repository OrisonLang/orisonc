#include "orison/link/host_linker.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include <cassert>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_host_linker_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::lowering::LlvmObjectEmitter object_emitter;
    auto object = object_emitter.emit(
        "declare double @cos(double)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = call double @cos(double 0.000000e+00)\n"
        "  %result = fptosi double %value to i32\n"
        "  %status = sub i32 %result, 1\n"
        "  ret i32 %status\n"
        "}\n"
    );
    assert(!object.has_errors());

    auto executable = std::filesystem::temp_directory_path() / "orison_host_linker_libm";
    orison::link::HostLinker linker;
    auto link = linker.link(object.object_bytes, executable, std::vector<std::string> {"m"});
    assert(!link.has_errors());

    assert(std::system(executable.string().c_str()) == 0);

    auto producer_object = object_emitter.emit(
        "define i32 @increment(i32 %value) {\n"
        "entry:\n"
        "  %result = add i32 %value, 1\n"
        "  ret i32 %result\n"
        "}\n"
    );
    assert(!producer_object.has_errors());
    auto consumer_object = object_emitter.emit(
        "declare i32 @increment(i32)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = call i32 @increment(i32 40)\n"
        "  %status = sub i32 %value, 41\n"
        "  ret i32 %status\n"
        "}\n"
    );
    assert(!consumer_object.has_errors());
    auto object_images = std::array<std::string_view, 2> {
        producer_object.object_bytes,
        consumer_object.object_bytes,
    };
    auto multi_object_executable = std::filesystem::temp_directory_path() / "orison_host_linker_multi_object";
    auto multi_object_link = linker.link(object_images, multi_object_executable);
    assert(!multi_object_link.has_errors());
    assert(std::system(multi_object_executable.string().c_str()) == 0);

    std::filesystem::remove(executable);
    std::filesystem::remove(multi_object_executable);
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
