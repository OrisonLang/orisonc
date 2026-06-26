#include <assert.h>
#include <stdint.h>

extern void* __orison_thread_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    uint64_t result_size,
    void* cleanup
);
extern void __orison_thread_join(void* handle);
extern void* __orison_task_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    uint64_t result_size,
    void* cleanup
);
extern void __orison_task_await(void* handle);
extern void __orison_concurrency_handle_destroy(void* handle);

static void add_one_entry(void* environment, void* result_storage) {
    int64_t* input = (int64_t*)environment;
    int64_t* output = (int64_t*)result_storage;
    *output = *input + 1;
}

static void write_flag_entry(void* environment, void* result_storage) {
    (void)result_storage;
    int* flag = (int*)environment;
    *flag = 1;
}

typedef struct cleanup_environment {
    int entry_ran;
    int cleanup_ran;
} cleanup_environment;

static void cleanup_entry(void* environment, void* result_storage) {
    (void)result_storage;
    cleanup_environment* state = (cleanup_environment*)environment;
    state->entry_ran = 1;
}

static void cleanup_callback(void* environment) {
    cleanup_environment* state = (cleanup_environment*)environment;
    assert(state->entry_ran == 1);
    state->cleanup_ran = 1;
}

static void test_thread_join_result_and_destroy(void) {
    int64_t environment = 41;
    int64_t result = 0;
    void* handle = __orison_thread_spawn(add_one_entry, &environment, &result, sizeof(result), 0);
    assert(handle != 0);

    __orison_thread_join(handle);
    assert(result == 42);
    __orison_concurrency_handle_destroy(handle);
}

static void test_task_await_result_and_destroy(void) {
    int64_t environment = 6;
    int64_t result = 0;
    void* handle = __orison_task_spawn(add_one_entry, &environment, &result, sizeof(result), 0);
    assert(handle != 0);

    __orison_task_await(handle);
    assert(result == 7);
    __orison_concurrency_handle_destroy(handle);
}

static void test_abandoned_handle_destroy_waits(void) {
    int flag = 0;
    void* handle = __orison_thread_spawn(write_flag_entry, &flag, 0, 0, 0);
    assert(handle != 0);

    __orison_concurrency_handle_destroy(handle);
    assert(flag == 1);
}

static void test_cleanup_runs_after_thread_entry(void) {
    cleanup_environment environment = {0, 0};
    void* handle = __orison_thread_spawn(cleanup_entry, &environment, 0, 0, cleanup_callback);
    assert(handle != 0);

    __orison_thread_join(handle);
    assert(environment.entry_ran == 1);
    assert(environment.cleanup_ran == 1);
    __orison_concurrency_handle_destroy(handle);
}

static void test_cleanup_runs_before_abandoned_destroy_returns(void) {
    cleanup_environment environment = {0, 0};
    void* handle = __orison_task_spawn(cleanup_entry, &environment, 0, 0, cleanup_callback);
    assert(handle != 0);

    __orison_concurrency_handle_destroy(handle);
    assert(environment.entry_ran == 1);
    assert(environment.cleanup_ran == 1);
}

int main(void) {
    test_thread_join_result_and_destroy();
    test_task_await_result_and_destroy();
    test_abandoned_handle_destroy_waits();
    test_cleanup_runs_after_thread_entry();
    test_cleanup_runs_before_abandoned_destroy_returns();
    return 0;
}
