#include "orison/link/host_linker.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

struct OrisonDynamicArrayDescriptor {
    void* data;
    std::uint64_t length;
    std::uint64_t capacity;
};

extern "C" auto __orison_dynamic_array_allocate(
    std::uint64_t element_size,
    std::uint64_t initial_capacity
) -> OrisonDynamicArrayDescriptor;
extern "C" auto __orison_dynamic_array_grow(
    OrisonDynamicArrayDescriptor descriptor,
    std::uint64_t element_size,
    std::uint64_t next_capacity
) -> OrisonDynamicArrayDescriptor;
extern "C" auto __orison_dynamic_array_deallocate(
    void* data,
    std::uint64_t element_size,
    std::uint64_t capacity
) -> void;

auto test_zero_capacity_allocation() -> void
{
    auto descriptor = __orison_dynamic_array_allocate(sizeof(std::uint32_t), 0);
    assert(descriptor.data == nullptr);
    assert(descriptor.length == 0);
    assert(descriptor.capacity == 0);
    __orison_dynamic_array_deallocate(descriptor.data, sizeof(std::uint32_t), descriptor.capacity);
}

auto test_allocation_and_deallocation() -> void
{
    auto descriptor = __orison_dynamic_array_allocate(sizeof(std::uint32_t), 4);
    assert(descriptor.data != nullptr);
    assert(descriptor.length == 0);
    assert(descriptor.capacity == 4);

    auto* data = static_cast<std::uint32_t*>(descriptor.data);
    data[0] = 7;
    data[1] = 11;

    __orison_dynamic_array_deallocate(descriptor.data, sizeof(std::uint32_t), descriptor.capacity);
}

auto test_grow_preserves_initialized_elements() -> void
{
    auto descriptor = __orison_dynamic_array_allocate(sizeof(std::uint32_t), 2);
    auto* data = static_cast<std::uint32_t*>(descriptor.data);
    data[0] = 7;
    data[1] = 11;
    descriptor.length = 2;

    auto grown = __orison_dynamic_array_grow(descriptor, sizeof(std::uint32_t), 4);
    assert(grown.data != nullptr);
    assert(grown.length == 2);
    assert(grown.capacity == 4);

    auto* grown_data = static_cast<std::uint32_t*>(grown.data);
    assert(grown_data[0] == 7);
    assert(grown_data[1] == 11);
    grown_data[2] = 13;

    __orison_dynamic_array_deallocate(grown.data, sizeof(std::uint32_t), grown.capacity);
}

auto test_llvm_calls_link_and_run_against_runtime() -> void
{
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_dynamic_array_runtime_abi_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::lowering::LlvmObjectEmitter object_emitter;
    auto object = object_emitter.emit(
        "declare void @__orison_dynamic_array_allocate(ptr sret({ ptr, i64, i64 }), i64, i64)\n"
        "declare void @__orison_dynamic_array_grow(ptr sret({ ptr, i64, i64 }), "
        "ptr byval({ ptr, i64, i64 }), i64, i64)\n"
        "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %descriptor.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate(ptr sret({ ptr, i64, i64 }) %descriptor.addr, i64 4, i64 2)\n"
        "  %descriptor = load { ptr, i64, i64 }, ptr %descriptor.addr\n"
        "  %data = extractvalue { ptr, i64, i64 } %descriptor, 0\n"
        "  %element.addr = getelementptr i32, ptr %data, i64 0\n"
        "  store i32 7, ptr %element.addr\n"
        "  %initialized = insertvalue { ptr, i64, i64 } %descriptor, i64 1, 1\n"
        "  %grow.input = alloca { ptr, i64, i64 }\n"
        "  store { ptr, i64, i64 } %initialized, ptr %grow.input\n"
        "  %grown.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_grow(ptr sret({ ptr, i64, i64 }) %grown.addr, "
        "ptr byval({ ptr, i64, i64 }) %grow.input, i64 4, i64 4)\n"
        "  %grown = load { ptr, i64, i64 }, ptr %grown.addr\n"
        "  %grown.data = extractvalue { ptr, i64, i64 } %grown, 0\n"
        "  %grown.capacity = extractvalue { ptr, i64, i64 } %grown, 2\n"
        "  %grown.element.addr = getelementptr i32, ptr %grown.data, i64 0\n"
        "  %value = load i32, ptr %grown.element.addr\n"
        "  call void @__orison_dynamic_array_deallocate(ptr %grown.data, i64 4, i64 %grown.capacity)\n"
        "  ret i32 %value\n"
        "}\n"
    );
    assert(!object.has_errors());

    auto executable = std::filesystem::temp_directory_path() / "orison_dynamic_array_runtime_abi";
    orison::link::HostLinker linker;
    auto link = linker.link(object.object_bytes, executable, std::vector<std::string> {});
    assert(!link.has_errors());

    auto status = std::system(executable.string().c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 7);

    std::filesystem::remove(executable);
    std::filesystem::remove_all(smoke_temp_root);
}

auto main() -> int
{
    test_zero_capacity_allocation();
    test_allocation_and_deallocation();
    test_grow_preserves_initialized_elements();
    test_llvm_calls_link_and_run_against_runtime();
    return 0;
}
