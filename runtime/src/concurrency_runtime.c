#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef void (*orison_concurrency_entry)(void* environment, void* result_storage);
typedef void (*orison_concurrency_cleanup)(void* environment);

typedef struct orison_concurrency_handle {
    pthread_t thread;
    int running;
} orison_concurrency_handle;

typedef struct orison_concurrency_start {
    orison_concurrency_entry entry;
    void* environment;
    void* result_storage;
} orison_concurrency_start;

static void* orison_concurrency_trampoline(void* raw_start) {
    orison_concurrency_start* start = (orison_concurrency_start*)raw_start;
    orison_concurrency_entry entry = start->entry;
    void* environment = start->environment;
    void* result_storage = start->result_storage;
    free(start);
    entry(environment, result_storage);
    return NULL;
}

static void* orison_concurrency_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    uint64_t result_size,
    void* cleanup
) {
    (void)result_size;
    (void)cleanup;

    orison_concurrency_handle* handle =
        (orison_concurrency_handle*)calloc(1, sizeof(orison_concurrency_handle));
    orison_concurrency_start* start =
        (orison_concurrency_start*)calloc(1, sizeof(orison_concurrency_start));
    if (handle == NULL || start == NULL) {
        free(handle);
        free(start);
        return NULL;
    }

    start->entry = (orison_concurrency_entry)entry;
    start->environment = environment;
    start->result_storage = result_storage;
    if (pthread_create(&handle->thread, NULL, orison_concurrency_trampoline, start) != 0) {
        free(handle);
        free(start);
        return NULL;
    }

    handle->running = 1;
    return handle;
}

void* __orison_thread_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    uint64_t result_size,
    void* cleanup
) {
    return orison_concurrency_spawn(entry, environment, result_storage, result_size, cleanup);
}

void __orison_thread_join(void* raw_handle) {
    orison_concurrency_handle* handle = (orison_concurrency_handle*)raw_handle;
    if (handle == NULL || !handle->running) {
        return;
    }
    pthread_join(handle->thread, NULL);
    handle->running = 0;
}

void* __orison_task_spawn(
    void* entry,
    void* environment,
    void* result_storage,
    uint64_t result_size,
    void* cleanup
) {
    return orison_concurrency_spawn(entry, environment, result_storage, result_size, cleanup);
}

void __orison_task_await(void* raw_handle) {
    __orison_thread_join(raw_handle);
}

void __orison_concurrency_handle_destroy(void* raw_handle) {
    orison_concurrency_handle* handle = (orison_concurrency_handle*)raw_handle;
    if (handle == NULL) {
        return;
    }
    if (handle->running) {
        pthread_join(handle->thread, NULL);
    }
    free(handle);
}

void __orison_concurrency_spawn_failed(void) {
    abort();
}
