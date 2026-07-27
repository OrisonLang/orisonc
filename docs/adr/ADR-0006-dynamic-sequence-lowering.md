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
- Dynamic-array `for` lowering consumes an internal descriptor-ownership plan before emission. Named bindings with
  bound descriptor storage are the only lowerable owned iterable shape today; computed owned iterables, including
  ternaries and helper-returned descriptors, remain blocked until lowering can prove a single cleanup owner.
- The descriptor-ownership plan now records cleanup-owner proof separately from descriptor readability. Lowered local
  descriptors and bound parameter descriptors count as proven cleanup owners; predicted semantic origins and
  audit-only parameter descriptors remain metadata-only and do not prove computed owned iterable safety.
- Dynamic-array `for` rejection diagnostics surface the descriptor-ownership plan report, so computed owned iterable
  failures explain both the need for a proven single descriptor owner and the current cleanup-owner proof status.
- Computed owned dynamic-array iterable planning now has a metadata-only ternary ownership-transfer model. Each arm is
  modeled as consuming its branch owner and then uses the existing ownership-transfer merge rule; differing owners
  remain blocked, while same-owner ternaries can distinguish proven cleanup owners from metadata-only predicted owners.
- Dynamic-array `for` rejection diagnostics include the computed ownership-transfer report for computed owned iterables,
  so ternary failures distinguish branch owner mismatches from missing cleanup-owner proof.
- Rejection coverage now pins same-owner computed ternaries separately from owner-mismatch ternaries: `flag ? items :
  items` can satisfy the ownership join but remains blocked until cleanup-owner proof is available.
- Local same-owner computed ternary coverage now pins the positive metadata-only proof state: lowered local descriptors
  can prove the cleanup owner, but computed owned `for` lowering still remains disabled until descriptor handoff and
  cleanup sequencing are implemented.
- Computed owned dynamic-array descriptor handoff now has a metadata-only internal plan. It reuses the computed
  ownership-transfer proof, records the single source owner, handoff owner, and descriptor storage when available, and
  keeps lowering explicitly disabled until cleanup sequencing is implemented.
- Dynamic-array `for` rejection diagnostics now include the computed descriptor-handoff report, so proven same-owner
  computed iterables can show the future handoff owner and descriptor storage while still reporting lowering disabled.
- Computed owned dynamic-array cleanup sequencing now has a metadata-only internal plan. It records loop-entry cleanup
  ownership, loop-exit ownership resumption, and descriptor storage for proven handoffs, while keeping cleanup sequence
  emission disabled.
- Dynamic-array `for` rejection diagnostics now include the computed cleanup-sequence report, so computed owned
  iterable failures show whether loop cleanup ownership is blocked, unproven, or planned-but-disabled.
- Computed owned dynamic-array descriptor rendering now has a disabled internal plan for proven handoffs. It records
  the descriptor load plus data and length projections needed by a future loop emitter while keeping render emission
  disabled.
- Dynamic-array `for` rejection diagnostics now include the computed descriptor-render report, so computed owned
  iterable failures show whether descriptor load/projection rendering is blocked, unproven, or planned-but-disabled.
- Computed owned dynamic-array loop-control rendering now has a disabled internal plan for proven descriptor renders.
  It records the entry branch, condition/body/continue/exit blocks, index phi, bounds check, and conditional branch
  while keeping render emission disabled.
- Dynamic-array `for` rejection diagnostics now include the computed loop-control render report, so computed owned
  iterable failures show whether loop branch/bounds control is blocked, unproven, or planned-but-disabled.
- Computed owned dynamic-array element-address rendering now has a disabled internal plan for proven loop-control
  renders. It records the element LLVM type, descriptor data pointer, loop index, and `getelementptr` element address
  while keeping render emission disabled.
- Dynamic-array `for` rejection diagnostics now include the computed element-address render report, so computed owned
  iterable failures show whether element address rendering is blocked, unproven, unlowerable, or planned-but-disabled.
- Computed owned dynamic-array element-load rendering now has a disabled internal plan for proven element addresses.
  It records the element address, item value name, and scalar `load` needed by a future loop item binding while keeping
  render emission disabled.
- Dynamic-array `for` rejection diagnostics now include the computed element-load render report, so computed owned
  iterable failures show whether loop item loading is blocked, unproven, unlowerable, or planned-but-disabled.
- Computed owned dynamic-array loop-continue rendering now has a disabled internal plan for proven element loads. It
  records the continue block, next-index increment, and condition backedge needed by a future computed owned loop while
  keeping render emission disabled.
- Dynamic-array `for` rejection diagnostics now include the computed loop-continue render report, so computed owned
  iterable failures show whether loop continuation is blocked, unproven, unlowerable, or planned-but-disabled.
- Computed owned dynamic-array loop rendering now has a composed disabled internal sequence plan. It aggregates the
  descriptor render, loop-control render, body block label, element address, element load, and loop continuation
  snippets in order while keeping render emission disabled.
- Dynamic-array `for` rejection diagnostics now include the computed loop render sequence report, so computed owned
  iterable failures expose the full disabled render-sequence readiness state.
- Computed owned dynamic-array loop exit cleanup now has a disabled internal plan. It records the planned exit block
  and function cleanup ownership resumption after the computed loop while keeping cleanup sequence emission disabled.
- Dynamic-array `for` rejection diagnostics now include the computed loop-exit cleanup report, so computed owned
  iterable failures expose exit-block and cleanup-resumption readiness without enabling cleanup emission.
- Computed owned dynamic-array loop lowering now has a disabled production emission gate. The gate consumes ownership,
  composed loop rendering, and loop-exit cleanup readiness, then still records production emission as disabled.
- Dynamic-array `for` rejection diagnostics now include the computed production-emission gate report, so computed owned
  iterable failures show whether the production gate is blocked, ready-but-disabled, or still missing prerequisites.
- The disabled production-emission gate now exposes the aggregated computed loop render and exit-cleanup snippets for
  ready gates as an internal handoff seam while still keeping those snippets out of module IR.
- LLVM emission now has a test-only metadata collector that discovers ready computed owned dynamic-array `for` gates
  from parsed function bodies and records their aggregated snippets on the emission result without emitting module IR.
- Computed dynamic-array `for` production-sequence metadata now preserves per-gate provenance: enclosing function,
  source line, cleanup owner, source type, element type, and the aggregated snippets.
- Computed dynamic-array `for` production-sequence metadata now has a report formatter so pipeline and driver audit
  surfaces can consume provenance without inspecting raw metadata vectors.
- Pipeline lowering-emission reports now carry computed dynamic-array `for` production-sequence reports, including
  intentionally rejected lowering attempts and the existing dynamic-array audit bundle.
- A test-only module-IR comment emission seam can project ready computed dynamic-array `for` production-sequence
  snippets into metadata IR text for audit consumers. The snippets remain comments, not executable control flow, until
  function CFG insertion and ownership cleanup emission are proven together.
- A test-only computed dynamic-array `for` lowering option can now insert the proven local same-owner loop sequence at
  the entry-block statement position and lower the source loop body between the planned body and continuation blocks.
  The path remains internal and disabled by default while nested insertion and broader cleanup proof are still pending.
- The test-only computed dynamic-array `for` lowering path now records the actual incoming block for loop-control phi
  construction, so insertion after a lowered branch merge no longer assumes `%entry`.
- Executable test-only computed dynamic-array `for` lowering now allocates a unique internal suffix from the function
  block-index stream for emitted labels and SSA names, preventing collisions when the same owner participates in
  multiple computed loops. Metadata-only audit plans retain their stable unsuffixed names.
- Computed dynamic-array `for` production-emission gates now expose explicit loop cleanup-ownership and function
  cleanup-resumption readiness flags. The executable test-only insertion path requires both flags in addition to
  ownership, render, exit-cleanup, and production-sequence readiness.
- Computed dynamic-array loop-exit cleanup now renders a named disabled cleanup-resumption operation that records the
  transfer from loop cleanup ownership back to the function cleanup owner. This replaces the generic placeholder
  resumption comment while still keeping production cleanup emission disabled.
- Computed dynamic-array cleanup sequencing now also names the matching disabled loop-entry cleanup-acquisition
  operation, and production emission gates require that acquisition artifact before reporting loop cleanup ownership
  ready.
- Computed dynamic-array `for` cleanup-transition metadata now reports the loop-entry cleanup acquisition and loop-exit
  function cleanup resumption as a paired audit surface, so consumers can verify both sides of the ownership transition
  without correlating separate sequence and exit-cleanup reports.
- Test-only computed dynamic-array `for` lowering now checks that the paired cleanup acquisition/resumption owners match
  before inserting the loop, and emits the disabled cleanup-acquisition marker immediately before descriptor rendering.
- Pipeline emission reports now derive an inserted cleanup-transition audit line from the actual lowered IR markers,
  keeping executable test-only insertion observability separate from metadata-only cleanup-transition readiness.
- Computed dynamic-array cleanup acquisition and resumption markers now render through a shared internal cleanup state
  handoff model. The lowered IR still contains disabled audit markers only; no cleanup calls, deallocation, or element
  drops are emitted by this seam.
- Pipeline emission reports now verify inserted cleanup state handoff sequences. Valid acquire/resume pairs report as
  paired with cleanup calls disabled, while malformed inserted handoff sequences report blocked verifier lines.
- Computed dynamic-array cleanup-call emission now has an internal verifier-driven gate. The gate requires verified
  inserted cleanup state handoffs and remains blocked while those handoffs declare cleanup calls disabled.
- Verified computed dynamic-array cleanup-call gates now also surface a disabled cleanup-call plan. The plan records
  the post-resumption cleanup operation seam and remains blocked on descriptor cleanup operands before any real
  cleanup call, deallocation, or element drop can be emitted.
- The disabled computed dynamic-array cleanup-call plan initially derives available descriptor cleanup operands from
  inserted IR, proving data pointer and scalar element-size operands for lowered computed loops before capacity
  projection is added.
- Computed dynamic-array descriptor rendering now also projects capacity for proven computed loops. Cleanup-call plans
  can prove data pointer, scalar element size, and capacity operands while cleanup-call emission remains disabled.
- Computed dynamic-array cleanup-call rendering now formats the exact disabled
  `__orison_dynamic_array_deallocate(ptr, i64, i64)` call from proven operands. The rendered call remains report-only
  and is not inserted into module IR.
- Computed dynamic-array cleanup-call insertion now has an explicit gate. The gate requires verified inserted cleanup
  state, proven cleanup operands, and cleanup-call authorization before any rendered cleanup call can enter module IR.
- A test-only computed dynamic-array cleanup-call authorization option can mark inserted cleanup handoffs as enabled.
  This lets the insertion gate report ready while production cleanup-call insertion remains disabled.
- Computed dynamic-array `for` descriptor-render metadata is now collected and reported separately from the broader
  production sequence so descriptor load/projection readiness can be audited independently before full loop emission is
  enabled.
- Computed dynamic-array `for` loop-control-render metadata is now collected and reported separately from the broader
  production sequence so branch, phi, bounds-check, and loop block readiness can be audited independently before full
  loop emission is enabled.
- Computed dynamic-array `for` element-address-render metadata is now collected and reported separately from the
  broader production sequence so element type, data pointer, index, and `getelementptr` readiness can be audited
  independently before full loop emission is enabled.
- Computed dynamic-array `for` element-load-render metadata is now collected and reported separately from the broader
  production sequence so element address, item value, and scalar `load` readiness can be audited independently before
  full loop emission is enabled.
- Computed dynamic-array `for` loop-continue-render metadata is now collected and reported separately from the broader
  production sequence so continue-block, next-index, and condition-backedge readiness can be audited independently
  before full loop emission is enabled.
- Computed dynamic-array `for` loop-render-sequence metadata is now collected and reported separately from the broader
  production sequence so the composed descriptor, control, body, element, and continuation snippet bundle can be
  audited independently before exit cleanup and production emission are enabled.
- Computed dynamic-array `for` loop-exit-cleanup metadata is now collected and reported separately from the broader
  production sequence so exit-block and cleanup-resumption readiness can be audited independently before production
  emission is enabled.
- Computed dynamic-array `for` production-emission-gate metadata is now collected and reported separately from the
  broader production sequence so ownership, loop-render, exit-cleanup, and disabled-emission readiness can be audited
  independently before production emission is enabled.

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
- Semantic and CLI coverage now pin the same pointer-backed field inference for concrete generic records, so
  `Pointer<Box<UInt32>>.value.missing` resolves `value` to `UInt32` before reporting the missing member.
- Semantic and CLI coverage now reject unknown fields in null-safe member chains, so `user?.profile?.missing` reports
  `type 'Profile' has no member 'missing'` before lowering.
- Semantic and CLI coverage now reject scalar continuations in null-safe member chains, so
  `user?.profile?.rating?.missing` reports `type 'UInt32' has no member 'missing'` before lowering.
- Semantic and CLI coverage now reject null-safe access on non-`Maybe` receivers, so `profile?.rating` reports
  `null-safe access requires Maybe base: Profile` before lowering.
- Semantic and CLI coverage now pin concrete generic payload inference in null-safe chains, so
  `box?.value?.missing` through `Maybe<Box<UInt32>>` reports `type 'UInt32' has no member 'missing'` before lowering.
- Semantic and CLI coverage now unwrap concrete generic `Maybe` payloads for null-safe member-call lookup, so
  `box?.missing()` reports `type 'Box<UInt32>' has no method 'missing'` and `box?.value?.missing()` reports
  `type 'UInt32' has no method 'missing'` before lowering.
- Semantic and CLI coverage now pin argument checking for concrete generic null-safe member calls, so
  `box?.scale(true)` through `Maybe<Box<UInt32>>` reports
  `method argument 'delta' type 'Bool' does not match declared type 'UInt32'` before lowering.
- Semantic and CLI coverage now pin arity checking for concrete generic null-safe member calls, so `box?.scale()` and
  `box?.scale(1 as UInt32, 2 as UInt32)` report expected-versus-actual method argument counts before lowering.
- Semantic and CLI coverage now pin positive lowering for concrete generic null-safe member calls, so
  `box?.scale(5 as UInt32)` through `Maybe<Box<UInt32>>` emits the monomorphized `method.Box_UInt32_.scale` call and
  merges the result as `Maybe<UInt32>`.
- Semantic and CLI coverage now pin aggregate-return lowering for concrete generic null-safe member calls, so
  `box?.bump(5 as UInt32)` and `box?.pair(7 as UInt32)` wrap `Box<UInt32>` and `Array<Box<UInt32>, 2>` method results
  in `Maybe` merge IR.
- Semantic and CLI coverage now pin approved chained field access after concrete generic null-safe aggregate-return
  calls, so `box?.bump(5 as UInt32)?.value` lowers to `Maybe<UInt32>` without introducing any null-safe index syntax.
- Semantic and CLI coverage now reject ordinary indexing on concrete generic null-safe aggregate-return values before
  lowering, so `box?.pair(7 as UInt32)[0 as UInt64]` reports that `Maybe<Array<Box<UInt32>, 2>>` is not an indexable
  base.
- Source-type query coverage now pins the same concrete generic null-safe aggregate-return chain metadata:
  `box?.bump(5 as UInt32)` recovers `Maybe<Box<UInt32>>`, `box?.bump(5 as UInt32)?.value` recovers `Maybe<UInt32>`,
  `box?.pair(7 as UInt32)` recovers `Maybe<Array<Box<UInt32>, 2>>`, and direct indexing that `Maybe` result has no
  element source type.
- Null-safe plan and expression-emitter coverage now pin `box?.bump(5 as UInt32)?.value` below the CLI layer,
  including the `Box<UInt32>` field segment and monomorphized `method.Box_UInt32_.bump` call feeding a
  `Maybe<UInt32>` merge.
- Runnable CLI coverage now consumes `box?.bump(5 as UInt32)?.value` through explicit `switch` handling for both
  present and empty `Maybe<Box<UInt32>>` inputs, pinning the generic null-safe aggregate-return path through linked
  execution.
- `examples/local_null_safe_generic_aggregate.or` now exposes the same path as a checked-in canonical pipeline demo
  across LLVM emission, object emission, direct `run`, and retained `--build` execution.
- Semantic and CLI coverage now reject ordinary field access after a concrete generic null-safe aggregate-return call,
  so `box?.bump(5 as UInt32).value` reports that `Maybe<Box<UInt32>>` has no `value` member before lowering.
- Semantic and CLI coverage now reject ordinary method calls after a concrete generic null-safe aggregate-return call,
  so `box?.bump(5 as UInt32).scale(1 as UInt32)` reports that `Maybe<Box<UInt32>>` has no `scale` method before
  lowering.
- Semantic and CLI coverage now pin declared choice receiver diagnostics beyond `Maybe<T>`, so `Result<UInt32>.value`
  and `Result<UInt32>.scale(1 as UInt32)` report missing member/method diagnostics before lowering.
- `examples/local_result_choice_switch.or` now pins generic `Result<UInt32>` construction and explicit switch payload
  consumption as a checked-in frontend example while backend generic choice local lowering remains pending.
- Generic concrete choice instantiations with the existing finite single-payload ABI now lower for function returns,
  local constructors, and explicit switch payload consumption. `local_result_choice_switch.or` now emits and runs
  through the backend while `Maybe<T>` remains on its dedicated null-safe `{ i1, payload }` ABI path.
- Concrete generic choices whose single-payload variants use distinct finite scalar/fixed LLVM payload types now lower
  through an explicit `{ i32, [N x i8] }` tagged payload-buffer ABI. The checked-in
  `local_result_distinct_choice_switch.or` example pins `Result<UInt32, Bool>` construction and switch payload recovery
  through linked execution.
- Distinct concrete generic choice payload buffering now sizes record payload variants with full lowering context
  record layouts. `local_result_distinct_record_choice_switch.or` pins `Result<Payload, Flag>` construction and switch
  payload recovery through linked execution.
- Distinct concrete generic choice payload buffering now also covers fixed-array payload variants whose elements are
  records. `local_result_array_payload_choice_switch.or` pins `Result<Array<Payload, 2>, Payload>` construction and
  array payload recovery through linked execution.
- Concrete choice variants with multiple fixed payload fields now lower by aggregating each variant payload before
  inserting it into the outer tagged choice payload slot. Distinct variant payload shapes still use the same finite
  `{ i32, [N x i8] }` buffer ABI, and `local_result_multi_payload_choice_switch.or` pins constructor emission plus
  multi-binding switch recovery through linked execution.
- Expected-choice semantic validation now treats declared function calls returning the expected choice type as ordinary
  calls rather than choice constructors. `local_result_multi_payload_choice_function_flow.or` pins multi-payload choice
  values across function returns, parameter passing, call arguments, and downstream switch recovery.
- Final ternary and `switch` branch production now has checked-in backend coverage for the same multi-payload choice
  ABI. `local_result_multi_payload_choice_branch_flow.or` pins branch phi values, call passing, and downstream
  payload-pattern recovery for `Result<UInt32>`.
- Mutable local reassignment now has checked-in backend coverage for multi-payload choice values across `if` and
  `switch` branches. `local_result_multi_payload_choice_reassignment.or` pins stores back into a `Result<UInt32>`
  local before downstream payload-pattern recovery.
- Record fields containing multi-payload choice values now have checked-in backend coverage through construction,
  mutable field assignment, and field-access `switch` recovery. `local_result_multi_payload_choice_record_field.or`
  pins `Holder.result: Result<UInt32>` across record constructors, field stores, calls, and payload binding recovery.
- Fixed arrays containing multi-payload choice values now have checked-in backend coverage through array literals,
  indexed assignment, calls, and indexed-read payload recovery. `local_result_multi_payload_choice_array_element.or`
  pins `Array<Result<UInt32>, 2>` values using the finite tagged payload-buffer ABI.
- Nested record-field fixed arrays containing multi-payload choice values now have checked-in backend coverage through
  record construction, nested indexed assignment, calls, and indexed-read payload recovery.
  `local_result_multi_payload_choice_record_array.or` pins `Holder.results: Array<Result<UInt32>, 2>`.
- Fixed arrays of records containing multi-payload choice fields now have checked-in backend coverage through array
  literals, indexed record-field assignment, calls, and indexed field-read payload recovery.
  `local_result_multi_payload_choice_array_record.or` pins `Array<Holder, 2>` with `Holder.result: Result<UInt32>`.
- Nested fixed arrays of records containing multi-payload choice fields now have checked-in backend coverage through
  nested array literals, multi-level indexed record-field assignment, calls, and indexed field-read payload recovery.
  `local_result_multi_payload_choice_nested_array_record.or` pins `Array<Array<Holder, 2>, 2>`.
- Unsupported choice payload ABI diagnostics are now pinned for descriptor-backed payloads. The
  `choice_dynamic_array_payload_rejected.or` fixture confirms `Buffered.Ready(values: DynamicArray<UInt32>)` fails
  explicitly as a function return type because `DynamicArray<UInt32>` does not yet have a lowered choice ABI.
- Unsupported choice payload ABI diagnostics are now also pinned for function parameter boundaries. The
  `choice_dynamic_array_payload_parameter_rejected.or` fixture confirms `Buffered` parameters explain that
  `DynamicArray<UInt32>` does not yet have a lowered choice ABI.
- Unsupported choice payload ABI diagnostics are now also pinned for annotated local `let` and `var` bindings. The
  `choice_dynamic_array_payload_let_rejected.or` and `choice_dynamic_array_payload_var_rejected.or` fixtures confirm
  `Buffered` locals explain that `DynamicArray<UInt32>` does not yet have a lowered choice ABI.
- Unsupported choice payload ABI diagnostics now flow through a shared lowering diagnostic helper used by both function
  and statement emitters. Assignment/reassignment diagnostics do not have a separate fixture yet because unsupported
  choice ABI values are rejected at return, parameter, or local-binding boundaries before mutable storage can exist.
- Choice-constructor expression lowering now records a structured `unsupported choice ABI` failure when a matched choice
  layout lacks a usable payload ABI. This keeps future callers from collapsing descriptor-backed payload failures into
  generic expression rejections while the finite payload-buffer ABI remains incomplete for descriptor-backed payloads.
- Aggregate path expression lowering now records a structured `unsupported aggregate path` failure for member/index
  layout failures. Direct expression smoke coverage pins `address_of(device.missing)` as an `unknown field` aggregate
  path failure instead of a generic unsupported-expression detail.
- Aggregate assignment target diagnostics now use the same aggregate-path error renderer as expression lowering.
  Statement smoke coverage pins scalar member assignment as `expected record` and scalar index assignment as
  `expected array`, preserving the concrete path failure at assignment sites.
- CLI aggregate assignment diagnostics now explicitly pin the semantic/lowering boundary for scalar member/index
  assignment targets. Those source forms remain semantic errors, so the lowerer-only aggregate assignment path failures
  are intentionally covered in statement-emitter smoke rather than as user-facing CLI diagnostics.
- CLI and pipeline return-expression failure tests now pin structured expression details for unsupported operators. The
  driver, compile-pipeline, and drop-report smoke paths require `unsupported operator: <`, preventing regressions that
  keep only the generic return-expression prefix.
- CLI and pipeline return-expression failure tests now also pin unary operator details for unsupported unsigned
  negation. The driver, compile-pipeline, and drop-report smoke paths require `unsupported operator: -` so the
  structured lowerer reason survives the same user-visible failure boundaries as comparison operators.
- CLI and pipeline return-expression failure tests now pin structured cast details for negative unsigned literal casts.
  The driver, compile-pipeline, and drop-report smoke paths require
  `unsupported cast: negative value to UInt32`, preventing the cast-specific reason from being flattened at
  user-visible failure boundaries.
- CLI and pipeline final control-flow failure tests now pin structured arm/case details. The driver, compile-pipeline,
  and drop-report smoke paths require `if then arm lowering failed: unsupported operator: <` and
  `switch case lowering failed: unsupported operator: <`, preventing nested branch expression failures from being
  flattened at user-visible failure boundaries.
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
- Non-generic and concrete generic choices now accept any lowerable finite payload aggregate shape, including record,
  fixed-array, and fixed-arity multi-field variant payloads.
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
  iteration. Mutation is restricted to explicit indexed assignment through `exclusive.View<T>`.
- `exclusive.View<T>` parameter descriptors now lower checked indexed element assignment. The mutation path projects
  the `{ ptr, i64 }` descriptor data and length fields, emits the same runtime bounds failure branch as checked reads,
  computes the element address, and stores the scalar value without ownership cleanup or capacity handling.
- View descriptor-loop lowering is now available on the default compile path for named `View<T>`, `shared.View<T>`,
  and `exclusive.View<T>` iterables.
- Computed View descriptor-loop lowering is now available for lowerable `View<T>` expressions such as helper-returned
  `shared.View<T>`. The loop consumes the computed `{ ptr, i64 }` descriptor value directly, while owned
  `DynamicArray<T>` iteration remains restricted to named descriptor storage.
- Method-returned View descriptor-loop lowering uses the same computed descriptor path for lowerable receiver calls
  such as `seed.forward_view(values) -> shared.View<T>`, with no separate ownership or cleanup path.
- Member-derived receiver calls that return `shared.View<T>`, including record-field and indexed-array receiver paths,
  use that same computed descriptor path.
- Computed owned `DynamicArray<T>` iterables remain rejected unless the iterable is a named descriptor-backed owner.
  Ternary-selected owned descriptors such as `flag ? left : right` have no single proven descriptor storage owner yet.
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
- Computed owned `DynamicArray<T>` cleanup-call insertion now has a final test-only seam: the rendered runtime
  deallocation call is inserted only when computed lowering, cleanup-call authorization, and explicit insertion are all
  enabled together. Default and production-disabled paths continue to stop at audit-ready state without mutating module
  IR.
- The test-only computed cleanup-call insertion seam now also registers the dynamic-array deallocation runtime request
  before module prelude emission. The inserted call therefore receives the normal
  `__orison_dynamic_array_deallocate` declaration and is object-emission checked by pipeline smoke.
- Test-only computed cleanup insertion now clears the proven owner descriptor storage after deallocation, allowing the
  existing function-exit descriptor cleanup hook to remain active without double-freeing the same backing allocation.
  Pipeline smoke links and runs a non-empty same-owner computed `DynamicArray<UInt32>` loop through this path.
- Inserted computed cleanup now has an explicit consumed-descriptor audit report. The report is emitted only when the
  inserted deallocation call is present and followed by the owner descriptor finalization store, moving the proof model
  toward an explicit consumed-owner state instead of relying on an incidental IR pattern.
- Consumed computed cleanup descriptor proof now has a lowering-model report before IR inspection. The metadata report
  records the source owner, descriptor storage, and cleanup-resumption operation when the computed cleanup insertion
  gates are all enabled; the inserted-IR report remains a separate verification that the runtime deallocation call and
  descriptor finalization store were actually emitted.
- Dynamic-array descriptor finalization is now rendered through a named lowering helper instead of hand-built inline
  IR in the computed-loop lowerer. This keeps the current descriptor clear shape unchanged while giving future
  consumed-owner lowerers a reusable finalization emitter.
- Consumed descriptor finalization now has a small generic lowering plan. Computed DynamicArray cleanup metadata
  carries that plan rather than duplicating owner/storage/operation fields directly, preserving the current report text
  while making the consumed-owner proof reusable by future descriptor-owning lowerers.
- Computed DynamicArray cleanup insertion now also uses the generic consumed descriptor finalization plan before
  emitting the owner descriptor clear. Planning and emitted IR now share the same owner/storage/operation readiness
  gate, while the generated finalization IR remains unchanged.
- The generic consumed descriptor finalization plan is now surfaced as its own pipeline and CLI audit report before the
  DynamicArray-specific consumed-cleanup wrapper report. This gives future descriptor-owning lowerers a reusable audit
  line without depending on DynamicArray wording.
- Cleanup insertion tests now treat the generic consumed descriptor finalization report as the primary proof and keep
  the DynamicArray-specific consumed-cleanup reports as compatibility context. The generic report must be present
  before the DynamicArray wrapper reports in the checked proof chain.
- The DynamicArray consumed-cleanup model wrapper no longer repeats the generic finalization readiness flags. It now
  records the DynamicArray context and references the generic finalization proof, leaving owner-consumed/finalization
  readiness wording in the reusable generic report.

## Follow-up work

- Extend production `DynamicArray<T>` lowered signatures to owned element types only after semantic ownership/drop
  analysis proves unique ownership, initialized length, capacity bounds, and deterministic cleanup.
- Extend `for ... in` lowering beyond named descriptor-backed owned `DynamicArray<T>` sequences only after ownership,
  cleanup, and descriptor-storage rules for computed owned iterables are proven.
