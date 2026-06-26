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
- `__orison_concurrency_spawn_failed() -> void`

The compiler will recover the typed result by loading from the explicit result storage after `join` or `await`
completes. This keeps the runtime ABI finite and untyped while preserving compiler-owned type, ownership, and
drop/cleanup decisions.

## Consequences

- `thread` and `task` can share most lowering machinery: environment allocation, result storage allocation, thunk
  emission, handle binding, and cleanup scheduling.
- `.join()` and `await` are synchronization operations first; result materialization remains an ordinary typed load.
- The runtime is implemented in C++ while exporting a stable C ABI; it does not know Orison source types.
- Module prelude emission declares only the concurrency runtime symbols requested by lowering and deduplicates them in
  first-use order.
- Lowering planning records each concurrency expression's operation kind, deterministic thunk symbol, inferred lowered
  result type, and semantic captures before any runtime call or thunk IR is emitted.
- The concurrency plan carries concrete LLVM type strings, field indexes, and target-sized byte counts for capture
  environments plus result storage.
- Scalar `thread` bindings can now lower their capture environment allocation, result storage allocation, capture
  stores, private entry thunk, and runtime spawn call shape with the planned result byte size. The thunk loads captures
  from the environment, lowers the final scalar body expression, and writes the result into compiler-owned result
  storage.
- Scalar `.join()` expressions can now lower by calling `__orison_thread_join(handle)` and loading the typed value from
  compiler-owned result storage.
- Successfully joined scalar thread handles are destroyed immediately after the typed result is loaded.
- Abandoned scalar thread handles that reach a normal lowered function return without `.join()` are destroyed before the
  return is emitted. The compiler does not materialize a result value for abandoned handles.
- Scalar thread spawn results are checked for `null` immediately after the runtime call. A null result branches to
  `__orison_concurrency_spawn_failed()` followed by `unreachable`; only the non-null continuation binds the handle for
  later join or abandoned-handle cleanup.
- Scalar `task` bindings now reuse the thread lowering machinery for capture environment allocation, result storage
  allocation, entry thunk emission, spawn failure checks, and abandoned-handle cleanup.
- Scalar `await` expressions for lowered task bindings call `__orison_task_await(handle)`, load the typed result from
  compiler-owned result storage, and destroy the handle after the load.
- `async function` lowering is currently a narrow backend admission rule for scalar task/await bodies; it does not yet
  model a distinct async function ABI, suspension, cancellation, or executor integration.
- A minimal pthread-backed runtime archive now provides the accepted concurrency ABI during host linking. Destroying an
  unjoined or unawaited handle joins before freeing the opaque handle so compiler-owned stack result/environment storage
  cannot outlive a still-running runtime thread.
- Direct runtime ABI smoke coverage now verifies thread join result transfer, task await result transfer, destroy after
  synchronization, and abandoned-handle destroy waiting behavior.
- The runtime now honors the optional spawn cleanup callback by invoking it with the environment pointer after entry
  completion, and also on spawn setup failure before returning `null`.
- `examples/concurrency_task_main.or` is the checked-in runnable smoke source for the current scalar task runtime path.
- `examples/concurrency_thread_main.or` is the checked-in runnable smoke source for the current scalar thread runtime
  path.

## Follow-up work

- Define the real async function ABI, suspension model, cancellation policy, and executor integration model beyond the
  current pthread-backed execution shim.
