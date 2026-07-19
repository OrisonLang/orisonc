# ADR-0006: Dynamic Sequence Lowering Model

## Status
Accepted

## Context

The current lowering path supports fixed-size `Array<T, N>` iteration, partial `View<T>` pointer-style ABI handling,
and no `DynamicArray<T>` lowered function-signature support. Before implementing dynamic `for ... in` lowering, the
compiler needs an internal model that separates source-level sequence ownership and access mode from the eventual LLVM
representation.

## Decision

- Lowering classifies `DynamicArray<T>` and `View<T>`-shaped source types as dynamic sequences before emitters decide
  whether a use site is currently supported.
- `DynamicArray<T>` is the owned contiguous dynamic sequence form. Its target ABI model is a descriptor containing a
  data pointer, a length, and a capacity, represented internally as `{ ptr, i64, i64 }`.
- `View<T>`, `shared.View<T>`, and `exclusive.View<T>` are non-owning contiguous sequence views. Their target ABI model
  is a descriptor containing a data pointer and a length.
- View-shaped function parameters and returns lower to the descriptor ABI as `{ ptr, i64 }`.
- Dynamic-array descriptor layout is pinned for sizing and future ABI work, but dynamic-array function signatures
  remain rejected until allocation, ownership, and drop invariants are implemented.
- Dynamic-array lowering requires a unique owning descriptor, an allocator/runtime allocation path, a proven
  `0 <= length <= capacity` descriptor invariant, and an element drop walk for initialized elements before lowered
  signatures can be enabled.
- Dynamic-array runtime ABI entry points are finite and internal: `__orison_dynamic_array_allocate(i64, i64)`,
  `__orison_dynamic_array_grow({ ptr, i64, i64 }, i64, i64)`, and
  `__orison_dynamic_array_deallocate(ptr, i64, i64)`. The `i64` parameters are element size/capacity-style scalar
  values chosen by lowering, not source-level variadic or spread arguments.
- Shared/exclusive access is tracked as source-type metadata, not by inventing different LLVM pointer spellings.
- The current fixed-array-only `for` lowering diagnostic remains valid until loop lowering consumes this model and
  emits descriptor-aware indexing and bounds.

## Consequences

- Dynamic sequence support can be added without changing the source grammar.
- Emitters have a single query for dynamic sequence element type, ownership, and mutation capability.
- View parameter indexing extracts the descriptor data pointer before element addressing. Length-aware bounds and
  dynamic iteration remain future work.
- Literal LLVM struct layout support sizes descriptor-shaped ABI values directly.
- Dynamic-array drop handling remains metadata-only under ADR-0005 until ownership/drop semantics authorize production
  cleanup calls.
- Dynamic-array runtime declarations are modeled but not emitted into modules until source-level construction,
  ownership, and cleanup lowering consume them.
- Module prelude emission supports dynamic-array runtime declarations only through an explicit internal operation list;
  the default list is empty, so ordinary modules do not gain these declarations until lowering requests them.
- LLVM IR emission records dynamic-array runtime operation requests separately from prelude emission. The collection
  pass currently returns an empty set until construction/lowering support has a proven reason to request allocation,
  growth, or deallocation declarations.
- Dynamic-array construction planning is metadata-only until construction lowering is enabled: it derives the finite
  `allocate` request operands from `DynamicArray<T>` source type metadata, the lowered element type, target-layout
  element size, and explicit initial capacity.
- A test-only LLVM emission seam can feed construction requests into that planner and surface the resulting operation
  request report. This may request the runtime declaration for audit coverage, but it still emits no allocation call
  instruction and does not expose new source syntax.
- A disabled allocation-call renderer can produce the exact LLVM `call` instruction text from a construction plan under
  test-only control. The rendered snippets are retained outside module IR until source-level construction lowering and
  ownership/drop authorization exist.
- A disabled descriptor-binding renderer models the future local descriptor `alloca` and `store` for an allocation
  result. Like the call renderer, it is retained outside module IR until construction lowering is authorized.
- Disabled descriptor-field projection helpers pin the `{ ptr, i64, i64 }` field order for data pointer, length, and
  capacity extraction. The snippets remain outside module IR until bounds checks and element access lowering consume
  the descriptor model.
- Disabled bounds-check renderers pin unsigned `i64` comparisons for future indexing, append-without-grow, and
  `length <= capacity` descriptor invariant checks. These snippets remain outside module IR until dynamic-array
  element access and growth lowering are authorized.
- A disabled element-address renderer pins future dynamic-array indexing as `getelementptr <element>, ptr <data>, i64
  <index>` after descriptor projection and bounds checks. The snippet remains outside module IR until element access
  lowering is authorized.
- A disabled element-load renderer pins the final future safe-indexing read as `load <element>, ptr <element.addr>`.
  The snippet remains outside module IR until element access lowering is authorized.
- A disabled element-store renderer pins future mutable indexing writes as `store <element> <value>, ptr
  <element.addr>`. The snippet remains outside module IR until mutable element access lowering is authorized.
- A disabled descriptor length-update renderer pins future append initialization as `add i64 <length>, 1` followed by
  `insertvalue { ptr, i64, i64 } <descriptor>, i64 <next.length>, 1`. The snippet remains outside module IR until
  dynamic-array mutation lowering is authorized.
- A disabled descriptor write-back renderer pins committing an updated dynamic-array descriptor back to its local slot
  as `store { ptr, i64, i64 } <updated>, ptr <descriptor.addr>`. The snippet remains outside module IR until
  dynamic-array mutation lowering is authorized.
- A disabled append sequence renderer composes append capacity checking, element addressing at current length, element
  store, descriptor length update, and descriptor write-back in order. The snippet remains outside module IR until
  dynamic-array mutation lowering is authorized.
- A disabled grow-call renderer pins the finite runtime call shape for capacity-failure handling as
  `__orison_dynamic_array_grow({ ptr, i64, i64 }, i64, i64)`. A disabled grow sequence currently doubles capacity,
  calls grow with the element size and next capacity, and writes the returned descriptor back to the local slot. The
  snippet remains outside module IR until dynamic-array growth semantics are authorized.
- A disabled append-with-grow sequence renderer pins the future branch shape for append mutation: check capacity,
  branch to grow only when needed, join on an active descriptor value, then perform the append store and descriptor
  update. The snippet remains outside module IR until dynamic-array control-flow and growth semantics are authorized.
- A disabled deallocation-call renderer pins the finite cleanup runtime call shape as
  `__orison_dynamic_array_deallocate(ptr, i64, i64)`. A disabled cleanup sequence currently extracts data, length, and
  capacity, then deallocates the backing storage with element size and capacity. Element drop walking remains future
  ownership/drop work.
- A disabled initialized-element drop-walk renderer pins a future cleanup loop over initialized indexes
  `0 <= index < length`. It computes each element address and emits only a planned-drop placeholder comment before
  deallocation; it does not emit drop calls or drop declarations until ownership/drop semantics authorize them.
- Dynamic-array element cleanup now feeds the existing drop-readiness metadata path for disabled test-only drop walks:
  owned element types add a metadata-only cleanup plan and planned element-drop action, while scalar and non-owning
  element types remain cleanup-neutral. This records future cleanup obligations without emitting drop calls or enabling
  `DynamicArray<T>` lowered source signatures.
- Dynamic-array element cleanup plans require positive semantic drop authorization in addition to emitted drop
  declaration metadata before readiness can be reported as authorized. Allowlist-only declaration metadata is
  insufficient for these owned-container element cleanup obligations.
- Pipeline-level drop cleanup authorization, readiness blocker, relation, and source-correlation reports can now carry
  disabled dynamic-array element cleanup blockers through the same report fields used by concurrency cleanup plans.
  This remains an internal test seam and does not add a public source syntax or production cleanup emission path.
- Pipeline smoke coverage also pins the positive metadata-only path: when a dynamic-array element cleanup has positive
  semantic drop authorization and emitted drop declaration metadata, readiness reports mark the cleanup authorized while
  generated IR still contains no dynamic-array element drop calls.
- Semantic analysis now records source-derived ownership facts for real `DynamicArray<T>` bindings: the owned container
  itself gets a planned drop site, and owned element types add an element planned drop site such as
  `items.element`. Scalar and non-owning element types do not add element drop sites.
- Disabled dynamic-array cleanup metadata can now carry the source owner name into its planned element-drop action, so
  readiness reports can correlate a real semantic site such as `items.element` with the internal cleanup plan while
  `DynamicArray<T>` signature lowering remains disabled.
- Semantic analysis now also records metadata-only dynamic-array descriptor origins for owned source bindings. These
  origins capture the source owner, full `DynamicArray<T>` type, element type, and declaration line so future lowering
  can connect a real binding descriptor to cleanup planning without relying on synthetic construction requests.
- Lowering now has a separate disabled descriptor-cleanup plan derived from semantic descriptor origins. This preserves
  the distinction between source-owned descriptor cleanup and allocation/construction planning while feeding the same
  metadata-only element-drop readiness path under test control.
- Descriptor-cleanup plans also record the expected local descriptor storage name, such as `%items.addr`, so future
  cleanup lowering can attach to the real addressable descriptor. This is still metadata-only and does not emit the
  local descriptor allocation or cleanup IR.
- A disabled descriptor-load cleanup renderer now composes a load from the expected descriptor storage, descriptor
  field projections, initialized-element drop walk placeholder, and deallocation call shape. The rendered snippet stays
  outside module IR until real descriptor storage and cleanup emission are authorized.
- Descriptor-origin cleanup readiness now separates container deallocation from element drops. Scalar element
  descriptors such as `DynamicArray<UInt32>` can report an authorized deallocation-only cleanup obligation with zero
  planned element-drop actions, while owned element descriptors still require semantic drop authorization.
- Descriptor-origin cleanup plans now label descriptor storage as predicted owner-local storage, such as `%items.addr`,
  rather than claiming a proven lowered binding. This keeps the metadata honest until real `DynamicArray<T>` descriptor
  ABI/storage lowering is enabled.
- A test-only dynamic-array parameter descriptor seam can lower `DynamicArray<T>` parameters as `{ ptr, i64, i64 }`,
  spill them to `%name.addr`, and mark matching semantic descriptor-origin cleanup plans as bound. The default
  production path still rejects `DynamicArray<T>` lowered function signatures.
- A second test-only seam can emit bound scalar/non-owning dynamic-array parameter cleanup at normal function returns:
  it loads the descriptor from `%name.addr` and calls `__orison_dynamic_array_deallocate` before `ret`. Owned element
  cleanup emission remains disabled until element drop authorization and sequencing are complete.
- The same test-only cleanup seam can now emit owned-element drop walking for bound `DynamicArray<T>` parameters when
  semantic drop lowering authorization is present. The generated loop calls the authorized element drop ABI for each
  initialized element before descriptor deallocation; without authorization, owned-element cleanup emission remains
  suppressed.
- Pipeline test-only semantic drop authorizations take precedence over automatically derived blocked authorizations so
  internal lowering seams can model the positive authorized path without requiring production source drop syntax first.
- Bound dynamic-array parameter cleanup is now pinned across normal returns, explicit `Unit` returns, and guard-failure
  returns under the test-only cleanup emission seam. This keeps all covered function-exit paths routed through the same
  descriptor cleanup hook before any production dynamic-array signature support is enabled.
- Branch-local return coverage now pins `defer` replay before bound dynamic-array descriptor cleanup in non-final `if`
  arms and `switch` cases. The descriptor deallocation remains test-only, but ordering now matches the existing
  function-exit cleanup model.
- Loop-control coverage now pins that `break` and `continue` replay branch-local `defer` blocks without cleaning bound
  dynamic-array parameters. Descriptor cleanup remains tied to the later function-exit path because the parameter is
  still live after loop control transfers.
- Authorized owned-element cleanup coverage now pins early-return paths for bound `DynamicArray<Payload>` parameters:
  guard failures and non-final `if` arm returns emit the element drop walk before descriptor deallocation on each
  function-exit branch, with branch-local `defer` replay still ordered before container cleanup.
- Authorized owned-element cleanup coverage now also pins explicit `Unit` returns and `switch` case returns for bound
  `DynamicArray<Payload>` parameters. Each covered return emits the element drop walk before descriptor deallocation,
  and branch-local `switch` defers still replay before owned-container cleanup.
- Bound dynamic-array parameter cleanup planning is now isolated from function emission in a dedicated lowering module.
  Function returns delegate to that planner after concurrency and `defer` cleanup, preserving the existing test-only
  sequencing while giving production enablement a single audited attachment point.
- Direct planner smoke coverage now pins deterministic owner-name ordering for multiple bound dynamic-array parameters,
  suppression of unauthorized owned-element cleanup, and positive owned-element cleanup authorization before descriptor
  deallocation.
- Semantic descriptor-origin cleanup now produces explicit dynamic-array cleanup obligation records before those records
  are converted into the generic drop-readiness model. This keeps production-facing cleanup obligations inspectable
  while actual `DynamicArray<T>` signature lowering remains disabled by default.
- Dynamic-array cleanup obligations now preserve the semantic descriptor-origin line and propagate it to planned
  element-drop actions. Pipeline reports can correlate blocked dynamic-array element cleanup back to the source owner
  and declaration line without enabling production cleanup emission.
- Dynamic-array cleanup obligations now also produce production-disabled sequencing plans. The sequence explicitly
  orders descriptor load, optional initialized-element drop walking, and descriptor deallocation while remaining
  metadata-only until allocation, construction, and ownership invariants are production-enabled.
- Dynamic-array cleanup sequence plans now have a disabled verifier that checks descriptor load first, optional
  initialized-element drop ordering, and descriptor deallocation last. Verification reports remain metadata-only and do
  not enable production cleanup emission.
- The test-only bound dynamic-array parameter cleanup renderer now consumes the sequence verifier before emitting any
  cleanup IR. Malformed sequence metadata blocks rendering before partial cleanup output or temporary allocation can
  occur.
- Lowering and pipeline results now expose a metadata-only dynamic-array cleanup emission-gate report derived from the
  sequence verifier, so allowed or blocked cleanup rendering is visible without converting verifier failures into
  source diagnostics or enabling production cleanup emission.
- Bound dynamic-array parameter cleanup rendering now requires a single internal cleanup-emission capability object.
  The capability proves test-only enablement, bound descriptor storage, verified sequence metadata, element cleanup
  authorization or non-requirement, and descriptor deallocation authorization before any cleanup IR is written.
- Lowering and pipeline results now expose a metadata-only cleanup-emission capability report that lists each invariant
  as present or missing. This keeps blocked cleanup emission explainable while preserving the production-disabled
  boundary.
- Function cleanup emission and LLVM/pipeline capability reporting now use the same cleanup-emission capability proof
  helper, preventing the renderer gate and report from drifting as dynamic-array cleanup moves toward production
  readiness.

## Follow-up work

- Implement the dynamic-array runtime entry points, construction lowering, and element cleanup lowering.
- Enable `DynamicArray<T>` lowered signatures only after semantic ownership/drop analysis proves unique ownership,
  initialized length, capacity bounds, and deterministic cleanup.
- Extend `for ... in` lowering to consume dynamic sequence descriptors after descriptor ABI support is in place.
