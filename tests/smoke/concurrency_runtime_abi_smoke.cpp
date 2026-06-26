#include <cassert>
#include <cstdint>

extern "C" auto __orison_thread_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    std::uint64_t result_size,
    void* cleanup
) -> void*;
extern "C" auto __orison_thread_join(void* handle) -> void;
extern "C" auto __orison_task_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    std::uint64_t result_size,
    void* cleanup
) -> void*;
extern "C" auto __orison_task_await(void* handle) -> void;
extern "C" auto __orison_concurrency_handle_destroy(void* handle) -> void;
extern "C" auto __orison_runtime_test_force_thread_create_failure(bool force) -> void;

auto add_one_entry(void* environment, void* result_storage) -> void
{
    auto* input = static_cast<std::int64_t*>(environment);
    auto* output = static_cast<std::int64_t*>(result_storage);
    *output = *input + 1;
}

auto write_flag_entry(void* environment, void* result_storage) -> void
{
    static_cast<void>(result_storage);
    auto* flag = static_cast<int*>(environment);
    *flag = 1;
}

struct CleanupEnvironment {
    int entry_ran;
    int cleanup_ran;
};

auto cleanup_entry(void* environment, void* result_storage) -> void
{
    static_cast<void>(result_storage);
    auto* state = static_cast<CleanupEnvironment*>(environment);
    state->entry_ran = 1;
}

auto cleanup_callback(void* environment) -> void
{
    auto* state = static_cast<CleanupEnvironment*>(environment);
    assert(state->entry_ran == 1);
    state->cleanup_ran = 1;
}

auto cleanup_after_spawn_failure(void* environment) -> void
{
    auto* state = static_cast<CleanupEnvironment*>(environment);
    assert(state->entry_ran == 0);
    state->cleanup_ran = 1;
}

template <typename Function>
auto abi_pointer(Function function) -> void*
{
    return reinterpret_cast<void*>(function);
}

auto test_thread_join_result_and_destroy() -> void
{
    std::int64_t environment = 41;
    std::int64_t result = 0;
    void* handle = __orison_thread_spawn(abi_pointer(add_one_entry), &environment, &result, sizeof(result), nullptr);
    assert(handle != nullptr);

    __orison_thread_join(handle);
    assert(result == 42);
    __orison_concurrency_handle_destroy(handle);
}

auto test_task_await_result_and_destroy() -> void
{
    std::int64_t environment = 6;
    std::int64_t result = 0;
    void* handle = __orison_task_spawn(abi_pointer(add_one_entry), &environment, &result, sizeof(result), nullptr);
    assert(handle != nullptr);

    __orison_task_await(handle);
    assert(result == 7);
    __orison_concurrency_handle_destroy(handle);
}

auto test_abandoned_handle_destroy_waits() -> void
{
    int flag = 0;
    void* handle = __orison_thread_spawn(abi_pointer(write_flag_entry), &flag, nullptr, 0, nullptr);
    assert(handle != nullptr);

    __orison_concurrency_handle_destroy(handle);
    assert(flag == 1);
}

auto test_cleanup_runs_after_thread_entry() -> void
{
    CleanupEnvironment environment = {0, 0};
    void* handle = __orison_thread_spawn(
        abi_pointer(cleanup_entry),
        &environment,
        nullptr,
        0,
        abi_pointer(cleanup_callback)
    );
    assert(handle != nullptr);

    __orison_thread_join(handle);
    assert(environment.entry_ran == 1);
    assert(environment.cleanup_ran == 1);
    __orison_concurrency_handle_destroy(handle);
}

auto test_cleanup_runs_before_abandoned_destroy_returns() -> void
{
    CleanupEnvironment environment = {0, 0};
    void* handle =
        __orison_task_spawn(abi_pointer(cleanup_entry), &environment, nullptr, 0, abi_pointer(cleanup_callback));
    assert(handle != nullptr);

    __orison_concurrency_handle_destroy(handle);
    assert(environment.entry_ran == 1);
    assert(environment.cleanup_ran == 1);
}

auto test_cleanup_runs_on_spawn_setup_failure() -> void
{
    CleanupEnvironment environment = {0, 0};
    __orison_runtime_test_force_thread_create_failure(true);
    void* handle = __orison_thread_spawn(
        abi_pointer(cleanup_entry),
        &environment,
        nullptr,
        0,
        abi_pointer(cleanup_after_spawn_failure)
    );
    __orison_runtime_test_force_thread_create_failure(false);

    assert(handle == nullptr);
    assert(environment.entry_ran == 0);
    assert(environment.cleanup_ran == 1);
}

auto main() -> int
{
    test_thread_join_result_and_destroy();
    test_task_await_result_and_destroy();
    test_abandoned_handle_destroy_waits();
    test_cleanup_runs_after_thread_entry();
    test_cleanup_runs_before_abandoned_destroy_returns();
    test_cleanup_runs_on_spawn_setup_failure();
    return 0;
}
