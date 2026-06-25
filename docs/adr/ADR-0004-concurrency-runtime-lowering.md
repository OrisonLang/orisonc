# ADR-0004: Concurrency Runtime Lowering Contract

## Status
Accepted

## Context

`task`, `thread`, `await`, and `.join()` are parsed and semantically checked, but the backend does not yet have a
runtime representation for concurrent execution. Lowering these constructs directly to ad hoc LLVM would hide
ownership, result storage, and cleanup costs, which conflicts with Orison's explicit systems model.

## Decision

Concurrency lowering will target a small runtime ABI whose handles are opaque LLVM `ptr` values. Generated code remains
responsible for typed result storage, environment layout, entry thunks, and cleanup policy; the runtime only owns
scheduling/execution handles.

The initial runtime ABI model is:

- `__orison_thread_spawn(ptr entry, ptr environment, ptr result_storage, i64 result_size, ptr cleanup) -> ptr`
- `__orison_thread_join(ptr handle) -> void`
- `__orison_task_spawn(ptr entry, ptr environment, ptr result_storage, i64 result_size, ptr cleanup) -> ptr`
- `__orison_task_await(ptr handle) -> void`
- `__orison_concurrency_handle_destroy(ptr handle) -> void`

The compiler will recover the typed result by loading from the explicit result storage after `join` or `await`
completes. This keeps the runtime ABI finite and untyped while preserving compiler-owned type, ownership, and
drop/cleanup decisions.

## Consequences

- `thread` and `task` can share most lowering machinery: environment allocation, result storage allocation, thunk
  emission, handle binding, and cleanup scheduling.
- `.join()` and `await` are synchronization operations first; result materialization remains an ordinary typed load.
- The runtime can be implemented in C/C++ without knowing Orison source types.
- Module prelude emission declares only the concurrency runtime symbols requested by lowering and deduplicates them in
  first-use order.
- `tour_11_concurrency.or` must remain frontend-only until entry thunk generation, capture environment layout, result
  storage, and runtime linking are implemented.

## Follow-up work

- Add lowering metadata for captured values and result storage layout.
- Implement thread spawn/join for scalar transferable results before task scheduling.
- Define cleanup behavior for abandoned handles and failed spawn paths.
