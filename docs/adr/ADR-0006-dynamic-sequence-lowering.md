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
- LLVM emission now has a production audit metadata collector that discovers ready computed owned dynamic-array `for`
  gates from parsed function bodies and records their aggregated snippets on the emission result without emitting
  module IR.
- Computed dynamic-array `for` production-sequence metadata now preserves per-gate provenance: enclosing function,
  source line, cleanup owner, source type, element type, and the aggregated snippets.
- Computed dynamic-array `for` production-sequence metadata now has a report formatter so pipeline and driver audit
  surfaces can consume provenance without inspecting raw metadata vectors.
- Pipeline lowering-emission reports now carry computed dynamic-array `for` production-sequence reports, including
  intentionally rejected lowering attempts and the existing dynamic-array audit bundle.
- A debug/audit module-IR comment emission surface can project ready computed dynamic-array `for`
  production-sequence snippets into metadata IR text for audit consumers. The snippets remain comments, not
  executable control flow, until function CFG insertion and ownership cleanup emission are proven together.
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
- Computed local same-owner dynamic-array cleanup-call insertion now has a conservative last-use production gate. The
  gate requires cleanup emission, a lowered-local descriptor cleanup owner, verified inserted acquire/resume handoff,
  proven cleanup operands, and no later sibling statement that references the same owner before emitting the
  `__orison_dynamic_array_deallocate(ptr, i64, i64)` call and descriptor finalization. Multi-use owner blocks keep
  earlier loops on the disabled computed cleanup-call path and only enable cleanup insertion for the final proven use.
- Nested statement blocks now preserve their outer continuation when proving computed local same-owner last use. A
  computed loop inside an `if` remains cleanup-call disabled when the same owner is referenced after the `if`, while
  the later final loop can still emit deallocation and descriptor finalization.
- The same continuation proof covers switch-case bodies, and computed local cleanup insertion is conservatively blocked
  while lowering inside an active loop body to avoid deallocating an owner that a later iteration can still observe.
- Computed local cleanup insertion remains enabled after a loop has exited when the owner has no later references,
  preserving safe final-use deallocation/finalization for post-loop computed `DynamicArray<T>` iteration.
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
- Emitted dynamic-array cleanup audit report lines now carry function symbol context, and emitted consumed descriptor
  finalization plans record the same function symbol. Cleanup symbols remain function-local; audit output is
  disambiguated by context rather than by changing generated IR names.
- Dynamic-array cleanup emission capability proofs now retain the cleanup operation names they summarize. This keeps a
  capability line explainable for functions with multiple descriptor cleanups while preserving the existing aggregate
  proof gate.
- Dynamic-array cleanup emission capability proofs now also retain the descriptor owner names they summarize, so the
  aggregate proof can be correlated with both cleanup operations and source owners without changing the gate decision.
- Dynamic-array cleanup emission capability proofs now also format compact owner/operation pairs. The separate owner
  and operation lists remain available, but audit consumers no longer need positional correlation for the common case.
- Dynamic-array cleanup emission capability proofs now also retain owned-element drop pairs when element cleanup is
  required. Each pair identifies the cleanup owner, element capture, and authorized drop ABI symbol summarized by the
  aggregate capability gate.
- DynamicArray receiver `.push` lowering now rejects owned element appends unless semantic Drop lowering authorizes the
  element Drop ABI. Receiver bodies use type-level Drop proof so `this.push(value)` can lower for any proven
  `DynamicArray<T>` specialization while missing-Drop owned appends fail before LLVM IR validation.
- DynamicArray receiver indexed replacement now accepts exclusive receiver descriptor mutation through `this[index] =
  value`. Owned element replacement emits the old-element Drop call under the same receiver type-level Drop proof used
  by receiver append.
- DynamicArray receiver `for item in this` lowering is now pinned for shared receiver methods. Concrete scalar and
  owned-element receiver specializations reuse the named descriptor iteration path with `%this.addr` storage, while the
  receiver descriptor remains non-cleanup-owned by the method body.
- Shared DynamicArray receiver index reads now reject owned element value copies. Receiver methods should project a
  non-owning scalar/member value, such as `this[0].value`, until the language has an explicit safe ownership-preserving
  borrow or clone model for owned elements.
- Shared DynamicArray receiver element paths such as `this[0].value` now lower through descriptor bounds checking,
  element-address projection, and ordinary record field loads. This keeps owned element copies rejected while allowing
  scalar field reads from the borrowed receiver element.
- Named DynamicArray element paths such as `items[0].field` now use the same bounded element-address projection.
  Direct index reads of owned elements are rejected for every named DynamicArray owner until explicit safe borrow or
  clone semantics exist.
- Nested DynamicArray element paths such as `items[0].child.value` are covered by the same aggregate cursor path after
  the bounded element address is established.
- DynamicArray element paths can also continue through fixed-array indexing after the element, such as
  `items[0].bytes[1]`, using the same aggregate cursor after the bounded element address is established.
- DynamicArray element paths reject owned projections such as `items[0].child` while allowing scalar terminal
  projections such as `items[0].child.value`.
- CLI cleanup-audit smoke coverage now pins those owned-element drop pairs for the authorized `DynamicArray<Payload>`
  fixture, so the end-to-end audit surface proves the element-drop context reaches users.
- CLI cleanup-audit smoke coverage also pins the blocked owned-element path: missing semantic/source drop proof reports
  `[element cleanup missing]` and omits owned-element drop pairs rather than implying an authorized drop target.
- Blocked dynamic-array cleanup capability reports now include `missing-element-drop-pairs`, which names the cleanup
  owner, element capture, and drop ABI symbol that still needs semantic/source drop proof while keeping authorized
  `element-drop-pairs` reserved for proven cleanup.
- Dynamic-array cleanup production-readiness reports now propagate `missing-element-drop-pairs` from cleanup
  capability reports, so readiness blockers can name the same owner/capture/drop proof that prevents cleanup readiness.
- Dynamic-array cleanup production-readiness planning now consumes structured missing element-drop pairs from
  `CompilePipelineResult` rather than parsing formatted cleanup capability report text.
- Exclusive `DynamicArray<T>` receiver methods now use an internal pointer receiver ABI for scalar or non-owning
  element types. Generic receiver specialization preserves the exclusive receiver marker, member-call lowering passes
  the caller descriptor storage pointer, and `this.push(value)` mutates the caller-owned descriptor.
- `DynamicArray<T>` receiver methods now specialize and lower for owned record element types when source Drop proof is
  available. Generic-call source inference recognizes non-generic record constructors such as `Payload(...)`, exclusive
  receiver methods pass descriptor storage by pointer, shared receiver methods pass descriptors by value, and the full
  receiver contract fixture now covers owned append, owned first-element read, and initialized-element cleanup.
- Generic functions taking `DynamicArray<T>` now keep owned-record descriptor ABI specializations available for emission.
  The actual specialization still lowers only when source-backed Drop proof authorizes the concrete parameter element
  cleanup owner, preserving the missing-Drop diagnostic boundary.
- Generic method specialization now observes inferred source types for unannotated generic record constructor locals.
  `DynamicArray<T>` receiver methods with their own method-level generic parameters are pinned as specialized methods
  over the concrete receiver descriptor ABI.
- Generic function specialization now infers generic record constructor argument types directly, allowing constructor
  arguments such as `Box(13 as UInt32)` to bind the callee generic parameter without a named temporary.
- Generic function specialization also infers ternary argument source types when both arms resolve to the same concrete
  generic record type; mismatched or unresolved arms remain unbound.
- Mismatched ternary generic-constructor argument arms are pinned as an unresolved specialization boundary rather than
  being coerced to a common generic type.
- The mismatched ternary boundary reports conflicting arm source types at the generic call argument site rather than
  falling through to a blank return-type mismatch.
- Generic call-resolution diagnostics now retain lowered generic parameter names so emit-time failures can report
  ordinary repeated generic-parameter conflicts without needing source syntax declarations.
- Ambiguous generic specialization diagnostics now enumerate matching lowered candidate symbols for both function and
  method calls.
- Duplicate source extension methods are rejected during semantic analysis before lowering; duplicate specialization
  lookup remains covered as an internal lowerer invariant for constructed state.
- Dynamic-array cleanup production-readiness planning now consumes structured sequence-verification and
  cleanup-capability booleans from `CompilePipelineResult` rather than scanning formatted report strings.
- Computed dynamic-array inserted cleanup handoffs are now analyzed once into structured verified pairs before the
  downstream transition, state-verification, cleanup-call gate, plan, render, insertion, and consumed-descriptor
  reports are formatted.
- Executable computed dynamic-array `for` lowering now records structured cleanup-call operand metadata for the planned
  cleanup resumption. Pipeline cleanup-call reports prefer that metadata over IR instruction scraping when proving
  data pointer, element size, and capacity operands.
- A test-only suppression seam covers the fallback path: when structured computed cleanup-call operand metadata is
  absent, cleanup-call plan/render/insertion-gate reports can still degrade through the existing IR-derived operands.
- Structured computed cleanup-call metadata now records whether the deallocation call and descriptor finalization were
  emitted. Pipeline inserted cleanup-call and consumed-descriptor reports prefer those proof flags before falling back
  to IR text matching.
- Executable computed cleanup handoffs are now retained as structured metadata. Pipeline inserted cleanup-transition
  and state-verification reports prefer recorded handoff operations before falling back to inserted IR comments.
- Computed cleanup-call report formatting now shares one analyzed operand set per verified inserted cleanup pair.
  Pipeline results expose structured-vs-IR-fallback operand provenance counts so the normal path can prove it avoids
  operand scraping while the suppression seam still covers fallback behavior.
- Inserted cleanup-call and consumed cleanup-descriptor reports now expose structured-vs-IR-fallback proof counts. The
  normal executable computed-loop path proves emitted cleanup and descriptor finalization through metadata, while the
  deliberate suppressed-metadata seam remains covered by IR fallback tests.
- Inserted cleanup handoff verification now exposes structured-vs-IR-comment provenance counts. A test-only handoff
  metadata suppression seam keeps the IR-comment fallback intentional and covered while the normal path proves handoffs
  from structured metadata.
- Computed cleanup report population now consumes a typed internal cleanup-proof model. The model bundles inserted
  handoff verification and verified cleanup-call operands before downstream cleanup gate, plan, render, inserted-call,
  and consumed-descriptor reports are formatted.
- The computed cleanup-proof model is now extracted into a reusable private pipeline component. Report formatting
  consumes the component output, and later production-oriented pipeline stages can consume the same typed proof bundle
  without depending on anonymous report state.
- Computed dynamic-array `for` audit collectors use production audit names for collected result metadata and helper
  functions. Fixture construction requests now use intent-named fields, while the older `test_only_dynamic_array_*`
  allocation/grow/deallocation snippet fields remain fixture-only renderers driven by explicit test-only options.
- Dynamic-array cleanup derivation, fixture parameter descriptors, and fixture bound-parameter cleanup emission use
  fixture-named internal option fields. Production descriptor cleanup planning, parameter descriptors, and cleanup
  emission keep separate production enablement fields.

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
- Scalar/non-owning `DynamicArray<T>` choice payloads now lower through the finite descriptor ABI
  `{ ptr, i64, i64 }` at return, parameter, annotated `let`, and annotated `var` boundaries. The
  `choice_dynamic_array_payload*.or` fixtures pin accepted `Buffered.Ready(values: DynamicArray<UInt32>)` ABI use, and
  constructor payload moves now suppress source descriptor cleanup after insertion into the choice value.
- Owned-element `DynamicArray<T>` choice payloads now lower through the same descriptor ABI for exact descriptor
  payload variants, and addressable choice owners emit tag-guarded nested descriptor cleanup. The
  `choice_dynamic_array_owned_payload.or` fixture pins `Buffered.Ready(values: DynamicArray<Payload>)` with
  source-backed element drops followed by descriptor deallocation.
- Distinct byte-buffer choice payload storage now reloads owned-element `DynamicArray<T>` descriptors from the finite
  `{ i32, [N x i8] }` payload slot before tag-guarded nested cleanup. The
  `choice_dynamic_array_distinct_payload.or` fixture pins `Buffered.Ready(values: DynamicArray<Payload>)` alongside a
  distinct scalar variant and verifies source-backed element drops followed by descriptor deallocation.
- Switch cases that bind an owned-element `DynamicArray<T>` choice payload now seed a scoped descriptor cleanup plan for
  returning case bodies. Branch binding scopes restore temporary cleanup plans between cases, and consumed choice
  payloads suppress parent nested cleanup so pattern extraction does not double-clean the moved descriptor.
- Local DynamicArray cleanup planning now skips descriptors already finalized by computed cleanup handoff metadata,
  preserving the single-deallocation invariant for computed `for` loops and final function cleanup.
- Value-producing switch cases that bind owned-element `DynamicArray<T>` choice payloads now have fixture coverage for
  passing the bound descriptor onward to a callee. The case-local and parent choice cleanup plans remain suppressed
  after the transfer, preserving single ownership across the branch merge.
- Final switch cases that return a bound owned-element `DynamicArray<T>` choice payload now mark the branch-local
  payload binding consumed before scoped case cleanup. The `return_switch_payload` fixture pins descriptor phi return
  without cleaning the returned payload alias or parent choice payload.
- The `choice_dynamic_array_return_payload_run.or` fixture now links and runs the same owned-element payload return
  transfer. It constructs a non-empty `DualBuffered.Primary`, returns the descriptor through `switch`, observes
  `.length()`, and lets the returned local own final element/drop cleanup.
- The same fixture now covers both `DualBuffered.Primary` and `DualBuffered.Secondary` return paths plus mutable
  reassignment of a returned `DynamicArray<Payload>` owner. Reassignment emits source-backed element drops and
  descriptor deallocation for the overwritten descriptor before storing the replacement descriptor.
- Aggregate assignment targets now carry deterministic cleanup-owner labels for member/index paths. The
  `dynamic_array_field_reassignment_run.or` fixture pins scalar `DynamicArray<UInt32>` record-field overwrite cleanup:
  the overwritten descriptor is deallocated before the replacement descriptor is stored.
- Semantic Drop-site collection now derives direct record-field `DynamicArray<T>` element owners such as
  `holder.values.element`. The `dynamic_array_owned_field_reassignment_run.or` fixture pins source-backed element drops
  plus descriptor deallocation before an owned `DynamicArray<Payload>` field replacement is stored.
- Local record bindings now seed direct `DynamicArray<T>` field descriptor cleanup plans for function-exit cleanup. The
  `dynamic_array_owned_field_scope_cleanup_run.or` fixture pins source-backed element drops and descriptor deallocation
  when a `Holder.values: DynamicArray<Payload>` field reaches final scope exit without explicit reassignment.
- Local record-field cleanup seeding now recurses through nested record fields. The
  `dynamic_array_owned_nested_field_scope_cleanup_run.or` fixture pins final-scope element drops and descriptor
  deallocation for `Outer.inner.values: DynamicArray<Payload>`.
- Local record-field cleanup seeding now also unrolls fixed arrays of records. The
  `dynamic_array_owned_indexed_field_scope_cleanup_run.or` fixture pins final-scope element drops and descriptor
  deallocation for `Outer.items[0].values` and `Outer.items[1].values`.
- Fixed-array record fields can now contain direct `DynamicArray<T>` descriptor elements. The
  `dynamic_array_owned_direct_indexed_scope_cleanup_run.or` fixture pins source-backed element drops and descriptor
  deallocation for each `Holder.values` element.
- Fixed-array record-field reassignment now cleans direct `DynamicArray<T>` descriptor elements before storing the
  replacement array. The `dynamic_array_owned_direct_indexed_field_reassignment_run.or` fixture pins per-element
  source-backed drops and descriptor deallocation before the replacement `Holder.values` store.
- Fixed-array indexed element reassignment now has direct owned-descriptor coverage. The
  `dynamic_array_owned_direct_indexed_element_reassignment_run.or` fixture pins old `Holder.values[0]` descriptor
  cleanup before storing the replacement element descriptor.
- Fixed-array indexed record-element field reassignment now uses the same literal-index owner labels. The
  `dynamic_array_owned_indexed_record_element_field_reassignment_run.or` fixture pins old
  `Holder.items[0].values` descriptor cleanup before storing the replacement field descriptor.
- Fixed-array indexed nested record-field reassignment now preserves literal-index owner labels through deeper member
  paths. The `dynamic_array_owned_indexed_nested_record_field_reassignment_run.or` fixture pins old
  `Holder.items[0].inner.values` descriptor cleanup before storing the replacement field descriptor.
- Fixed-array indexed nested record sibling-field reassignment now pins the same path for a second owned descriptor.
  The `dynamic_array_owned_indexed_nested_record_sibling_field_reassignment_run.or` fixture covers
  `Holder.items[0].inner.spare` cleanup before storing the replacement field descriptor.
- Computed fixed-array indexed nested record sibling-field reassignment now normalizes fixed-array element owner labels
  for Drop authorization while keeping source type and ABI symbol checks exact. The
  `dynamic_array_owned_computed_index_nested_record_sibling_field_reassignment_run.or` fixture pins
  `Holder.items[index].inner.spare` cleanup before storing the replacement field descriptor.
- DynamicArray-indexed record-field reassignment now projects the selected owned element through descriptor load,
  bounds check, and element address lowering before field cleanup. Semantic Drop-site collection records nested
  descriptor fields below owned DynamicArray element records, and the
  `dynamic_array_owned_dynamic_index_record_field_reassignment_run.or` fixture pins
  `items[index].values` cleanup before replacement storage.
- Nested DynamicArray-indexed record-field reassignment now recurses through descriptor fields below owned DynamicArray
  element records nested inside other owned DynamicArray element records. The
  `dynamic_array_owned_nested_dynamic_index_record_field_reassignment_run.or` fixture pins
  `groups[group_index].items[item_index].values` cleanup before replacement storage.
- Nested DynamicArray-indexed sibling-field reassignment now covers a second owned descriptor field under the same
  nested dynamic owner path. The
  `dynamic_array_owned_nested_dynamic_index_sibling_field_reassignment_run.or` fixture pins
  `groups[group_index].items[item_index].spare` cleanup before replacement storage.
- Nested DynamicArray-indexed whole-record reassignment now pins cleanup of every owned descriptor field inside the
  replaced element before storing the replacement record. The
  `dynamic_array_owned_nested_dynamic_index_multi_field_record_reassignment_run.or` fixture also runs end to end now
  that record-constructor returns consume local `DynamicArray<T>` field arguments instead of cleaning moved descriptors
  in the callee.
- Returned record constructors now also consume nested owned record arguments that contain DynamicArray descriptors.
  The `dynamic_array_owned_returned_nested_record_field_move_run.or` fixture pins `Outer(inner)` without stale
  `inner.values` or `inner.spare` cleanup in the callee, while retaining cleanup under the receiving `outer.inner`
  owner.
- Returned record constructors now also consume fixed-array descendants below owned record arguments. The
  `dynamic_array_owned_returned_fixed_array_record_field_move_run.or` fixture pins `Outer(items)` without stale
  `items.elementN.values` or `items.elementN.spare` cleanup in the callee, while retaining cleanup under the receiving
  `outer.items.elementN` owner.
- Record constructors now consume fixed-array descendants below owned record arguments on the general expression path.
  The `dynamic_array_owned_constructor_fixed_array_record_field_move_run.or` fixture pins both local initialization and
  assignment with `Outer(items)` without later cleanup under moved source owners, while preserving replacement and final
  cleanup under `outer.items.elementN`.
- Return lowering now relies on the same general record-constructor expression transfer path. The returned nested and
  fixed-array record fixtures continue to pin callee cleanup suppression without a return-specific constructor helper.
- Record constructor argument cleanup transfer now accepts member-only owned aggregate paths. The
  `dynamic_array_owned_constructor_member_path_move_run.or` fixture pins `Outer(holder.items)` without stale cleanup
  under `holder.items.elementN`, while retaining replacement and final cleanup under `outer.items.elementN`.
- Constructor member-path move diagnostics now reject reusing `holder.items` after `Outer(holder.items)`. The
  `dynamic_array_owned_constructor_member_path_reuse_rejected.or` fixture pins the user-written
  `use after move: holder.items` diagnostic while cleanup metadata keeps descendant descriptor ownership.
- Record constructor argument cleanup transfer now has explicit nested member-path coverage. The
  `dynamic_array_owned_constructor_nested_member_path_move_run.or` fixture pins `Outer(nested.holder.items)` cleanup
  handoff, and the paired rejected fixture pins `use after move: nested.holder.items`.
- Record constructor indexed ownership moves now support fixed-array decimal literal element transfers. The
  `dynamic_array_owned_constructor_indexed_member_path_move_run.or` fixture pins `Outer(holder.items[0])` cleanup
  handoff to `outer.item` while preserving sibling cleanup under `holder.items.element1`.
- Record constructor literal-index partial ownership now has same-element and sibling proof coverage. Reusing
  `holder.items[0]` after transfer reports `use after move: holder.items.element0`, while moving `holder.items[1]`
  after `holder.items[0]` remains valid.
- Record constructor computed indexed ownership moves remain rejected until runtime-index partial ownership is modeled.
  The `dynamic_array_owned_constructor_computed_index_member_path_move_rejected.or` fixture pins
  `Outer(holder.items[index])` with `indexed constructor ownership move requires explicit partial ownership support`.
- Single-payload choice constructor indexed ownership moves now support fixed-array decimal literal element transfers.
  The `choice_constructor_indexed_member_path_move_run.or` fixture pins `Some(holder.items[0])` cleanup handoff to
  `selected.Some.item` while preserving sibling cleanup under `holder.items.element1`.
- Single-payload choice constructor literal-index partial ownership now mirrors record constructor coverage: same-element
  reuse reports `use after move: holder.items.element0`, and sibling transfer cleanup moves under `sibling.Some.item`.
- Single-payload choice constructor computed indexed ownership moves remain rejected until runtime-index partial
  ownership is modeled. The `choice_constructor_computed_index_member_path_move_rejected.or` fixture pins
  `Some(holder.items[index])` with `indexed constructor ownership move requires explicit partial ownership support`.
- Choice constructor payload cleanup transfer now accepts member-only owned aggregate paths for directly lowered
  dynamic-array payloads. The `choice_constructor_member_path_move_run.or` fixture pins `Some(holder.values)` cleanup
  handoff to `selected.Some.values`, and the paired rejected fixture pins `use after move: holder.values`.
- Choice payload cleanup emission now recurses through fixed arrays and records. The
  `choice_constructor_nested_member_path_move_run.or` fixture pins `Some(holder.items)` cleanup handoff for
  `Array<RecordWithDynamicArray, N>` payloads under `selected.Some.items.elementN.field`.
- Multi-payload choice constructor cleanup now has explicit tuple-payload extraction coverage. The
  `choice_constructor_multi_payload_nested_member_path_move_run.or` fixture pins
  `Ready(holder.items, 7 as UInt32)` cleanup handoff under `selected.Ready.items.elementN.field` while preserving the
  scalar payload slot.
- Multi-payload choice constructor reuse diagnostics now cover moved nested payloads. The
  `choice_constructor_multi_payload_nested_member_path_reuse_rejected.or` fixture pins a second
  `Ready(holder.items, 11 as UInt32)` with `use after move: holder.items`.
- Multi-payload choice constructor indexed ownership moves now support fixed-array decimal literal element transfers in
  the first payload slot. The `choice_constructor_multi_payload_indexed_member_path_move_run.or` fixture pins
  `Ready(holder.items[0], 7 as UInt32)` cleanup handoff to `selected.Ready.item` while preserving sibling cleanup under
  `holder.items.element1`.
- Multi-payload choice constructor literal-index partial ownership now has same-element and sibling proof coverage. The
  `choice_constructor_multi_payload_indexed_member_path_reuse_rejected.or` fixture pins
  `use after move: holder.items.element0`, while sibling run fixtures pin moving `holder.items[1]` after
  `holder.items[0]` in both payload positions.
- Multi-payload choice constructor computed indexed ownership moves remain rejected until runtime-index partial
  ownership is modeled. The `choice_constructor_multi_payload_computed_index_member_path_move_rejected.or` fixture pins
  `Ready(holder.items[index], 7 as UInt32)` with `indexed constructor ownership move requires explicit partial
  ownership support`.
- Multi-payload choice constructor cleanup now has symmetric payload-index coverage. The
  `choice_constructor_multi_payload_second_nested_member_path_move_run.or`,
  `choice_constructor_multi_payload_second_nested_member_path_reuse_rejected.or`, and
  `choice_constructor_multi_payload_second_indexed_member_path_move_run.or` fixtures pin
  `Ready(7 as UInt32, holder.items)` transfer, reuse, and `Ready(7 as UInt32, holder.items[0])` indexed transfer
  behavior.
- Multi-payload choice constructor second-slot computed indexed ownership moves remain rejected until runtime-index
  partial ownership is modeled. The
  `choice_constructor_multi_payload_second_computed_index_member_path_move_rejected.or` fixture pins
  `Ready(7 as UInt32, holder.items[index])` with `indexed constructor ownership move requires explicit partial
  ownership support`.
- Multi-variant choice constructor cleanup now has explicit tag-gated nested payload coverage. The
  `choice_constructor_multi_variant_nested_member_path_move_run.or` fixture pins `Secondary(holder.items)` cleanup
  handoff while retaining inactive `Primary(...)` cleanup blocks behind their own tag checks.
- Multi-variant choice constructor reuse diagnostics now remain constructor-agnostic after a nested payload move. The
  `choice_constructor_multi_variant_nested_member_path_reuse_rejected.or` fixture pins `Secondary(holder.items)`
  followed by `Primary(holder.items)` with `use after move: holder.items`.
- Multi-variant single-payload choice constructor indexed ownership moves now share the fixed-array decimal literal
  element transfer path. The `choice_constructor_multi_variant_indexed_member_path_move_run.or` fixture pins
  `Secondary(holder.items[0])` cleanup handoff under the active variant while preserving sibling cleanup under
  `holder.items.element1`.
- Multi-variant choice constructor literal-index partial ownership now mirrors the single-variant path. Reusing
  `holder.items[0]` after `Secondary(holder.items[0])` reports `use after move: holder.items.element0`, while moving
  `holder.items[1]` into `Primary(...)` remains valid.
- Computed-index constructor ownership moves remain intentionally rejected across record, single-payload choice,
  multi-payload choice, and multi-variant choice constructors. Supporting `holder.items[index]` safely requires a
  runtime-index partial-owner model plus cleanup lowering that skips the moved element while still cleaning every
  remaining element; the multi-variant computed-index fixture pins the current rejection boundary.
- Runtime-index partial-owner metadata now exists behind that rejection boundary. The model records owner, index
  expression, element source type, moved source type, cleanup strategy, and disabled constructor-move status, and the
  multi-variant computed-index fixture pins the rendered diagnostic metadata without enabling the move.
- Runtime-index cleanup-skip planning now derives from the partial-owner metadata and records the owner, index
  expression, element source type, moved source type, skip operation, and disabled production-cleanup status. The same
  computed-index fixture pins the rendered plan while constructor moves remain rejected.
- Runtime-index cleanup proof gates now validate owner, index expression, matching element/moved types, and supported
  skip operation. The gate can report complete prerequisites while lowering remains disabled, preserving the rejection
  boundary until runtime-index cleanup emission exists.
- Runtime-index cleanup emission sketches now derive from complete proof gates and render the intended report-only
  sequence: load owner length, iterate cleanup indexes, skip the moved runtime index, drop live elements, then
  deallocate the owner. Production emission remains disabled.
- Runtime-index cleanup capabilities now consume the complete proof gate plus emission sketch and expose one disabled
  production toggle. They can report ready prerequisites while keeping computed-index constructor moves rejected.
- Runtime-index cleanup audit reports now render partial-owner metadata, cleanup-skip plans, proof gates, emission
  sketches, and disabled capabilities from `OwnershipTransferState`, giving future pipeline/driver integration a
  structured report hook outside the rejection diagnostic.
- Runtime-index cleanup audit metadata now flows through function emission, LLVM emission, pipeline results, and the
  `--runtime-indexed-cleanup-audit` driver command. The command exits successfully for the computed-index rejection
  fixture and prints the structured report without relying on the ordinary lowering diagnostic text.
- Runtime-index cleanup capability metadata now also flows as typed pipeline state: capability count, prerequisite
  readiness, production-enable status, and the underlying capability records are available without parsing report text.
- Runtime-index cleanup emission planning is now typed separately from the report sketch. The plan records five named
  operations, prerequisite readiness, and disabled production status for future IR emission.
- Runtime-index cleanup emission plans now carry five comment-only IR preview lines. The preview is exposed as typed
  metadata and is not appended to module IR while production emission remains disabled.
- Runtime-index cleanup production emission now has an explicit disabled-by-default pipeline/lowering gate. Pipeline
  state reports whether the gate was requested, so complete preview metadata still proves emission is blocked by policy.
- Runtime-index cleanup emission plans now expose the first gated IR slice for length loading. The slice appears only
  when the explicit production gate is requested and prerequisites are ready, while the computed-index constructor move
  rejection boundary remains in place.
- Runtime-index cleanup gated IR metadata now extends through the cleanup-loop block skeleton: entry branch, condition
  block, cleanup index PHI, and length bounds comparison are visible behind the explicit gate.
- Runtime-index cleanup gated IR metadata now includes the moved-index skip decision: cleanup index comparison,
  branch-to-skip/drop labels, and typed pipeline readiness are exposed without accepting computed-index moves.
- Runtime-index cleanup gated IR metadata now includes live-element drop intent: element address calculation and
  source Drop call planning are visible behind the explicit gate while normal compilation stays rejected.
- Runtime-index cleanup gated IR metadata now includes the cleanup tail: convergence to the continue block, next-index
  increment, loop backedge, exit block, and descriptor deallocation intent remain behind the explicit gate.
- Runtime-index cleanup emission now carries a structured IR plan with named blocks, SSA values, callees, readiness
  flags, and pipeline counts. Rendered gated lines remain a verification artifact rather than the only bridge to future
  emitter integration.
- Runtime-index cleanup now has an emitter-facing renderer that consumes the structured IR plan and reproduces the
  gated verification artifact. Future production integration can replace artifact comparison with real module emission.
- Runtime-index cleanup IR rendering now flows through pipeline state from the structured plan. The pipeline still
  compares the rendered output with the gated artifact field, keeping the artifact as a regression guard while
  production-facing consumers read the structured renderer output.
- Runtime-index cleanup rendered IR now has a guarded module-IR artifact state that remains separate from module
  `ir_text`. The artifact is populated only when structured plans are complete and still match the gated verification
  artifact.
- Runtime-index cleanup module-IR artifact insertion now has an explicit pipeline gate. The gate can prove insertion
  readiness from artifact availability and render parity, but rendered lines still remain separate from module `ir_text`
  until actual module insertion is implemented.
- Runtime-index cleanup insertion now exposes a non-mutating module-IR preview. The preview records the append-point
  line index and projected module line count while leaving module `ir_text` unchanged.
- Runtime-index cleanup insertion now produces a separate candidate module-IR string when the preview is enabled. The
  candidate contains the rendered cleanup lines, while the emitted module `ir_text` remains unchanged.
- Runtime-index cleanup candidate IR now has a verifier that checks the rendered cleanup anchor appears exactly once in
  the candidate and remains absent from emitted module `ir_text`.
- Runtime-index cleanup module insertion now has an aggregate production-readiness signal. It can report a verified
  candidate as ready while keeping production readiness blocked until module mutation is explicitly implemented.
- The `--runtime-indexed-cleanup-audit` driver report now includes the aggregate module-IR production-readiness line,
  exposing insertion gate, preview, candidate, verification, and module-mutation status.
- Driver smoke coverage now pins the runtime-index cleanup emission-plan audit line, including lowerable slices,
  structured-plan completion, and the gated IR line count.
- Runtime-index cleanup module mutation now has an explicit disabled-by-default pipeline gate. When enabled after
  candidate verification, it copies the verified candidate into module `ir_text` and marks production readiness ready.
- Runtime-index computed constructor moves now have an explicit disabled-by-default acceptance gate. Ordinary emission
  still rejects the move, while the audit workflow can enable constructor-move acceptance only alongside cleanup
  emission, module insertion, and module mutation.
- A gated executable smoke fixture now exercises the accepted multi-variant computed-index constructor move path through
  `--test-only-runtime-indexed-constructor-move-run`; the command remains a compiler test seam rather than user syntax
  and does not enable the pseudo module-mutation artifact.
- Runtime-index partial-owner reuse now reports `use after move: holder.items[index]` when the accepted move path later
  reads the same owner/index pair under the gated constructor-move workflow.
- Runtime-index partial-owner sibling access now has a gated run fixture proving `holder.items[1]` remains readable
  after moving `holder.items[index]` when `index` is `0`.
- Runtime-index gated constructor-move smoke coverage now spans record constructors, single-payload choice
  constructors, and multi-variant choice constructors, including accepted moves, sibling reads, and same-index reuse
  rejection.
- Runtime-index module-IR production readiness now includes an explicit function-integration gate. Candidate module
  mutation can still be verified, but production readiness remains blocked until the cleanup slice is inserted into the
  owning function control flow rather than appended as an artifact.
- Runtime-index cleanup emission plans now carry typed owner-function insertion targets. The module emitter stamps the
  owning function symbol plus insertion, predecessor, and continuation block names while function integration remains
  blocked until CFG placement is implemented.
- Runtime-index cleanup function integration now has a non-mutating CFG rewrite candidate model. The pipeline records
  the owning function, predecessor, inserted branch, replaced terminator, continuation block, and cleanup slice line
  count while keeping function IR unchanged.
- Runtime-index cleanup CFG rewrite candidates now have function-local verification. The verifier checks the target
  function body for the predecessor block, confirms the cleanup insertion block is absent, and keeps verification
  blocked until the continuation block exists in function IR.
- Choice payload cleanup still handles concrete record and fixed-array paths only; generic payload shapes need the same
  recursive descriptor collection once their runtime layout is materialized in the lowering context.
- Fixed-array record-field reassignment now also descends through record elements. The
  `dynamic_array_owned_indexed_record_field_reassignment_run.or` fixture pins old `Holder.items[N].values` descriptor
  cleanup before storing the replacement `Holder.items` array.
- Fixed-array record-field reassignment now also covers multiple owned descriptors per record element. The
  `dynamic_array_owned_multi_field_indexed_record_reassignment_run.or` fixture pins old
  `Holder.items[N].values` and `Holder.items[N].spare` cleanup before replacement storage.
- Nested record-field reassignment now covers multiple owned descriptor descendants. The
  `dynamic_array_owned_multi_field_nested_record_reassignment_run.or` fixture pins old
  `Outer.inner.values` and `Outer.inner.spare` cleanup before storing the replacement `Outer.inner`.
- Fixed-array record-field reassignment now also covers nested records with multiple owned descriptor descendants. The
  `dynamic_array_owned_indexed_nested_multi_field_reassignment_run.or` fixture pins old
  `Holder.items[N].inner.values` and `Holder.items[N].inner.spare` cleanup before replacement storage.
- The same reassignment cleanup recursion is now pinned for multi-dimensional fixed arrays. The
  `dynamic_array_owned_multidimensional_record_field_reassignment_run.or` fixture covers
  `Holder.grid[row][column].values` cleanup before replacement storage.
- Computed multi-dimensional fixed-array indexed record-field reassignment now uses the normalized fixed-array element
  owner labels across both computed indexes. The
  `dynamic_array_owned_computed_multidimensional_record_field_reassignment_run.or` fixture pins
  `Holder.grid[row][col].values` cleanup before storing the replacement field descriptor.
- Mixed literal/computed multi-dimensional fixed-array indexed record-field reassignment now has explicit coverage for
  both partial owner-label forms. The
  `dynamic_array_owned_literal_computed_multidimensional_record_field_reassignment_run.or` and
  `dynamic_array_owned_computed_literal_multidimensional_record_field_reassignment_run.or` fixtures pin
  `Holder.grid[0][col].values` and `Holder.grid[row][0].values` cleanup before replacement storage.
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
- `examples/local_dynamic_array_computed_for.or` now pins computed same-owner `DynamicArray<UInt32>` `for` iteration
  with final-use deallocation and descriptor finalization through the normal CLI run/build path.
- The same example is now checked through CLI LLVM/object emission, including the computed loop labels, enabled
  cleanup handoff markers, deallocation call, and descriptor finalization store.
- Computed same-owner final-use cleanup now consumes the local cleanup plan after deallocation and descriptor
  finalization, suppressing the later function-exit local cleanup plan for the same owner.
- `DynamicArray<T>.push(value)` now marks owned element bindings and owned record-member element paths consumed after
  the successful store into array storage, so subsequent reads reuse the existing `use after move` diagnostics.
- Local `DynamicArray<T>` indexed assignment now lowers for scalar/non-owning elements by reloading the descriptor,
  checking `index < length`, and storing into the computed element address.
- Local `DynamicArray<T>` indexed assignment now lowers owned-element replacement when `items.element` has authorized
  source Drop lowering. The lowerer emits the old-element drop at the checked element address before the replacement
  store and consumes named/member RHS owners after the store.
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
- Computed same-owner final-use cleanup now reuses source-backed element Drop authorization for owned local
  `DynamicArray<Payload>` iterables. The lowered path emits the initialized-element drop walk after the loop exit
  handoff and before descriptor deallocation/finalization, while keeping the consumed local cleanup plan suppression.
- `examples/local_dynamic_array_owned_computed_for.or` is now the checked-in owned computed-loop cleanup demo, covered
  by the array CLI smoke across run, LLVM IR emission, object emission, and retained executable build paths.
- Missing authorized element Drop now blocks computed same-owner final-use cleanup for owned `DynamicArray<T>` elements
  before descriptor deallocation can be emitted.
- `tests/fixtures/dynamic_array_owned_computed_cleanup_missing_drop.or` is the checked-in CLI diagnostic fixture for
  the missing authorized element Drop boundary.
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
- Computed same-owner `DynamicArray<T>` iterables over scalar/non-owning bound parameters now lower on the normal path.
  Parameter binding records a proof-only cleanup-owner plan from the callee descriptor spill, so computed-loop planning
  can prove `flag ? items : items` without adding a second descriptor cleanup. Function-exit parameter cleanup remains
  the only production deallocation; computed-loop cleanup-call insertion remains gated behind the explicit test-only
  authorization/insertion seam.
- Computed same-owner `DynamicArray<T>` ownership planning now recursively accepts nested ternary leaves when every
  reachable leaf is a name that resolves to the same descriptor owner and every leaf has cleanup-owner proof. This keeps
  `flag ? items : other_flag ? items : items` on the proven single-owner path while non-name computed leaves and
  mismatched owners remain blocked.
- Shared descriptor-loop lowering now emits neutral `sequence_for` temporary names in generated LLVM IR. The remaining
  DynamicArray-specific option names are intentionally gate-oriented rather than loop-shape-oriented.
- Local `DynamicArray<T>` lowering is now available on the default compile path for constructed local descriptors:
  default construction, append with growth, checked scalar index reads, length reads, descriptor `for` loops, and local
  descriptor cleanup are enabled together.
- Scalar/non-owning `DynamicArray<T>` parameter lowering is now available on the default compile path: descriptor
  signatures, parameter cleanup, `.length()`, checked index reads, and descriptor `for` loops move together. Owned
  element parameters such as `DynamicArray<Payload>` now lower on the default compile path when source-derived semantic
  Drop proof authorizes the `owner.element` cleanup site; unproven owned parameters remain rejected.
- `examples/dynamic_array_parameter_reads.or` is the checked-in scalar parameter descriptor demo. It is covered by
  examples and canonical pipeline smoke tests, while `tests/fixtures/dynamic_array_owned_parameter_rejected.or` pins
  the production rejection boundary for owned-element parameters.
- Owned-element `DynamicArray<T>` parameter rejection now reports the specific parameter name and element type that
  needs ownership/drop proof, rather than falling through to the generic unsupported-parameter diagnostic.
- Production dynamic-array parameter descriptor lowering can now consume positive semantic drop authorization for an
  owned element site named `owner.element`. Scalar/non-owning parameters still lower directly; owned-element parameters
  use the same authorization to seed bound cleanup-owner plans for callee-side element drop walks and descriptor
  deallocation.
- Authorized owned-element `DynamicArray<T>` parameters now share the existing descriptor `.length()` lowering path
  with scalar/non-owning parameters: the callee reads field 1 from the bound `{ ptr, i64, i64 }` descriptor before
  cleanup emission.
- Authorized owned-element `DynamicArray<T>` parameters now share the existing checked index-read lowering path with
  scalar/non-owning parameters. Aggregate elements load through the descriptor data pointer, recover fields through
  normal aggregate extraction, and still emit source-backed element cleanup before descriptor deallocation.
- Authorized owned-element `DynamicArray<T>` parameters now share the existing descriptor `for` iteration lowering path
  with scalar/non-owning parameters. Loop items load through the descriptor data pointer, bind as aggregate values for
  field reads, and preserve callee-side element cleanup ordering.
- Owned-element `DynamicArray<T>` parameter iteration without authorized `owner.element` Drop proof remains rejected on
  the CLI path; `tests/fixtures/dynamic_array_owned_parameter_iteration_missing_drop.or` pins that boundary.
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
- Named local `DynamicArray<T>` cleanup emission now also uses the generic consumed descriptor finalization plan before
  clearing the owner descriptor after deallocation. This reuses the same owner/storage/cleanup-operation readiness seam
  beyond computed-loop cleanup and makes local cleanup idempotency explicit in emitted IR.
- `examples/local_dynamic_array_owned_replacement.or` is now the checked-in local owned-element replacement demo. The
  array CLI smoke pins it through `orisonc run`, `--emit-llvm`, `--emit-object`, and `--build`, asserting that
  `__orison_drop.Payload` is source-defined, the old element is dropped before the replacement store, and the remaining
  live element is dropped during normal local descriptor cleanup.
- Bound `DynamicArray<T>` parameter cleanup now uses the same consumed descriptor finalization plan for the callee-local
  descriptor spill. The clear targets `%parameter.addr` storage owned by the current function frame, not caller storage,
  and runs after descriptor deallocation for scalar/non-owning and authorized owned-element parameter cleanup paths.
- Full LLVM module emission now collects consumed descriptor finalization plans from actual function cleanup emission.
  The generic audit report therefore covers computed cleanup metadata plus emitted local and bound cleanup descriptor
  finalization without inferring local/bound cleanup from source-level construction plans.
- `--dynamic-array-cleanup-audit` now merges the consumed descriptor finalization report from a successful full-emission
  pass into the metadata audit output. Individual dynamic-array cleanup report commands remain metadata-only.
- Report-mode descriptor cleanup planning now upgrades semantic origins that match a source `let`/`var`
  `DynamicArray<T> = DynamicArray()` constructor to `local` descriptor storage. Predicted origins without matching
  source-local construction remain predicted.
- Successful `--dynamic-array-cleanup-audit` full-emission probes now replace metadata cleanup
  obligation/sequence/verification/gate/capability sections with emitted cleanup reports. This keeps emitted local
  cleanup operation names aligned with consumed descriptor finalization report lines.
- DynamicArray implementation gap review after owned-parameter read coverage keeps the next backend work focused on
  parameter descriptor mutation policy, generic DynamicArray lowering beyond concrete examples, retiring remaining
  computed-cleanup test-only seams, and expanding runtime/allocator APIs beyond allocate/grow/deallocate plus bounds
  failure. These are implementation-readiness gaps, not surface syntax changes.
- Bound `DynamicArray<T>` parameter mutation is intentionally rejected on the current production path.
  `DynamicArray<T>` parameters support descriptor reads, iteration, transfer, and cleanup; they do not support
  `items[index] = value` or `items.push(value)` as parameter descriptor mutation. Parameter-style mutable element
  writes remain represented by `exclusive.View<T>` indexed assignment.
- Generic `DynamicArray<T>` function use is now pinned as a front-to-back lowering boundary. A generic function that
  returns `T` from `values[0]` parses, but a concrete `DynamicArray<UInt32>` call still reaches lowering with an
  unconcretized return type. The remaining work is generic function monomorphization/substitution before descriptor
  index lowering can reuse the existing concrete parameter path.
- The first generic function specialization path now lowers same-module calls with named arguments whose source types
  are known in the caller. The current slice specializes `first<T>(values: DynamicArray<T>) -> T` to `first__UInt32`,
  substitutes the return and parameter source types, reuses the concrete `DynamicArray<UInt32>` descriptor ABI, and
  emits the concrete function body once. This is intentionally narrower than full generic dispatch.
- Generic function call dispatch now resolves multiple same-module concrete specializations by matching the call
  argument source types against each specialized signature. The current DynamicArray proof emits `first__UInt32` and
  `first__UInt64` side by side and routes each call to the matching symbol; broader inference and method dispatch stay
  out of this slice.
- Generic function specialization collection now infers direct non-generic call-result argument types from lowered
  source return metadata. The checked fixture lowers `first(make_values())` where `make_values()` returns a
  `DynamicArray<UInt32>` descriptor, specializes `first<T>` to `first__UInt32`, and transfers the local descriptor out
  of the helper without callee-side local cleanup before the consuming generic call cleans it up.
- Generic function specialization collection now prefers generic substitution over raw generic signature metadata when
  inferring call-result arguments. The checked nested fixture lowers `consume(first(make_values()))`, collecting both
  `first__UInt32` and `consume__UInt32` from source types rather than returning unresolved `T` metadata from the inner
  generic call.
- Generic function specialization collection now records inferred source types for unannotated local bindings when the
  initializer is a source-type-resolvable call expression. The checked fixture lowers `let value = first(make_values())`
  followed by `consume(value)`, collecting `consume__UInt32` from the local binding without requiring an explicit
  `UInt32` annotation.
- Generic call source-type inference now has a shared resolver used by specialization collection and emitter-side
  specialization matching. Collector mode still uses original generic syntax plus local source bindings, while emitter
  mode uses the lowered context and function lowering state, removing the duplicated call-argument matching path from
  `expression_emitter.cpp`.
- `generic_call_resolution` now has direct smoke coverage for collector-mode nested generic substitution, unannotated
  local source-type binding reuse, and emitter-mode lookup of the matching concrete specialization. CLI fixtures remain
  end-to-end coverage for emitted IR and linked execution.
- `generic_call_resolution` negative-path coverage now pins conflicting generic substitutions, no-match concrete
  specialization lookup, and ambiguous duplicate specialization lookup. These failures stay resolver-level invariants
  before they reach CLI lowering fixtures.
- Generic method-call inference now has a shared lowered-method specialization matcher that uses receiver source type,
  method name, expected LLVM return type, and user argument source types while skipping the lowered `this` parameter.
  Direct smoke coverage pins receiver call source-type queries plus positive, no-match, and ambiguous method
  specialization lookup. Expression-emitter smoke coverage pins ambiguous lowered method lookup resolving to the
  concrete source-typed method specialization and emitting that symbol.
- Generic method specialization collection now records concrete method clones for generic methods with explicit method
  parameters, appends supported lowered signatures to `context.methods`, and emits those cloned methods after ordinary
  methods. CLI smoke coverage pins `UInt32.select<T>(..., UInt64)` specializing to
  `method.UInt32.select__UInt64` and routing the member call to that symbol. Generic receiver-parameter collection for
  extension receiver patterns still needs a separate syntax/modeling slice.
- Collector-mode generic call source typing now honors explicit cast expressions, so direct `9 as UInt64` arguments can
  specialize generic functions and generic methods without an annotated temporary binding. The collector also recognizes
  uncast `Bool` and `Text` literals, while uncast numeric literals remain unresolved to avoid implicit width or
  signedness guesses.
- Generic method specialization collection now recognizes receiver-bound generic record parameters when the extension
  receiver mirrors the record declaration, such as `record Box<T>` with `extend Box<T>`. The original receiver-pattern
  method body is skipped during ordinary method emission, while collected concrete receiver methods are emitted through
  the specialization list. CLI smoke coverage pins `Box<UInt32>.value()` routing to
  `method.Box_UInt32_.value__UInt32`.
- Receiver-bound generic method coverage now includes multi-parameter receiver patterns and nested concrete receiver
  substitutions. Resolver smoke coverage pins `Pair<A, B>` binding from `Pair<UInt32, UInt64>` and `Box<T>` binding
  `T` to `Pair<UInt32, UInt64>`, while CLI smoke coverage pins emitted specializations for
  `Pair<UInt32, UInt64>.first()` and `Box<Pair<UInt32, UInt64>>.value()`.
- Built-in `DynamicArray<T>` receiver-pattern specialization now supports a minimal `count()` fixture that lowers
  `DynamicArray<UInt32>.count()` to `method.DynamicArray_UInt32_.count__UInt32`, preserves receiver `this` as a
  non-cleaned-up method receiver, and runs successfully. A fuller checked contract fixture currently records the next
  gap: owned-element `DynamicArray<Payload>.append_value(...)` is not yet collected/lowered as a receiver
  specialization.

## Follow-up work

- Extend production `DynamicArray<T>` lowered signatures to owned element types only after semantic ownership/drop
  analysis proves unique ownership, initialized length, capacity bounds, and deterministic cleanup.
- Extend `for ... in` lowering beyond proven local and bound-parameter same-owner `DynamicArray<T>` sequences, including
  nested same-owner ternary leaves, only after ownership, cleanup, and descriptor-storage rules for broader computed
  owned iterables are proven.
