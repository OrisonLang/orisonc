#include <pthread.h>

#include <cstdint>
#include <cstdlib>

using OrisonConcurrencyEntry = void (*)(void* environment, void* result_storage);
using OrisonConcurrencyCleanup = void (*)(void* environment);

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
    if (pthread_create(&handle->thread, nullptr, orison_concurrency_trampoline, start) != 0) {
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
