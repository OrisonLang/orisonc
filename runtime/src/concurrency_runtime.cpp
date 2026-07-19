#include <pthread.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

using OrisonConcurrencyEntry = void (*)(void* environment, void* result_storage);
using OrisonConcurrencyCleanup = void (*)(void* environment);
using OrisonThreadEntry = void* (*)(void* start);

struct OrisonConcurrencyHandle {
    pthread_t thread;
    bool running;
};

struct OrisonConcurrencyStart {
    OrisonConcurrencyEntry entry;
    OrisonConcurrencyCleanup cleanup;
    void* environment;
    void* result_storage;
};

struct OrisonDynamicArrayDescriptor {
    void* data;
    std::uint64_t length;
    std::uint64_t capacity;
};

auto orison_dynamic_array_byte_count(
    std::uint64_t element_size,
    std::uint64_t capacity
) -> std::size_t
{
    if (capacity == 0) {
        return 0;
    }
    if (element_size == 0) {
        std::abort();
    }
    auto const max_size = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
    if (element_size > max_size / capacity) {
        std::abort();
    }
    return static_cast<std::size_t>(element_size * capacity);
}

#ifdef ORISON_RUNTIME_TESTING
bool force_thread_create_failure = false;

extern "C" auto __orison_runtime_test_force_thread_create_failure(bool force) -> void
{
    force_thread_create_failure = force;
}
#endif

auto orison_pthread_create(
    pthread_t* thread,
    pthread_attr_t const* attributes,
    OrisonThreadEntry entry,
    void* start
) -> int
{
#ifdef ORISON_RUNTIME_TESTING
    if (force_thread_create_failure) {
        return 1;
    }
#endif
    return pthread_create(thread, attributes, entry, start);
}

auto orison_concurrency_trampoline(void* raw_start) -> void*
{
    auto* start = static_cast<OrisonConcurrencyStart*>(raw_start);
    auto entry = start->entry;
    auto cleanup = start->cleanup;
    auto* environment = start->environment;
    auto* result_storage = start->result_storage;
    std::free(start);

    entry(environment, result_storage);
    if (cleanup != nullptr) {
        cleanup(environment);
    }

    return nullptr;
}

auto orison_concurrency_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    std::uint64_t result_size,
    void* cleanup
) -> void*
{
    static_cast<void>(result_size);

    auto* handle = static_cast<OrisonConcurrencyHandle*>(std::calloc(1, sizeof(OrisonConcurrencyHandle)));
    auto* start = static_cast<OrisonConcurrencyStart*>(std::calloc(1, sizeof(OrisonConcurrencyStart)));
    if (handle == nullptr || start == nullptr) {
        std::free(handle);
        std::free(start);
        if (cleanup != nullptr) {
            reinterpret_cast<OrisonConcurrencyCleanup>(cleanup)(environment);
        }
        return nullptr;
    }

    start->entry = reinterpret_cast<OrisonConcurrencyEntry>(entry);
    start->cleanup = reinterpret_cast<OrisonConcurrencyCleanup>(cleanup);
    start->environment = environment;
    start->result_storage = result_storage;
    if (orison_pthread_create(&handle->thread, nullptr, orison_concurrency_trampoline, start) != 0) {
        std::free(handle);
        std::free(start);
        if (cleanup != nullptr) {
            reinterpret_cast<OrisonConcurrencyCleanup>(cleanup)(environment);
        }
        return nullptr;
    }

    handle->running = true;
    return handle;
}

extern "C" auto __orison_thread_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    std::uint64_t result_size,
    void* cleanup
) -> void*
{
    return orison_concurrency_spawn(entry, environment, result_storage, result_size, cleanup);
}

extern "C" auto __orison_thread_join(void* raw_handle) -> void
{
    auto* handle = static_cast<OrisonConcurrencyHandle*>(raw_handle);
    if (handle == nullptr || !handle->running) {
        return;
    }
    pthread_join(handle->thread, nullptr);
    handle->running = false;
}

extern "C" auto __orison_task_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    std::uint64_t result_size,
    void* cleanup
) -> void*
{
    return orison_concurrency_spawn(entry, environment, result_storage, result_size, cleanup);
}

extern "C" auto __orison_task_await(void* raw_handle) -> void
{
    __orison_thread_join(raw_handle);
}

extern "C" auto __orison_concurrency_handle_destroy(void* raw_handle) -> void
{
    auto* handle = static_cast<OrisonConcurrencyHandle*>(raw_handle);
    if (handle == nullptr) {
        return;
    }
    if (handle->running) {
        pthread_join(handle->thread, nullptr);
    }
    std::free(handle);
}

extern "C" auto __orison_concurrency_spawn_failed() -> void
{
    std::abort();
}

extern "C" auto __orison_dynamic_array_allocate(
    std::uint64_t element_size,
    std::uint64_t initial_capacity
) -> OrisonDynamicArrayDescriptor
{
    auto byte_count = orison_dynamic_array_byte_count(element_size, initial_capacity);
    if (byte_count == 0) {
        return OrisonDynamicArrayDescriptor {
            .data = nullptr,
            .length = 0,
            .capacity = 0,
        };
    }

    auto* data = std::calloc(1, byte_count);
    if (data == nullptr) {
        std::abort();
    }
    return OrisonDynamicArrayDescriptor {
        .data = data,
        .length = 0,
        .capacity = initial_capacity,
    };
}

extern "C" auto __orison_dynamic_array_grow(
    OrisonDynamicArrayDescriptor descriptor,
    std::uint64_t element_size,
    std::uint64_t next_capacity
) -> OrisonDynamicArrayDescriptor
{
    if (descriptor.length > descriptor.capacity || next_capacity < descriptor.length) {
        std::abort();
    }

    auto next_byte_count = orison_dynamic_array_byte_count(element_size, next_capacity);
    if (next_byte_count == 0) {
        std::free(descriptor.data);
        return OrisonDynamicArrayDescriptor {
            .data = nullptr,
            .length = 0,
            .capacity = 0,
        };
    }

    auto* next_data = std::calloc(1, next_byte_count);
    if (next_data == nullptr) {
        std::abort();
    }

    auto initialized_byte_count = orison_dynamic_array_byte_count(element_size, descriptor.length);
    if (initialized_byte_count != 0) {
        std::memcpy(next_data, descriptor.data, initialized_byte_count);
    }
    std::free(descriptor.data);

    return OrisonDynamicArrayDescriptor {
        .data = next_data,
        .length = descriptor.length,
        .capacity = next_capacity,
    };
}

extern "C" auto __orison_dynamic_array_deallocate(
    void* data,
    std::uint64_t element_size,
    std::uint64_t capacity
) -> void
{
    static_cast<void>(element_size);
    static_cast<void>(capacity);
    std::free(data);
}

extern "C" auto __orison_dynamic_array_bounds_failed() -> void
{
    std::abort();
}

extern "C" auto __orison_dynamic_array_capacity_failed() -> void
{
    std::abort();
}
