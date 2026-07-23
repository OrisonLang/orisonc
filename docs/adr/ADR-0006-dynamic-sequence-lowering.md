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
- Dynamic-array runtime ABI entry points are finite and internal: conceptual descriptor-returning allocation and growth
  map to concrete LLVM C ABI calls using `sret({ ptr, i64, i64 })`, grow receives the prior descriptor through
  `byval({ ptr, i64, i64 })`, and deallocation remains `__orison_dynamic_array_deallocate(ptr, i64, i64)`. The `i64`
  parameters are element size/capacity-style scalar values chosen by lowering, not source-level variadic or spread
  arguments.
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
- A disabled grow-call renderer pins the finite runtime call shape for capacity-failure handling as a C-compatible
  `sret` result plus `byval` prior descriptor. A disabled grow sequence currently doubles capacity, calls grow with the
  element size and next capacity, loads the returned descriptor, and writes it back to the local slot. The snippet
  remains outside module IR until dynamic-array growth semantics are authorized.
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
- Branch joins for owned dynamic-array transfer now require every continuing branch/case to agree on the consumed
  binding set before cleanup planning observes the post-branch state. Matching moves across all continuing paths are
  accepted; move/no-move mismatches are rejected rather than choosing an unsafe unconditional cleanup policy.
- Consumed owned binding tracking has been extracted into a reusable lowering ownership-transfer state. Dynamic arrays
  remain the first integrated cleanup consumer, but the branch-join and post-move diagnostic machinery no longer depends
  on a dynamic-array-specific function-state field.
- The reusable ownership-transfer state now also derives stable owned aggregate member keys for record fields and choice
  payloads. Record-field reads consume those keys for use-after-move diagnostics before aggregate IR is emitted; source
  syntax that produces aggregate-field moves remains separate future work.
- Direct record-field call arguments now produce those member keys when the callee parameter expects the field's owned
  source type. This gives `box.payload` transfers a source-backed producer while keeping choice-payload production and
  index-containing aggregate-field transfer as follow-up work.
- Record-field transfer production and diagnostics now cover nested member-only paths such as `box.inner.payload`.
  Paths that include indexing remain excluded until element-level ownership transfer has dedicated safety rules.
- Choice constructor-pattern payload binding now produces owned payload transfer keys for named choice subjects, such as
  `maybe.Some.value`, without changing switch pattern syntax, planning, or exhaustiveness behavior.
- Whole-binding expression reads now check consumed owned descendant keys. Reusing a choice after an owned payload
  destructure is rejected with the precise consumed payload key, while preserving the existing branch-join set model.
- Driver-level source coverage now pins that post-destructure failure for a named choice payload carrying a lowerable
  record: `Loaded(payload)` consumption rejects later `holder` reuse with `holder.Loaded.payload`.
- Contextual function-signature coverage now pins module-local choice parameters with aggregate payload ABI, so
  `Holder` can be passed as a parameter using the same `{ i32, %record.Payload }` representation as locals and returns.
  Positive driver coverage passes `Holder` to `classify(holder)` where the callee switches over `Loaded(payload)`.
- Final `switch` ownership joins normalize owned choice-payload destructure keys across continuing cases for the
  switched subject. This keeps post-switch reuse conservative without rejecting valid variant-specific payload matches.
- Direct control-flow smoke coverage now verifies that post-switch reads of the original choice subject are still
  rejected with the precise consumed payload key.
- Non-value `switch` ownership joins now normalize consumed descendants of the named switched subject, including owned
  choice-payload destructure keys, and driver coverage pins post-switch source reuse as the same precise diagnostic.
- Ownership-transfer smoke coverage pins the normalization below the CLI path by merging a consumed
  `holder.Loaded.payload` case with an otherwise empty continuing case.
- Final and non-value `switch` joins now share the ownership-transfer descendant normalization helper instead of
  carrying parallel switch-specific implementations.
- Ownership-join mismatch diagnostics now say `owned transfers must match` instead of naming only `DynamicArray`,
  matching the broader aggregate-descendant transfer model.
- Driver aggregate-field coverage now pins that generalized `if branch ownership mismatch` diagnostic for a
  non-`DynamicArray` record-field transfer mismatch across continuing branches.
- Driver aggregate-field coverage now also pins the generalized `switch case ownership mismatch` diagnostic for a
  non-`DynamicArray` record-field transfer mismatch across continuing cases.
- Direct control-flow smoke coverage now pins the same aggregate-descendant `if` and `switch` ownership mismatch
  diagnostics below the CLI layer.
- Direct control-flow smoke coverage now also pins balanced aggregate-descendant `if` and `switch` ownership joins
  below the CLI layer.
- Direct control-flow smoke coverage now pins post-merge whole-owner reuse rejection after balanced
  aggregate-descendant `if` and `switch` transfers.
- Driver aggregate-field coverage now pins the same post-merge whole-owner reuse rejection after balanced
  aggregate-descendant `if` and `switch` transfers through the CLI lowering path.
- Driver aggregate-field coverage now pins nested aggregate-descendant post-merge reuse rejection for
  `nested.box.payload` after balanced `if` and `switch` transfers.
- Driver aggregate-field coverage now pins nested aggregate-descendant `if` and `switch` ownership mismatch
  diagnostics for `nested.box.payload`.
- Driver aggregate-field coverage now pins scalar-terminal nested member call success for `nested.box.count`, proving
  scalar field arguments do not trigger ownership-transfer diagnostics.
- Driver aggregate-field coverage now pins scalar-terminal nested member failure paths for `nested.box.count` and
  `nested.box.count.payload`, proving invalid scalar paths stay out of ownership-transfer diagnostics at the CLI
  lowering boundary.
- Semantic analysis now rejects scalar member paths such as `total.status` and `nested.box.count.payload` with
  source-level `type 'UInt32' has no member 'status'` and `type 'UInt32' has no member 'payload'` diagnostics before
  lowering.
- Semantic and CLI coverage now reject known record receivers with unknown fields, such as `header.missing`, while
  unresolved placeholder receiver types continue to avoid cascade diagnostics.
- Semantic and CLI coverage now pin the same unknown-field diagnostic for concrete generic record receivers such as
  `Box<UInt32>` through `box.missing`.
- Semantic and CLI coverage now reject unknown fields on pointer-backed declared-record receivers such as
  `Pointer<Registers>` through `regs.missing`, while preserving valid pointer aggregate field access.
- Semantic type inference now resolves pointer-backed record fields before validating chained member access, so
  `regs.status.missing` reports `type 'UInt32' has no member 'missing'` before lowering.
- Direct control-flow smoke coverage now pins nested aggregate-descendant mismatch, balanced join, and post-merge reuse
  diagnostics for `nested.box.payload` below the CLI layer.
- Direct control-flow aggregate ownership smoke coverage now uses shared helpers for seeded aggregate states and
  post-merge reuse diagnostics, reducing fixture drift risk.
- Driver aggregate-field ownership smoke coverage now uses shared source-line helpers for `box.payload` and
  `nested.box.payload` mismatch/reuse fixtures, reducing CLI fixture drift risk.
- Ownership-transfer smoke coverage now pins nested record-member transfer rejection for missing fields, scalar
  terminal fields, and paths that attempt to continue through a scalar field.
- Call-emitter smoke coverage now pins the same nested record-member call-argument transfer boundaries, including
  scalar terminal success without ownership consumption.
- Non-generic single-payload choices now accept any lowerable single LLVM payload type, including record payloads.
  Multi-payload variants and generic choice ABI lowering remain unsupported.
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
- Pipeline smoke coverage now pins the blocked capability-report path for requested owned-element dynamic-array cleanup
  without semantic drop authorization: the report identifies missing element cleanup and no owned-element drop call is
  emitted.
- The driver now exposes report-only `--semantic-dynamic-array-descriptor-origins <file>` output so source-derived
  dynamic-array descriptor origins can be inspected before the lowering cleanup-plan reports consume them.
- The driver now exposes report-only `--dynamic-array-descriptor-cleanup-plan <file>`,
  `--dynamic-array-cleanup-obligations <file>`, and `--dynamic-array-cleanup-sequence-plan <file>` surfaces so the
  metadata chain leading into cleanup verification is inspectable from the CLI.
- The driver now exposes report-only `--dynamic-array-cleanup-sequence-verification <file>` and
  `--dynamic-array-cleanup-emission-gate <file>` surfaces that print the raw sequence verifier and verifier-derived
  emission gate for the same internal metadata seams used by cleanup capability reporting.
- The driver now exposes a report-only `--dynamic-array-cleanup-capability <file>` surface that runs the same internal
  metadata seams and prints the cleanup-emission capability report without exposing new source syntax or production
  cleanup behavior.
- The driver now exposes report-only `--dynamic-array-cleanup-audit <file>` output that concatenates semantic
  descriptor origins, descriptor cleanup plans, cleanup obligations, sequence plans, verification, emission gate, and
  capability proof in that order for single-command inspection.
- `tests/fixtures/dynamic_array_cleanup_audit.or` is the checked-in positive audit fixture for this report chain. It
  keeps the CLI demo stable while production `DynamicArray<T>` cleanup remains disabled by default.
- The pipeline now exposes a single dynamic-array cleanup production-readiness predicate/report. It can see the
  metadata chain and cleanup capability proof, but remains blocked until production signature lowering, construction
  lowering, and cleanup emission gates are explicitly enabled.
- Those production-readiness blockers are explicit pipeline options, all defaulting to disabled. Tests can flip them
  independently to audit readiness transitions without changing the default production behavior.
- The production signature blocker now maps to a default-disabled lowering option that enables dynamic-array parameter
  descriptors as `{ ptr, i64, i64 }`. The older test-only descriptor seam remains as a compatibility alias for existing
  smoke fixtures, and construction/cleanup production gates remain separate.
- The production construction blocker now maps to a default-disabled lowering option that renders finite allocation
  calls from internal construction plans. Until source construction placement is implemented, these calls remain
  observable artifacts outside module IR; runtime declarations and construction reports are still emitted from the
  requested operations.
- Production construction discovery can now derive plans from existing source syntax: an annotated local
  `DynamicArray<T>` binding initialized with `DynamicArray()`. The plan records the source owner and an initial
  capacity of zero; allocation calls still remain outside module IR until dynamic-array local storage placement is
  implemented.
- Production-gated local descriptor placement now lowers that default construction form into function IR as an
  allocation call followed by local descriptor storage. Dynamic-array indexing, growth, append, and local cleanup remain
  separate disabled work.
- Production-gated local cleanup now records constructed local descriptors as real lowered storage and emits descriptor
  load plus backing-storage deallocation before function returns. Dynamic-array indexing, growth, append, and
  unauthorized owned-element cleanup remain separate disabled work.
- Production-gated local index reads now lower `items[index]` for constructed local `DynamicArray<T>` descriptors by
  loading the descriptor, projecting length/data, emitting an unsigned `index < length` bounds predicate, computing the
  element address, and loading the scalar element. Bounds-failure branching/trapping remains separate disabled work.
- Local dynamic-array index reads now consume that bounds predicate as control flow: in-bounds execution continues to
  the data projection, address calculation, and scalar load, while out-of-bounds execution calls the finite
  `__orison_dynamic_array_bounds_failed()` runtime trap before `unreachable`.
- Production-gated no-growth append now lowers `items.push(value)` for mutable local `DynamicArray<T>` descriptors by
  checking `length < capacity`, trapping through `__orison_dynamic_array_capacity_failed()` on capacity failure,
  storing the element at the current length, incrementing length, and writing the descriptor back. Growth remains a
  separate disabled step.
- The C++ runtime now implements the finite internal dynamic-array ABI. Allocation returns `{data, 0, capacity}`,
  zero-capacity allocation returns `{null, 0, 0}`, grow copies exactly initialized bytes and preserves length, and
  deallocation releases the backing storage. LLVM emission uses C-compatible `sret`/`byval` call shapes for descriptor
  values, making emitted dynamic-array runtime calls linkable without adding source syntax.
- Production-gated append now grows instead of trapping on full capacity. The append branch grows zero-capacity
  descriptors to capacity `1`, doubles nonzero capacity, joins on the active descriptor, stores the element, increments
  length, and writes the descriptor back. The capacity-failure trap remains available for future impossible-capacity
  diagnostics but is no longer requested by ordinary append lowering.
- Source-level append followed by index read is now pinned end-to-end: `items.push(value)` writes the updated
  descriptor back, a later `items[index]` reloads that descriptor from local storage, and pipeline smoke verifies the
  generated object links/runs against the runtime and returns the appended scalar value.
- The production cleanup-emission blocker now maps to a default-disabled lowering option that can prove and emit bound
  dynamic-array parameter cleanup without relying on the older test-only cleanup flag. The test-only flag remains as a
  compatibility alias for existing focused fixtures.
- Dynamic-array cleanup report surfaces opt into the explicit source-derived Drop lowering gate, allowing proven
  `implements Drop for T` fixtures to demonstrate authorized owned-element cleanup capability while default compilation
  continues to leave source Drop lowering disabled until requested by a compiler/pipeline option.
- Production-gated `DynamicArray<T>.length()` now resolves semantically to `IntSize` and lowers by reloading the local
  descriptor and projecting its length field. This is an internal descriptor read, not a runtime call; scalar lowering
  now maps `IntSize`/`UIntSize` to pointer-width `i64` in the current target model, and pipeline smoke verifies append
  followed by `length()` links/runs against the runtime and returns the initialized element count.
- Production-gated `for item in items` now lowers for named local `DynamicArray<T>` descriptors. The loop loads the
  descriptor once, projects data and length, emits a runtime `index < length` loop, loads each initialized element into
  the loop binding, and reuses the existing `break`/`continue` targets. Dynamic-array parameter, view, and computed
  iterable lowering remain separate work.
- Local owned-element dynamic-array cleanup is now pinned with source-level coverage: with an authorized `Drop`
  implementation for the element type, local descriptor cleanup emits an initialized-element drop walk before backing
  storage deallocation. Without element cleanup authorization, owned-element local cleanup remains blocked rather than
  silently deallocating initialized owned elements.
- Production-gated dynamic-array parameter descriptor lowering is now limited to scalar or non-owning element types.
  `DynamicArray<UInt32>` parameters lower to `{ ptr, i64, i64 }` and can emit descriptor cleanup under the production
  signature/cleanup gates, while owned-element parameters such as `DynamicArray<Payload>` remain rejected unless the
  explicit test-only descriptor seam is used for internal cleanup-readiness coverage.
- Production-gated scalar/non-owning dynamic-array parameter reads are now pinned: descriptor parameters are spilled to
  `%name.addr`, `.length()` reloads and projects the length field, and `items[index]` emits the same finite
  bounds-check/data-projection/element-load sequence used by local descriptors before descriptor cleanup. Runtime
  prelude collection now detects parameter-only dynamic-array index reads so `__orison_dynamic_array_bounds_failed()`
  is declared even when no source construction plan exists.
- Production-gated scalar/non-owning dynamic-array parameter iteration is now pinned through the same descriptor spill
  path: `for item in items` over a `DynamicArray<UInt32>` parameter reloads `%items.addr`, projects data/length, emits
  the runtime `index < length` loop, loads each scalar element into the loop binding, and delays descriptor cleanup
  until the function-exit cleanup hook.
- View read-only descriptor parity is now pinned for parameter descriptors. `View<T>`/`shared.View<T>`/`exclusive.View<T>`
  lower as `{ ptr, i64 }`; `.length()` projects the length field, `items[index]` emits a finite bounds check and traps
  through `__orison_dynamic_array_bounds_failed()` on failure, and `for item in items` reuses the descriptor-loop
  lowering without ownership cleanup or capacity handling.
- Access-qualified View descriptor parity is covered explicitly in smoke tests: `shared.View<T>` and
  `exclusive.View<T>` use the same read-only descriptor ABI for length, checked index reads, and descriptor-loop
  iteration. Exclusive View mutation remains future work rather than an implicit side effect of read support.
- View descriptor-loop lowering is now available on the default compile path for named `View<T>`, `shared.View<T>`,
  and `exclusive.View<T>` iterables.
- Shared descriptor-loop lowering now emits neutral `sequence_for` temporary names in generated LLVM IR. The remaining
  DynamicArray-specific option names are intentionally gate-oriented rather than loop-shape-oriented.
- Local `DynamicArray<T>` lowering is now available on the default compile path for constructed local descriptors:
  default construction, append with growth, checked scalar index reads, length reads, descriptor `for` loops, and local
  descriptor cleanup are enabled together.
- Scalar/non-owning `DynamicArray<T>` parameter lowering is now available on the default compile path: descriptor
  signatures, parameter cleanup, `.length()`, checked index reads, and descriptor `for` loops move together. Owned
  element parameters such as `DynamicArray<Payload>` remain rejected on the production path until ownership/drop proof
  is complete; the test-only descriptor seam remains available for internal cleanup-readiness coverage.
- `examples/dynamic_array_parameter_reads.or` is the checked-in scalar parameter descriptor demo. It is covered by
  examples and canonical pipeline smoke tests, while `tests/fixtures/dynamic_array_owned_parameter_rejected.or` pins
  the production rejection boundary for owned-element parameters.
- Owned-element `DynamicArray<T>` parameter rejection now reports the specific parameter name and element type that
  needs ownership/drop proof, rather than falling through to the generic unsupported-parameter diagnostic.
- Production dynamic-array parameter descriptor lowering can now consume positive semantic drop authorization for an
  owned element site named `owner.element`. Scalar/non-owning parameters still lower directly; owned-element parameters
  still reject by default unless the semantic/source-drop proof path authorizes the element cleanup ABI.
- Dynamic-array cleanup report paths now use production-facing descriptor cleanup planning and cleanup-emission gates.
  They no longer need the parameter-descriptor signature bypass to inspect missing element cleanup proof.
- Dynamic-array cleanup report paths no longer require the parameter descriptor signature bypass. Report emission can
  stop after metadata collection, and parameter-origin descriptor cleanup plans can be marked `audit` to prove report
  storage inspectability without claiming a lowered function ABI.
- Dynamic-array cleanup report commands now call a dedicated pipeline metadata collection path, backed by
  `LlvmIrEmitter::emit_metadata`, instead of passing a metadata-only flag through normal LLVM emission options.

## Follow-up work

- Extend production `DynamicArray<T>` lowered signatures to owned element types only after semantic ownership/drop
  analysis proves unique ownership, initialized length, capacity bounds, and deterministic cleanup.
- Extend `for ... in` lowering beyond named descriptor-backed dynamic sequences to consume computed dynamic iterables.
