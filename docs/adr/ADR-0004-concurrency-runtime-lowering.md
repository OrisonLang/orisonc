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
- Direct runtime ABI smoke coverage uses a test-only thread-create failure seam to deterministically verify cleanup on
  spawn setup failure.
- Lowering now emits deterministic private cleanup thunks for captured task/thread environments and passes those thunks
  through the runtime cleanup slot; the current thunk body is intentionally empty until owned capture drop lowering
  exists.
- Concurrency plans now record cleanup drop candidates for non-scalar captured environment fields so future owned-capture
  cleanup lowering has explicit field metadata without changing the runtime ABI.
- Target layout now resolves named record LLVM types through the lowering context, allowing aggregate capture
  environments to report concrete byte sizes when their record field layouts are known.
- Scalar task/thread lowering now has smoke coverage for record result storage, including context-sized `%record.*`
  result buffers passed through the runtime ABI.
- Cleanup thunks now lower cleanup-candidate environment fields to deterministic field addresses, providing the
  insertion point for future owned-capture drop emission while still emitting no drops today.
- Cleanup candidate plans now include deterministic type-specific drop symbol names; cleanup thunks document those
  planned symbols but do not call them until drop semantics are accepted.
- Module prelude emission has an explicit drop-declaration seam for future `__orison_drop.<Type>` declarations, but
  planned declarations are disabled by default and current concurrency lowering does not request them.
- LLVM IR emission now collects cleanup-candidate drop declarations as disabled metadata before prelude emission, so
  needed `__orison_drop.<Type>` symbols are discoverable end-to-end without changing emitted IR.
- Lowering metadata scans now share a syntax traversal helper so runtime-symbol discovery and disabled drop-declaration
  discovery walk expressions consistently.
- Planned drop declarations now retain the source type and concurrency expression line where the future drop need was
  discovered, while still emitting no drop ABI by default.
- Lowering now separates cleanup-site planned drop actions from deduped drop declaration metadata, so each captured
  environment field keeps its own future drop insertion metadata while module prelude declaration planning remains
  one-entry-per-drop-symbol.
- Each concurrency cleanup thunk now also carries a metadata-only drop cleanup plan containing the per-field planned
  drop actions for that thunk; cleanup thunk IR still only emits field-address setup and comments, not drop calls.
- Drop cleanup plans now model drop-call emission eligibility explicitly. Cleanup thunk calls can only be authorized by
  matching every planned drop action to an enabled planned drop declaration, and the default path remains metadata-only
  until ownership/drop semantics are accepted.
- Drop readiness reporting now includes a compact blocker report derived from the same snapshot as the readiness summary
  and relation reports, listing blocked cleanup count plus semantic-lowering and missing-declaration blockers without
  changing normal lowering behavior.
- Drop cleanup authorization reports now split semantic-lowering blockers into unresolved semantic drops versus
  source-drop-lowering-not-accepted blockers. This lets parsed/proven source-derived drop candidates reduce the
  semantic uncertainty in reports while normal lowering still emits no drop declarations or calls.
- Drop readiness source-correlation reporting is pipeline-owned because it combines semantic planned-drop sites with
  lowering cleanup actions. It traces cleanup action source type/capture metadata back to semantic owner/drop-site
  status and emitted-declaration status without changing generated IR.
  The source-correlation formatter lives in a dedicated pipeline report component so compile orchestration does not own
  report matching and rendering policy.
- `examples/concurrency_task_main.or` is the checked-in runnable smoke source for the current scalar task runtime path.
- `examples/concurrency_thread_main.or` is the checked-in runnable smoke source for the current scalar thread runtime
  path.

## Follow-up work

- Define the real async function ABI, suspension model, cancellation policy, and executor integration model beyond the
  current pthread-backed execution shim.
