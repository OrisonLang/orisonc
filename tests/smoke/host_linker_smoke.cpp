#include "orison/link/host_linker.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <vector>

auto main() -> int {
    orison::lowering::LlvmObjectEmitter object_emitter;
    auto object = object_emitter.emit(
        "declare double @cos(double)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = call double @cos(double 0.000000e+00)\n"
        "  %result = fptosi double %value to i32\n"
        "  ret i32 %result\n"
        "}\n"
    );
    assert(!object.has_errors());

    auto executable = std::filesystem::temp_directory_path() / "orison_host_linker_libm";
    orison::link::HostLinker linker;
    auto link = linker.link(object.object_bytes, executable, std::vector<std::string> {"m"});
    assert(!link.has_errors());

    auto status = std::system(executable.string().c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 1);

    std::filesystem::remove(executable);
    return 0;
}
