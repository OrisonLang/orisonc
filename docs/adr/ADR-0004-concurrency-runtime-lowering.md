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
- Semantic analysis retains one concurrency-capture record per source occurrence, including the owning concurrency
  expression's source line. Lowering selects captures by expression kind and exact owner line, then converts those
  occurrences into one environment field per captured binding in first-use order. Cleanup planning uses the same
  owner-specific deduplicated field layout.
- Nested concurrency expressions retain one active capture context per crossed task/thread boundary. A reference from
  an inner body to an outer binding records both the direct inner capture and each required transitive outer capture,
  while bindings declared inside an outer body remain local to that outer thunk.
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
- Task and thread entry thunks lower straight-line body prefixes through the shared statement-block lowerer before
  storing the final value. The admitted prefixes are local `let`, local `var`, assignment, and call statements.
  Semantic expression summaries provide result types when the final value is a thunk-local binding.
- Nested scalar task/thread bindings lower as body prefixes. Nested entry and cleanup definitions, generated symbols,
  capture environments, result storage, and synchronization calls propagate from the inner lowering session into the
  enclosing module. Semantic expression summaries retain each nested result type for outer planning.
- A binding declared inside an outer concurrency body is captured only by an inner concurrency expression that uses it.
  The binding is computed in the outer thunk and stored into the inner environment; it is not added to the outer
  environment.
- Mixed task/thread nesting lowers in both directions. The module requests both runtime symbol families, generated
  symbol names retain the enclosing thunk identity, and semantic capture validation applies the marker rule for every
  crossed boundary. A `Shareable`-only value still fails when any crossed boundary is a thread.
- Scalar `await` expressions for lowered task bindings call `__orison_task_await(handle)`, load the typed result from
  compiler-owned result storage, and destroy the handle after the load.
- Semantic expression inference now preserves the result type of `await` when it appears inside a larger expression,
  not only as a final standalone statement. Nested task lowering therefore supports scalar compositions such as
  `outer_value + await inner`; the generated thunk awaits, loads, destroys the inner handle, and emits the arithmetic
  before storing the outer result.
- Ternary arms lower from the same concurrency-binding snapshot. When only one arm awaits a task, the merge block
  represents that task handle with a pointer PHI selecting `null` for the consumed path and the live handle for the
  other path. Existing null-safe automatic cleanup then destroys only the handle left live at runtime.
- Concurrency plans and bindings retain the semantic result source type alongside its lowered LLVM type. Source-type
  queries use that exact type for `await`, allowing unannotated locals to infer composed comparison, direct-call, and
  ternary results without reconstructing nominal or alias identity from LLVM types.
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
- Module-wide cleanup planning now mirrors nested thunk scopes. Each scope owns an independent concurrency ordinal,
  nested cleanup symbols use the generated enclosing thunk identity, and each cleanup contains only the owned capture
  candidates assigned to that boundary. Cleanup call emission remains disabled until semantic authorization and ABI
  declarations are proven for the nested plans.
- Non-empty concurrency cleanup plans require semantic owned-cleanup authorization. Readiness correlation applies one
  type-level authorization and its emitted ABI declaration to every matching boundary-local action while retaining a
  distinct authorization result for each generated cleanup thunk. Declaration-only metadata cannot make readiness
  succeed, and normal cleanup call emission remains disabled without the existing explicit authorization options.
- Source-backed IR coverage now proves nested cleanup-thunk composition under explicit semantic test authorization.
  The outer thunk addresses both fields in its two-field environment, while the inner thunk addresses its sole field
  in a separate one-field environment; all calls target the same authorized concrete cleanup definition. Normal
  cleanup emission remains unchanged.
- The complete explicitly authorized nested-cleanup module passes LLVM parsing and verification. Each task spawn passes
  its exact boundary-local cleanup thunk through the runtime ABI cleanup slot: the outer spawn uses the outer cleanup,
  and the inner spawn uses the cleanup derived from the generated outer thunk identity.
- Paired normal-options coverage proves the same nested module retains both cleanup thunks and their boundary-local
  candidate metadata while readiness remains blocked and no concrete owned-cleanup definition or call is emitted. The
  metadata-only module also passes LLVM verification.
- Target layout now resolves named record LLVM types through the lowering context, allowing aggregate capture
  environments to report concrete byte sizes when their record field layouts are known.
- Scalar task/thread lowering now has smoke coverage for record result storage, including context-sized `%record.*`
  result buffers passed through the runtime ABI.
- Record-valued task lowering now has checked-in compile, link, and execution coverage. The compiler stores the record
  in typed result storage, the runtime remains source-type-agnostic, and `await` restores the record for ordinary field
  access without changing the runtime ABI.
- Record-valued thread lowering now has matching compile, link, and execution coverage. `.join()` restores the typed
  record from compiler-owned result storage, and both fields remain available to ordinary expression lowering.
- Named `%record.Pair` results also cross two nested task or thread boundaries. Each boundary resolves the same nominal
  layout and owns an 8-byte typed result buffer; the enclosing thunk forwards the synchronized aggregate into its own
  result storage.
- Scalar-payload choice results also cross two nested task or thread boundaries. Each boundary owns an 8-byte
  `{ i32, i32 }` result buffer; the enclosing thunk forwards the synchronized tag and payload together before the
  caller reconstructs the selected variant.
- Distinct-layout scalar-payload choices also cross two nested task or thread boundaries. Each boundary owns an 8-byte
  `{ i32, [4 x i8] }` result buffer; the enclosing thunk forwards the tag and erased payload bytes together before the
  caller reconstructs the selected `Bool` or `UInt32` payload.
- The checked-in record task and thread examples now also capture an outer scalar. IR coverage pins the scalar capture
  environment, thunk-side load, record construction, typed result store, synchronization, and typed record load; linked
  execution validates the complete composition.
- Scalar-payload choice results now cross task and thread synchronization through the same compiler-owned result
  storage. Lowering infers unique payload-compatible choice constructors, preserves the nonzero variant tag and scalar
  payload in `{ i32, i32 }`, and passes the existing 8-byte result size to the unchanged runtime ABI.
- Choices with distinct scalar payload layouts now cross the same task and thread boundary as
  `{ i32, [4 x i8] }`. The task copies a captured `UInt32` and the thread copies a captured `Bool` into erased payload
  bytes; result storage preserves both forms through synchronization and switch lowering reconstructs each source type.
- Fixed `Array<UInt32, 2>` results now cross task and thread boundaries as inline `[2 x i32]` values. Compiler-owned
  result storage uses the target-layout size of 8 bytes, while the runtime ABI remains source-type-agnostic.
- Fixed `Array<UInt32, 2>` results also cross two nested task or thread boundaries. Each boundary owns an 8-byte typed
  result buffer; the enclosing thunk synchronizes with the inner handle, loads `[2 x i32]`, and stores that value into
  its own result buffer.
- Nested `Array<Array<UInt32, 2>, 2>` results now cross the same boundaries as inline `[2 x [2 x i32]]` values.
  Recursive construction and target sizing produce 16-byte compiler-owned result storage without runtime ABI changes.
- Fixed `Array<Pair, 2>` results now preserve named `%record.Pair` elements inside `[2 x %record.Pair]`. Recursive
  record and array construction produces 16-byte compiler-owned result storage without runtime ABI changes.
- `Batch` record results containing `[2 x i32]` plus an `i32` scalar now use a 12-byte `%record.Batch` layout. Nested
  construction and field/index reads compose through compiler-owned result storage without runtime ABI changes.
- Choice results carrying `Array<UInt32, 2>` preserve the tag and aggregate payload in `{ i32, [8 x i8] }`. Typed
  payload erasure and reconstruction compose through 12-byte compiler-owned result storage without runtime ABI changes.
- Choice results carrying named `Pair` records preserve `%record.Pair` through the same `{ i32, [8 x i8] }` layout.
  Target-layout resolution and typed record reconstruction compose without runtime ABI changes.
- Fixed-arity choice variants with two `UInt32` payloads cross task and thread boundaries through the same
  `{ i32, [8 x i8] }` layout. Lowering packs the fields into `{ i32, i32 }`, preserves the 12-byte result storage,
  and reconstructs both switch bindings after synchronization without runtime ABI changes.
- Mixed-layout choice variants with `UInt32` and `Bool` payloads also use `{ i32, [8 x i8] }`. Lowering packs the
  fields into `{ i32, i1 }`, preserves target padding through payload erasure, and reconstructs both typed bindings.
- Aggregate-plus-scalar choice variants pack `Array<UInt32, 2>` and `UInt32` into `{ [2 x i32], i32 }`. The choice
  expands to `{ i32, [12 x i8] }`, uses 16-byte result storage, and reconstructs both payload fields after synchronization.
- Named-record-plus-scalar choice variants pack `Pair` and `UInt32` into `{ %record.Pair, i32 }`. The choice retains
  `{ i32, [12 x i8] }` and 16-byte result storage while restoring typed record fields and the scalar marker.
- Concrete `Result<UInt32>` choice values specialize `Success(left: T, right: T)` to `{ i32, i32 }`, retain
  `{ i32, [8 x i8] }` and 12-byte result storage, and emit the concrete `Result_UInt32_` implementation symbol.
- Concrete `Result<Array<UInt32, 2>>` choice values specialize the same fields to
  `{ [2 x i32], [2 x i32] }`, retain `{ i32, [16 x i8] }` and 20-byte result storage, and emit the concrete
  `Result_Array_UInt32__2__` implementation symbol.
- Concrete `Result<Array<Array<UInt32, 2>, 2>>` choice values recursively specialize the same fields to
  `{ [2 x [2 x i32]], [2 x [2 x i32]] }`, retain `{ i32, [32 x i8] }` and 36-byte result storage, and emit the
  concrete `Result_Array_Array_UInt32__2___2__` implementation symbol.
- Concrete `Result<Array<Pair, 2>>` choice values compose named records and fixed arrays as
  `{ [2 x %record.Pair], [2 x %record.Pair] }`, retain `{ i32, [32 x i8] }` and 36-byte result storage, and emit the
  concrete `Result_Array_Pair__2__` implementation symbol.
- Concrete `Result<Batch>` choice values compose records containing fixed arrays as
  `{ %record.Batch, %record.Batch }`, where `%record.Batch` is `{ [2 x i32], i32 }`; they use
  `{ i32, [24 x i8] }`, 28-byte result storage, and the concrete `Result_Batch_` implementation symbol.
- Concrete `Result<Batch>` choice values also compose records containing fixed arrays of named records as
  `{ %record.Batch, %record.Batch }`, where `%record.Batch` is `{ [2 x %record.Pair], i32 }`; they use
  `{ i32, [40 x i8] }`, 44-byte result storage, and preserve the concrete `Result_Batch_` implementation symbol.
- Concrete `Result<Batch>` choice values recursively compose records containing nested fixed arrays of named records
  as `{ %record.Batch, %record.Batch }`, where `%record.Batch` is `{ [2 x [2 x %record.Pair]] }`; they use
  `{ i32, [64 x i8] }`, 68-byte result storage, and preserve the concrete `Result_Batch_` implementation symbol.
- Cleanup thunks now lower cleanup-candidate environment fields to deterministic field addresses, providing the
  insertion point for future owned-capture drop emission while still emitting no drops today.
- Cleanup candidate plans now include deterministic type-specific drop symbol names; cleanup thunks document those
  planned symbols but do not call them until drop semantics are accepted.
- Module prelude emission has an explicit drop-declaration seam for future `__orison_owned_cleanup.<Type>` declarations, but
  planned declarations are disabled by default and current concurrency lowering does not request them.
- LLVM IR emission now collects cleanup-candidate drop declarations as disabled metadata before prelude emission, so
  needed `__orison_owned_cleanup.<Type>` symbols are discoverable end-to-end without changing emitted IR.
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
  changing normal lowering behavior. Direct smoke coverage pins blocker report formatting while pipeline smoke verifies
  typed blocker summary state rendered at the consumer edge.
- Drop cleanup authorization reports now split semantic-lowering blockers into unresolved semantic drops versus
  source-drop-lowering-not-accepted blockers. This lets parsed/proven source-derived drop candidates reduce the
  semantic uncertainty in reports while normal lowering still emits no drop declarations or calls.
- Drop readiness source-correlation reporting is pipeline-owned; it combines semantic planned-drop sites with lowering
  cleanup actions. It traces cleanup action source type/capture metadata back to semantic owner/drop-site
  status and emitted-declaration status without changing generated IR.
  The source-correlation formatter lives in a dedicated pipeline report component so compile orchestration does not own
  report matching and rendering policy, and direct smoke coverage pins its matching behavior independently from the
  full compile pipeline. Pipeline smoke coverage verifies typed snapshot state rendered as source-correlation text at
  the consumer edge.
- Direct smoke coverage now pins drop readiness relation report formatting independently from the full compile
  pipeline, including empty, blocked, authorized, and multi-action cleanup states. Pipeline smoke verifies typed
  snapshot state rendered as relation text at the consumer edge.
- Direct smoke coverage now pins drop cleanup authorization report formatting independently from the full compile
  pipeline, including unresolved semantic blockers, source-lowering gates, authorized cleanups, and multi-action blocked
  cleanups. Pipeline smoke verifies cleanup authorization reporting is wired and populated from fixture metadata.
- Direct smoke coverage now pins drop readiness snapshot and summary report formatting independently from the full
  compile pipeline, including empty, blocked, authorized, and mixed readiness states. Pipeline smoke verifies typed
  snapshot and summary state rendered at the consumer edge.
- Driver smoke now treats drop-readiness and drop-cleanup report commands as CLI wiring checks, asserting successful
  command execution plus representative report fragments while dedicated formatter smokes own exact rendering.
- `examples/concurrency_task_main.or` is the checked-in runnable smoke source for the current scalar task runtime path.
- `examples/concurrency_record_task_main.or` is the checked-in runnable smoke source for record-valued task result
  transfer through compiler-owned storage.
- `examples/concurrency_record_thread_main.or` is the checked-in runnable smoke source for record-valued thread result
  transfer through compiler-owned storage.
- `examples/concurrency_choice_task_main.or` and `examples/concurrency_choice_thread_main.or` are the checked-in
  runnable smoke sources for scalar-payload choice result transfer.
- `examples/concurrency_distinct_choice_task_main.or` and
  `examples/concurrency_distinct_choice_thread_main.or` cover erased distinct-layout scalar payload transfer.
- `examples/concurrency_array_task_main.or` and `examples/concurrency_array_thread_main.or` cover inline fixed-array
  result transfer and indexed reads after synchronization.
- `examples/concurrency_nested_array_task_main.or` and `examples/concurrency_nested_array_thread_main.or` cover
  recursive fixed-array result transfer and two-level indexed reads after synchronization.
- `examples/concurrency_record_array_task_main.or` and `examples/concurrency_record_array_thread_main.or` cover fixed
  arrays of named records and indexed field reads after synchronization.
- `examples/concurrency_array_field_record_task_main.or` and
  `examples/concurrency_array_field_record_thread_main.or` cover named records containing fixed arrays plus scalars.
- `examples/concurrency_array_choice_task_main.or` and `examples/concurrency_array_choice_thread_main.or` cover erased
  fixed-array choice payload transfer and typed switch-case reconstruction.
- `examples/concurrency_record_choice_task_main.or` and `examples/concurrency_record_choice_thread_main.or` cover
  erased named-record choice payload transfer and typed switch-case reconstruction.
- `examples/concurrency_multi_payload_choice_task_main.or` and
  `examples/concurrency_multi_payload_choice_thread_main.or` cover erased fixed-arity multi-payload choice transfer.
- `examples/concurrency_mixed_payload_choice_task_main.or` and
  `examples/concurrency_mixed_payload_choice_thread_main.or` cover mixed-layout multi-payload choice transfer.
- `examples/concurrency_aggregate_scalar_choice_task_main.or` and
  `examples/concurrency_aggregate_scalar_choice_thread_main.or` cover aggregate-plus-scalar choice transfer.
- `examples/concurrency_record_scalar_choice_task_main.or` and
  `examples/concurrency_record_scalar_choice_thread_main.or` cover named-record-plus-scalar choice transfer.
- `examples/concurrency_generic_choice_task_main.or` and `examples/concurrency_generic_choice_thread_main.or` cover
  concrete generic multi-payload choice transfer.
- `examples/concurrency_generic_aggregate_choice_task_main.or` and
  `examples/concurrency_generic_aggregate_choice_thread_main.or` cover concrete generic named-record multi-payload
  choice transfer.
- `examples/concurrency_generic_array_choice_task_main.or` and
  `examples/concurrency_generic_array_choice_thread_main.or` cover concrete generic fixed-array multi-payload choice
  transfer.
- `examples/concurrency_generic_nested_array_choice_task_main.or` and
  `examples/concurrency_generic_nested_array_choice_thread_main.or` cover concrete generic nested-array multi-payload
  choice transfer.
- `examples/concurrency_generic_record_array_choice_task_main.or` and
  `examples/concurrency_generic_record_array_choice_thread_main.or` cover concrete generic record-array multi-payload
  choice transfer.
- `examples/concurrency_generic_array_field_record_choice_task_main.or` and
  `examples/concurrency_generic_array_field_record_choice_thread_main.or` cover concrete generic record payloads
  containing fixed arrays.
- `examples/concurrency_generic_record_array_field_record_choice_task_main.or` and
  `examples/concurrency_generic_record_array_field_record_choice_thread_main.or` cover concrete generic record
  payloads containing fixed arrays of named records.
- `examples/concurrency_generic_nested_record_array_field_record_choice_task_main.or` and
  `examples/concurrency_generic_nested_record_array_field_record_choice_thread_main.or` cover concrete generic record
  payloads containing nested fixed arrays of named records.
- `examples/concurrency_thread_main.or` is the checked-in runnable smoke source for the current scalar thread runtime
  path.
- `examples/concurrency_nested_task_main.or` and `examples/concurrency_nested_thread_main.or` are checked-in runnable
  sources for transitive scalar capture, nested generated thunks, and nested synchronization.
- `examples/concurrency_nested_local_capture_task_main.or` and
  `examples/concurrency_nested_local_capture_thread_main.or` prove local-only inner capture ownership.
- `examples/concurrency_mixed_task_in_thread_main.or` and `examples/concurrency_mixed_thread_in_task_main.or` prove
  mixed runtime symbol composition and nested synchronization in both directions.
- `examples/concurrency_nested_array_result_task_main.or` and
  `examples/concurrency_nested_array_result_thread_main.or` prove typed fixed-array transfer through two result buffers.
- `examples/concurrency_nested_record_result_task_main.or` and
  `examples/concurrency_nested_record_result_thread_main.or` prove nominal record transfer through two result buffers.
- `examples/concurrency_nested_choice_result_task_main.or` and
  `examples/concurrency_nested_choice_result_thread_main.or` prove tagged scalar-payload transfer through two result
  buffers.
- `examples/concurrency_nested_distinct_choice_result_task_main.or` and
  `examples/concurrency_nested_distinct_choice_result_thread_main.or` prove erased distinct-layout scalar-payload
  transfer through two result buffers.
- Module emission now validates concurrency runtime prelude declarations plus generated task/thread entry and cleanup
  thunk symbols through the shared lowering module symbol registry before generated LLVM IR is accepted.
- `examples/concurrency_composed_await_ternary_left_main.or` and
  `examples/concurrency_composed_await_ternary_right_main.or` compile, link, and run both task-valued ternary outcomes.
  Their IR checks pin selected-branch synchronization, null/live handle joins, and automatic destruction of the
  unselected task handle. No source syntax or concurrency ABI changed.
- `examples/concurrency_await_if_true_main.or` and `examples/concurrency_await_if_false_main.or` compile, link, and run
  both Boolean task outcomes as direct `if` conditions. Their IR checks pin await, result load, consumed-handle
  destruction, and conditional branch order. No source syntax or concurrency ABI changed.
- `examples/concurrency_await_guard_continue_main.or` and `examples/concurrency_await_guard_return_main.or` compile,
  link, and run direct Boolean task results as `guard` conditions. Their IR checks pin synchronization and consumed-
  handle destruction before continuation or early return. No source syntax or concurrency ABI changed.
- `examples/concurrency_await_switch_match_main.or` and `examples/concurrency_await_switch_default_main.or` compile,
  link, and run direct scalar task results as `switch` subjects. Their IR checks pin synchronization and consumed-
  handle destruction before matched/default dispatch. No source syntax or concurrency ABI changed.
- `examples/concurrency_await_fixed_array_index_main.or` compiles, links, and runs a direct `UInt64` task result as a
  fixed-array index. Prelude discovery now infers unannotated array-literal local types, keeping bounds-failure symbol
  declarations aligned with emitted checks. IR pins synchronization, cleanup, bounds checking, GEP, and element load.
- `examples/concurrency_await_fixed_array_index_out_of_bounds_main.or` proves the paired failure path. The consumed task
  handle is destroyed before the fixed-array bounds branch, and index `2` against length `2` reaches the runtime bounds
  abort with deterministic host exit code `134`.
- `examples/concurrency_await_fixed_array_assignment_index_main.or` compiles, links, and runs a direct `UInt64` task
  result as a fixed-array assignment index. IR pins single synchronization and result evaluation, consumed-handle
  destruction, bounds checking, indexed addressing, and the final element store.
- `examples/concurrency_await_fixed_array_assignment_index_out_of_bounds_main.or` proves the paired assignment failure
  path. The consumed task handle is destroyed before bounds dispatch, the element store remains confined to the
  in-bounds block, and runtime execution exits with code `134` through the bounds abort.
- `examples/concurrency_await_dynamic_array_index_main.or` compiles, links, and runs a direct `UInt64` task result as a
  `DynamicArray<UInt32>` read index. IR pins task cleanup, descriptor-length bounds checking, data addressing, element
  loading, and automatic DynamicArray deallocation before return.
- `examples/concurrency_await_dynamic_array_index_out_of_bounds_main.or` proves the paired failure path. Task cleanup
  precedes descriptor inspection and bounds dispatch; index `2` against runtime length `2` aborts before the isolated
  in-bounds element-address and load block, producing host exit code `134`.
- `examples/concurrency_await_dynamic_array_assignment_index_main.or` compiles, links, and runs a direct `UInt64` task
  result as a `DynamicArray<UInt32>` assignment index. IR pins task cleanup, descriptor-length bounds checking, one
  indexed store, readback, and automatic DynamicArray deallocation before return.
- `examples/concurrency_await_dynamic_array_assignment_index_out_of_bounds_main.or` proves the paired assignment
  failure path. Task cleanup precedes descriptor-length bounds dispatch, index `2` against runtime length `2` aborts,
  and the target element store remains isolated in the unexecuted in-bounds block.
- `examples/concurrency_await_dynamic_array_record_field_index_main.or` composes a scalar task result with an owned
  `DynamicArray<UInt32>` record field. The generated path projects the field, consumes the task, checks the descriptor
  length, loads the selected element, and automatically cleans the aggregate-owned descriptor before return.
- `examples/concurrency_await_dynamic_array_record_field_index_out_of_bounds_main.or` proves the paired failure path.
  Field projection and task cleanup precede descriptor-length bounds dispatch; index `2` against runtime length `2`
  aborts before the isolated data projection, element address, and load block.
- `examples/concurrency_await_dynamic_array_nested_record_field_index_main.or` composes a scalar task result with an
  owned `DynamicArray<UInt32>` reached through two record projections. The generated path consumes the task, loads the
  selected scalar, and invokes recursive outer-to-inner aggregate cleanup before return.
- `examples/concurrency_await_dynamic_array_nested_record_field_index_out_of_bounds_main.or` proves the paired nested
  failure path. Both record projections and task cleanup precede descriptor-length bounds dispatch; index `2` aborts
  before data projection, element addressing, or loading.

## Follow-up work

- Define the real async function ABI, suspension model, cancellation policy, and executor integration model beyond the
  current pthread-backed execution shim.
