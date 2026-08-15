# Frontend Parity Audit

- 2026-08-15: Runtime-index constructor-move production-readiness now reports typed member-cleanup promotion state
  from member production-readiness, mutation production-readiness, and rewrite promotion-status records. The headline
  reports `member-cleanup-promotion none|blocked|ready` plus typed record counts without consulting audit text.
- 2026-08-15: Runtime-index member-cleanup mutation operation plan, operation validation, and conflict detection now
  flow through typed pipeline records and the driver production-readiness report. This removes another audit-text
  dependency from mutation-stage visibility.
- 2026-08-15: Runtime-index member-cleanup apply authorization, apply preview, post-apply verification, and promotion
  summary now flow through typed pipeline records and the driver production-readiness report. Driver output no longer
  needs audit-line filtering to expose those mutation-stage readiness details.
- 2026-08-15: Runtime-index member-cleanup helper Drop-binding refresh now updates the mutation readiness chain and
  regenerated audit lines. Operation validation, conflict detection, apply authorization, apply preview, post-apply
  verification, promotion summary, production readiness, and verdict reports remove `member-helper-drop-bindings` when
  concrete helper binding evidence exists.
- 2026-08-14: Runtime-index member-cleanup production readiness now records `production_gate_ready` and
  `production_enabled` separately. Helper Drop-binding refresh can recompute production readiness from explicit
  prerequisites instead of preserving stale readiness from the earlier report-only state.
- 2026-08-14: Runtime-index member-cleanup production readiness now requires concrete helper Drop-binding readiness
  for non-empty moved member paths. Whole-element paths treat helper bindings as not required, while member paths
  report `member-helper-drop-bindings` as an explicit blocker until concrete helper binding evidence is present.
- 2026-08-14: Runtime-index member-cleanup helper Drop bindings now flow as typed LLVM/pipeline records. The
  constructor-move production-readiness report shows concrete nested sibling Drop binding counts, Drop definition
  readiness, helper-definition readiness, and production-disabled status without inspecting emitted helper IR.
- 2026-08-14: Runtime-index member-cleanup guarded-rewrite and rewrite-execution readiness now flow through typed
  lowering, LLVM emission, and pipeline result records. The constructor-move production-readiness report formats the
  mutation verdict, rewrite authorization, execution plan, execution verdict, and promotion status without audit-text
  filtering.
- 2026-08-14: Runtime-index member-cleanup mutation-production readiness now formats from typed pipeline records in
  the constructor-move production-readiness report. Driver coverage asserts the mutation gate blockers without reading
  them back out of audit text.
- 2026-08-14: Runtime-index member-cleanup production readiness now flows through typed lowering emission and pipeline
  result vectors. The driver formats those typed records directly instead of extracting production-readiness lines from
  the audit text.
- 2026-08-14: Runtime-index constructor-move production-readiness now surfaces member-cleanup readiness when a
  member-granular cleanup fixture reaches typed member readiness but remains blocked for default promotion. The report
  includes the blocked `member-cleanup-module-mutation` and `production-member-cleanup` details without enabling
  ordinary `--emit-llvm`.
- 2026-08-14: Driver smoke coverage now pins the runtime-index member-cleanup promotion boundary directly: ordinary
  `--emit-llvm` still rejects the nested owned member-transfer fixture, while
  `--test-only-runtime-indexed-member-cleanup-run` accepts and executes the same source through the gated seam.
- 2026-08-14: The driver now exposes `--test-only-runtime-indexed-member-cleanup-run` as a narrow executable seam for
  the gated runtime-index member cleanup path. Driver smoke coverage runs the nested member-transfer fixture through
  object emission, host linking, and execution without changing ordinary `--emit-llvm` behavior.
- 2026-08-14: Runtime-index member cleanup now has executable smoke coverage for the nested source-backed
  `DynamicArray<T>` member-transfer path. The pipeline emits the gated mutated IR, compiles it to an object, links a
  native executable, and runs the fixture successfully while ordinary surface syntax remains unchanged.
- 2026-08-14: Runtime-index member cleanup rewrite mutation now emits the owner cleanup loop for source-backed
  `DynamicArray<T>` member transfers. The generated CFG loads the owner descriptor, walks initialized elements, invokes
  the member-preserving helper for the moved index, invokes the full element Drop for other indexes, deallocates the
  descriptor storage, and zeroes the consumed descriptor slot.
- 2026-08-14: Runtime-index member cleanup sibling metadata now supports nested moved member paths such as
  `items[index + zero].box.item`. The emitter records full sibling field paths, per-hop field indexes, and container
  LLVM type chains; helper generation renders chained `getelementptr` cleanup addresses for both root-level siblings
  and nested siblings.
- 2026-08-14: Runtime-index member cleanup helper generation now derives concrete sibling cleanup from parsed record
  fields and emitted source Drop definitions for top-level member transfers. The fixture-backed helper skips the moved
  `item` field, drops owned siblings on both sides, and zeroes each dropped sibling slot; nested paths and generic
  record layouts remain follow-up work.
- 2026-08-14: Runtime-index member cleanup sibling discovery now materializes `RuntimeIndexedMemberCleanupSiblingField`
  metadata in the lowering namespace before helper rendering. The pipeline still derives the metadata from parsed
  records and emitted IR today, but helper emission now consumes a typed cleanup-field artifact rather than ad hoc
  pipeline-local field data.
- 2026-08-14: Runtime-index member cleanup sibling metadata derivation now lives in LLVM lowering emission. The emitter
  uses lowered record layouts, planned member cleanup edit scripts, and source Drop definition symbols to populate
  `runtime_indexed_member_cleanup_sibling_fields`; the pipeline now only selects matching typed records while rendering
  the helper body.
- 2026-08-14: Pipeline smoke coverage now directly asserts the emitted
  `runtime_indexed_member_cleanup_sibling_fields` records for the two-sibling `Box.except.item` fixture, pinning owner,
  index expression, moved path, sibling field names, field indexes, LLVM field types, and Drop symbols separately from
  helper-body IR rendering.
- 2026-08-13: DynamicArray runtime-index constructor moves now cover nested scalar sibling reads after a computed
  whole-element move, such as moving `items[index + zero]` and reading `items[1].item.value`.
- 2026-08-13: nested owned member transfer from a runtime-indexed DynamicArray element, such as
  `items[index + zero].item`, remains pinned as rejected until cleanup can prove the moved member type matches the
  cleanup element boundary. Lowering smoke coverage now pins the underlying `type-match false` proof gate.
- 2026-08-13: runtime-index partial-owner metadata now carries moved member paths and exposes a report-only
  member-cleanup plan for future member-granular cleanup proof.
- 2026-08-13: runtime-index member cleanup now has a report-only proof artifact that proves the moved member scope
  while keeping whole-element cleanup and production acceptance blocked.
- 2026-08-13: runtime-index cleanup proof gates now consume member cleanup proof metadata and report
  member-proof-ready/member-blocks-whole-element separately from whole-element type matching.
- 2026-08-13: runtime-index member cleanup now exposes a report-only emission sketch for the future member-granular
  cleanup sequence: skip moved index, drop live sibling members, preserve moved member, then deallocate the owner.
- 2026-08-13: runtime-index member cleanup emission now has a typed gate that keeps production blocked on explicit
  member Drop metadata and member cleanup IR insertion support.
- 2026-08-13: runtime-index member cleanup now derives report-only sibling cleanup target metadata, letting the
  emission gate report member Drop metadata ready while IR insertion remains blocked.
- 2026-08-13: runtime-index member cleanup now has a report-only IR insertion plan naming future anchor, skip,
  sibling-drop, preserve, and exit blocks without emitting IR.
- 2026-08-13: runtime-index member cleanup now has a report-only IR composition plan that validates the planned
  anchor-to-entry, skip, sibling-drop, preserve, and exit topology while production emission remains disabled.
- 2026-08-13: runtime-index member cleanup now has a report-only CFG slice renderer for the verified topology. The
  slice is audit text only and still cannot mutate module IR.
- 2026-08-13: runtime-index member cleanup now has a disabled module-mutation gate that consumes the rendered CFG
  slice and reports missing slice, disabled module mutation, and disabled production member cleanup blockers.
- 2026-08-13: runtime-index member cleanup now has a non-mutating production-readiness report aggregating proof,
  target metadata, CFG slice, module mutation, and production-member-cleanup readiness.
- 2026-08-13: runtime-index member cleanup production-readiness blockers now render stable diagnostic detail lines
  for proof, Drop metadata, CFG slice, module mutation, and production-member-cleanup blockers.
- 2026-08-13: README now records the runtime-index member cleanup production-emission gap analysis: report-only
  readiness is covered, while real member Drop binding, verified rewrite insertion, and production gates remain open.
- 2026-08-13: source-derived record Drop definitions now emit nested source Drop dependencies before the definitions
  that call them, preventing undeclared nested Drop calls in runtime-index cleanup fixtures.
- 2026-08-13: runtime-index source-key rendering now lives in a shared lowering utility with direct smoke coverage.
  The behavior remains unchanged for computed DynamicArray keys and LLVM operand retargeting.
- 2026-08-12: DynamicArray runtime-index cleanup now distinguishes source expression keys from lowered LLVM index
  operands. Computed indexes such as `index + zero` lower on the default path, cleanup skips the lowered `%tmp`
  operand, and reuse diagnostics still report the source key `items[(index + zero)]`.
- 2026-08-12: DynamicArray runtime-index move tracking now has negative coverage for a valid sibling read followed by
  same computed-index reuse. `items[1].value` remains allowed after moving `items[index]`, while later
  `items[index].value` still reports `use after move: items[index]` on the default path.
- 2026-08-12: source-backed DynamicArray runtime-index cleanup now targets the function final block for descriptor
  owners. This preserves valid post-move sibling reads before skip-aware cleanup and deallocation, while fixed-array
  source-slot cleanup keeps its earlier constructor-argument predecessor behavior.
- 2026-08-12: source-backed `DynamicArray<T>` runtime-index constructor moves now lower on the ordinary driver path
  through a production-named verified function-IR rewrite gate. The emitted IR inserts skip-aware runtime cleanup into
  the owning function, drops remaining live elements, deallocates the descriptor, and keeps moved-index reuse rejected.
- 2026-08-12: fixed-array runtime-index constructor moves are enabled on the ordinary driver path with source-slot
  zeroing, while source-backed DynamicArray runtime-index owners use verified function cleanup CFG insertion.
- 2026-08-12: runtime-index cleanup proof/emission no longer implies source-derived Drop declaration/definition
  emission. The broader runtime-index Drop surface is now limited to module-IR insertion, mutation, or function-rewrite
  gates plus an explicit constructor-move run-seam opt-in, keeping the constructor-move cleanup gate narrow for future
  default-promotion work.
- 2026-08-12: default runtime-index constructor-move promotion previously needed a narrower production gate to avoid
  broad source-derived Drop emission. That gate now exists as verified function-IR rewrite insertion.
- 2026-08-12: runtime-index constructor-move production readiness has a non-test CLI report. The checked fixed-array
  and source-backed DynamicArray fixtures now report cleanup proof ready, cleanup emission enabled, constructor-move
  acceptance enabled, and ordinary LLVM emission accepted.
- 2026-08-02: scalar/non-owning `DynamicArray<T>` choice payloads now lower through the finite descriptor ABI
  `{ ptr, i64, i64 }` at return, parameter, annotated `let`, and annotated `var` boundaries. The
  `choice_dynamic_array_payload*.or` fixtures pin the accepted `Buffered.Ready(values: DynamicArray<UInt32>)` ABI and
  constructor payload moves now suppress source descriptor cleanup after insertion into the choice value.
- 2026-08-02: owned-element `DynamicArray<T>` choice payloads now lower through the same descriptor ABI for exact
  descriptor payload variants, and addressable choice owners emit tag-guarded nested descriptor cleanup. The
  `choice_dynamic_array_owned_payload.or` fixture pins `Buffered.Ready(values: DynamicArray<Payload>)` with
  source-backed element drops followed by descriptor deallocation; distinct byte-buffer choice payload storage remains
  outside this cleanup slice.
- 2026-08-02: computed DynamicArray cleanup handoff metadata now records whether cleanup-call authorization came from
  the production local-cleanup plan or the explicit fixture seam, and pipeline audit state exposes counts for each
  origin.
- 2026-08-02: the DynamicArray cleanup audit inserted-handoff summary now renders production and fixture authorization
  counts so enabled cleanup calls can be traced to their proof origin.
- 2026-08-02: production-capable computed DynamicArray cleanup pipeline smokes no longer opt into explicit
  authorization/insertion seam flags; the remaining authorization-only fixture keeps the seam isolated.
- 2026-08-02: test-only computed DynamicArray cleanup report commands now disable production local cleanup insertion
  so they exercise the explicit fixture seam while normal report commands prove the production origin.
- 2026-08-02: internal computed DynamicArray cleanup option fields now use fixture- and metadata-suppression names
  instead of broad `test_only_*` names; the explicit CLI report commands keep their test-only surface names.
- 2026-08-01: DynamicArray cleanup emission capability now has typed pipeline state, and the driver capability report
  renders from that state.
- 2026-08-02: pipeline results no longer expose raw semantic planned-drop, drop-implementation, drop-resolution,
  drop-diagnostic, drop-lowering-authorization, or drop-summary report strings. Semantic-drop consumers render from
  `SemanticDropState`, `SemanticAnalysisResult::planned_drop_sites`, and typed lowering authorizations.
- 2026-08-02: pipeline results no longer duplicate DynamicArray missing element-drop pairs as a standalone top-level
  vector; consumers use typed cleanup availability or cleanup emission capability state.
- 2026-08-02: pipeline results expose computed DynamicArray production sequence module comment IR through a named
  artifact state instead of a bare top-level string vector.
- 2026-08-02: DynamicArray allocation call rendered IR snippets now live under a named IR artifact state nested in the
  allocation call emission state, keeping readiness/count fields separate from artifact text.
- 2026-08-02: the remaining `CompilePipelineResult` string-vector payloads are classified as metadata identifiers,
  diagnostics, link libraries, or named IR artifacts; raw report-vector cleanup at the pipeline boundary is complete.
- 2026-08-02: the complete DynamicArray contract fixture now runs through the canonical pipeline smoke path
  (`--emit-llvm`, `--emit-object`, `run`, and `--build`) to pin receiver methods, append, replacement, projection,
  iteration, owned cleanup, and object/link coverage together.
- 2026-08-02: generic `DynamicArray<Payload>` parameters now specialize on the default path when source Drop proof
  authorizes `values.element`; the callee emits initialized-element drop walks plus descriptor deallocation, while the
  missing-Drop fixture is rejected at the owned-parameter proof boundary.
- 2026-08-02: unannotated generic record constructor locals now carry inferred source types into lowering, so generic
  receiver method specialization can discover calls such as `let box = Box(13 as UInt32); box.value()`. DynamicArray
  receiver methods with additional method-level generics are now pinned through CLI run and LLVM checks.
- 2026-08-02: generic function specialization now infers generic record constructor argument types directly, so
  `value(Box(13 as UInt32))` specializes to `value__UInt32` without requiring a named temporary.
- 2026-08-02: generic function specialization now infers ternary argument source types when both arms resolve to the
  same concrete generic record type, so `value(flag ? Box(...) : Box(...))` specializes and lowers through an aggregate
  phi.
- 2026-08-02: mismatched ternary generic-constructor argument arms, such as `Box<UInt32>` versus `Box<UInt64>`, remain
  unresolved and are pinned as a rejected generic specialization boundary.
- 2026-08-02: that rejected boundary now reports the conflicting ternary arm source types directly instead of falling
  through to a blank generic call return-type mismatch.
- 2026-08-02: generic call-resolution diagnostics now also report ordinary repeated generic-parameter conflicts, such as
  `T` first binding to `UInt32` and later seeing `UInt64`.
- 2026-08-02: ambiguous generic specialization matches now report all matching lowered candidate symbols instead of
  collapsing to a silent no-match.
- 2026-08-02: duplicate top-level functions and duplicate extension receiver/method pairs are now rejected during
  semantic analysis before lowering; the lowerer ambiguity diagnostic remains an internal guard for constructed
  duplicate specialization state.
- 2026-08-02: duplicate interface methods and duplicate implementation `(interface, receiver, method)` entries are now
  rejected during semantic analysis before method signature collection can create ambiguous downstream state.
- 2026-08-02: top-level type declarations now share a semantic namespace across `type`, `record`, `choice`, and
  `interface`; duplicate names are rejected before type and lowering metadata collection.
- 2026-08-02: record fields and choice variants now reject duplicate member names during semantic analysis before
  aggregate and variant metadata collection.
- 2026-08-02: function-like parameter lists and choice variant payload lists now reject duplicate names during semantic
  analysis before signature and variant metadata consumers run.
- 2026-08-02: generic parameter lists and `where` clauses now reject duplicate parameter names, duplicate constrained
  parameters, and duplicate requirements during semantic analysis before generic resolution runs.
- 2026-08-02: imports, constants, source functions, and type declarations now share a semantic top-level name collision
  check, so user-visible declaration names cannot diverge into ambiguous per-kind namespaces.
- 2026-08-02: foreign import local function names now participate in that top-level callable namespace collision check
  while source/foreign function name conflicts keep the existing single duplicate-function diagnostic.
- 2026-08-02: source function symbols plus foreign import/export ABI aliases using the compiler-reserved `__orison_`
  prefix are now rejected before LLVM module-prelude runtime declarations can collide with user-emitted symbols.
- 2026-08-02: semantic name hygiene versus lowering symbol collision risk is summarized in
  `docs/name-hygiene-gap-analysis.md`; the next implementation slice is a lowering-level module symbol registry.
- 2026-08-02: the lowering context now has an initial module symbol registry that rejects C foreign declaration aliases
  colliding with generated generic function specialization symbols or generated method symbols before IR emission.
- 2026-08-02: the lowering symbol registry now also rejects source function symbols colliding with generated generic
  specialization symbols and C foreign export aliases colliding with generated method symbols.
- 2026-08-02: the lowering symbol registry is now a reusable component with constructed-state smoke coverage for
  generated concurrency thunk, generated concurrency cleanup, and planned drop declaration collisions.
- 2026-08-02: production module emission now validates runtime prelude declarations, planned drop declarations,
  generated concurrency thunk/cleanup symbols, and generated DynamicArray cleanup helper symbols through the shared
  lowering module symbol registry before accepting generated LLVM IR.
- 2026-08-02: production module emission now validates emitted record LLVM type identifiers through a separate
  type-symbol namespace in the shared lowering module symbol registry.
- 2026-08-02: import bindings and foreign import local function names now reject duplicates during semantic analysis
  before callable signature collection.
- 2026-08-01: pipeline results no longer expose raw semantic DynamicArray descriptor-origin report strings; pipeline
  consumers render `SemanticAnalysisResult::dynamic_array_descriptor_origins` at the reporting edge.
- 2026-08-01: pipeline results no longer expose raw DynamicArray cleanup production-readiness report strings; pipeline
  consumers use typed DynamicArray cleanup production-readiness state.
- 2026-08-01: pipeline results no longer expose raw consumed descriptor finalization plan report strings; pipeline
  consumers use typed consumed descriptor finalization state.
- 2026-08-01: pipeline results no longer expose raw computed DynamicArray consumed cleanup descriptor or descriptor-model
  report strings; pipeline consumers use typed consumed cleanup descriptor states.
- 2026-08-01: pipeline results no longer expose raw computed DynamicArray inserted cleanup-call report strings; pipeline
  consumers use typed inserted cleanup-call state.
- 2026-08-01: pipeline results no longer expose raw computed DynamicArray inserted cleanup transition or inserted
  cleanup state verification report strings; pipeline consumers use typed inserted-cleanup transition and verification
  states.
- 2026-08-01: pipeline results no longer expose raw computed DynamicArray cleanup-call emission gate, cleanup-call plan,
  cleanup-call render, or cleanup-call insertion readiness report strings; pipeline consumers use typed cleanup-call
  states for those sections.
- 2026-08-01: pipeline results no longer expose raw computed DynamicArray production emission gate or production
  sequence report strings; pipeline consumers use typed production states for those sections.
- 2026-08-01: pipeline results no longer expose raw computed DynamicArray loop-render report strings for descriptor,
  loop-control, element-address, element-load, loop-continue, loop-render-sequence, loop-exit-cleanup, or
  cleanup-transition sections; pipeline consumers use typed states for those sections.
- 2026-08-01: the full DynamicArray cleanup audit now renders computed DynamicArray loop-exit-cleanup and
  cleanup-transition sections from typed pipeline state at the driver edge instead of appending those report vectors
  directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders computed DynamicArray loop-continue and
  loop-render-sequence sections from typed pipeline state at the driver edge instead of appending those report vectors
  directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders computed DynamicArray element-address and element-load
  render sections from typed pipeline state at the driver edge instead of appending those report vectors directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders computed DynamicArray descriptor and loop-control render
  sections from typed pipeline state at the driver edge instead of appending those report vectors directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders dynamic-array production readiness from typed pipeline
  state at the driver edge instead of appending the pipeline report vector directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders computed DynamicArray production emission gates and
  production sequences from typed pipeline state at the driver edge instead of appending those report vectors directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders consumed descriptor finalization plans and computed
  descriptor models from typed pipeline state at the driver edge instead of appending those report vectors directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders cleanup-call plan and render summaries from typed
  pipeline state at the driver edge instead of appending those pipeline report vectors directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders inserted cleanup state verification and cleanup-call
  emission gates from typed pipeline state at the driver edge instead of appending those pipeline report vectors
  directly.
- 2026-08-01: the full DynamicArray cleanup audit now includes the typed cleanup proof summary at the driver edge,
  exposing structured proof counters in the aggregate audit path.
- 2026-08-01: DynamicArray cleanup emission-capability typed state now carries function symbols, and the full cleanup
  audit renders capability lines from typed state instead of the raw pipeline report vector.
- 2026-08-01: lower-level emitted DynamicArray cleanup emission-capability metadata now travels as structured records
  instead of formatted report strings; pipeline state derives function symbols from those records.
- 2026-08-01: lower-level emitted DynamicArray cleanup obligation, sequence-plan, sequence-verification, and
  emission-gate metadata now travels as structured records; pipeline report population owns audit formatting.
- 2026-08-01: DynamicArray descriptor cleanup-plan and cleanup-obligation pipeline state is now typed, and the driver
  renders those audit sections from typed state instead of direct pipeline report vectors.
- 2026-08-01: DynamicArray cleanup sequence-plan and sequence-verification pipeline state is now typed, and the driver
  renders sequence-plan, sequence-verification, and emission-gate audit sections from typed state.
- 2026-08-01: the full DynamicArray cleanup audit now renders inserted cleanup handoffs from typed pipeline state at
  the driver edge instead of appending the inserted-cleanup transition report vector directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders inserted cleanup calls and consumed cleanup descriptors
  from typed pipeline state at the driver edge instead of appending those pipeline report vectors directly.
- 2026-08-01: the full DynamicArray cleanup audit now renders cleanup-call insertion readiness from typed pipeline
  state at the driver edge instead of appending the pipeline report vector directly.
- 2026-07-30: aggregate projection access collection now uses an internal metadata-named option and typed access-plan
  records through lowering/pipeline boundaries; only the explicit CLI report command renders text.
- 2026-07-30: computed owned `DynamicArray<T>` `for` audit collectors now reserve production audit naming for result
  metadata and helper functions. The remaining broad `test_only_dynamic_array_*` snippet fields are classified as
  fixture-only renderers, while fixture construction requests use intent-named fields.
- 2026-08-02: DynamicArray fixture construction request fields now use fixture-named internal API surface instead of
  the prior broad test-only construction-request naming.
- 2026-08-02: DynamicArray cleanup derivation, fixture parameter descriptors, and fixture bound-parameter cleanup
  emission options now use fixture-named internal fields while production enablement keeps production names.
- 2026-07-30: `examples/local_dynamic_array_computed_for.or` now pins computed same-owner `DynamicArray<UInt32>` `for`
  iteration with final-use cleanup through normal CLI run/build coverage.
- 2026-07-30: the computed DynamicArray example smoke now also asserts CLI LLVM/object emission, including computed
  loop labels, enabled cleanup handoffs, the final-use deallocation call, and descriptor finalization.
- 2026-07-30: computed same-owner final-use cleanup now consumes the local cleanup plan after descriptor finalization,
  so the function-exit cleanup hook does not emit a duplicate zero-descriptor cleanup.
- 2026-07-30: `DynamicArray<T>.push(value)` now consumes owned element bindings and owned record-member element paths
  after a successful store, so later reads report `use after move` instead of reusing a value transferred into array
  storage.
- 2026-07-30: local `DynamicArray<T>` indexed assignment now lowers for scalar/non-owning elements with the normal
  bounds-failure branch.
- 2026-07-30: local `DynamicArray<T>` indexed assignment now also lowers owned-element replacement when `items.element`
  has authorized source Drop lowering, ordering the old-element drop before the replacement store and consuming owned
  RHS bindings.
- 2026-07-30: `examples/local_dynamic_array_owned_replacement.or` now pins local owned-element `DynamicArray<Payload>`
  indexed replacement through `orisonc run`, `--emit-llvm`, `--emit-object`, and `--build`, including source drop
  definition, replacement-drop/store ordering, and later cleanup drop coverage.
- 2026-08-11: local owned-element `DynamicArray<T>` RHS move-reuse diagnostics now have checked-in CLI fixtures for
  `.push(payload)`, `.push(box.payload)`, and `items[0] = payload`, pinning later reuse diagnostics for the moved
  source owner.
- 2026-07-30: computed same-owner final-use cleanup now emits a source-backed initialized-element drop walk for owned
  `DynamicArray<Payload>` elements before descriptor deallocation/finalization on the default pipeline path.
- 2026-07-30: `examples/local_dynamic_array_owned_computed_for.or` now pins that owned computed-loop cleanup path
  through CLI `run`, `--emit-llvm`, `--emit-object`, and `--build` coverage.
- 2026-07-30: computed same-owner final-use cleanup now rejects owned `DynamicArray<T>` element cleanup when no
  authorized element Drop exists, preventing descriptor deallocation from bypassing initialized owned elements.
- 2026-07-30: `tests/fixtures/dynamic_array_owned_computed_cleanup_missing_drop.or` now pins that rejection through
  CLI `--emit-llvm` diagnostic coverage.
- 2026-08-11: DynamicArray temp-fixture audit found the remaining `compile_pipeline_smoke.cpp` generated source files
  fall into checked-in user-facing coverage or intentionally pipeline-only metadata/runtime/test-seam coverage. No
  additional surface syntax or spec/tour updates are required.

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, import, type, record, choice, interface, implements, extend, function, public, private, from, as, shared, exclusive, let, var, return, switch, default, guard, if, else, while, for, in, defer, break, continue, repeat, where, `async`, `await`, `task`, `thread`, `and`, `or`, `not`, `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shift_left`, `shift_right`, `true`, and `false` keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, import blocks, type aliases, record declarations, choice declarations, interface declarations, `implements` declarations, `extend` declarations, function headers, and single-line `where` clauses
- parsed top-level visibility modifiers for supported declarations plus field visibility modifiers in records
- parsed choice variants including payload-bearing variants and generic choice parameter lists
- parsed interface method signatures including generic interface parameter lists, `implements Interface for Type` method blocks, and `extend Type` method blocks with method visibility
- parsed record fields, typed parameters, return types, generic `where` constraints, nested generic type syntax, and `shared`/`exclusive`-qualified type names
- statement parsing for `let`, `var`, plain `=` assignment, compound `+=`, `-=`, `*=`, `/=`, and `%=` assignment, `return`, `break`, `continue`, expression statements, inline and block-arm `switch`, `guard ... else`, `if` with an optional `else` block, `while`, `repeat ... while`, `for ... in`, block `unsafe`, and block `defer`
- expression parsing for names, decimal/hex/binary integer literals, string literals, boolean literals, array literals, `task` and `thread` block expressions, unary `-`, `not`, `bit_not`, and `await`, calls, member access, null-safe member access, index access, ternary `?:`, and binary `+`, `-`, `*`, `%`, `/`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `and`, `or`, `bit_and`, `bit_or`, `bit_xor`, `shift_left`, and `shift_right`
- first semantic validation for concurrency surface rules: `await` and `task` now produce frontend diagnostics when used outside `async` functions or methods, `await` now also requires a structural task value or a value produced by a declared `async` call, member-call awaitability no longer falls through the global top-level async-function name bucket, consume-site diagnostics now distinguish `thread`, `task`, and declared-async-call misuse (`await` on thread values, `join()` on task values, and `join()` on declared-async-call results), return sites now also reject forwarding bare thread values and task/declared-async-call values directly, plain-name assignments preserve local provenance, ternary expressions preserve provenance when both branches carry the same structural origin, `if`/`else` plus `switch` branch assignment shaping now merges same-origin provenance back into outer locals, `guard ... else` failure blocks now analyze for diagnostics without leaking failure-path provenance into the continuing path, `switch` constructor-pattern payload names now bind as case-local immutable names in semantics across nested constructor subpatterns, including generic choice payload specialization from the switched value type, and both top-level plus nested call-shaped constructor-pattern head and arity validation are now resolved against the concrete switched choice type or payload choice type instead of a single global variant-name map, while non-constructor value patterns now also reject obvious switched-type mismatches using the same shallow integer compatibility policy already used elsewhere, so same-width integer casts still pass but cases like `switch flag` with a `Text` arm do not, and exact repeated literal value patterns like duplicate `true` or duplicate `"ready"` arms are now rejected explicitly, while mixed value-versus-constructor arm kinds, unknown constructor heads, unsupported constructor payload shapes, duplicate payload bindings, declared-variant arity mismatches, and semantic `default` cardinality/placement violations are rejected, `break` and `continue` now also produce semantic diagnostics when used outside `while`/`repeat`/`for` loops, receiver-only `this`/`This` now also produce semantic diagnostics outside `implements`/`extend` method contexts, declared `unsafe` functions and methods now require an active `unsafe` function or `unsafe` block at call sites, placeholder `Pointer(...)` construction now also requires an active `unsafe` function or `unsafe` block, exactly one source argument, and a structurally address-like source operand, while explicit `Pointer<...>`-typed bindings and pointer-returning functions now also require pointer-like expressions and explicit `Address`-typed bindings plus address-returning functions now also require address-like expressions, with those low-level checks now preferring shallow inferred binding types over raw shape alone, preserving explicit pointee types for `Pointer(...)` in annotated binding and return contexts, and using pointee-aware placeholders for `raw_read`, `raw_write`, `volatile_read`, and `volatile_write` when `Pointer<T>` is already known, unsafe intrinsics like `raw_read`, `raw_write`, `raw_offset`, `address_of`, `volatile_read`, and `volatile_write` now require an active `unsafe` function or `unsafe` block plus structurally valid storage/address operands, `while` and `for` loop shaping now preserve only provenance that survives both the zero-iteration and body-executed paths, and `repeat` loop shaping now preserves provenance established by the guaranteed first body execution, while `task`/`thread` bodies must end in a value-producing statement shape, concurrency bodies reject captures of mutable outer locals and receiver `this`, and the semantics pass now classifies captured outer values with provisional type names plus split marker recognition so `thread` captures and result values require placeholder `Transferable` support while `task` captures and result values still accept placeholder `Transferable` or `Shareable` markers across both generic constraints and concrete implementations
- CLI parse output for import/type/choice/interface/implementation/extension counts, declaration visibility, implementation/extension targets, function `where` constraints, and first-statement nested/alternate/switch-arm counts

### Pending

- additional top-level forms and modifiers from the updated docs
- richer expression, literal, and pattern grammar beyond the current narrow subset
- semantic analysis beyond the current validation subset, full type checking, ownership checking, and backend code generation
- lowering gaps after the current recursive statement path: aggregate construction and assignment outside the pinned
  scalar, record, fixed-array, and source-backed `DynamicArray<T>` paths; computed owned `DynamicArray<T>` iterables
  beyond proven local and bound-parameter same-owner descriptor paths, including nested same-owner ternary leaves;
  mutable `exclusive.View<T>` operations; future standard-library iterator abstractions; and production backend
  completeness beyond the current LLVM/object/link/run smoke paths

## Latest update

- 2026-07-29: computed local same-owner `DynamicArray<T>` cleanup-call insertion now has a conservative last-use
  production gate. Cleanup emission plus a lowered-local descriptor cleanup owner, verified acquire/resume handoff,
  proven cleanup operands, and no later sibling owner reference now enable default deallocation/finalization for the
  final computed local use; repeated-owner blocks keep earlier loops cleanup-call disabled.
- 2026-07-29: computed local same-owner `DynamicArray<T>` last-use proof now carries outer continuation statements
  through nested blocks. A computed loop inside an `if` remains cleanup-call disabled when a later statement after the
  `if` references the owner, while the later final computed loop can still emit cleanup.
- 2026-07-29: computed local same-owner `DynamicArray<T>` last-use proof now covers switch-case continuations and
  conservatively blocks cleanup insertion while lowering inside an active loop body, preventing deallocation before a
  possible later iteration can reuse the owner.
- 2026-07-29: computed local same-owner `DynamicArray<T>` cleanup insertion now has explicit post-loop coverage:
  after a loop exits, a final computed local use can still emit deallocation/finalization when no later owner reference
  exists.
- 2026-07-29: constructed-local nested same-owner `DynamicArray<T>` `for` coverage now mirrors the bound-parameter
  nested same-owner path. `flag ? items : other_flag ? items : items` lowers through the normal local descriptor
  construction/iteration gates with computed cleanup calls still disabled.
- 2026-07-29: constructed-local nested owner-mismatch `DynamicArray<T>` `for` coverage now mirrors the
  bound-parameter rejection boundary. `flag ? items : other_flag ? items : other` stays blocked after local descriptor
  construction and reports every recursive ternary leaf owner.
- 2026-07-29: nested computed `DynamicArray<T>` owner-mismatch coverage now pins the rejection boundary for recursive
  ternary leaves. `flag ? items : other_flag ? items : other` remains blocked with all leaf owners reported.
- 2026-07-29: computed same-owner `DynamicArray<T>` `for` ownership planning now recursively proves nested ternary
  leaves when every reachable branch is the same named descriptor owner with cleanup proof. Mismatched owners and
  non-name computed leaves remain rejected.
- 2026-07-29: computed same-owner bound-parameter `DynamicArray<T>` `for` iterables now lower on the normal path when
  the callee descriptor spill proves the single cleanup owner. The proof feeds computed-loop ownership planning without
  adding an extra cleanup-emission plan, so the function-exit descriptor cleanup remains the only production
  deallocation and computed-loop cleanup-call insertion stays test-only.
- 2026-07-29: computed same-owner local `DynamicArray<T>` `for` iterables now lower on the normal path when the
  descriptor owner is proven by local descriptor cleanup state. The existing production gate and cleanup-transition
  checks are reused, while cleanup-call authorization/insertion remains explicitly test-only.
- 2026-07-29: normal LLVM emission now promotes semantically resolved owned-element `DynamicArray<T>` cleanup sites into
  authorized source-drop lowering for the DynamicArray descriptor path only. This keeps ordinary parsed `Drop` lowering
  gated while allowing source-backed owned-element `DynamicArray<T>` parameters to lower without a test-only option.
- 2026-07-29: driver now exposes computed inserted-cleanup-handoff reports with transition/verification/pairing counts,
  metadata provenance, cleanup-call enablement, and owner acquire/resume detail lines for default and explicitly
  test-only enabled paths.
- 2026-07-28: driver now exposes a dedicated
  `--computed-dynamic-array-cleanup-call-insertion-capability` report so computed cleanup-call insertion authorization
  can be inspected without running the full dynamic-array cleanup audit.
- 2026-07-28: driver smoke coverage now also exercises the explicitly test-only enabled computed cleanup-call
  insertion capability report, keeping default metadata reports disabled while preserving a positive authorization path.
- 2026-07-28: driver now exposes dedicated computed cleanup-call insertion readiness reports with gate counts,
  verification/proof/authorization flags, and owner/operation detail lines for both default and test-only enabled paths.
- 2026-07-28: driver now exposes computed inserted-cleanup-call state reports with call/proof counts plus owner,
  data-pointer, and capacity detail lines for default absent and explicitly test-only inserted paths.
- 2026-07-28: driver now exposes computed consumed-cleanup-descriptor state reports with descriptor/proof counts plus
  owner and descriptor-storage detail lines for default absent and explicitly test-only finalized paths.
- 2026-07-28: computed cleanup report command dispatch now uses shared driver helpers for collector-backed and
  emission-backed single-file reports, reducing duplicate CLI routing without changing report behavior.
- 2026-07-28: computed cleanup state report formatting now uses shared driver summary/detail helpers for insertion
  readiness, inserted cleanup calls, and consumed cleanup descriptors while preserving existing report text.
- 2026-07-28: computed cleanup state report formatting now lives in a dedicated driver report component, keeping
  `compiler_app.cpp` focused on CLI command orchestration while preserving existing report text.
- 2026-07-28: computed cleanup driver report formatting now has focused smoke coverage that constructs typed pipeline
  states directly, pinning formatter output independently from full CLI execution.
- 2026-07-28: computed cleanup driver report smoke coverage now pins `<unknown>` detail fallbacks when optional
  operation, data, capacity, or descriptor vectors are shorter than cleanup-owner vectors.
- 2026-07-29: driver now exposes computed cleanup proof summary reports with model, verified-pair, structured-proof,
  and IR-fallback counters for default and explicitly test-only inserted cleanup paths.
- 2026-07-28: computed dynamic-array cleanup-transition metadata now populates typed pipeline state with transition
  counts, pairing readiness, function names, owners, source/element types, acquisition owners/operations, and
  resumption owners/operations.
- 2026-07-28: computed dynamic-array loop-exit cleanup metadata now populates typed pipeline state with cleanup counts,
  snippet counts, cleanup-resumption readiness, function names, owners, source/element types, exit blocks, loop-entry
  and loop-exit cleanup owners, and cleanup-resumption operation names.
- 2026-07-28: computed dynamic-array loop-render sequence metadata now populates typed pipeline state with sequence
  counts, snippet counts, body-block readiness, function names, owners, source/element types, and loop body block names.
- 2026-07-28: computed dynamic-array loop-continue render metadata now populates typed pipeline state with render
  counts, snippet counts, loop-continue input readiness, function names, owners, source/element types, continue and
  condition blocks, and current/next index names.
- 2026-07-28: computed dynamic-array element-load render metadata now populates typed pipeline state with render counts,
  snippet counts, element-load input readiness, function names, owners, source/element types, lowered LLVM element
  types, element-address names, and loaded item value names.
- 2026-07-28: computed dynamic-array element-address render metadata now populates typed pipeline state with render
  counts, snippet counts, element-address input readiness, function names, owners, source/element types, lowered LLVM
  element types, data pointers, loop indexes, and element-address names.
- 2026-07-28: computed dynamic-array loop-control render metadata now populates typed pipeline state with render counts,
  snippet counts, control-flow-name readiness, function names, owners, source/element types, block labels, loop indexes,
  and bounds-check names.
- 2026-07-28: computed dynamic-array descriptor-render metadata now populates typed pipeline state with render counts,
  snippet counts, projection readiness, function names, owners, source/element types, descriptor storage, and projected
  descriptor value/data/length/capacity names.
- 2026-07-28: computed consumed-cleanup descriptor model metadata now populates typed pipeline state with descriptor
  model counts, finalization readiness counts, function names, owners, descriptor storage, cleanup operations, source
  types, and element types.
- 2026-07-28: computed inserted-cleanup transition and state-verification reports now also have separate typed
  pipeline states, so transition endpoints and verification outcomes are available without parsing audit strings.
- 2026-07-28: computed `DynamicArray<T>` `for` production readiness now consumes typed inserted-cleanup transition
  and state-verification state, including sequence/transition and transition/verification count checks.
- 2026-07-28: computed `DynamicArray<T>` `for` smoke coverage now runs production-sequence collection and inserted
  cleanup lowering together, proving the typed sequence/transition and transition/verification count matches.
- 2026-07-28: computed `DynamicArray<T>` `for` production-emission readiness now turns on only when emission
  metadata is collected under the explicit test-only cleanup-call authorization plus insertion gate, with pipeline
  smoke coverage.
- 2026-07-28: computed cleanup-call insertion now has a named internal capability object, consumed by production
  emission metadata, runtime deallocate declaration registration, and the actual test-only insertion branch.
- 2026-07-28: computed cleanup-call insertion capability now also populates pipeline-visible typed state, including
  disabled, authorization-only, and fully enabled smoke coverage.
- 2026-07-28: computed `DynamicArray<T>` `for` production readiness now consumes the pipeline-visible cleanup-call
  insertion capability for production-emission enablement, while gate/sequence metadata remains required evidence.
- 2026-07-28: driver dynamic-array cleanup audit output now includes computed cleanup-call insertion capability state,
  so CLI users can see disabled authorization/insertion without inferring it from downstream reports.
- 2026-07-28: computed inserted-cleanup handoff verification now populates typed pipeline state with
  transition/verification counts, paired/blocked counts, metadata provenance, cleanup-call enablement, owners, and
  acquire/resume operation names from proof-model events.
- 2026-07-28: computed cleanup-proof summary state is now the sole pipeline API for proof counters; the legacy scalar
  counter fields and compatibility parity helper have been removed.
- 2026-07-28: computed cleanup-call emission gates now populate typed pipeline state with gate counts,
  ready-vs-blocked counts, state-verification and cleanup-call enablement booleans, owners, and handoff operation names.
- 2026-07-28: computed cleanup-call plan/render reports now populate typed pipeline state with plan/render counts,
  renderability, aggregate operand readiness, cleanup-call enablement, cleanup owners, operations, and operands.
- 2026-07-28: computed cleanup-call insertion gates now populate typed pipeline state with gate counts, ready-vs-blocked
  counts, aggregate readiness booleans, cleanup owners, and cleanup operation names from proof-model decisions.
- 2026-07-28: computed inserted cleanup calls now populate typed pipeline state with call counts, structured-vs-IR
  proof counts, cleanup owners, and deallocation operand names from proof-model events.
- 2026-07-28: computed consumed cleanup descriptors now populate typed pipeline state with descriptor counts,
  structured-vs-IR proof counts, owner names, and descriptor storage names from proof-model events.
- 2026-07-28: consumed descriptor finalization now also populates typed pipeline state with computed descriptor,
  emitted finalization, ready, and blocked plan counts plus owner/descriptor names.
- 2026-07-28: consumed descriptor finalization now exposes typed readiness state, and cleanup emission plus computed
  descriptor metadata collection branch on that state instead of a boolean-only finalization helper.
- 2026-07-28: test-only computed dynamic-array loop insertion now uses shared typed lowering predicates for production
  gate readiness and cleanup-transition consistency instead of duplicating local boolean chains.
- 2026-07-28: computed dynamic-array production readiness now composes typed gate and sequence state, checking gate
  readiness, sequence availability, count/snippet/owner consistency, and production-emission enablement without report
  parsing.
- 2026-07-28: computed dynamic-array production-emission gate metadata now populates typed pipeline state with
  ownership, loop-render, cleanup-resumption, exit-cleanup, sequence-planning, and emission-enable booleans.
- 2026-07-28: computed dynamic-array production-sequence metadata now populates typed pipeline state with sequence,
  snippet, comment-emission, and cleanup-owner counts so downstream consumers do not need to parse audit strings.
- 2026-07-28: dynamic-array cleanup production readiness now consumes typed cleanup availability state populated from
  semantic and lowering metadata instead of deriving availability from formatted report vectors.
- 2026-07-28: computed cleanup-call report inputs now live as structured proof events, with the private report
  companion rendering plan, render, insertion-gate, inserted-call, and consumed-descriptor audit text from those events.
- 2026-07-28: inserted cleanup state analysis now stores structured transition and verification events, with the
  private report companion rendering those events into audit text for pipeline consumers.
- 2026-07-27: inserted cleanup state transition and verification report formatting now also lives in the private
  computed cleanup proof report companion, leaving the core model with parsing, analysis, and proof construction.
- 2026-07-27: computed cleanup proof report formatting now lives in a private companion module, keeping the core proof
  model focused on typed state while preserving the prebuilt report bundle for pipeline consumers.
- 2026-07-27: the computed cleanup-proof model now carries its cleanup-call report bundle, so pipeline report population
  copies prebuilt proof reports instead of assembling computed cleanup report inputs manually.
- 2026-07-27: the computed cleanup-proof model now exposes aggregate summary counts, so pipeline report population
  reads proof-model counters instead of deriving structured/fallback counts from raw emission metadata.
- 2026-07-27: verified computed cleanup calls now cache insertion, inserted-call, and consumed-descriptor decisions at
  proof-model construction time so downstream reports and counters read stable proof state.
- 2026-07-27: inserted computed cleanup-call and consumed-descriptor proof checks now flow through typed proof-model
  decisions, keeping report generation and fallback counters out of proof ownership.
- 2026-07-27: computed cleanup-call insertion readiness is now exposed as a typed proof-model decision, so report
  formatting consumes reusable compiler state instead of recomputing gate booleans locally.
- 2026-07-27: pipeline smoke coverage now consumes the reusable computed cleanup-proof model directly, proving the
  typed proof bundle can be used outside lowering-emission report formatting.
- 2026-07-26: computed owned `DynamicArray<T>` cleanup-call emission now has an internal verifier-driven gate that
  remains blocked while inserted cleanup state handoffs declare cleanup calls disabled.
- 2026-07-26: verified computed owned `DynamicArray<T>` cleanup-call gates now expose a disabled cleanup-call plan that
  records the post-resumption operation seam while descriptor cleanup operands remain pending.
- 2026-07-26: computed owned `DynamicArray<T>` cleanup-call plans initially derive available operands from inserted IR,
  proving data pointer and scalar element-size operands before capacity projection is added.
- 2026-07-26: computed owned `DynamicArray<T>` descriptor rendering now projects capacity for proven computed loops, so
  cleanup-call plans can prove data pointer, scalar element size, and capacity while cleanup emission remains disabled.
- 2026-07-26: computed owned `DynamicArray<T>` cleanup-call rendering now formats the exact disabled
  `__orison_dynamic_array_deallocate(ptr, i64, i64)` call from proven operands while keeping module IR unchanged.
- 2026-07-26: computed owned `DynamicArray<T>` cleanup-call insertion now has an explicit gate requiring verified
  inserted state, proven operands, and cleanup-call authorization before module IR insertion is allowed.
- 2026-07-26: a test-only computed owned `DynamicArray<T>` cleanup-call authorization option now flips inserted
  handoffs to enabled so the insertion gate can report ready without enabling production cleanup-call insertion.
- 2026-07-26: pipeline emission reports now verify computed owned `DynamicArray<T>` inserted cleanup state handoff
  sequences, reporting valid acquire/resume pairs separately from inserted-transition observability.
- 2026-07-26: computed owned `DynamicArray<T>` cleanup acquisition/resumption markers now render through a shared
  internal cleanup state handoff model while still emitting no cleanup calls, deallocation, or element drops.
- 2026-07-26: pipeline emission reports now derive a computed owned `DynamicArray<T>` inserted cleanup-transition line
  from actual test-only lowered IR markers, separate from metadata-only cleanup-transition readiness.
- 2026-07-26: test-only computed owned `DynamicArray<T>` `for` lowering now checks the paired cleanup transition before
  insertion and emits the disabled loop-entry cleanup-acquisition marker ahead of descriptor rendering.
- 2026-07-26: computed owned `DynamicArray<T>` `for` cleanup-transition metadata now pairs the disabled loop-entry
  cleanup acquisition with the disabled loop-exit cleanup resumption in lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` cleanup sequencing now names the disabled loop-entry cleanup-acquisition
  operation and requires it before production gates report loop cleanup ownership ready.
- 2026-07-26: computed owned `DynamicArray<T>` loop-exit cleanup now renders a named disabled cleanup-resumption
  operation, preserving audit visibility of the loop-owner-to-function-owner transition without enabling cleanup calls.
- 2026-07-26: computed owned `DynamicArray<T>` `for` production-emission gates now report and require explicit loop
  cleanup-ownership plus function cleanup-resumption readiness before test-only executable insertion.
- 2026-07-26: computed owned `DynamicArray<T>` `for` executable test-only lowering now uniquifies emitted internal
  labels and SSA names, with smoke coverage for two computed loops over the same local owner in one function.
- 2026-07-26: computed owned `DynamicArray<T>` `for` lowering now uses the actual incoming block for loop-control phi
  construction, with smoke coverage for insertion after a lowered `if`/`else` merge.
- 2026-07-26: computed owned `DynamicArray<T>` `for` lowering now has a first test-only executable insertion path for
  the proven local same-owner entry-block case, with default lowering still rejecting computed owned iterables.
- 2026-07-26: computed owned `DynamicArray<T>` production-sequence snippets now have a test-only metadata-IR comment
  emission seam, keeping executable lowering disabled while proving the output boundary is wired.
- 2026-07-26: computed owned `DynamicArray<T>` smoke coverage now shares canonical local same-owner audit expectation
  strings across lowering, pipeline, driver, and examples tests to keep report parity assertions synchronized.
- 2026-07-26: computed owned `DynamicArray<T>` report formatting and rendered-snippet aggregation now share internal
  lowering helpers, preserving existing audit text while reducing repetition before the next production-emission seam.
- 2026-07-26: pipeline lowering-emission reports now carry computed owned `DynamicArray<T>` `for`
  production-sequence report lines, including intentionally rejected lowering attempts and audit output.
- 2026-07-26: driver CLI smoke coverage now pins `--dynamic-array-cleanup-audit` output for computed local same-owner
  `DynamicArray<T>` `for` production-sequence report lines end-to-end.
- 2026-07-26: computed owned `DynamicArray<T>` `for` descriptor-render metadata is now surfaced separately from the
  full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` descriptor-render and production-sequence collectors now share
  traversal and local descriptor-binding setup while keeping their report artifacts separate.
- 2026-07-26: computed owned `DynamicArray<T>` `for` loop-control-render metadata is now surfaced separately from the
  full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` element-address-render metadata is now surfaced separately from
  the full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` element-load-render metadata is now surfaced separately from the
  full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` loop-continue-render metadata is now surfaced separately from the
  full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` loop-render-sequence metadata is now surfaced separately from the
  full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` loop-exit-cleanup metadata is now surfaced separately from the
  full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` production-emission-gate metadata is now surfaced separately from
  the full production sequence through lowering, pipeline, and driver audit reports.
- 2026-07-26: computed owned `DynamicArray<T>` `for` production-sequence metadata now has a report formatter that
  surfaces function, line, owner, source type, element type, and snippet count for audit consumers.
- 2026-07-26: computed owned `DynamicArray<T>` `for` production-sequence metadata now records per-gate provenance:
  function name, source line, cleanup owner, source type, element type, and aggregated snippets.
- 2026-07-26: LLVM emission now has a production audit metadata collector for ready computed owned `DynamicArray<T>`
  `for` production gates. The collector records aggregated snippets on the result but still keeps them out of module
  IR.
- 2026-07-26: computed owned `DynamicArray<T>` production-emission gates now expose the aggregated loop-render and
  exit-cleanup snippets for ready gates as an internal seam, while keeping production emission disabled.
- 2026-07-26: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the production-emission gate
  report. The report surfaces ownership, loop-render, and exit-cleanup readiness while keeping production emission
  disabled.
- 2026-07-25: computed owned `DynamicArray<T>` loop lowering now has a disabled production emission gate. The gate
  consumes ownership, composed loop rendering, and loop-exit cleanup readiness while still keeping production emission
  disabled.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the loop-exit cleanup report.
  The report surfaces exit-block and cleanup-resumption readiness without enabling cleanup emission.
- 2026-07-25: computed owned `DynamicArray<T>` loop exit cleanup now has a disabled internal plan. It records the
  future loop exit block and function cleanup ownership resumption without enabling cleanup sequence emission.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the composed loop render
  sequence report. The report surfaces descriptor, control, body, element, and continuation readiness without enabling
  render emission.
- 2026-07-25: computed owned `DynamicArray<T>` loop rendering now has a composed disabled internal sequence plan. It
  aggregates descriptor rendering, loop control, the body block label, element addressing, item loading, and loop
  continuation without enabling render emission.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the loop-continue render report.
  The report distinguishes blocked, unproven, unlowerable, and planned-but-disabled loop continuation without enabling
  render emission.
- 2026-07-25: computed owned `DynamicArray<T>` loop-continue rendering now has a disabled internal plan. Proven element
  loads can record the future continue block, next-index increment, and condition backedge while keeping render emission
  disabled.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the element-load render report.
  The report distinguishes blocked, unproven, unlowerable, and planned-but-disabled loop item loading without enabling
  render emission.
- 2026-07-25: computed owned `DynamicArray<T>` element-load rendering now has a disabled internal plan. Proven element
  addresses can record the future loop item value and scalar `load` while keeping render emission disabled.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the element-address render
  report. The report distinguishes blocked, unproven, unlowerable, and planned-but-disabled element address rendering
  without enabling render emission.
- 2026-07-25: computed owned `DynamicArray<T>` element-address rendering now has a disabled internal plan. Proven
  loop-control renders can record the future element LLVM type, descriptor data pointer, loop index, and element
  `getelementptr` while keeping render emission disabled.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the loop-control render report.
  The report distinguishes blocked, unproven, and planned-but-disabled entry branch, index phi, bounds check, and loop
  branch rendering without enabling render emission.
- 2026-07-25: computed owned `DynamicArray<T>` loop-control rendering now has a disabled internal plan. Proven
  descriptor renders can record the future entry branch, condition/body/continue/exit blocks, index phi, bounds check,
  and conditional branch while keeping render emission disabled.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the descriptor render report.
  The report distinguishes blocked, unproven, and planned-but-disabled descriptor load/projection rendering without
  enabling render emission.
- 2026-07-25: computed owned `DynamicArray<T>` descriptor rendering now has a disabled internal plan. Proven same-owner
  handoffs can record the future descriptor load plus data and length projections while keeping render emission
  disabled.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the cleanup sequence report.
  The report distinguishes blocked, unproven, and planned-but-disabled loop cleanup ownership without enabling cleanup
  sequence emission.
- 2026-07-25: computed owned `DynamicArray<T>` cleanup sequencing now has a metadata-only internal plan. Proven
  same-owner descriptor handoffs can record loop-entry cleanup ownership and function cleanup resumption while keeping
  cleanup sequence emission disabled.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the descriptor handoff report.
  Same-owner local descriptors can report a planned handoff owner and descriptor storage, but the diagnostic still
  records `[lowering disabled]`.
- 2026-07-25: computed owned `DynamicArray<UInt32>` descriptor handoff now has a metadata-only internal plan. It
  distinguishes ownership-join blockers, cleanup-owner proof blockers, and the positive same-owner local descriptor
  handoff state while keeping computed owned loop lowering disabled.
- 2026-07-24: computed `View<T>` descriptor-loop lowering is now pinned for helper-returned
  `shared.View<UInt32>` values. The lowered loop consumes the computed `{ ptr, i64 }` descriptor directly and keeps
  owned `DynamicArray<T>` iteration restricted to named descriptor storage.
- 2026-07-24: method-returned `shared.View<UInt32>` descriptor-loop lowering is now pinned through scalar receiver
  method calls. The same computed descriptor path handles `seed.forward_view(values)` without adding syntax or
  changing owned `DynamicArray<T>` restrictions.
- 2026-07-24: method-returned `shared.View<UInt32>` descriptor-loop lowering now also covers member-derived receivers:
  `wrapper.bucket.forward_view(values)` and `shelf.buckets[0].forward_view(values)` both lower through the computed
  descriptor loop and preserve the owned `DynamicArray<T>` boundary.
- 2026-07-24: computed owned `DynamicArray<UInt32>` iterables are now explicitly rejected when no named descriptor
  owner exists. The fixture `dynamic_array_computed_iterable_rejected.or` pins `for word in flag ? left : right` as
  requiring a named descriptor iterable until ownership, cleanup, and descriptor-storage rules are proven.
- 2026-07-24: `exclusive.View<T>` parameter descriptors now lower checked indexed element assignment. The descriptor
  path projects data/length, traps through `__orison_dynamic_array_bounds_failed()` on out-of-bounds indexes, and
  stores the scalar element without adding new source syntax.
- 2026-07-24: CLI and pipeline final control-flow failure tests now pin structured arm/case detail. The driver,
  compile-pipeline, and drop-report smoke paths require `if then arm lowering failed: unsupported operator: <` and
  `switch case lowering failed: unsupported operator: <`.
- 2026-07-24: CLI and pipeline return-expression failure tests now pin structured cast detail for negative unsigned
  literal casts. The driver, compile-pipeline, and drop-report smoke paths require
  `unsupported cast: negative value to UInt32`.
- 2026-07-24: CLI and pipeline return-expression failure tests now also pin unary operator detail for unsupported
  unsigned negation. The driver, compile-pipeline, and drop-report smoke paths require `unsupported operator: -`
  instead of only proving comparison-operator detail.
- 2026-07-24: CLI and pipeline return-expression failure tests now pin the structured expression detail for unsupported
  operators. The driver, compile-pipeline, and drop-report smoke paths require `unsupported operator: <` instead of only
  the generic return-expression prefix.
- 2026-07-24: CLI aggregate assignment diagnostics now explicitly pin the semantic/lowering boundary. The driver
  aggregate-lowering smoke confirms scalar member/index assignment targets remain semantic errors and do not leak the
  lowerer-only aggregate assignment path failure messages.
- 2026-07-24: aggregate assignment target diagnostics now reuse the shared aggregate-path error renderer. Statement
  smoke coverage pins scalar member/index assignment targets as `expected record` and `expected array` failures instead
  of generic unsupported target messages.
- 2026-07-24: aggregate path expression lowering now records a structured `unsupported aggregate path` failure for
  member/index layout failures instead of flattening them into generic unsupported-expression details. The expression
  emitter smoke pins `address_of(device.missing)` as `address_of member path: unknown field`.
- 2026-07-24: choice-constructor expression lowering now records a structured `unsupported choice ABI` failure when a
  matched choice layout lacks a usable payload ABI. `orison_expression_emitter_smoke` pins the direct failure detail so
  callers can append a specific ABI reason instead of losing context behind a generic expression rejection.
- 2026-07-24: unsupported choice payload ABI diagnostics now use a shared lowering diagnostic helper across function
  and statement emitters. Return, parameter, annotated `let`, and annotated `var` boundaries keep the same pinned
  diagnostics; assignment/reassignment cannot yet reach an unsupported choice ABI value because those boundary checks
  reject the value before mutable storage exists.
- 2026-07-24: unsupported choice payload ABI diagnostics were pinned for descriptor-backed payloads at return,
  parameter, annotated `let`, and annotated `var` boundaries. This historical rejection was superseded on 2026-08-02
  for scalar/non-owning `DynamicArray<T>` descriptor payloads; owned-element descriptor payload cleanup inside choices
  remains outside the accepted boundary.
- 2026-07-24: nested fixed arrays of records containing multi-payload choice fields are now pinned through nested
  array literals, multi-level indexed record-field assignment, calls, and downstream indexed field-read payload
  recovery. `local_result_multi_payload_choice_nested_array_record.or` exercises `Array<Array<Holder, 2>, 2>` where
  `Holder.result` is `Result<UInt32>`.
- 2026-07-24: fixed arrays of records containing multi-payload choice fields are now pinned through array literals,
  indexed record-field assignment, calls, and downstream indexed field-read payload recovery.
  `local_result_multi_payload_choice_array_record.or` exercises `Array<Holder, 2>` where `Holder.result` is
  `Result<UInt32>`.
- 2026-07-24: nested record-field fixed arrays containing multi-payload choice values are now pinned through record
  construction, nested indexed assignment, calls, and downstream nested indexed-read payload recovery.
  `local_result_multi_payload_choice_record_array.or` exercises `Holder.results: Array<Result<UInt32>, 2>`.
- 2026-07-24: fixed arrays containing multi-payload choice values are now pinned through array literals, indexed
  assignment, function calls, and downstream indexed-read payload recovery. `local_result_multi_payload_choice_array_element.or`
  exercises `Array<Result<UInt32>, 2>` values whose elements use the finite tagged payload-buffer ABI.
- 2026-07-24: record fields containing multi-payload choice values are now pinned through construction, field
  assignment, and field-access `switch` recovery. `local_result_multi_payload_choice_record_field.or` exercises a
  `Holder.result: Result<UInt32>` field across ternary record construction, mutable field stores, and payload-pattern
  recovery from `holder.result`.
- 2026-07-24: mutable local reassignment is now pinned for multi-payload choice values across `if` and `switch`
  branches. `local_result_multi_payload_choice_reassignment.or` exercises reassignment into a `Result<UInt32>` local
  before downstream payload-pattern recovery.
- 2026-07-24: multi-payload choice values are now pinned through final ternary and `switch` branch production. The
  checked-in `local_result_multi_payload_choice_branch_flow.or` example exercises branch-produced `Result<UInt32>`
  values through function calls and downstream payload-pattern `switch` recovery.
- 2026-07-24: expected-choice semantic validation now distinguishes ordinary declared function calls from choice
  constructor calls. `consume(make_ok())` where `make_ok() -> Result<UInt32>` no longer reports an unknown
  constructor, and `local_result_multi_payload_choice_function_flow.or` pins multi-payload choice values across
  function returns, parameters, calls, and downstream `switch` recovery.
- 2026-07-24: concrete choice variants with multiple fixed payload fields now lower by first building a per-variant
  payload aggregate and then storing that aggregate in the existing tagged payload-buffer choice ABI when needed.
  `examples/local_result_multi_payload_choice_switch.or` pins generic `Result<UInt32>` construction plus explicit
  `switch` recovery of both payload bindings through the backend.
- 2026-07-20: pipeline/lowering boundary review after the pipeline facade extraction found the next highest-payoff
  implementation gap is not another facade split. The current default backend path covers local `DynamicArray<UInt32>`,
  scalar/non-owning `DynamicArray<T>` parameters, and read-only `View<T>` descriptor length/index/iteration. The
  remaining dynamic-sequence blocker is owned-element `DynamicArray<T>` parameters on the production path: lowering
  already has an authorization seam keyed to `owner.element`, but default compilation still rejects those parameters
  until source-derived Drop proof, initialized-element cleanup ordering, and descriptor deallocation are all proven
  together.
- 2026-07-20: `tests/fixtures/dynamic_array_owned_parameter_source_drop.or` now pins the first source-backed positive
  owned-element `DynamicArray<T>` parameter path. With the production-facing source Drop lowering gate enabled,
  `implements Drop for Payload` authorizes `items.element`, the parameter lowers as `{ ptr, i64, i64 }`, and generated
  IR orders initialized-element drop calls before dynamic-array descriptor deallocation and return.
- 2026-07-30: source-backed owned-element `DynamicArray<T>` parameter lowering now runs on the default compile path
  when semantic Drop proof authorizes the `owner.element` cleanup site. Pipeline smoke pins initialized-owner transfer
  and forwarding through descriptor ABI emission, single callee-side descriptor deallocation, and linked execution.
- 2026-07-30: `examples/dynamic_array_owned_parameter.or` is the checked-in CLI demo for owned-element
  `DynamicArray<Payload>` parameter transfer. Array CLI smoke now covers `run`, `--emit-llvm`, `--emit-object`, and
  retained `--build` executable paths for the default source-backed cleanup path.
- 2026-07-30: owned-element `DynamicArray<T>` parameter descriptor `.length()` is now pinned on the default path.
  The checked-in owned-parameter demo returns success only after reading the callee descriptor length, then emits
  source-backed element drops and a single descriptor deallocation.
- 2026-07-30: owned-element `DynamicArray<T>` parameter checked index reads are now pinned on the default path for
  aggregate elements. The owned-parameter demo validates bounds checks, `%record.Payload` loads, field extraction, and
  cleanup ordering before returning success.
- 2026-07-30: owned-element `DynamicArray<T>` parameter descriptor `for` iteration is now pinned on the default path.
  The owned-parameter demo validates descriptor loop control, `%record.Payload` loop-item loads, record-field reads,
  length checks, and source-backed cleanup ordering.
- 2026-08-11: owned-element `DynamicArray<T>` parameter branch-transfer behavior now has checked-in CLI fixtures.
  `tests/fixtures/dynamic_array_owned_parameter_branch_join_run.or` pins the valid all-branches-transfer case, while
  `tests/fixtures/dynamic_array_owned_parameter_branch_mismatch_rejected.or` pins the mismatch diagnostic when only one
  continuing branch transfers the owned descriptor.
- 2026-08-11: owned-element `DynamicArray<T>` parameter forwarding now has checked-in CLI fixtures. Straight
  forwarding links and runs through `tests/fixtures/dynamic_array_owned_parameter_forwarding_run.or`; forwarding then
  reading the moved parameter reports `use after move: items`.
- 2026-08-11: statement-level owned-parameter branch mismatch now has checked-in CLI coverage. Moving `items` in one
  branch and reading it after the branch reports the continuing-branch ownership mismatch.
- 2026-08-11: owned-element `DynamicArray<T>` parameter post-move diagnostics now have checked-in CLI fixtures for a
  second owned call, descriptor `.length()` after move, and descriptor `.push(...)` after move. Each fixture pins
  `use after move: items` on the default CLI path.
- 2026-07-30: `tests/fixtures/dynamic_array_owned_parameter_iteration_missing_drop.or` now pins the negative CLI
  boundary for owned-element parameter iteration without source-backed Drop proof. The driver still rejects the
  unproven `items.element` cleanup site before production lowering.
- 2026-07-30: DynamicArray backend gap review after owned-parameter read coverage: default-path coverage now spans
  local scalar construction/append/grow/length/index/for/cleanup, source-backed owned local replacement and computed
  final-use cleanup, scalar/non-owning parameter descriptors, and authorized owned-parameter transfer/read/iteration
  with callee cleanup. Remaining implementation gaps are parameter mutation policy (`push` and indexed assignment on
  parameter descriptors), broader generic DynamicArray lowering beyond concrete examples, productionizing the remaining
  computed-cleanup authorization/insertion seams, and fuller runtime/allocator surface beyond allocate/grow/deallocate
  plus bounds failure.
- 2026-07-30: DynamicArray parameter mutation policy is now pinned as an explicit rejection boundary. Bound
  `DynamicArray<T>` descriptors remain readable/iterable/cleaned up, while `items[index] = value` and
  `items.push(value)` on parameters stay out of lowering; mutable parameter-style element writes remain represented by
  the existing `exclusive.View<T>` indexed-assignment path.
- 2026-08-11: DynamicArray parameter mutation diagnostics now report that explicit policy rather than generic
  assignment/member-call lowering failures. Indexed assignment points to `exclusive.View<T>` and push points to an
  owned local descriptor or view-style mutable element writes.
- 2026-07-30: Generic `DynamicArray<T>` function use now has an explicit CLI boundary fixture. A generic function
  `first<T>(values: DynamicArray<T>) -> T` can parse, but instantiating it from a concrete `DynamicArray<UInt32>`
  call still fails before backend emission with an unconcretized return-type mismatch. This pins the current gap as
  generic function monomorphization/type substitution rather than descriptor read/index lowering.
- 2026-07-30: The first generic function specialization slice is now lowered for a same-module call with named
  arguments carrying known source types. `tests/fixtures/dynamic_array_generic_parameter.or` proves
  `first<T>(values: DynamicArray<T>) -> T` specializes to `first__UInt32`, accepts a `DynamicArray<UInt32>` descriptor,
  performs a checked index read, returns `UInt32`, transfers cleanup to the callee, and links/runs through the generic
  CLI smoke. Broader generic dispatch, multiple simultaneous instantiations, non-name argument inference, methods, and
  generic owned-element Drop proof remain pending.
- 2026-07-30: Generic function call dispatch now selects a matching concrete specialization by argument source types
  at the call site. The DynamicArray generic fixture now emits and runs both `first__UInt32` and `first__UInt64` in the
  same module, with each call using the matching descriptor element width, bounds check, return type, and callee-side
  descriptor cleanup. Remaining generic gaps are non-name argument inference, ambiguous overload diagnostics, methods,
  and generic owned-element Drop proof.
- 2026-07-20: source-derived finite Drop implementations now emit narrow no-op LLVM ABI bodies such as
  `define void @__orison_drop.Payload(ptr %value)` for proven empty/naked-return `implements Drop` bodies. Pipeline
  smoke now links and runs the source-drop owned-parameter path for an empty local `DynamicArray<Payload>` passed to
  `use_items`. Initialized owned transfer still needs move/consume analysis before the caller can safely suppress local
  descriptor cleanup after passing ownership to the callee.
- 2026-07-20: straight-line owned local `DynamicArray<T>` calls now mark exact source-typed local arguments as consumed
  when passed to owned dynamic-array parameters, and local cleanup planning skips those transferred owners. Pipeline
  smoke now links and runs an initialized `DynamicArray<Payload>` local passed to `use_items` with only the callee-side
  descriptor cleanup remaining. Broader ownership diagnostics for post-move uses, branch joins, and parameter
  forwarding remain pending.
- 2026-07-20: straight-line post-move diagnostics now reject additional uses of a consumed owned local
  `DynamicArray<T>` through repeated owned calls, `length()` reads, and `push(...)` mutation. This remains a local
  descriptor ownership check rather than full borrow/move analysis across branch joins or parameter forwarding.
- 2026-07-21: owned `DynamicArray<T>` parameter forwarding now uses the same consumed-binding tracking as local moves:
  forwarding an owned parameter to another owned-parameter function suppresses cleanup in the forwarding function, while
  later parameter reuse is rejected with `use after move`. Branch-join ownership analysis remains pending.
- 2026-07-21: final and non-value branch joins now require matching consumed owned `DynamicArray<T>` binding state
  across every continuing arm/case. Matching moves in all arms remain valid and suppress post-branch cleanup, while
  move/no-move branch mismatches are rejected instead of emitting unconditional cleanup or suppression.
- 2026-07-21: consumed owned binding tracking now lives in a reusable lowering ownership-transfer state instead of a
  dynamic-array-specific function state field. `DynamicArray<T>` remains the first producer/consumer, but branch joins,
  post-move diagnostics, and cleanup suppression now depend on a generic consumed-owned-binding API.
- 2026-07-21: reusable ownership-transfer metadata now identifies owned record fields and choice payloads by stable
  member keys such as `box.payload` and `maybe.Some.value`. Lowered record-field reads consult those keys and report
  use-after-move before emitting aggregate-load IR; source-level aggregate move production remains future work.
- 2026-07-21: direct record-field call arguments now produce ownership-transfer state when the callee parameter expects
  that owned field type. Passing `box.payload` to an owned `Payload` parameter marks `box.payload` consumed, so later
  record-field reads reuse the same use-after-move diagnostic path.
- 2026-07-21: record-field ownership-transfer production and read diagnostics now support nested member-only aggregate
  paths such as `box.inner.payload`. Index-containing paths remain outside this slice until element ownership transfer
  has a separate proven model.
- 2026-07-21: choice constructor-pattern payload binding now produces ownership-transfer state for named owned choice
  subjects. Destructuring a `Some(value: Payload)`-style variant marks keys such as `maybe.Some.value` consumed while
  leaving switch pattern syntax and exhaustiveness behavior unchanged.
- 2026-07-21: expression lowering now rejects whole-binding reuse when any owned descendant key has been consumed.
  After destructuring an owned choice payload, later reads or moves of the original choice binding fail with the precise
  consumed payload key, such as `maybe.Some.value`.
- 2026-07-21: driver-level choice coverage now pins the same post-destructure diagnostic through source lowering:
  after `switch holder` binds and consumes `Loaded(payload: Payload)`, parameter forwarding or local rebinding fails with
  `use after move: holder.Loaded.payload`.
- 2026-07-21: contextual function signatures now have driver coverage for module-local choice parameters with aggregate
  payload ABI. `function consume_holder(holder: Holder)` accepts the same `{ i32, %record.Payload }` layout used for
  choice locals and returns, and a positive runnable fixture now passes `Holder` to `classify(holder)` where the callee
  switches over `Loaded(payload)`.
- 2026-07-21: final `switch` ownership joins now normalize owned choice-payload destructure keys across continuing
  cases for the switched subject, so `Loaded(payload)` and `Empty` arms can merge while preserving the post-switch
  consumed subject state.
- 2026-07-21: direct control-flow smoke coverage now verifies that the normalized final `switch` state still rejects a
  later read of the original choice subject with `use after move: holder.Loaded.payload`.
- 2026-07-21: non-value `switch` ownership joins now normalize consumed descendants of the named switched subject,
  including owned choice-payload destructure keys, and driver coverage pins post-switch source reuse as `use after move`.
- 2026-07-21: ownership-transfer smoke coverage now pins that normalization below the CLI path by merging a consumed
  `holder.Loaded.payload` case with an otherwise empty continuing case.
- 2026-07-21: final and non-value `switch` joins now share the ownership-transfer descendant normalization helper
  instead of carrying parallel switch-specific implementations.
- 2026-07-21: ownership-join mismatch diagnostics now say `owned transfers must match` instead of naming only
  `DynamicArray`, matching the broader aggregate-descendant transfer model.
- 2026-07-22: driver aggregate-field coverage now pins that generalized `if branch ownership mismatch` diagnostic for
  a non-`DynamicArray` record-field transfer mismatch across continuing branches.
- 2026-07-22: driver aggregate-field coverage now also pins the generalized `switch case ownership mismatch`
  diagnostic for a non-`DynamicArray` record-field transfer mismatch across continuing cases.
- 2026-07-22: direct control-flow smoke coverage now pins the same aggregate-descendant `if` and `switch` ownership
  mismatch diagnostics below the CLI layer.
- 2026-07-22: direct control-flow smoke coverage now also pins balanced aggregate-descendant `if` and `switch`
  ownership joins below the CLI layer.
- 2026-07-22: direct control-flow smoke coverage now pins post-merge whole-owner reuse rejection after balanced
  aggregate-descendant `if` and `switch` transfers.
- 2026-07-22: driver aggregate-field coverage now pins the same post-merge whole-owner reuse rejection after balanced
  aggregate-descendant `if` and `switch` transfers through the CLI lowering path.
- 2026-07-22: driver aggregate-field coverage now pins nested aggregate-descendant post-merge reuse rejection for
  `nested.box.payload` after balanced `if` and `switch` transfers.
- 2026-07-22: driver aggregate-field coverage now pins nested aggregate-descendant `if` and `switch` ownership
  mismatch diagnostics for `nested.box.payload`.
- 2026-07-22: driver aggregate-field coverage now pins scalar-terminal nested member call success for
  `nested.box.count`, proving scalar field arguments do not trigger ownership-transfer diagnostics.
- 2026-07-22: driver aggregate-field coverage now pins scalar-terminal nested member failure paths for
  `nested.box.count` and `nested.box.count.payload`, proving invalid scalar paths stay out of ownership-transfer
  diagnostics at the CLI lowering boundary.
- 2026-07-22: semantic analysis now rejects scalar member paths such as `total.status` and
  `nested.box.count.payload` with source-level `type 'UInt32' has no member 'status'` and
  `type 'UInt32' has no member 'payload'` diagnostics before lowering.
- 2026-07-22: semantic and CLI coverage now reject known record receivers with unknown fields, such as
  `header.missing`, while unresolved placeholder receiver types continue to avoid cascade diagnostics.
- 2026-07-22: semantic and CLI coverage now pin the same unknown-field diagnostic for concrete generic record
  receivers such as `Box<UInt32>` through `box.missing`.
- 2026-07-22: semantic and CLI coverage now reject unknown fields on pointer-backed declared-record receivers such as
  `Pointer<Registers>` through `regs.missing`, while preserving existing valid pointer aggregate field access.
- 2026-07-22: semantic type inference now resolves pointer-backed record fields before validating chained member
  access, so `regs.status.missing` reports `type 'UInt32' has no member 'missing'` before lowering.
- 2026-07-22: semantic and CLI coverage now pin the same pointer-backed field inference for concrete generic records,
  so `Pointer<Box<UInt32>>.value.missing` resolves `value` to `UInt32` before reporting the missing member.
- 2026-07-22: semantic and CLI coverage now reject unknown fields in null-safe member chains, so
  `user?.profile?.missing` reports `type 'Profile' has no member 'missing'` before lowering.
- 2026-07-22: semantic and CLI coverage now reject scalar continuations in null-safe member chains, so
  `user?.profile?.rating?.missing` reports `type 'UInt32' has no member 'missing'` before lowering.
- 2026-07-22: semantic and CLI coverage now reject null-safe access on non-`Maybe` receivers, so `profile?.rating`
  reports `null-safe access requires Maybe base: Profile` before lowering.
- 2026-07-23: semantic and CLI coverage now pin concrete generic payload inference in null-safe chains, so
  `box?.value?.missing` through `Maybe<Box<UInt32>>` reports `type 'UInt32' has no member 'missing'` before lowering.
- 2026-07-23: semantic and CLI coverage now unwrap concrete generic `Maybe` payloads for null-safe member-call lookup,
  so `box?.missing()` reports `type 'Box<UInt32>' has no method 'missing'` and `box?.value?.missing()` reports
  `type 'UInt32' has no method 'missing'` before lowering.
- 2026-07-23: semantic and CLI coverage now pin argument checking for concrete generic null-safe member calls, so
  `box?.scale(true)` through `Maybe<Box<UInt32>>` reports
  `method argument 'delta' type 'Bool' does not match declared type 'UInt32'` before lowering.
- 2026-07-23: semantic and CLI coverage now pin arity checking for concrete generic null-safe member calls, so
  `box?.scale()` and `box?.scale(1 as UInt32, 2 as UInt32)` report expected-versus-actual method argument counts
  before lowering.
- 2026-07-23: semantic and CLI coverage now pin positive lowering for concrete generic null-safe member calls, so
  `box?.scale(5 as UInt32)` through `Maybe<Box<UInt32>>` emits the monomorphized
  `method.Box_UInt32_.scale` call and merges the result as `Maybe<UInt32>`.
- 2026-07-23: semantic and CLI coverage now pin aggregate-return lowering for concrete generic null-safe member calls,
  so `box?.bump(5 as UInt32)` and `box?.pair(7 as UInt32)` wrap `Box<UInt32>` and
  `Array<Box<UInt32>, 2>` method results in `Maybe` merge IR.
- 2026-07-23: semantic and CLI coverage now pin approved chained field access after concrete generic null-safe
  aggregate-return calls, so `box?.bump(5 as UInt32)?.value` lowers to `Maybe<UInt32>` without introducing any
  null-safe index syntax.
- 2026-07-23: semantic and CLI coverage now reject ordinary indexing on concrete generic null-safe aggregate-return
  values before lowering, so `box?.pair(7 as UInt32)[0 as UInt64]` reports that
  `Maybe<Array<Box<UInt32>, 2>>` is not an indexable base.
- 2026-07-23: source-type query coverage now pins the same concrete generic null-safe aggregate-return chain metadata:
  `box?.bump(5 as UInt32)` recovers `Maybe<Box<UInt32>>`, `box?.bump(5 as UInt32)?.value` recovers
  `Maybe<UInt32>`, `box?.pair(7 as UInt32)` recovers `Maybe<Array<Box<UInt32>, 2>>`, and direct indexing that
  `Maybe` result has no element source type.
- 2026-07-23: null-safe plan and expression-emitter coverage now pin `box?.bump(5 as UInt32)?.value` below the CLI
  layer, including the `Box<UInt32>` field segment and monomorphized `method.Box_UInt32_.bump` call feeding a
  `Maybe<UInt32>` merge.
- 2026-07-23: runnable CLI coverage now consumes `box?.bump(5 as UInt32)?.value` through explicit `switch` handling
  for both present and empty `Maybe<Box<UInt32>>` inputs, pinning the generic null-safe aggregate-return path through
  linked execution.
- 2026-07-23: `examples/local_null_safe_generic_aggregate.or` now exposes the same path as a checked-in canonical
  pipeline demo across LLVM emission, object emission, direct `run`, and retained `--build` execution.
- 2026-07-23: semantic and CLI coverage now reject ordinary field access after a concrete generic null-safe
  aggregate-return call, so `box?.bump(5 as UInt32).value` reports that `Maybe<Box<UInt32>>` has no `value` member
  before lowering.
- 2026-07-23: semantic and CLI coverage now reject ordinary method calls after a concrete generic null-safe
  aggregate-return call, so `box?.bump(5 as UInt32).scale(1 as UInt32)` reports that `Maybe<Box<UInt32>>` has no
  `scale` method before lowering.
- 2026-07-23: semantic and CLI coverage now pin declared choice receiver diagnostics beyond `Maybe<T>`, so
  `Result<UInt32>.value` and `Result<UInt32>.scale(1 as UInt32)` report missing member/method diagnostics before
  lowering.
- 2026-07-23: `examples/local_result_choice_switch.or` now pins generic `Result<UInt32>` construction and explicit
  switch payload consumption as a checked-in frontend example while backend generic choice local lowering remains
  pending.
- 2026-07-23: generic concrete choice instantiations with the existing finite single-payload ABI now lower for
  function returns, local constructors, and explicit switch payload consumption. `local_result_choice_switch.or` now
  emits and runs through the backend while `Maybe<T>` remains on its dedicated null-safe `{ i1, payload }` ABI path.
- 2026-07-23: concrete generic choices whose single-payload variants use distinct finite scalar/fixed LLVM payload
  types now lower through an explicit `{ i32, [N x i8] }` tagged payload-buffer ABI. The checked-in
  `local_result_distinct_choice_switch.or` example pins `Result<UInt32, Bool>` construction and switch payload
  recovery through linked execution.
- 2026-07-24: distinct concrete generic choice payload buffering now sizes record payload variants with full lowering
  context record layouts. `local_result_distinct_record_choice_switch.or` pins `Result<Payload, Flag>` construction and
  switch payload recovery through linked execution.
- 2026-07-24: distinct concrete generic choice payload buffering now also covers fixed-array payload variants whose
  elements are records. `local_result_array_payload_choice_switch.or` pins `Result<Array<Payload, 2>, Payload>`
  construction and array payload recovery through linked execution.
- 2026-07-22: direct control-flow smoke coverage now pins nested aggregate-descendant mismatch, balanced join, and
  post-merge reuse diagnostics for `nested.box.payload` below the CLI layer.
- 2026-07-22: direct control-flow aggregate ownership smoke coverage now uses shared helpers for seeded aggregate
  states and post-merge reuse diagnostics, reducing fixture drift risk.
- 2026-07-22: driver aggregate-field ownership smoke coverage now uses shared source-line helpers for `box.payload`
  and `nested.box.payload` mismatch/reuse fixtures, reducing CLI fixture drift risk.
- 2026-07-22: ownership-transfer smoke coverage now pins nested record-member transfer rejection for missing fields,
  scalar terminal fields, and paths that attempt to continue through a scalar field.
- 2026-07-22: call-emitter smoke coverage now pins the same nested record-member call-argument transfer boundaries,
  including scalar terminal success without ownership consumption.
- 2026-07-21: non-generic single-payload choices now accept lowerable aggregate payload ABI shapes, such as
  `{ i32, %record.Payload }`, instead of being limited to scalar LLVM payload types. Multi-payload variants and generic
  choice ABI lowering remain unsupported.
- 2026-07-17: dynamic iterable gap boundaries are now pinned: `DynamicArray<UInt32>` remains an unsupported lowered
  function-signature parameter type, while `View<UInt32>` parameters lower far enough to reject `for ... in` with the
  fixed-size-array-only iterable diagnostic instead of implying view iteration support.
- 2026-07-17: ADR-0006 and source-type query smoke coverage now pin the internal dynamic sequence model:
  `DynamicArray<T>` is classified as owned contiguous storage, while `View<T>`/`shared.View<T>`/`exclusive.View<T>`
  are classified as non-owning contiguous views with access-mode metadata. Emitters still reject dynamic iteration
  until descriptor ABI lowering is implemented.
- 2026-07-17: `View<T>`/qualified view function parameters and returns now lower through the descriptor ABI
  `{ ptr, i64 }`; view parameter indexing extracts the descriptor data pointer before element addressing, while
  length-aware bounds and dynamic `for ... in` lowering remain pending.
- 2026-07-17: dynamic-array descriptor layout is now pinned internally as `{ ptr, i64, i64 }`, and target layout can
  size literal LLVM structs such as view and dynamic-array descriptors. `DynamicArray<T>` lowered signatures remain
  intentionally rejected until ownership/allocation/drop invariants are implemented.
- 2026-07-17: dynamic-array lowering invariants are now pinned in smoke coverage: signatures stay disabled until a
  unique owning descriptor, allocator path, `length <= capacity` invariant, and initialized-element drop walk are
  proven. Drop declaration emission remains metadata-only under ADR-0005.
- 2026-07-17: dynamic-array runtime ABI entry points are now modeled internally as finite allocate/grow/deallocate
  calls over descriptor and `i64` scalar operands. The declarations are intentionally not emitted until construction,
  ownership, and cleanup lowering consume them.
- 2026-07-17: module prelude emission now has opt-in dynamic-array runtime declaration support, deduplicated by
  symbol, while the default operation list remains empty so ordinary lowering still emits no dynamic-array runtime
  declarations.
- 2026-07-17: refreshed pending lowering-gap wording after the nested non-generic aggregate expansion; record and
  fixed-array aggregate construction/assignment coverage is now broad enough that the remaining aggregate wording
  points to forms outside the pinned scalar, record, and fixed-array paths.
- 2026-07-17: nested non-generic record/fixed-array aggregate loop-built returns now have lowering smoke coverage:
  records containing records and records containing fixed arrays of records lower through `while`-built and fixed-array
  `for`-built return paths with caller-side nested reads.
- 2026-07-17: nested non-generic record/fixed-array aggregate final `switch` returns now have lowering smoke coverage:
  records containing records and records containing fixed arrays of records lower through switch merges and caller-side
  nested reads.
- 2026-07-17: nested non-generic record/fixed-array aggregate function control and early-return paths now have lowering
  smoke coverage: records containing records and records containing fixed arrays of records lower through final `if`,
  guard failure, deferred cleanup, and caller-side nested reads.
- 2026-07-17: nested non-generic record/fixed-array aggregate receiver-method boundaries now have lowering smoke
  coverage: records containing records and records containing fixed arrays of records cross method parameter and
  return boundaries with caller/callee nested field reads.
- 2026-07-17: nested non-generic record/fixed-array aggregate function boundaries now have lowering smoke coverage:
  records containing records and records containing fixed arrays of records cross parameter and return boundaries with
  caller/callee nested field reads.
- 2026-07-17: nested local non-generic aggregate assignment now has lowering smoke coverage: records containing records
  and records containing fixed arrays of records lower nested field writes, nested indexed field writes, and subsequent
  reads through address-backed local storage.
- 2026-07-17: stale negative lowering smoke fixtures were refreshed after boolean equality and null-safe member-call
  progress; pipeline/driver failure checks now use an intentionally unsupported `<` Bool expression, and the
  expression-emitter diagnostic check now expects the current non-`Maybe` receiver failure.
- 2026-07-17: refreshed pending lowering-gap wording after the generic aggregate coverage expansion; the stale
  generic-record parameter/return, pointer-backed, mutable-local, and fixed-array iterable gaps now point to the
  remaining broader aggregate, dynamic iterable, and backend-completeness work.
- 2026-07-17: generic aggregate lowering gap review found nested plain-function final `switch` returns were the next
  unpinned peer of existing one-level coverage; `Array<Array<Tag<UInt32>, 2>, 2>` and `Array<Tag<Tag<UInt32>>, 2>`
  now lower through final `switch` merge paths.
- 2026-07-17: underconstrained bare inline nested generic record constructor array literals in `for` iterables now
  preserve generic inference diagnostics instead of falling through to unknown lowered functions.
- 2026-07-17: bare inline nested concrete generic record constructor array literals in `for` iterables now have
  coverage: nested `Tag(...)` constructor arrays synthesize concrete item types and support nested field reads.
- 2026-07-17: nested concrete generic record fixed-array `for` iterables now have coverage:
  `Array<Array<Tag<UInt32>, 2>, 2>` loop items and `Array<Tag<Tag<UInt32>>, 2>` loop items become addressable and
  support nested field reads.
- 2026-07-17: nested concrete generic record mutable-local assignments now have coverage:
  `Array<Array<Tag<UInt32>, 2>, 2>` and `Array<Tag<Tag<UInt32>>, 2>` locals lower nested indexed field writes and
  subsequent reads.
- 2026-07-17: nested concrete generic record pointer-backed unsafe paths now have coverage:
  `Pointer<NestedArrayLog>` and `Pointer<NestedRecordLog>` lower nested generic aggregate `address_of(...)` plus
  `raw_read(...)` paths and direct pointer-backed nested field assignments.
- 2026-07-17: nested concrete generic record array plain function loop-built returns now have coverage:
  `Array<Array<Tag<UInt32>, 2>, 2>` and `Array<Tag<Tag<UInt32>>, 2>` lower through `while`-built and fixed-array
  `for`-built return paths.
- 2026-07-17: nested concrete generic record array plain function final-control and early-return paths now have
  coverage: `Array<Array<Tag<UInt32>, 2>, 2>` and `Array<Tag<Tag<UInt32>>, 2>` lower through final `if`, guard
  failure, and deferred-cleanup returns.
- 2026-07-17: nested concrete generic record array plain function boundaries now have coverage:
  `Array<Array<Tag<UInt32>, 2>, 2>` and `Array<Tag<Tag<UInt32>>, 2>` parameters and returns lower nested
  `Tag(...)` constructors plus caller/callee indexed field reads.
- 2026-07-17: nested concrete generic record array receiver method parameters now have coverage:
  `Array<Array<Tag<UInt32>, 2>, 2>` and `Array<Tag<Tag<UInt32>>, 2>` parameter contexts lower nested `Tag(...)`
  arguments and address-backed callee field reads.
- 2026-07-17: nested concrete generic record array receiver method returns now have coverage:
  `Array<Array<Tag<UInt32>, 2>, 2>` and `Array<Tag<Tag<UInt32>>, 2>` return contexts lower nested `Tag(...)`
  constructors and caller-side indexed field reads.
- 2026-07-17: concrete generic record array receiver method loop-built returns now have coverage: `while`-built and
  fixed-array `for`-built `Array<Tag<UInt32>, 2>` returns lower `Tag(...)` elements through declared method return
  context.
- 2026-07-17: concrete generic record array receiver method returns now have final-control-flow and early-return
  coverage: `Array<Tag<UInt32>, 2>` methods lower `Tag(...)` elements through final `if`, guard failure, and
  deferred-cleanup returns.
- 2026-07-16: generic record constructor receiver method loop-built returns now have expected-return-context coverage:
  `while`-built and fixed-array `for`-built values return `Tag(...)` through a declared `Tag<UInt32>` method return.
- 2026-07-16: generic record constructor receiver method early returns now have expected-return-context coverage:
  guard failure returns and deferred-cleanup returns lower `Tag(...)` through a declared `Tag<UInt32>` method return.
- 2026-07-16: generic record constructor receiver method returns now have final control-flow expected-return-context
  coverage: final `if` and `switch` arms lower `Tag(...)` through a declared `Tag<UInt32>` method return type.
- 2026-07-16: generic record constructor receiver method-call arguments now have expected-parameter-context lowering
  coverage: `receiver.take(Tag(...))` lowers when the method declares `Tag<UInt32>`.
- 2026-07-16: generic record constructor call arguments now have expected-parameter-context lowering coverage:
  `take(Tag(...))` lowers when `take` declares `Tag<UInt32>`, while scalar mismatches remain semantic diagnostics.
- 2026-07-16: direct underconstrained generic record constructor inferred `let` and `var` bindings now preserve the
  generic parameter inference diagnostic instead of reporting only broad unsupported binding messages.
- 2026-07-16: underconstrained generic record constructor diagnostics now retain template parameter metadata without
  exposing generic templates as lowerable layouts, so array literal `for` failures identify the missing generic
  parameter instead of surfacing only an unknown lowered function fallback.
- 2026-07-16: lowering context no longer exposes uninstantiated generic record templates as lowerable record layouts;
  underconstrained generic record constructor array literal `for` iterables now fail instead of falling back to a
  template-shaped `%record.Name` layout.
- 2026-07-16: CLI generic diagnostics now pin safe refusal for inline generic-record constructor array literal `for`
  iterables when repeated generic field bindings conflict, preventing the lowering path from treating an unprovable
  constructor instantiation as concrete.
- 2026-07-16: checked-in CLI coverage now pins bare inline generic-record constructor array literal `for` iterables
  across LLVM emission, object emission, direct `run`, and retained `--build` using
  `examples/local_generic_record_array_literal_for.or`.
- 2026-07-16: bare inline generic-record constructor array literals now participate in fixed-array `for` lowering when
  constructor arguments prove a single concrete instantiation: `[Box(7 as UInt32), Box(9 as UInt32)]` synthesizes
  `Box<UInt32>` layout metadata, makes each loop item addressable, and lowers `item.value`.
- 2026-07-15: concrete generic record aggregates now participate in fixed-array `for` item reads in lowering smoke
  coverage: iterating `Array<Box<UInt32>, 2>` makes each `item` addressable so `item.value` lowers inside the loop body.
- 2026-07-15: concrete generic record aggregates now participate in pointer-backed unsafe writes in lowering smoke
  coverage: `Pointer<Box<UInt32>>.value = value` and `Pointer<GenericLog>.entries[index].value = value` lower through
  the same direct pointer-backed assignment path as non-generic aggregates.
- 2026-07-15: concrete generic record aggregates now participate in pointer-backed unsafe reads in lowering smoke
  coverage: `Pointer<Box<UInt32>>.value` and `Pointer<GenericLog>.entries[index].value` lower through
  `address_of(...)`, `Pointer(...)`, and `raw_read(...)`.
- 2026-07-15: concrete generic record aggregates now cross guard/defer early-return paths in lowering smoke coverage:
  callers read fields from a guard-failure `Box<UInt32>` return and a deferred-cleanup
  `Array<Box<UInt32>, 2>` early return.
- 2026-07-15: concrete generic record aggregates now cross loop-built return paths in lowering smoke coverage: callers
  read fields from a `while`-built `Box<UInt32>` return and a `for`-built `Array<Box<UInt32>, 2>` return.
- 2026-07-15: concrete generic record aggregates now cross final control-flow return merges in lowering smoke
  coverage: callers read fields from final `if` and final `switch` functions returning `Box<UInt32>` and
  `Array<Box<UInt32>, 2>`.
- 2026-07-15: concrete generic record aggregates now cross plain function return boundaries in lowering smoke
  coverage: callers read fields from functions returning `Box<UInt32>` and `Array<Box<UInt32>, 2>` through the
  existing temporary aggregate spill/read path.
- 2026-07-15: concrete generic record aggregates now cross method-return boundaries in lowering smoke coverage:
  callers read fields from scalar methods returning `Box<UInt32>` and `Array<Box<UInt32>, 2>` through the existing
  temporary aggregate spill/read path.
- 2026-07-15: concrete generic record aggregates now cross method-parameter boundaries in lowering smoke coverage:
  scalar receiver methods read fields from `Box<UInt32>` parameters and `Array<Box<UInt32>, 2>` parameters using the
  same read-only aggregate parameter storage path as plain functions.
- 2026-07-15: concrete generic record receiver methods are now pinned in lowering smoke coverage: `extend Box<UInt32>`
  methods can read `this.value`, and direct member calls pass the monomorphized receiver through the same read-only
  aggregate parameter storage path.
- 2026-07-15: concrete generic record aggregates now cross plain function boundaries in lowering smoke coverage:
  callees read fields from `Box<UInt32>` parameters and from `Array<Box<UInt32>, 2>` parameters using the same
  read-only aggregate parameter storage path as non-generic records and fixed arrays.
- 2026-07-15: mutable fixed arrays of concrete generic records are now pinned: annotated
  `var boxes: Array<Box<UInt32>, 2>` retains source-type metadata, `boxes[0].value = ...` lowers through the mixed
  index-plus-field aggregate assignment path, and a later `boxes[0].value` read observes the stored value.
- 2026-07-15: fixed arrays of concrete generic records are now pinned: `Array<Box<UInt32>, 2>` literals materialize
  as fixed LLVM arrays of monomorphized record values, and `boxes[0].value` lowers through indexed element recovery
  followed by record-field extraction.
- 2026-07-15: nested concrete generic record lowering is now pinned: `Box<Box<UInt32>>` annotations synthesize both
  inner and outer monomorphized layouts, nested constructors materialize both aggregate values, and
  `outer.value.value` lowers through chained record-field reads.
- 2026-07-15: concrete generic record mutable-local field assignment is now pinned: annotated
  `var box: Box<UInt32>` initialization retains monomorphized source-type metadata, `box.value = ...` lowers through
  the existing aggregate assignment path, and a later `box.value` read observes the stored value.
- 2026-07-15: concrete generic record field extraction is now pinned for annotated immutable aggregate locals:
  `Box<UInt32>` constructor lowering retains source-type metadata and `box.value` lowers through the existing
  address-backed record field read path.
- 2026-07-15: lowering now synthesizes monomorphized layouts for concrete generic record types discovered from
  source annotations/signatures, emits deterministic LLVM struct declarations such as `%record.Box_UInt32_`, and
  lowers the simplest `Box<UInt32>` constructor `let` path end-to-end.
- 2026-07-15: generic record constructor lowering was pinned as an explicit unsupported aggregate-construction
  gap before the first monomorphized-layout slice: semantic analysis could recognize `Box<UInt32>` constructor
  use while LLVM lowering still rejected the concrete generic `let` type.
- 2026-07-15: fixed-array `for` iterable lowering audit confirmed helper-returned, scalar-method-returned,
  record-method-returned, member-receiver method-returned, nested record-backed, indexed record-field, and
  ternary-selected fixed-array sources are implemented; lowering smoke now pins helper-returned plus scalar-method
  returned fixed-array iterables end-to-end.
- 2026-07-15: loop-nested branch-local `defer` cleanup replay now covers record-receiver `Unit` member-call
  statements with signed negative `Int32` ternary argument lowering and unsigned negative `UInt32` ternary argument
  rejection for `while` body `if`/`continue` and block-arm `switch`/`break` paths.
- 2026-07-15: statement-level `if` and block-arm `switch` branch-local `defer` cleanup replay now covers
  record-receiver `Unit` member-call statements with signed negative `Int32` ternary argument lowering and unsigned
  negative `UInt32` ternary argument rejection.
- 2026-07-15: `for` continue and `repeat` break `defer` cleanup replay now cover record-receiver `Unit` member-call
  statements with signed negative `Int32` ternary argument lowering and unsigned negative `UInt32` ternary argument
  rejection.
- 2026-07-15: nested `unsafe` block `defer` cleanup replay now covers record-receiver `Unit` member-call statements
  with signed negative `Int32` ternary argument lowering and unsigned negative `UInt32` ternary argument rejection
  across loop `break` and loop `continue`.
- 2026-07-15: early-exit `defer` cleanup replay now covers record-receiver `Unit` member-call statements with signed
  negative `Int32` ternary argument lowering and unsigned negative `UInt32` ternary argument rejection across explicit
  `return`, loop `break`, and loop `continue`.
- 2026-07-15: `defer` cleanup replay now covers record-receiver `Unit` member-call statements with signed negative
  `Int32` ternary argument lowering and unsigned negative `UInt32` ternary argument rejection.
- 2026-07-15: statement-level `if`/`switch` record-receiver `Unit` member-call statement paths now cover signed
  negative `Int32` argument lowering and unsigned negative `UInt32` argument rejection.
- 2026-07-15: `repeat` and `for` record-receiver `Unit` member-call statement paths now cover signed negative
  `Int32` ternary argument lowering and unsigned negative `UInt32` ternary argument rejection.
- 2026-07-15: scoped record-receiver `Unit` member-call statement paths now cover signed negative `Int32` ternary
  argument lowering and unsigned negative `UInt32` ternary argument rejection inside `while`, `guard`, and `unsafe`.
- 2026-07-15: record-receiver `Unit` member-call statement argument paths now cover signed negative `Int32`
  direct and ternary argument lowering plus unsigned negative `UInt32` direct and ternary argument rejection.
- 2026-07-15: final `if`/`switch` record-receiver member-call argument paths now cover signed negative `Int32`
  argument lowering and unsigned negative `UInt32` argument rejection.
- 2026-07-15: record-receiver member-call argument paths now cover signed negative `Int32` ternary argument
  lowering through aggregate receiver method calls and unsigned negative `UInt32` ternary argument rejection.
- 2026-07-14: null-safe member-call argument paths now cover signed negative `Int32` ternary argument
  lowering through the null-safe call merge and unsigned negative `UInt32` ternary argument rejection.
- 2026-07-14: scalar member `Unit` call statement argument paths now cover signed negative `Int32` ternary argument
  lowering into the discarded method-call operand and unsigned negative `UInt32` ternary argument rejection.
- 2026-07-14: direct `Unit` call statement argument paths now cover signed negative `Int32` ternary argument lowering
  into the discarded call operand and unsigned negative `UInt32` ternary argument rejection.
- 2026-07-14: `unsafe` fixed-array element assignment paths now cover signed negative `Int32` ternary RHS lowering and
  unsigned negative `UInt32` ternary RHS rejection inside the unsafe block, completing the scoped assignment wrapper slice.
- 2026-07-14: `guard ... else` record-field assignment paths now cover signed negative `Int32` ternary RHS lowering
  and unsigned negative `UInt32` ternary RHS rejection inside the failure block.
- 2026-07-14: `repeat` loop record-field assignment paths now cover signed negative `Int32` ternary RHS lowering and
  unsigned negative `UInt32` ternary RHS rejection inside the loop body.
- 2026-07-14: `for` loop fixed-array element assignment paths now cover signed negative `Int32` ternary RHS lowering
  and unsigned negative `UInt32` ternary RHS rejection inside the loop body.
- 2026-07-14: explicit same-target casts now lower nested expressions, preserving parsed ternary RHS casts in
  assignment contexts; `while` loop record-field assignments cover signed negative `Int32` ternary lowering and
  unsigned negative `UInt32` ternary rejection inside the loop body.
- 2026-07-14: fixed-array literal method returns now cover signed negative `Int32` ternary arm lowering through the
  aggregate method return ABI and unsigned negative `UInt32` ternary arm rejection.
- 2026-07-14: record-constructor method returns now cover signed negative `Int32` ternary arm lowering through the
  aggregate method return ABI and unsigned negative `UInt32` ternary arm rejection.
- 2026-07-14: remaining ternary negative-cast coverage audit found method-return aggregate ternaries are the next
  highest-value slice: loop/scoped assignment wrappers mostly rewrap already-covered assignment RHS lowering, while
  aggregate method returns combine method emission, aggregate return ABI shape, and ternary merge behavior.
- 2026-07-14: fixed-array element assignment RHS paths now cover signed negative `Int32` ternary arm lowering through
  the merged assignment value and unsigned negative `UInt32` arm rejection.
- 2026-07-14: record-field assignment RHS paths now cover signed negative `Int32` ternary arm lowering through the
  merged assignment value and unsigned negative `UInt32` arm rejection.
- 2026-07-14: fixed-array element call-argument paths now cover signed negative `Int32` ternary arm lowering through
  array construction, element extraction, and the final direct-call operand, plus unsigned negative `UInt32` arm rejection.
- 2026-07-14: record-field aggregate call-argument paths now cover signed negative `Int32` ternary arm lowering through
  record construction, field extraction, and the final direct-call operand, plus unsigned negative `UInt32` arm rejection.
- 2026-07-14: scalar member-call ternary argument paths now cover signed negative `Int32` arm lowering into the merged
  method-call operand and unsigned negative `UInt32` arm rejection.
- 2026-07-14: direct-call ternary argument paths now cover signed negative `Int32` arm lowering into the merged call
  operand and unsigned negative `UInt32` arm rejection.
- 2026-07-14: scalar ternary return paths now cover signed negative `Int32` arm lowering and unsigned negative `UInt32`
  arm rejection, closing the first audited `?:` negative-cast carrier gap.
- 2026-07-14: negative-cast lowering coverage audit found the next highest-value missing slice is ternary `?:`
  expression carriers: existing tests cover general ternary lowering plus signed/unsigned negative casts through returns,
  methods, calls, aggregate access, and aggregate assignments, but not signed negative ternary arms or unsigned negative
  rejection through ternary arms.
- 2026-07-14: aggregate assignment control-flow smoke tests now share record-field and array-element signed assertion
  helpers, matching the call-argument helper pattern while keeping source fixtures explicit.
- 2026-07-14: record-field and array-element assignment RHS paths now have paired `if`/`switch` signed negative
  lowering and unsigned negative rejection coverage.
- 2026-07-14: aggregate-access final control-flow call-argument smoke tests now share record-field and array-element
  signed assertion helpers, preserving explicit source fixtures while reducing repeated IR-shape checks.
- 2026-07-14: final `if`/`switch` call-argument paths now cover signed negative values flowing through record-field
  and array-element arguments plus unsigned negative rejection for the same aggregate access forms.
- 2026-07-14: direct-call and member-call final control-flow argument smoke tests now share a named signed negative
  argument assertion helper while keeping direct/member call prefixes and source fixtures explicit.
- 2026-07-14: final `if`/`switch` direct-call return paths now cover signed negative argument lowering and unsigned
  negative argument rejection for ordinary scalar function calls.
- 2026-07-14: final `if`/`switch` member-call return paths now cover signed negative argument lowering and unsigned
  negative argument rejection for scalar receiver methods.
- 2026-07-14: control-flow method-return lowering smoke assertions now share named helpers for final `if`/`switch`
  block shape plus signed scalar and aggregate method return shape, preserving existing coverage with less repetition.
- 2026-07-14: aggregate-valued method control flow now has signed negative cast lowering and unsigned negative cast
  rejection coverage for final `if`/`switch` record-constructor and array-literal return paths.
- 2026-07-14: value-producing method control flow now has signed negative cast lowering and unsigned negative cast
  rejection coverage for final `if` and `switch` scalar return paths.
- 2026-07-14: negative method-return smoke coverage now uses named helpers for method definition fragments and
  aggregate insertion fragments, preserving scalar, record-receiver, and aggregate-valued method coverage while
  reducing repeated substring checks.
- 2026-07-14: aggregate-valued method bodies now have signed negative cast lowering and unsigned negative cast
  rejection coverage for record constructor returns and fixed-array literal returns.
- 2026-07-14: record receiver method bodies now have signed negative `Int32` return lowering and unsigned negative
  `UInt32` return rejection coverage for supported non-generic record layouts.
- 2026-07-14: scalar method bodies now have signed negative `Int32` return lowering and unsigned negative `UInt32`
  return rejection coverage, extending negative cast checks beyond top-level functions.
- 2026-07-14: negative call-argument smoke coverage now uses named assertion helpers for lowered signed `Int32`
  temporary operands and null-safe member-call diagnostic/result fragments, preserving direct/member/null-safe coverage
  while reducing duplicated substring checks.
- 2026-07-14: null-safe member-call arguments now have signed negative cast lowering and unsigned negative cast
  rejection coverage through the driver LLVM smoke path.
- 2026-07-14: scalar member-call arguments now have signed negative cast lowering and unsigned negative cast rejection
  coverage for both value-returning calls and `Unit` statement calls.
- 2026-07-14: aggregate-derived call arguments now have signed negative cast lowering and unsigned negative cast
  rejection coverage for record-field and fixed-array element reads.
- 2026-07-14: negative aggregate cast smoke coverage now uses shared local assertion helpers for repeated signed
  lowering and unsigned rejection checks, preserving coverage while reducing duplicated assertions.
- 2026-07-14: scoped aggregate assignment now has signed negative cast lowering and unsigned negative cast rejection
  coverage across `guard ... else` and `unsafe` block statement paths.
- 2026-07-14: loop-body aggregate assignment now has signed negative cast lowering and unsigned negative cast rejection
  coverage across `while`, `for`, and `repeat` statement paths.
- 2026-07-14: branch-local aggregate assignment now has signed negative cast lowering and unsigned negative cast
  rejection coverage across `if` record-field and `switch` fixed-array element paths.
- 2026-07-14: aggregate field/index assignment now has signed negative cast lowering coverage and unsigned negative
  cast rejection coverage for record fields and fixed-array elements.
- 2026-07-14: unsigned negative integer casts now have aggregate rejection coverage for record fields and fixed-array
  elements, preserving the targeted `negative value to UInt32` diagnostic detail.
- 2026-07-14: signed negative integer casts now have aggregate construction/extraction coverage for record fields and
  fixed-array elements.
- 2026-07-14: signed negative integer casts now have explicit return-expression and call-argument lowering coverage in
  addition to `let`/`var` initializer coverage.
- 2026-07-14: unsigned negative integer casts such as `-1 as UInt32` remain rejected with a targeted
  `unsupported cast: negative value to UInt32` lowering diagnostic.
- 2026-07-14: signed unary integer operands now lower inside explicit cast contexts such as `-27 as Int32`, unblocking
  negative signed integer `let`/`var` initializers and signed compound-assignment fixtures that start negative.
- 2026-07-14: signed integer compound assignment smoke coverage now pins `/=` and `%=` to LLVM `sdiv` and `srem`,
  matching signed expression lowering while preserving direct load/compute/store assignment lowering.
- 2026-07-14: compound assignment lowering now rejects non-integer targets with a dedicated diagnostic before
  emitting arithmetic, covering both mutable local `Bool` and aggregate `Bool` field targets.
- 2026-07-14: compound assignment tokens and statements now parse for the EBNF-listed `+=`, `-=`, `*=`, `/=`, and
  `%=` forms; mutable-local and supported aggregate target lowering now emits direct target-pointer
  load/compute/store sequences without re-evaluating member/index assignment targets.
- 2026-07-14: unsupported unary negation contexts now fail with `unsupported operator: -` diagnostics instead of
  falling through generic unsupported-expression reporting, with unsigned integer and boolean return-expression
  regression coverage.
- 2026-07-14: signed integer unary negation now lowers for fixed-width `Int*` operands as LLVM `sub 0, value`, with
  `tour_05_bindings_operators.or` covering `-value`; unsigned negation remains intentionally unsupported pending an
  explicit overflow/wrapping policy.
- 2026-07-14: integer modulo now lowers for fixed-width integer operands, emitting unsigned `urem` and signed `srem`;
  `tour_05_bindings_operators.or` now exercises `%` through backend example coverage.
- 2026-07-14: boolean equality and inequality now lower directly as `icmp eq/ne i1`, while unsupported boolean
  relational operators remain rejected with explicit unsupported-operator diagnostics.
- 2026-07-14: non-void null-safe member-call statements now lower by reusing the null-safe expression path and
  discarding the produced `Maybe<T>` value; void-returning null-safe member-call statements remain unsupported until a
  `Maybe<Unit>` ABI policy is accepted.
- 2026-07-14: void-returning null-safe member-call statements now fail with a dedicated `Maybe<Unit>` ABI diagnostic
  instead of falling through generic result-type inference failure.
- 2026-07-05: source-type recovery now propagates through ternary expressions when both branches have the same source
  type, allowing fixed-array `for` iterables selected by `?:`; `local_ternary_array_for.or` pins backend validation.
- 2026-07-05: source-type recovery now recognizes lowerable explicit casts and homogeneous explicitly typed array
  literals, enabling ternary-selected fixed-array literal `for` iterables; `local_ternary_array_literal_for.or` pins
  backend validation.
- 2026-07-05: source-type recovery now recognizes record constructor calls as their record source type, enabling
  ternary-selected fixed-array record literal `for` iterables; `local_ternary_record_array_literal_for.or` pins backend
  validation.
- 2026-07-05: inferred `let`/`var` initializer metadata now delegates to shared expression source-type recovery before
  falling back to LLVM type reconstruction, preserving pointee-aware `raw_offset` bindings for later unsafe intrinsic
  lowering.
- 2026-07-05: initializer source-type recovery now lives in the shared source-query component instead of as a private
  statement-emitter helper, so expression and binding metadata recovery share one API.
- 2026-07-05: shared source-type queries now recover value-producing final `if`/`switch` statement result types when
  all branches resolve to the same source type, including branch-local inferred aggregate bindings.
- 2026-07-05: shared source-type queries now recover `Pointer<T>` from the bounded `Pointer(address_of(value))`
  constructor shape when the addressed value has a known source type, preserving pointee metadata for inferred unsafe
  intrinsic bindings.
- 2026-07-05: source-type recovery now pins the composed `raw_offset(Pointer(address_of(value)), n)` path, preserving
  the recovered `Pointer<T>` pointee through direct offset chains without requiring an intermediate annotated binding.
- 2026-07-05: direct function-call arguments now have regression coverage for
  `raw_offset(Pointer(address_of(value)), n)`, ensuring pointee metadata survives into expected `Pointer<T>` parameters.
- 2026-07-05: receiver-aware member-call arguments now have matching regression coverage for
  `raw_offset(Pointer(address_of(value)), n)`, ensuring the explicit receiver path preserves expected pointer pointees.
- 2026-06-26: the pthread-backed concurrency runtime now invokes the optional spawn cleanup callback after entry
  completion and on spawn setup failure; direct ABI smoke coverage pins post-entry cleanup for joined and abandoned
  handles.
- 2026-06-26: removed the accidental C runtime/test translation units; the concurrency runtime is now C++23 with
  explicit `extern "C"` ABI exports for generated LLVM calls.
- 2026-06-26: added deterministic direct ABI smoke coverage for spawn setup failure cleanup through a test-only
  runtime seam.
- 2026-06-26: scalar task/thread lowering now emits deterministic private cleanup thunks for captured environments and
  passes them through the runtime cleanup callback slot; the thunk body is currently a no-op pending owned-capture drop
  lowering.
- 2026-06-26: added lowering smoke coverage that no-capture scalar task/thread expressions still pass `ptr null` for
  the runtime cleanup slot and do not emit unnecessary cleanup thunks.
- 2026-06-26: concurrency planning now records cleanup drop candidates for non-scalar captured environment fields;
  current cleanup thunk emission remains metadata-only/no-op until owned-capture drop lowering exists.
- 2026-06-26: target-layout sizing now resolves named record LLVM types through the lowering context, so aggregate
  capture environments can report concrete byte sizes instead of falling back to zero for known records.
- 2026-06-26: added lowering smoke coverage for record-returning scalar task/thread bodies, pinning context-sized
  `%record.*` result storage and runtime result-size operands.
- 2026-06-26: cleanup thunks now compute deterministic field addresses for non-scalar cleanup-candidate captures,
  giving future owned-capture drop lowering a concrete insertion point while remaining no-op for drops today.
- 2026-06-26: cleanup candidate plans now carry deterministic type-specific drop symbol names, surfaced in cleanup
  thunk comments without emitting drop calls yet.
- 2026-06-26: added lowering smoke coverage that cleanup candidate drop symbols remain metadata-only today: aggregate
  cleanup thunks may mention planned `__orison_drop.*` symbols in comments, but must not declare or call them until
  drop semantics are accepted.
- 2026-06-26: module prelude emission now has an explicit disabled-by-default drop declaration seam for future
  `__orison_drop.<Type>` declarations, with smoke coverage proving planned declarations do not emit unless explicitly
  enabled.
- 2026-06-26: LLVM IR emission now discovers cleanup-candidate drop declarations end-to-end as disabled metadata, and
  lowering smoke coverage pins both the discovered `__orison_drop.Payload` symbol and the absence of emitted drop ABI.
- 2026-06-26: lowering metadata scans now share one syntax traversal helper for runtime-symbol discovery and disabled
  drop-declaration discovery, reducing duplicated recursion without changing generated IR.
- 2026-06-26: added dedicated syntax traversal smoke coverage for assignment targets, switch patterns/cases, nested
  task/thread bodies, `await`, `.join()`, implementation methods, and extension methods.
- 2026-06-26: planned drop declarations now retain source type and concurrency expression line metadata, giving future
  diagnostics a precise discovery site without emitting drop declarations or calls.
- 2026-06-26: added a pure formatter for planned drop declaration metadata so future diagnostics can reuse a stable
  message shape without wiring it into normal compilation yet.
- 2026-06-26: expanded lowering smoke coverage for multiple aggregate captures so planned drop metadata now pins
  per-type symbol discovery, shared discovery lines, and metadata-only IR behavior for more than one capture.
- 2026-06-26: added same-type aggregate capture coverage proving planned drop declarations dedupe per drop symbol while
  cleanup thunks still retain one cleanup candidate per captured environment field.
- 2026-06-26: factored planned drop declaration deduplication into a shared helper so future metadata producers can
  reuse the pinned one-entry-per-drop-symbol behavior.
- 2026-06-27: split planned drop metadata types and helpers out of module prelude into a dedicated lowering module so
  future diagnostics and ownership/drop planning can reuse them without depending on prelude emission.
- 2026-06-27: renamed the planned drop metadata type to `PlannedDropDeclaration`, removing the outdated prelude-specific
  name now that the helper lives in a general lowering metadata module.
- 2026-06-27: aligned planned-drop local and parameter naming with `PlannedDropDeclaration` so prelude emission and
  concurrency metadata collection no longer use stale generic drop-declaration wording.
- 2026-06-27: added dedicated drop metadata smoke coverage for planned-drop formatting and deduplication, keeping
  module prelude smoke focused on declaration emission behavior.
- 2026-06-27: added a planned-drop reporting seam on lowering results so diagnostics/tooling can inspect formatted
  drop metadata without changing the normal diagnostic stream or emitted LLVM IR.
- 2026-06-27: added an explicit `orisonc --planned-drops <file>` inspection command that prints planned-drop metadata
  reports while leaving normal compile, run, and LLVM/object emission output unchanged.
- 2026-06-29: expanded `orisonc --planned-drops` smoke coverage for multiple aggregate captures and same-type capture
  deduplication, matching the lower-level planned-drop metadata guarantees through the CLI inspection path.
- 2026-06-29: split planned-drop metadata into per-cleanup-site actions plus deduped declaration metadata, preserving
  metadata-only IR behavior while giving future owned-drop lowering concrete cleanup action records.
- 2026-06-29: added a dedicated planned-drop action report formatter on lowering results so future cleanup/drop lowering
  can inspect per-capture field actions separately from deduped declaration reports.
- 2026-06-29: added explicit `orisonc --planned-drop-actions <file>` CLI inspection for per-cleanup-site planned drop
  actions, mirroring `--planned-drops` while preserving normal compilation output.
- 2026-06-29: shared driver report-line rendering between `--planned-drops` and `--planned-drop-actions`, keeping both
  explicit inspection commands behaviorally aligned without changing their output.
- 2026-06-29: added metadata-only drop cleanup plans to concurrency cleanup thunks, preserving per-field planned drop
  actions at the thunk boundary while still avoiding emitted drop calls.
- 2026-06-29: added a focused formatter for concurrency drop cleanup plans so thunk-scoped planned drop metadata can be
  inspected separately from global planned-drop action reports.
- 2026-06-29: modeled drop-call emission eligibility on concurrency drop cleanup plans, defaulting it to disabled so
  cleanup thunks remain metadata-only while future owned-drop lowering has an explicit switch.
- 2026-06-29: added an isolated smoke-only cleanup thunk emission seam that manually enables planned drop calls and pins
  the guarded LLVM call shape without enabling drop calls through normal lowering.
- 2026-06-29: strengthened CLI `emit-llvm` smoke coverage for aggregate thread captures so normal compilation must keep
  cleanup thunk drop actions metadata-only, with no emitted drop declarations or calls.
- 2026-06-29: replaced the raw cleanup drop-call flag with an explicit declaration-gated eligibility helper; smoke
  coverage now proves drop calls only enable when every cleanup action has a matching emitted drop declaration.
- 2026-06-29: added an internal allowlist-based drop declaration producer and combined smoke coverage proving emitted
  drop declarations and authorized cleanup thunk calls line up for an explicit test payload type.
- 2026-06-29: added an explicitly test-only LLVM emission option that wires the drop declaration allowlist through
  module emission, preserving normal metadata-only output while integration smoke coverage exercises declaration plus
  cleanup-call IR.
- 2026-06-30: added negative integration smoke coverage for mixed aggregate cleanup actions, proving a partial
  declaration allowlist may emit one declaration but still does not enable any calls for that cleanup thunk.
- 2026-06-30: added a drop cleanup authorization report seam that identifies missing emitted drop declarations per
  cleanup action, giving future diagnostics/tooling an explanation for blocked cleanup-call emission.
- 2026-06-30: exposed drop cleanup authorization blockers through `orisonc --drop-cleanup-authorization <file>`, keeping
  normal compilation output unchanged while matching the existing planned-drop inspection style.
- 2026-06-30: made drop cleanup authorization reports cleanup-site aware, so CLI blocker reports now include the actual
  generated cleanup thunk symbol instead of only a module-wide summary.
- 2026-06-30: accepted ADR-0005 to pin owned-drop semantics and ABI gating direction: normal lowering remains
  metadata-only until source-level drop semantics and emitted finite declarations are accepted together.
- 2026-06-30: added a neutral semantic drop model scaffold for future source-derived drop implementations and planned
  drop sites without adding parser syntax or changing normal lowering behavior.
- 2026-06-30: semantic analysis now records metadata-only planned drop sites for owned source-declared bindings at
  scope exit, excluding scalar, shared/exclusive, pointer, address, receiver, constant, and concurrency-handle bindings.
- 2026-06-30: added a semantic planned-drop report path and `orisonc --semantic-planned-drops <file>` so source-derived
  drop sites can be inspected without running lowering or enabling LLVM drop declarations/calls.
- 2026-06-30: added `orisonc --semantic-drop-resolution <file>` as an analyze-only report that marks semantic planned
  drop sites missing until source-derived, proven drop implementations exist.
- 2026-06-30: extended the internal semantic drop implementation representation with origin and finite body-summary
  metadata so future source-derived drop bodies can be modeled without adding source syntax or lowering authorization.
- 2026-06-30: added an explicitly test-only semantic drop implementation injection path for analyze-only pipeline
  coverage, proving semantic resolution can report both resolved and missing drop sites without exposing source syntax.
- 2026-06-30: added mixed semantic drop-resolution coverage where a test-only implementation resolves one owned source
  type while another owned type remains missing, keeping partial readiness visible before source syntax exists.
- 2026-06-30: added a pure semantic drop-resolution summary helper that groups resolved and missing planned drop sites
  by source type and ABI symbol for future diagnostics.
- 2026-06-30: exposed semantic drop-resolution summary lines on the analyze-only pipeline result, while keeping CLI
  output unchanged pending an explicit inspection-command decision.
- 2026-06-30: added `orisonc --semantic-drop-summary <file>` as an explicit analyze-only inspection command for
  grouped semantic drop-resolution counts.
- 2026-06-30: added a pure internal semantic collector for source-derived drop implementation candidates, including
  stable first-candidate wins deduplication and finite-body proof preservation.
- 2026-06-30: threaded source-derived drop implementation candidates through the analyze-only pipeline's test-only
  options so candidate metadata can drive semantic drop resolution without adding syntax or lowering authorization.
- 2026-06-30: added an analyze-only semantic drop implementation report on pipeline results so diagnostics can inspect
  the internal implementations considered for resolution.
- 2026-07-01: added analyze-only discovery provenance to semantic drop implementation report lines, distinguishing
  explicit test injection from candidate collection without changing source syntax or resolution behavior.
- 2026-07-01: added semantic drop diagnostic classification for unresolved planned drop sites, distinguishing missing
  implementation discovery from discovered-but-unproven implementations in analyze-only pipeline results.
- 2026-07-01: exposed semantic drop diagnostics through `orisonc --semantic-drop-diagnostics <file>` as an explicit
  analyze-only inspection command while keeping normal compile, build, run, and LLVM output unchanged.
- 2026-07-01: added a pure semantic extractor for source-derived drop implementation candidates from existing parsed
  `implements Drop for Type` metadata with a `drop` method, leaving finite proof and pipeline wiring as future work.
- 2026-07-01: threaded parsed source-derived drop implementation candidates through analyze-only pipeline reports so
  semantic drop diagnostics can distinguish discovered-but-unproven source metadata from undiscovered implementations.
- 2026-07-01: added a conservative finite-body proof scaffold for parsed source-derived drop implementations; all
  parsed bodies remain unproven until explicit proof rules are accepted and tested.
- 2026-07-01: added the first narrow parsed drop finite-body proof rule: empty drop bodies and naked-return-only drop
  bodies are semantically proven finite, still without authorizing LLVM drop declarations or calls.
- 2026-07-01: added driver smoke coverage proving semantically proven parsed drop implementations still do not emit
  `__orison_drop.*` declarations or calls in normal LLVM output.
- 2026-07-01: added an analyze-only semantic drop lowering-authorization report that distinguishes semantic planning
  resolution from blocked LLVM drop emission while source drop lowering remains unaccepted.
- 2026-07-01: exposed semantic drop lowering authorization through
  `orisonc --semantic-drop-lowering-authorization <file>` as an explicit analyze-only inspection command.
- 2026-07-01: added an internal semantic drop lowering authorization transition object requiring both semantic proof
  and an explicit source-drop-lowering gate, with the gate disabled by default.
- 2026-07-01: threaded structured semantic drop lowering authorizations into the lowering-facing metadata path while
  leaving the source-drop-lowering gate disabled and generated LLVM IR unchanged.
- 2026-07-01: connected semantic drop lowering authorizations to cleanup authorization reports so blocked cleanup thunks
  distinguish disabled semantic/source lowering gates from missing emitted drop ABI declarations.
- 2026-07-01: added a pure bridge from authorized semantic drop lowering sites to planned drop declarations, preserving
  disabled-by-default behavior because current source-drop-lowering authorizations remain blocked.
- 2026-07-01: added lowering smoke coverage for explicitly authorized semantic drop metadata, proving declarations can
  appear from semantic authorization while cleanup calls still require full per-thunk authorization.
- 2026-07-01: added a lowering/pipeline report seam for emitted drop declarations so tooling can inspect
  source-derived declaration readiness separately from cleanup action readiness.
- 2026-07-01: added an internal drop readiness snapshot that groups semantic drop authorization, emitted declaration
  readiness, and cleanup authorization readiness for future diagnostics.
- 2026-07-01: added `orisonc --drop-readiness <file>` as an explicit debug inspection command for the internal drop
  readiness snapshot while preserving normal compile/run output.
- 2026-07-01: added `tests/fixtures/drop_readiness.or` as a stable checked-in source for `--drop-readiness` CLI smoke
  coverage instead of relying only on temporary fixtures.
- 2026-07-02: extended pipeline smoke coverage to pin the checked-in `drop_readiness.or` fixture below the CLI through
  `CompilePipeline::emit_llvm` readiness snapshot reports.
- 2026-07-02: added a pure drop readiness summary helper and report that count authorized/blocked semantic drops,
  emitted declarations, and authorized/blocked cleanup sites for future compact diagnostics.
- 2026-07-02: exposed `orisonc --drop-readiness-summary` as an explicit diagnostics-only CLI command for the compact
  drop readiness counts while leaving detailed `--drop-readiness` output unchanged.
- 2026-07-02: added CLI smoke coverage proving `--drop-readiness-summary` propagates lowering failures with no stdout,
  matching the other diagnostics-only inspection commands.
- 2026-07-02: added pipeline smoke coverage proving drop readiness snapshot and summary reports remain empty when LLVM
  emission fails before readiness aggregation.
- 2026-07-02: consolidated driver smoke failure assertions for diagnostics/report commands that must fail with no
  stdout and an expected stderr diagnostic.
- 2026-07-02: extended the shared driver smoke failure helper to nearby scalar aggregate-assignment lowering
  diagnostics so failure-shape checks stay consistent.
- 2026-07-02: routed parse-failure smoke helpers through the same no-stdout failure assertion while keeping
  object-write and host-link failures explicit.
- 2026-07-02: added `orisonc --emitted-drops <file>` as a diagnostics-only CLI inspection command for emitted drop
  declaration reports; normal sources still report none until drop lowering is accepted.
- 2026-07-02: added a drop readiness relation report and `orisonc --drop-readiness-relations <file>` to correlate
  cleanup sites with semantic blockers, emitted declarations, and missing declarations without changing normal output.
- 2026-07-02: factored diagnostics-only driver report command routing through shared analyze/LLVM report helpers to
  reduce copy/paste across planned-drop, semantic-drop, and drop-readiness inspection commands.
- 2026-07-02: consolidated driver smoke single-file command wrappers through one helper while preserving named wrappers
  for readable call sites.
- 2026-07-02: expanded drop readiness relation reports with per-symbol semantic blocker and missing declaration detail
  lines so blocked cleanup sites identify the exact capture/type requiring future drop readiness work.
- 2026-07-03: added `tests/fixtures/drop_readiness_multi.or` so multi-capture blocked readiness relation output is
  pinned through checked-in pipeline and CLI smoke coverage.
- 2026-07-03: replaced the overlapping temporary multi-drop CLI smoke source with the checked-in
  `drop_readiness_multi.or` fixture for planned-drop, action, and cleanup-authorization report coverage.
- 2026-07-03: expanded pipeline smoke coverage for `drop_readiness_multi.or` to pin planned-drop, action,
  cleanup-authorization, readiness snapshot, and readiness summary reports below the CLI.
- 2026-08-01: removed the pipeline-level drop-readiness summary report vector; pipeline consumers now render summary
  report text from typed `DropReadinessSummary` state.
- 2026-08-01: removed the pipeline-level drop-readiness snapshot report vector; pipeline consumers now render snapshot
  report text from typed `DropReadinessSnapshot` state.
- 2026-08-01: removed the pipeline-level drop-readiness relation report vector; pipeline consumers now render relation
  report text from typed `DropReadinessSnapshot` state.
- 2026-08-01: removed the pipeline-level drop-readiness blocker report vector; pipeline consumers now render blocker
  report text from typed `DropReadinessBlockerSummary` state.
- 2026-08-01: removed the pipeline-level drop-readiness source-correlation report vector; pipeline consumers now render
  source-correlation report text from typed `DropReadinessSnapshot` state.
- 2026-07-03: replaced the remaining single-capture planned-drop/readiness temporary CLI smoke source with the
  checked-in `drop_readiness.or` fixture and removed redundant fixture-only readiness assertions.
- 2026-07-03: isolated `orison_driver_smoke` generated temp files under a per-process `TMPDIR`, preventing direct
  binary and CTest wrapper invocations from deleting or overwriting each other's fixed temp filenames.
- 2026-07-04: isolated `orison_pipeline_smoke` generated temp files under a per-process `TMPDIR` as well, removing
  the same direct binary versus CTest wrapper race for pipeline smoke fixtures.
- 2026-07-04: isolated `orisonc_cli_smoke` generated source/object/executable files under a per-process `TMPDIR`,
  covering the shell-out CLI smoke surface with the same concurrent-run protection.
- 2026-07-04: isolated `orison_module_parser_smoke` generated parser fixtures under a per-process `TMPDIR`, removing
  fixed-name parser fixture races for concurrent direct and CTest runs.
- 2026-07-04: isolated `orison_semantics_smoke` generated semantic-analysis fixtures under a per-process `TMPDIR`,
  extending fixed-temp race protection to the largest generated-fixture smoke binary.
- 2026-07-04: isolated `orison_lowering_smoke` generated lowering fixtures and object output under a per-process
  `TMPDIR`, matching the parser and semantic smoke isolation pattern.
- 2026-07-04: isolated `orison_statement_emitter_smoke` under a per-process `TMPDIR`, covering its generated statement
  lowering fixture.
- 2026-07-04: isolated `orison_function_emitter_smoke` generated function-lowering fixtures under a per-process
  `TMPDIR`.
- 2026-07-05: isolated `orison_expression_emitter_smoke` under a per-process `TMPDIR`, covering its generated
  expression lowering fixture.
- 2026-07-05: isolated `orison_control_flow_emitter_smoke` under a per-process `TMPDIR`, covering its generated final
  control-flow lowering fixture.
- 2026-07-05: isolated `orison_examples_smoke` under a per-process `TMPDIR`, covering its generated example-boundary
  validation fixtures.
- 2026-07-05: isolated `orison_host_linker_smoke` under a per-process `TMPDIR`, covering its generated executable and
  host-link temporary object path.
- 2026-07-05: isolated `orison_lexer_smoke` under a per-process `TMPDIR`, covering its generated keyword/token fixture.
- 2026-07-05: isolated `orison_canonical_pipeline_smoke` under a per-process `TMPDIR`, covering retained
  object/executable outputs plus driver-created temporary link artifacts.
- 2026-07-05: isolated `orison_concurrency_plan_smoke` under a per-process `TMPDIR`, covering its generated
  concurrency-planning fixture.
- 2026-07-05: added `orison_smoke_temp_isolation_guard` so CTest now fails if any smoke source uses
  `std::filesystem::temp_directory_path()` without first setting `TMPDIR`.
- 2026-06-26: added direct runtime ABI smoke coverage for pthread-backed thread join, task await, destroy-after-sync,
  and abandoned-handle destroy waiting behavior.
- 2026-06-26: added `examples/concurrency_thread_main.or` as a checked-in runnable scalar thread/join demo and promoted
  it into canonical compile/link/run smoke coverage.
- 2026-06-26: added `examples/concurrency_task_main.or` as a checked-in runnable scalar task/await demo and promoted it
  into canonical compile/link/run smoke coverage.
- 2026-06-25: host linking now includes a minimal pthread-backed concurrency runtime archive that resolves the accepted
  task/thread/await/join/destroy/spawn-failed ABI, allowing scalar task programs to link and execute through
  `orisonc run`.
- 2026-06-25: scalar task lowering now reuses the thread path for capture environments, result storage, entry thunks,
  spawn failure checks, abandoned-handle cleanup, and `await` result loads; `async function` lowering is still a narrow
  scalar backend admission rule rather than a real async ABI or suspension model.
- 2026-06-25: scalar thread spawn lowering now checks the returned handle for `null` and branches to
  `__orison_concurrency_spawn_failed()` plus `unreachable` before any join or abandoned-handle cleanup can observe the
  binding.
- 2026-06-25: abandoned scalar thread handles that reach a normal lowered function return without `.join()` now emit
  `__orison_concurrency_handle_destroy(handle)` before returning, without loading a result value.
- 2026-06-25: scalar `.join()` cleanup now emits `__orison_concurrency_handle_destroy(handle)` after loading the joined
  thread result, making successfully joined handles explicitly released.
- 2026-06-25: scalar `.join()` expressions for lowered thread bindings now emit `__orison_thread_join(handle)`, load the
  typed result from compiler-owned result storage, and return that value through ordinary expression lowering.
- 2026-06-25: scalar `thread` let-bindings now emit private entry thunks that load captures from the planned
  environment, lower the final scalar body expression, and store into compiler-owned result storage; spawn calls now
  pass the thunk pointer instead of `null`.
- 2026-06-25: scalar `thread` let-bindings now lower the planned capture environment allocation, capture stores, result
  storage allocation, and `__orison_thread_spawn(..., i64 result_size, ...)` call shape.
- 2026-06-25: concurrency planning now sizes capture environments and result storage through the lowering target
  layout helper; the first scalar `Int64` task/thread plans record 8-byte environments and 8-byte result storage.
- 2026-06-25: concurrency planning now records concrete LLVM capture-environment type strings, capture field indexes,
  and result-storage type strings.
- 2026-06-25: concurrency lowering now has a planning model for `task` and `thread` expressions that records the
  runtime spawn operation, deterministic thunk symbol, inferred lowered result type, and semantic captures without
  emitting runtime calls or thunk IR yet.
- 2026-06-25: module prelude emission can now declare the exact concurrency runtime symbols requested by lowering,
  deduplicated in first-use order, while ordinary modules still emit no concurrency runtime declarations.
- 2026-06-25: accepted ADR-0004 for the initial concurrency runtime lowering contract and added a pinned lowering
  model for opaque concurrency handles plus spawn/join/await/destroy runtime symbols; `tour_11_concurrency.or` remains
  frontend-only until thunk, environment, result-storage, and runtime-linking work lands.
- 2026-06-25: concurrency lowering remains intentionally frontend-only until async/task/thread runtime representation
  is designed; lowering diagnostics now report unsupported `task`, `thread`, and `await` expression shapes directly
  instead of falling back to generic let-binding or expression failures.
- 2026-06-25: uninstantiated generic function and method definitions are now accepted as metadata-only during LLVM
  module emission, so generic-only examples can emit object files before monomorphization exists; this promotes
  `tour_04_generics_ownership.or` to backend validation without claiming generic body lowering.
- 2026-06-25: `View<T>` parameters now lower as pointer operands for supported element types, view indexing lowers
  through `getelementptr`/`load`, and hex/binary integer literals normalize to LLVM decimal constants, so
  `tour_08_collections.or` is now backend-validated.
- 2026-06-25: modules with implementation or extension methods no longer require a top-level function before LLVM IR
  emission, so `tour_03_interfaces_methods.or` is now backend-validated through method body lowering.
- 2026-06-25: `tour_01_packages_imports.or`, `tour_02_records_choices.or`, `tour_06_control_flow.or`,
  `tour_07_recursion.or`, and `tour_10_unsafe_memory.or` are now backend-validated examples because their existing
  lowering paths emit object code successfully.
- 2026-06-25: integer bitwise and shift expressions now lower to LLVM, `bit_not` lowers through an all-bits XOR, and
  unannotated scalar `let` bindings can infer supported binary/unary initializer types; `tour_05_bindings_operators.or`
  is now backend-validated.
- 2026-06-25: `address_of(...)` aggregate path lowering now advances member/index path steps through one local helper
  that keeps index expression lowering and diagnostic mapping together.
- 2026-06-25: `address_of(...)` aggregate path lowering now resolves pointer-backed and mutable-local base storage
  through a dedicated base helper before walking the shared aggregate cursor path.
- 2026-06-25: `Pointer(...)` construction lowering now lives behind a dedicated expression helper, keeping constructor
  validation and `Address`-to-pointer conversion out of the main expression dispatcher.
- 2026-06-25: `raw_offset` lowering now shares local address-offset and pointer-offset emission helpers, keeping
  temporary allocation and LLVM instruction formatting centralized while retaining existing result-type validation.
- 2026-06-25: raw and volatile low-level read/write lowering now share LLVM load/store emission helpers, keeping
  volatile spelling and temporary allocation centralized while preserving intrinsic-specific validation.
- 2026-06-25: pointer/address conversion emission is centralized inside expression lowering, so `address_of`, generic
  address operands, and `Pointer(...)` construction share the same `ptrtoint`/`inttoptr` instruction shape.
- 2026-06-25: aggregate assignment target lowering now uses the shared member/index cursor-advancement helpers, keeping
  temporary naming centralized while preserving assignment-specific diagnostics.
- 2026-06-24: storage-backed aggregate path reads now consistently call the shared member/index cursor-advancement
  helpers from expression lowering, keeping temporary naming policy centralized.
- 2026-06-24: aggregate index-step temporary naming and cursor advancement now live in the shared aggregate path helper;
  expression lowering still owns recursive lowering of the index operand.
- 2026-06-24: aggregate member-step temporary naming and cursor advancement now live in the shared aggregate path helper;
  index-step handling remains in expression lowering because index operands recursively lower expressions.
- 2026-06-24: aggregate path cursor load emission now lives in the shared aggregate path helper, while expression
  lowering retains expected-type validation, index-expression lowering, and diagnostics.
- 2026-06-24: aggregate path collection now exposes named-base and temporary-base classifiers, removing duplicated
  path-shape checks from expression lowering while preserving storage-backed versus temporary-spill behavior.
- 2026-06-24: named aggregate path reads now receive storage plus optional source-type metadata from the shared
  aggregate resolver, preserving the distinction between non-addressable paths and addressable paths missing metadata.
- 2026-06-24: named aggregate path reads now resolve mutable storage and read-only addressable storage through the
  shared aggregate helper module, preserving mutable-over-read-only precedence while reducing expression-emitter policy.
- 2026-06-24: temporary aggregate path reads and runtime-index fixed-array value reads now share the aggregate spill
  helper module, preserving emitted IR shape while reducing duplicated alloca/store lowering setup.
- 2026-06-24: read-only aggregate addressable binding setup now uses a shared lowering helper, keeping parameter,
  immutable aggregate `let`, and aggregate `for` item storage conventions aligned without changing emitted IR shape.
- 2026-06-24: aggregate `for` iteration values now become read-only addressable bindings when the element type is a
  lowered record or fixed array; checked-in CLI coverage pins `%record.Entry` loop item storage and field loads inside
  a returned-container `for` body.
- 2026-06-20: immutable record and fixed-array aggregate `let` bindings now retain read-only storage alongside their SSA
  value so later member/index reads reuse the address-backed aggregate path; checked-in CLI coverage pins inferred
  record-array, array-record, nested mixed, and branch-local inferred aggregate lets.
- 2026-06-20: helper-returned and method-returned record/fixed-array aggregate values that are immediately read through
  member/index paths now store the returned aggregate once and walk the collected path with `getelementptr`/`load`;
  checked-in CLI coverage pins helper, scalar-method, record-method, and member-receiver method aggregate access.
- 2026-06-20: aggregate record and fixed-array parameters now receive read-only function-entry storage so member/index
  reads can reuse address-backed `getelementptr`/`load` paths instead of repeatedly extracting from SSA aggregate
  values; checked-in CLI coverage pins plain function, method, control-flow, loop, guard, defer, FFI, and returned-container paths.
- 2026-06-20: mutable local aggregate member/index expression reads now reuse storage-derived aggregate paths with
  direct `getelementptr`/`load` operations instead of loading the whole aggregate and extracting from the value;
  checked-in backend and CLI coverage pins constant and runtime nested mutable aggregate reads.
- 2026-06-20: dynamic value-aggregate indices now spill the aggregate value to temporary storage, use `getelementptr`
  with the runtime index, and load the selected element; checked-in backend and CLI coverage now pins dynamic-index
  `while`-built returned record containers plus `for`-built returned fixed-array containers from aggregate-derived
  scalar operands.
- 2026-06-20: checked-in backend and CLI coverage now pins branch-local returned record and fixed-array containers
  built from aggregate-derived scalar operands through final `if` and `switch` merges.
- 2026-06-20: checked-in backend and CLI coverage now pins aggregate-derived scalar operands used to construct
  returned nested record-with-array and fixed-array containers.
- 2026-06-20: checked-in backend and CLI coverage now pins aggregate-derived scalar operands used to construct
  returned record and fixed-array containers.
- 2026-06-20: checked-in backend and CLI coverage now pins aggregate-derived scalar operands passed to explicit
  fixed-parameter source-level C FFI adapter calls.
- 2026-06-20: checked-in backend and CLI coverage now pins aggregate-derived scalar operands passed directly to
  plain function and member method call arguments.
- 2026-06-20: checked-in backend and CLI coverage now pins aggregate-derived scalar values from pointer-backed
  aggregate `address_of` plus `raw_read` paths inside an `unsafe` block.
- 2026-06-19: checked-in backend and CLI coverage now pins aggregate-derived scalar values through deferred cleanup
  replay and early/final returns.
- 2026-06-19: checked-in backend and CLI coverage now pins aggregate-derived scalar guard conditions, guard failure
  returns, and final expressions after guard merges.
- 2026-06-19: checked-in backend and CLI coverage now pins aggregate-derived scalar arithmetic through `while` and
  `for` loop-body accumulation.
- 2026-06-19: checked-in backend and CLI coverage now pins aggregate-derived scalar arithmetic through final `if` and
  `switch` expression merges.
- 2026-06-19: aggregate member/index expression lowering now preserves scalar type inference and record-field
  signedness, so binary expressions can combine aggregate-derived values with receiver fields inside methods.
- 2026-06-19: checked-in backend and CLI coverage now pins field and index extraction from record and fixed-array
  aggregate parameters across both plain function and method call boundaries.
- 2026-06-19: checked-in backend and CLI coverage now pins field and index extraction from method-returned record and
  fixed-array aggregates when the receiver is recovered through member and index access.
- 2026-06-19: checked-in backend and CLI coverage now pins field and index extraction from record-method-returned
  record and fixed-array aggregates.
- 2026-06-19: checked-in backend and CLI coverage now pins field and index extraction from scalar-method-returned
  record and fixed-array aggregates.
- 2026-06-19: checked-in backend and CLI coverage now pins field and index extraction from helper-returned record and
  fixed-array aggregates.
- 2026-06-19: checked-in backend and CLI coverage now pins switch-local nested fixed-array element assignment on an
  inferred mutable aggregate followed by post-switch nested index access.
- 2026-06-19: checked-in backend and CLI coverage now pins branch-local nested fixed-array element assignment on an
  inferred mutable aggregate followed by post-branch nested index access.
- 2026-06-19: checked-in backend and CLI coverage now pins switch-local nested field assignment on an inferred mutable
  aggregate followed by post-switch nested field/index access.
- 2026-06-19: checked-in backend and CLI coverage now pins branch-local nested field assignment on an inferred mutable
  aggregate followed by post-branch nested field/index access.
- 2026-06-19: checked-in backend and CLI coverage now pins switch-local whole-value reassignment of an inferred mutable
  aggregate followed by post-switch nested field/index access.
- 2026-06-19: checked-in backend and CLI coverage now pins branch-local whole-value reassignment of an inferred
  mutable aggregate followed by post-branch nested field/index access.
- 2026-06-18: checked-in backend and CLI coverage now pins inferred mutable aggregate whole-value reassignment followed
  by nested field/index access, proving `var` source-type metadata survives direct aggregate stores.
- 2026-06-18: CLI smoke coverage now pins LLVM branch/aggregate extraction shape, object emission, direct `run`, and
  retained `--build` execution for branch-local inferred aggregate `let` bindings in final `if` arms.
- 2026-06-18: checked-in backend coverage now pins branch-local inferred aggregate `let` bindings inside final `if`
  arms, proving branch scopes preserve aggregate source metadata within each arm and restore it afterward.
- 2026-06-18: CLI smoke coverage now emits an object file and builds plus executes a retained binary for the nested
  inferred mixed aggregate demo, completing `--emit-llvm`, `--emit-object`, `run`, and retained `--build` coverage.
- 2026-06-18: CLI smoke coverage now pins user-facing LLVM IR extraction shapes for the nested inferred mixed aggregate
  demo, covering record-field-to-array-index-to-record-field immutable `let` metadata composition.
- 2026-06-18: checked-in backend coverage now pins nested inferred mixed aggregate extraction through
  record-field-to-array-index-to-record-field immutable `let` metadata composition.
- 2026-06-18: CLI smoke coverage now builds and executes retained binaries for the mixed inferred aggregate demos,
  completing `--emit-llvm`, `--emit-object`, `run`, and retained `--build` coverage for both metadata-composition
  directions.
- 2026-06-18: CLI smoke coverage now emits object files for the mixed inferred aggregate demos after the pinned
  `--emit-llvm` extraction checks, extending record-field-to-array-index and array-index-to-record-field coverage
  through host object generation.
- 2026-06-18: CLI smoke coverage now pins user-facing LLVM IR extraction shapes for the mixed inferred aggregate
  demos, covering both record-field-to-array-index and array-index-to-record-field immutable `let` paths.
- 2026-06-18: checked-in backend coverage now pins immutable record-field extraction from an inferred fixed array of
  records, proving fixed-array element metadata composes into record field extraction.
- 2026-06-18: checked-in backend coverage now pins immutable array-field index extraction from an unannotated
  record-constructor `let`, proving constructor-derived metadata composes from record fields into fixed-array elements.
- 2026-06-18: checked-in backend coverage now pins nested immutable field extraction from an unannotated
  record-constructor `let`, proving constructor-derived source metadata composes across record field chains.
- 2026-06-18: checked-in backend coverage now pins unannotated record-constructor `let` source-type recovery through
  immutable field extraction, proving constructor-derived source metadata survives beyond aggregate materialization.
- 2026-06-18: checked-in backend coverage now pins inferred nested immutable fixed-array `let` bindings with explicit
  leaf element types, exercising recursive array-literal inference plus retained source-type metadata through nested
  index access.
- 2026-06-18: immutable `let` lowering now prefers annotated or inferred initializer types over the enclosing return
  fallback, enabling unannotated fixed-array aggregate bindings with explicit element types and preserving inferred
  source type names for later indexed use.
- 2026-06-18: README development workflow now advertises the `canonical_pipeline` CTest label, so contributors can
  discover the focused compiler pipeline smoke path from the main repo entry point.
- 2026-06-18: added a dedicated `orison_canonical_pipeline_smoke` CTest with the `canonical_pipeline` label so
  contributors can quickly run the canonical `minimal.or`, local aggregate assignment, and pointer-backed aggregate
  assignment demos across `--emit-llvm`, `--emit-object`, `run`, and retained `--build` paths.
- 2026-06-18: examples documentation now identifies `minimal.or`, `local_record_field_assignment.or`, and
  `pointer_record_field_assignment.or` as the canonical compiler pipeline demos for the currently pinned IR, object,
  run, and retained build paths.
- 2026-06-18: CLI smoke coverage now also builds and executes a retained binary for the focused pointer-backed
  array-of-record field assignment example, completing local/pointer parity across `--emit-llvm`, `--emit-object`,
  `run`, and retained `--build` paths.
- 2026-06-18: CLI smoke coverage now builds and executes a retained binary for the focused local array-of-record field
  assignment example, pinning the compile-link-run path beyond `orisonc run`.
- 2026-06-18: CLI smoke coverage now also emits object files for the focused local and pointer-backed
  array-of-record field assignment examples, pinning host object generation after the already-pinned user-facing LLVM
  IR path.
- 2026-06-18: CLI smoke coverage now emits LLVM for the focused local and pointer-backed array-of-record field
  assignment examples, pinning the user-facing GEP/store shape for `log.entries[index].status = value` in both
  mutable-local and `Pointer<Log>` paths.
- 2026-06-18: compiler-app and CLI smoke coverage now pin source-reachable aggregate assignment lowering failures for
  scalar member and scalar index targets, so user-facing `--emit-llvm` diagnostics stay aligned with the direct
  statement-emitter diagnostics.
- 2026-06-18: statement-emitter smoke coverage now pins aggregate assignment diagnostics for missing source-type
  metadata plus scalar member/index targets, so unsupported aggregate assignment shapes fail explicitly instead of
  silently drifting while happy-path local and pointer-backed assignment coverage expands.
- 2026-06-18: checked-in example coverage now includes a focused backend demo for local array-of-record field
  assignment, pinning `log.entries[index].status = value` through mutable local record storage separately from the
  pointer-backed aggregate assignment demos.
- 2026-06-18: checked-in example coverage now includes a focused backend demo for pointer-backed array-of-record field
  assignment, pinning `log.entries[index].status = value` through a `Pointer<Log>` source separately from the mixed
  pointer-backed aggregate assignment demo.
- 2026-06-18: checked-in example coverage now includes a focused backend demo for pointer-backed nested fixed-array
  assignment, pinning `matrix.rows[index][inner] = value` through a `Pointer<Matrix>` source separately from the mixed
  pointer-backed aggregate assignment example.
- 2026-06-18: checked-in example coverage now includes a small backend demo for pointer-backed nested record and
  fixed-array addressing, pinning `address_of(log.entries[index].status)` through a `Pointer<Log>` source and
  `address_of(matrix.rows[index][inner])` through a `Pointer<Matrix>` source separately from local record-backed
  addressing and pointer-backed assignment examples.
- 2026-06-17: record/member/index aggregate path collection and LLVM `getelementptr` cursor advancement now live in a
  shared lowering helper consumed by both `address_of(...)` expression lowering and aggregate assignment lowering, with
  direct smoke coverage for path collection, cursor transitions, GEP text, and primary error outcomes.
- 2026-06-17: source-type recovery for record fields, fixed arrays, pointers, lowered LLVM types, call returns, and
  member-call returns now lives in a shared lowering query component consumed by member-call receiver inference,
  expression emission, statement emission, and function `for` iterable recovery.
- 2026-06-17: lowering now resolves fixed-size arrays returned by methods on non-generic record receivers with
  supported layouts, so shapes like `wrapper.view()` lower through the same single-array-lower plus
  per-element-extract path.
- 2026-06-17: member-call receiver inference now recovers source types through record member access, fixed-array index
  access, function-call returns, and method-call returns, so method-returned fixed-array iteration can lower shapes like
  `wrapper.bucket.view()` and `shelf.buckets[0].view()`.
- 2026-06-16: lowering now also resolves fixed-size arrays returned by methods for `for` loops, so shapes like
  `value.triple()` on scalar receivers lower through the same single-array-lower plus per-element-extract path.
- 2026-06-16: lowering now also resolves fixed-size arrays returned by helper calls for `for` loops, so shapes like
  `make_values()` lower through the same single-array-lower plus per-element-extract path.
- 2026-06-16: lowering now also resolves fixed-size arrays reached through indexed record fields for `for` loops, so
  shapes like `grid.rows[0].values` lower through the same single-array-lower plus per-element-extract path.
- 2026-06-16: lowering now materializes immutable aggregate `let` bindings for lowerable non-generic record
  constructor calls and fixed-array literals, so immutable locals can hold supported record and array values without
  introducing a mutable stack slot; broader aggregate construction remains pending.
- 2026-06-16: lowering now unrolls `for` loops over named fixed-size array values too, by lowering the array once and
  extracting each element per iteration; inline array literals still use the existing per-element path, and broader
  iterable support remains pending.
- 2026-06-16: lowering now also resolves nested record-backed fixed-size array sources for `for` loops, so member
  chains like `wrapper.bucket.values` follow the same single-lower plus per-element-extract path as local array names.
- 2026-06-16: lowering now also resolves index-derived fixed-size array sources for `for` loops, so indexed shapes
  like `grid.rows[0]` lower through the same single-array-lower plus per-element-extract path.
- 2026-06-16: checked-in example coverage now also includes a small backend demo for nested immutable aggregate `let`
  bindings, pinning nested record and array literals on a single immutable local separate from the flat record and
  array binding example.
- 2026-06-16: checked-in example coverage now includes a small backend demo for nested pointer-backed aggregate
  assignment, using a local record/pointer round-trip to keep the `pointer.items[index].field = value` and
  `pointer.rows[index][inner] = value` slice pinned outside the frozen spec and tour documents.
- 2026-06-16: checked-in example coverage now also includes a small backend demo for local record and fixed-array
  reassignment, pinning the whole-value aggregate path independently from the pointer-backed demo.
- 2026-06-16: checked-in example coverage now also includes a small backend demo for record-value-backed nested
  addressing, pinning `address_of(local.entries[index].status)` and `address_of(local.rows[index][inner])` as a
  separate runnable example.
- 2026-06-16: checked-in example coverage now also includes a small backend demo for nested record-field addressing on
  local values, pinning `address_of(local.registers.status)` and `address_of(local.buffer.bytes[index])` separately
  from the array-backed addressing examples.
- 2026-06-16: checked-in example coverage now also includes a small backend demo for nested record-field and
  fixed-array assignment on a local value, pinning `device.registers.status = value` and
  `device.buffer.bytes[index] = value` separately from the address-only examples.
- 2026-06-16: lowering now materializes lowerable non-generic record constructor calls as LLVM aggregate values via
  `insertvalue`, allows those values in mutable local `var` storage, and lowers `address_of(local.field)` through the
  same record-layout metadata already used by pointer-backed aggregate paths; immutable aggregate `let` bindings and
  broader aggregate construction remain pending.
- 2026-06-16: lowering now materializes fixed array literals as LLVM aggregate values when an expected fixed-array
  shape is already known, which extends mutable local aggregate coverage to local record fields such as
  `address_of(local.items[index])`, nested local arrays such as `address_of(local.rows[index][inner])`, array-of-record
  local fields such as `address_of(local.entries[index].status)`, and direct annotated mutable local arrays such as
  `address_of(bytes[index])`; immutable aggregate `let` bindings remain pending.
- 2026-06-16: whole-value mutable-local aggregate reassignment now lowers for the same supported non-generic
  record/fixed-array subset as aggregate initialization, so `var regs = ...; regs = ...` and
  `var bytes: Array<Byte, 4> = ...; bytes = ...` now emit direct aggregate stores before later field/index address use;
  broader aggregate assignment shapes remain pending.
- 2026-06-16: parser assignment statements now accept indexed targets such as `items[index] = value`, and lowering now
  supports supported aggregate field/index assignment on the current non-generic record/fixed-array subset for both
  mutable-local storage and pointer-backed unsafe paths, including `local.field = value`, `local.items[index] = value`,
  `pointer.field = value`, `pointer.items[index] = value`, and nested pointer-backed mixed paths like
  `pointer.items[index].field = value` plus `pointer.rows[index][inner] = value`.
- 2026-06-16: fixed `Array<T, N>` record fields now lower to LLVM array field types when `T` is supported, and
  `address_of(pointer.array[index])` plus `address_of(pointer.inner.array[index])` lower through terminal array element
  GEPs for known `Pointer<Record>` sources; array-of-record element field paths like
  `address_of(pointer.items[index].field)` and nested-array shapes like `address_of(pointer.rows[index][inner])` now
  lower through the same mixed member/index walker.
- 2026-06-16: `address_of(pointer.inner.field)` now lowers for chained record-field paths on known `Pointer<Record>`
  sources by emitting one struct GEP per field hop; general indexed aggregate chains and record-value-backed field
  addresses remain pending.
- 2026-06-16: lowering context now collects record field layout metadata from parsed records, preserving field order,
  source type names, supported LLVM field types, and named LLVM struct type names; non-generic records with fully
  supported fields now emit named LLVM structs, and `address_of(pointer.field)` lowers through a struct GEP for known
  `Pointer<Record>` sources while record value and array layout emission remain pending.
- 2026-06-16: the initial raw/MMIO backend slice now lowers `Pointer<T>`-proven `raw_read`, `raw_write`,
  `raw_offset`, `volatile_read`, and `volatile_write`; volatile operations emit LLVM volatile loads/stores, while
  record-value forms of `address_of` remain pending.
- 2026-06-16: ordinary non-`Unit` function bodies now also accept leading `repeat`, `for`, and `unsafe` statements
  before the final expression, while still rejecting statements after a terminating non-`Unit` statement.
- 2026-06-16: `unsafe function` declarations now lower identically to ordinary functions; the unsafe marker remains a
  semantic boundary and does not affect emitted LLVM signatures.
- 2026-06-11: expression lowering now records structured first-failure reasons for unsupported expressions, names, type/signedness mismatches, casts, operators, branch mismatches, and call lookup/type/arity/argument failures; let/return diagnostics include rendered detail.
- 2026-06-11: function definitions now consume the authoritative shared lowered signature for return/parameter types and signedness; unsupported ordinary signatures remain in context for targeted diagnostics, eliminating parallel syntax-to-LLVM remapping without changing IR.
- 2026-06-11: LLVM local, temporary, and indexed block naming now use a shared stateless utility with direct counter/collision coverage; expression, control-flow, and function output remain unchanged.
- 2026-06-11: final value-producing `if`/`switch` CFG emission, nested value blocks, and branch-local immutable bindings now compile in a dedicated control-flow component; function assembly and generated IR remain unchanged.
- 2026-06-11: recursive LLVM expression lowering now compiles in a dedicated translation unit; function statement/control-flow emission consumes shared expression, inference, and literal APIs with direct and full-suite output preserved.
- 2026-06-11: expression lowering now exposes a dedicated API plus a shared SSA value/state model for bindings, temporary names, block identities, and current-block tracking; direct ternary/arithmetic coverage pins deterministic IR while full module output remains unchanged.
- 2026-06-11: per-function LLVM signature, SSA expression, branch, switch, and return emission now live behind a dedicated component with direct success and diagnostic coverage; module orchestration and generated IR remain unchanged.
- 2026-06-11: LLVM string-global and foreign-declaration formatting now live in a dedicated module-prelude component with independent fixed and adapted declaration coverage; complete emitter output remains unchanged.
- 2026-06-11: string literal decoding, null termination, deduplication, nested module traversal, and LLVM byte encoding now live in a dedicated lowering component with independent escape and ordering coverage; emitted globals remain unchanged.
- 2026-06-11: module-wide callable collection and foreign ABI adaptation now live in a dedicated lowering-context builder with independent success and diagnostic coverage; the LLVM emitter retains only IR-specific string-global state and generated IR remains unchanged.
- 2026-06-11: lowered function signatures now use a shared structured model and builder across ordinary context collection and C ABI adaptation, with independent coverage for complete, adapted, and unsupported signatures; generated IR remains unchanged.
- 2026-06-11: Orison-to-LLVM scalar type mapping, integer signedness, and parameter-vector lowering now live in a dedicated shared lowering module with independent smoke coverage; emitter output remains unchanged.
- 2026-06-11: C ABI adapter lookup, fixed-prefix/trailing-type validation, and promotion classification now live in a dedicated lowering module with independent smoke coverage; LLVM IR emission consumes the policy without changing generated output.
- 2026-06-11: fixed-to-variadic C adapter declarations now reject unsupported trailing parameter representations before function lowering; targeted coverage rejects `Text` while retaining pointers, integers through 64 bits, `Float32`, and `Float64`.
- 2026-06-11: canonical decimal/exponent float literals now lex, parse, infer, and lower as `Float32`/`Float64`; fixed `printf` adapter coverage pins C's required `Float32`-to-`Float64` promotion while explicit `Float64` arguments remain LLVM `double`.
- 2026-06-11: fixed `printf` adapter coverage now pins explicit trailing `Pointer<Byte>`, `Int64`, and `UInt64` parameters as LLVM `ptr`, signed `i64`, and unsigned `i64` operands without promotion; only narrow integer arguments use C default promotions.
- 2026-06-11: lowering now uses explicit external-symbol adapter metadata to map fixed Orison `printf` declarations onto LLVM's variadic C ABI form; the adapter validates the fixed `Pointer<Byte>` prefix and applies C integer promotions only to statically declared trailing arguments, with no variadic or spread syntax added.
- 2026-06-11: explicit C `library "name"` clauses now propagate through the shared pipeline into `--build` and `run` as deduplicated direct linker arguments; host-linker coverage resolves and executes `cos` from `libm`, and the checked-in FFI example includes the tour's math-library declarations.
- 2026-06-11: C FFI declarations remain finite and fixed-arity at the Orison surface; semantic analysis now diagnoses call arity mismatches, LLVM golden coverage pins a two-parameter `strcmp` call, and `examples/ffi_fixed_parameters.or` compiles, links, and exits successfully without variadic or spread syntax.
- 2026-06-11: the C FFI hello-world example now lowers and runs; C imports without library clauses emit LLVM declarations, string literals passed to foreign `Pointer<Byte>` parameters become immutable null-terminated globals, host objects use PIC relocation, and the string-to-pointer conversion remains unavailable to ordinary function calls.
- 2026-06-11: `orisonc run <file>` now compiles, links, and executes through the shared pipeline while propagating the program exit status and cleaning temporary artifacts; eleven frontend-validated `examples/tour_*.or` files now cover the implemented tour surface, including a C `printf` hello-world source whose backend requirements are explicitly tracked.
- 2026-06-11: shared source-loading, parse, semantic, verified-IR, and object stages now live in `orison_pipeline`; all compiler CLI modes reuse it, and `examples/minimal.or` provides a checked-in end-to-end build/link/run demo.
- 2026-06-11: `orisonc --build <file> -o <executable>` now compiles through native object emission and links a runnable host executable through the dedicated `orison_link` library and a CMake-discovered Clang/C driver.
- 2026-06-11: `orisonc --emit-object <file> -o <output>` now lowers verified IR through a host LLVM `TargetMachine`, applies the host triple and data layout, and writes a native object file after successful code generation.
- 2026-06-11: lowering now links LLVM in-process, parses every generated textual module, and runs LLVM module verification before publishing IR; malformed generated output becomes a lowering diagnostic and is never emitted by `orisonc --emit-llvm`.
- 2026-06-11: `orisonc --emit-llvm <file>` now runs source loading, parsing, semantic analysis, and LLVM IR emission through the compiler driver, writing successful textual IR to stdout and path-aware stage diagnostics to stderr.
- 2026-06-10: final scalar value-pattern `switch` statements now emit LLVM `switch` terminators, isolated case blocks, and merge `phi` values; integer/boolean patterns, case-local bindings, explicit defaults, exhaustive no-default boolean switches, and recursively nested final branch containers are supported.
- 2026-06-10: final branch blocks now lower recursively when their last statement is another value-producing `if ... else`; nested arms inherit outer branch bindings, restore sibling scopes, and feed their inner merge value into the outer `phi`.
- 2026-06-10: final `if ... else` lowering now supports multi-statement arms with leading immutable bindings; branch scopes restore outer bindings between arms while function-wide SSA name counters uniquify repeated local names.
- 2026-06-10: lowering now emits final value-producing `if ... else` statements with branch labels and an LLVM `phi`, covering implicit expression arms, explicit value-return arms, and ternaries nested inside an arm.
- 2026-06-10: lowering now emits integer and boolean ternary expressions as conditional branches with then/else labels and an LLVM `phi` result; current-block tracking preserves correct predecessor labels for nested ternaries.
- 2026-06-10: lowering now emits boolean literals and word-based boolean operators: `true`/`false` become `i1 1`/`i1 0`, `not` emits `xor`, and `and`/`or` emit LLVM `i1` operations with nested comparison operands.
- 2026-06-05: lowering now preserves fixed-width integer signedness in lowered value metadata, so signed `Int*` division emits LLVM `sdiv` and signed comparisons emit `slt`/`sle`/`sgt`/`sge` while unsigned paths keep `udiv` and unsigned predicates.
- 2026-06-05: lowering now emits fixed-width integer comparisons returning `Bool`/`i1` through LLVM `icmp`, including `eq`, `ne`, and unsigned ordering predicates for the current `UInt32` path.
- 2026-06-05: lowering now maps fixed-width integer `-`, `*`, and `/` through the existing recursive SSA path, emitting LLVM `sub`, `mul`, and unsigned `udiv` alongside the existing `add` coverage.
- 2026-06-05: lowering now has golden coverage for multi-parameter fixed-width integer functions and calls, including signatures such as `define i32 @add(i32 %left, i32 %right)` and calls such as `call i32 @add(i32 40, i32 2)`.
- 2026-06-05: lowering now supports single fixed-width integer parameters in same-module functions and calls, binding incoming parameters as local SSA values and emitting typed call operands such as `call i32 @increment(i32 41)`.
- 2026-06-05: lowering now supports zero-argument same-module function calls through a precomputed signature map, emitting call temporaries such as `%tmp0 = call i32 @one()` and allowing call results to feed the existing `+` lowering path.
- 2026-06-05: lowering now emits simple fixed-width integer `+` expressions as SSA temporaries, reusing the immutable binding value map for operands such as `%tmp0 = add i32 %left, %right` before the final return.
- 2026-06-05: lowering now supports a leading immutable `let` initialized from the existing constant integer/cast slice and a final name return, emitting a simple SSA local such as `%value = add i32 0, 1` before `ret i32 %value`.
- 2026-06-05: first LLVM IR lowering scaffold added as `orison_lowering`; it consumes parsed modules plus successful semantic results and emits golden-tested text IR for a constant-returning `UInt32` function while diagnosticing unsupported function and expression shapes.
- 2026-06-05: aggregate-context closure pass now treats expected-context propagation as broad enough to stop expanding by default; `docs/lowering-prep.md` records the first LLVM IR emission target and the minimal AST plus semantic facts needed before lowering work starts.
- 2026-06-05: holder final `if ... else` aggregate smoke coverage now pins `Holder(Items([[Box(...)]]))` and `Holder(Items([[Slot(...)]]))` arms, confirming return context flows through branch containers, record fields, choice constructors, and nested array payloads.
- 2026-06-05: aggregate-context matrix review confirmed the remaining useful frontend gap is final-container propagation; final `if ... else` smoke coverage now pins `Items([[Box(...)]])` and `Items([[Slot(...)]])` arms, confirming return context flows through branch containers into choice-payload nested arrays.
- 2026-06-05: holder choice-payload nested-array return and final-expression smoke coverage now pins `return Holder(Items([[Box(...)]]))`, final `Holder(Items([[Box(...)]]))`, and matching `Slot` pointer variants, confirming return context flows through record fields, choice constructors, and nested array payloads into generic record-constructor ternary fields.
- 2026-06-05: choice-payload nested-array return and final-expression smoke coverage now pins `return Items([[Box(...)]])`, final `Items([[Box(...)]])`, and matching `Slot` pointer variants, confirming function return context flows through choice constructors into nested array payloads and generic record-constructor ternary fields.
- 2026-06-05: holder choice-payload nested-array assignment smoke coverage now pins `holder = Holder(Items([[Box(...)]]))` and `holder = Holder(Items([[Slot(...)]]))`, confirming assignment target context flows through record fields, choice constructors, and nested array payloads into generic record-constructor ternary fields.
- 2026-06-05: choice-payload nested-array assignment smoke coverage now pins `item = Items([[Box(...)]])` and `item = Items([[Slot(...)]])`, confirming assignment target context flows through choice constructors into nested array payloads and onward to generic record-constructor ternary fields.
- 2026-06-05: nested-array pointer assignment smoke coverage now pins `slots = [[Slot(flag ? raw_offset(...) : raw_offset(...))]]`, confirming assignment target context flows into nested array literals and onward to generic `Pointer<T>` record-constructor ternary fields.
- 2026-06-05: nested-array assignment smoke coverage now pins `values = [[Box(...)]]` record-constructor fields and `values = [[flag ? Some(...) : Empty]]` choice elements, confirming assignment target context flows into nested array literals and onward to generic choice-constructor ternary arms.
- 2026-06-05: nested-array function and method call argument smoke coverage now shares semantic and CLI source builders plus assertion helpers, preserving the explicit `consume(...)` and `consumer.consume(...)` matrix while reducing duplicate fixture declarations.
- 2026-06-05: nested-array method-call argument smoke coverage now pins `consumer.consume([[Box(...)]])` and `consumer.consume([[flag ? Some(...) : Empty]])`, confirming receiver method parameter context flows into nested array literals and onward to record-constructor fields plus generic choice-constructor ternary arms.
- 2026-06-05: nested-array function-call argument smoke coverage now pins `consume([[Box(...)]])` and `consume([[flag ? Some(...) : Empty]])`, confirming expected parameter context flows into nested array literals and onward to record-constructor fields plus generic choice-constructor ternary arms.
- 2026-06-05: direct record-field nested-array smoke coverage now shares semantic and CLI assertion helpers across `Shelf`, `Rack`, and `ChoiceShelf` shapes, reducing duplicate temp-path and diagnostic wiring while keeping each aggregate source shape explicit.
- 2026-06-05: direct record-field nested-array choice-element smoke coverage now pins `ChoiceShelf([[flag ? Some(...) : Empty]])`, confirming expected context flows from record fields into nested array literals and onward to generic choice-constructor ternary arms without record or choice-payload wrappers.
- 2026-06-05: direct record-field nested-array smoke coverage now pins `Shelf([[Box(...)]])` and `Rack([[Slot(...)]])` shapes, confirming expected context flows from record fields into nested array literals and onward to generic `Maybe<T>` and `Pointer<T>` record-constructor ternaries without a choice payload wrapper.
- 2026-06-05: choice-payload aggregate ternary smoke coverage now shares semantic and CLI assertion helpers for `Items(...)` and `Holder(Items(...))` shapes, preserving the explicit direct/nested matrix while reducing duplicated path, fixture, and diagnostic wiring.
- 2026-06-05: inverse choice payload nested-array smoke coverage now pins record fields containing `Array<Array<Box<T>, 1>, 1>` and `Array<Array<Slot<T>, 1>, 1>` choice payloads, confirming expected context reaches nested record-field ternaries through `Holder(Items([[Box(...)]]))` and `Holder(Items([[Slot(...)]]))` shapes.
- 2026-06-05: choice payload nested-array smoke coverage now pins record constructors inside `Array<Array<Box<T>, 1>, 1>` and `Array<Array<Slot<T>, 1>, 1>` payloads, confirming expected context reaches `Maybe<T>` and `Pointer<T>` record-field ternaries through `Items([[Box(...)]])` and `Items([[Slot(...)]])` shapes.
- 2026-06-05: nested array literal smoke coverage now pins record constructors inside `Array<Array<Box<UInt32>, 1>, 1>` and `Array<Array<Slot<UInt32>, 1>, 1>`, confirming expected context reaches `Maybe<T>` and `Pointer<T>` record-field ternaries through `[[Box(...)]]` and `[[Slot(...)]]` shapes.
- 2026-06-05: direct and nested record-constructor ternary smoke fixtures now share local source builders for `Box`/`Maybe`, `Slot`/`Pointer`, `Outer`, and `Wrapper` declaration shapes across semantic and CLI coverage.
- 2026-06-05: aggregate-context smoke fixtures now share local source builders for the recurring `Box`/`Maybe`, `Slot`/`Pointer`, and `Wrap`/`Holder` shapes, reducing duplicated declarations across semantic and CLI coverage.
- 2026-06-05: inverse deeper aggregate smoke coverage now pins record fields containing choice payload arrays of record constructors, confirming expected context reaches `Maybe<T>` and `Pointer<T>` ternaries through `Holder(Items([Box(...)]))` and `Holder(Items([Slot(...)]))` shapes.
- 2026-06-05: deeper aggregate smoke coverage now pins choice payload arrays containing record constructors, confirming expected context reaches `Maybe<T>` and `Pointer<T>` record-field ternaries through `Items([Box(...)])` and `Items([Slot(...)])` shapes.
- 2026-06-05: matching record-constructor expected-context validation is now centralized behind a shared semantic helper, keeping typed expressions, choice payloads, nested record fields, and array elements aligned before adding more aggregate coverage.
- 2026-06-04: choice payload and array literal expected contexts now validate matching record constructor payloads/elements before unresolved generic aggregate fallback, preserving `Maybe<T>` and `Pointer<T>` record-field ternary diagnostics through `Item(Box(...))`, `[Box(...)]`, `Item(Slot(...))`, and `[Slot(...)]` shapes.
- 2026-06-04: nested generic record constructor fields now validate matching inner constructors against the substituted outer field type before generic binding conflict fallback, so `Outer(Box(flag ? Some(...) : Empty))` and `Wrapper(Slot(flag ? raw_offset(...) : ...))` preserve expected context into the inner ternary arms.
- 2026-06-04: record constructor field validation now pushes declared field context into ternary initializer arms before inferred initializer fallback, while unresolved generic record constructor expressions defer context-free diagnostics to matching typed record contexts; semantic and CLI smoke coverage pins generic `Maybe<T>` payload diagnostics and generic `Pointer<T>` raw-offset pointee diagnostics without regressing concrete `Pair<T>` argument mismatch diagnostics.
- 2026-06-04: low-level final-container `raw_read`/`volatile_read` smoke fixtures now share final `if ... else`/`switch` source builders across expression, explicit-return, and nested-`unsafe` mismatch shapes.
- 2026-06-04: ordinary and choice final-container smoke fixtures now share source builders across expression and explicit-return cases, keeping semantic and CLI final `if ... else`/`switch` shapes aligned.
- 2026-06-04: final `if ... else` and `switch` containers now have explicit `return` parity coverage across ordinary typed values, generic choice constructors, and low-level `raw_read`/`volatile_read` result checks.
- 2026-06-04: final-value boundary coverage now pins empty `return` as invalid for non-`Unit` functions while allowing empty `return` and non-value-producing final statements in `Unit` functions.
- 2026-06-03: trailing `if ... else` and `switch` blocks now participate in implicit final-expression return validation, with semantic and CLI coverage for value-shaped `raw_read`/`volatile_read` arms.
- 2026-06-03: trailing `if ... else` and `switch` final-expression containers now also have semantic and CLI coverage for ordinary typed expression mismatches and generic choice-constructor payload validation, including successful `Maybe<UInt32>` constructor arms.
- 2026-06-03: nested final-expression containers now have semantic and CLI coverage for low-level reads composed across `unsafe` with final `if ... else` and `switch` in both directions.
- 2026-06-03: non-`Unit` functions now diagnose final statements that are not value-producing expression statements, value returns, or total final-expression containers, with coverage for final `if` without `else`, final `unsafe` without a value, and switch arms ending in non-value statements.
- 2026-06-03: trailing `unsafe` blocks now participate in implicit final-expression return validation, with semantic and CLI coverage for direct and rebound `raw_read`/`volatile_read` result types inside unsafe blocks.
- 2026-06-03: semantic and CLI smoke coverage for implicit final-expression `raw_read`/`volatile_read` shapes now share low-level read source builders across direct, rebound, branch, switch, guard, while, repeat, and for fixtures.
- 2026-06-03: implicit final-expression returns now have `guard ... else` failure-path `raw_read` and `volatile_read` result-type regression coverage, including compatible continuing-path success and final `Byte` versus `UInt32` mismatch diagnostics.
- 2026-06-03: implicit final-expression returns now have `for` loop-shaped `raw_read` and `volatile_read` result-type regression coverage for zero-iteration preservation.
- 2026-06-03: implicit final-expression returns now have loop-shaped `raw_read` and `volatile_read` result-type regression coverage for `while` zero-iteration preservation and `repeat` body-result preservation.
- 2026-06-03: implicit final-expression returns now have switch-merged `raw_read` and `volatile_read` result-type regression coverage.
- 2026-06-03: implicit final-expression returns now have branch-merged `raw_read` and `volatile_read` result-type regression coverage.
- 2026-06-03: implicit final-expression returns now have rebound-local `raw_read` and `volatile_read` result-type regression coverage.
- 2026-06-03: implicit final-expression returns now have explicit `raw_read` and `volatile_read` result-type regression coverage.
- 2026-06-03: implicit final-expression returns now reuse explicit return-position low-level checks for pointer/address return types.
- 2026-06-03: implicit final-expression returns now validate against declared function return types, including choice constructor payload checks and zero-payload expected choice contexts.
- 2026-06-02: unannotated choice constructor bindings now also diagnose ambiguous same-name constructor expressions that match multiple choice types by payload shape.
- 2026-06-02: unannotated choice constructor bindings now diagnose constructor expressions that require an expected choice type, including generic zero-payload constructors such as `let value = Empty`.
- 2026-06-02: zero-payload choice constructors now have ordinary typed-context coverage for expected generic choice types, including annotated bindings, returns, assignments, and direct call arguments.
- 2026-06-02: unique choice constructor expressions now infer concrete choice types when payloads resolve generic parameters, so unannotated bindings like `let value = Some(1 as UInt32)` can flow into later typed contexts.
- 2026-06-02: ordinary typed contexts now validate choice constructor expressions outside constants, so annotated bindings, returns, assignments, and call arguments reuse constructor arity and payload diagnostics.
- 2026-06-02: repeated generic binding conflict detection is now centralized across function/method calls, record constructors, and choice constructors so future generic checks can reuse one compatibility policy.
- 2026-06-02: generic choice constructor validation now records repeated payload generic binding conflicts and diagnoses the payload that introduced an incompatible nominal binding, while preserving same-width fixed integer payload compatibility.
- 2026-06-02: generic record constructor validation now records repeated field generic binding conflicts and diagnoses the field that introduced an incompatible nominal binding, while preserving same-width fixed integer field compatibility.
- 2026-06-02: generic call argument binding now records repeated generic binding conflicts and diagnoses the argument that introduced an incompatible nominal binding, while still accepting same-width fixed integer bindings.
- 2026-06-02: ordinary typed-expression compatibility now compares parsed generic type structure, allowing same-width fixed integer compatibility in nested generic arguments while preserving nominal generic container names.
- 2026-06-02: generic function and receiver-method argument checks now bind available generic parameters from earlier arguments or receiver context before validating dependent argument positions, with generic record constructor calls preserving inferred argument types.
- 2026-06-02: declared function and method calls now diagnose ordinary argument type mismatches, including nominal record constructor arguments.
- 2026-06-02: assignment statements now diagnose ordinary target/value type mismatches, including nominal record rebinding and field assignment coverage.
- 2026-06-02: annotated binding and return expressions now diagnose ordinary type mismatches, including nominal record constructor mismatches.
- 2026-06-02: record constructor arity and field value diagnostics now run for ordinary expressions, including let-binding and return-expression smoke coverage.
- 2026-06-02: record constructor calls now validate constant initializer arity and field value types, enabling field-shaped constant initializer smoke coverage.
- 2026-06-02: constant initializer smoke coverage now pins indexed fixed-array constant reads for success and element-type mismatch diagnostics.
- 2026-06-01: constant choice-constructor smoke fixtures now share Maybe-with-extra-declarations builders across semantic and CLI coverage.
- 2026-06-01: non-runtime constant array smoke fixtures now share scalar and nested array declaration builders across semantic and CLI coverage.
- 2026-06-01: constant array runtime-rejection smoke fixtures now share scalar-array and choice-array-payload builders across semantic and CLI coverage.
- 2026-06-01: constant initializer smoke coverage now pins `await`, `task`, and `thread` rejection inside array literal elements and choice-constructor array payloads.
- 2026-06-01: constant initializer smoke coverage now pins unsafe intrinsic rejection inside array literal elements and choice-constructor array payloads.
- 2026-06-01: constant initializer smoke coverage now pins ordinary function and method call rejection inside array literal elements and choice-constructor array payloads.
- 2026-06-01: constant choice-constructor validation now delegates array-typed payloads through constant array literal checks, with semantic and CLI smoke coverage for nested array payload diagnostics.
- 2026-06-01: constant array literal validation now delegates choice-typed elements through the constant choice-constructor checker, with semantic and CLI smoke coverage for nested constructor payload diagnostics.
- 2026-06-01: constant array literal smoke coverage now includes `Address` element success and low-level element diagnostics, including rejected `Pointer(...)` construction inside array initializers.
- 2026-06-01: constant array literal smoke coverage now pins direct and indirect initializer-cycle diagnostics through array elements.
- 2026-06-01: constant array literal smoke coverage now includes forward constant element references and unknown element-name diagnostics.
- 2026-06-01: semantic and CLI smoke coverage for constant array literals now reuses local scalar and nested array fixture helpers.
- 2026-06-01: nested constant array literals now recursively validate inner fixed-array lengths and element types in semantic and CLI smoke coverage.
- 2026-06-01: constant array literal diagnostics now distinguish declared length mismatches from element type mismatches in semantic and CLI smoke coverage.
- 2026-06-01: fixed-size `Array<T, N>` type syntax now parses numeric generic arguments, and constant array literals validate declared element compatibility plus length in semantic and CLI smoke coverage.
- 2026-06-01: CLI smoke coverage for constant choice constructors now reuses local source builders for repeated Status, Maybe, Boxed Maybe, and Result failure fixtures.
- 2026-06-01: semantic smoke coverage for constant choice constructors now reuses local fixture writers for repeated Status, Maybe, Boxed Maybe, and Result source shapes.
- 2026-05-31: semantic and CLI smoke coverage now pin recursive unknown-constructor diagnostics inside nested generic constant choice constructors.
- 2026-05-31: semantic and CLI smoke coverage now pin recursive wrong-choice diagnostics inside nested generic constant choice constructors.
- 2026-05-31: semantic and CLI smoke coverage now pin recursive zero-payload arity diagnostics inside nested generic constant choice constructors.
- 2026-05-31: semantic and CLI smoke coverage now pin zero-payload constant choice constructors that are called with unexpected payload values.
- 2026-05-31: CLI smoke coverage now spans wrong-type, arity, and payload mismatch diagnostics for constant choice constructors.
- 2026-05-31: CLI smoke coverage now checks the unknown choice-constructor diagnostic for choice-typed constant initializers.
- 2026-05-31: choice-typed constant initializers now diagnose unknown constructor calls with constant-type context.
- 2026-05-31: constant choice-constructor initializers now diagnose constructors that belong to a different declared choice type.
- 2026-05-27: nested generic choice constructor constants now recursively validate substituted payload constructors.
- 2026-05-27: generic choice constructor constants now have coverage for substituted payload success and mismatch diagnostics.
- 2026-05-27: constant initializers now support declared choice constructors and validate constructor arity and payload types.
- 2026-05-27: constant initializers now reject declared ordinary method calls with a constant-specific diagnostic.
- 2026-05-27: constant initializers now reject declared ordinary function calls with a constant-specific diagnostic.
- 2026-05-27: constant initializer semantic checks now share one expression traversal helper.
- 2026-05-27: constant initializers now reject runtime-only task, await, unsafe-call, intrinsic, and Pointer construction expressions.
- 2026-05-27: direct and indirect constant initializer cycles now produce semantic diagnostics before lowering.
- 2026-05-27: duplicate top-level constant names now produce a semantic diagnostic and keep references deterministic.
- 2026-05-27: constant initializers now cover forward constant references and diagnose unresolved constant names.
- 2026-05-27: pointer- and address-typed constants now validate initializer operand structure with constant-specific diagnostics.
- 2026-05-27: constant declarations now validate initializer type compatibility at the declaration site.
- 2026-05-26: top-level constants now participate in semantic expression type inference, including address operands and raw-write value checks.
- 2026-05-26: single-capture smoke helpers now share fixture analysis and capture-field assertion setup.
- 2026-05-26: await/join guidance wrappers now use named guidance and value-kind builders.
- 2026-05-26: async-placement and await-value diagnostics now use named message and requirement builders.
- 2026-05-26: current-requirement operand diagnostics now reuse named address-like and structural-expression requirement builders.
- 2026-05-26: unsafe intrinsic, unsafe call, and pointer construction boundary diagnostics now share unsafe-boundary message builders.
- 2026-05-26: receiver and `This` context diagnostics now use named subject and message builders.
- 2026-05-26: switch default and redundant-coverage diagnostics now share named message and coverage-subject builders.
- 2026-05-26: switch constructor duplicate-binding and arity diagnostics now use named message builders.
- 2026-05-26: mutable-local and receiver capture diagnostics now share named cannot-capture subject builders.
- 2026-05-26: task/thread value-boundary diagnostics now share message construction and focused assertion wrappers.
- 2026-05-26: thread/task future-marker result diagnostics and success checks now share generic result helpers.
- 2026-05-26: marker capture success smoke fixtures now share a single-capture success assertion helper.
- 2026-05-26: await/join guidance diagnostics now share current-requirement and cannot-action message construction.
- 2026-05-26: pointer/raw/volatile mismatch diagnostics now share pointer-element, value-element, and result-type message construction.
- 2026-05-26: pointer and address structural diagnostics now reuse current-requirement message construction.
- 2026-05-26: unsafe operand and index diagnostics now share current-requirement message construction.
- 2026-05-26: loop-control and receiver context diagnostics now share placement message helpers.
- 2026-05-26: switch missing-coverage and redundant-default diagnostics now share message construction helpers.
- 2026-05-26: switch value-pattern diagnostics now share value-pattern message construction.
- 2026-05-26: switch-constructor diagnostics now share constructor-pattern message construction.
- 2026-05-26: mutable-local and receiver capture diagnostics now share a cannot-capture assertion helper.
- 2026-05-26: task/thread value-return success helpers now share a concurrency-expression success helper.
- 2026-05-26: Transferable/Shareable result and thread-capture diagnostics now share future-marker assertion helpers.
- 2026-05-25: concurrency capture smoke assertions now share capture-field checking.
- 2026-05-25: await/join/return concurrency misuse diagnostics now share use-await/use-join assertion helpers.
- 2026-05-25: task and await placement diagnostics now share a concurrency-expression assertion helper.
- 2026-05-25: pointer field binding success coverage now uses shared fixture generation and success helpers.
- 2026-05-25: raw_offset source and raw_read binding mismatch diagnostics now use focused assertion helpers.
- 2026-05-25: Pointer construction source mismatch and pointer binding initializer diagnostics now reuse focused assertion helpers.
- 2026-05-25: Pointer construction arity and address-source diagnostics now use focused assertion helpers.
- 2026-05-25: index-access and unsafe-function call diagnostics now use focused assertion helpers.
- 2026-05-25: unsafe intrinsic and address-like operand diagnostics now use focused assertion helpers.
- 2026-05-25: receiver placement and receiver-parameter diagnostics now use focused assertion helpers.
- 2026-05-25: loop-control placement diagnostics now use focused assertion helpers.
- 2026-05-25: final switch missing zero-payload choice and nonfinal default diagnostics now use focused assertion helpers.
- 2026-05-25: switch multi-payload missing choice and duplicate constructor diagnostics now reuse focused assertion helpers.
- 2026-05-25: switch choice exhaustiveness and redundant default diagnostics now share focused assertion helpers.
- 2026-05-25: switch boolean default and missing value-pattern diagnostics now share focused assertion helpers.
- 2026-05-25: switch value-pattern mix, type mismatch, and duplicate diagnostics now share focused assertion helpers.
- 2026-05-25: switch constructor payload-shape, duplicate-binding, and arity diagnostics now share focused assertion helpers.
- 2026-05-25: switch low-level payload mismatch and wrong-choice variant semantics smoke fixtures now reuse focused diagnostic helpers.
- 2026-05-25: nested switch duplicate Wrap-pattern semantics smoke fixtures now share a path-based duplicate diagnostic helper.
- 2026-05-25: switch unknown-constructor semantics smoke fixtures now share a focused choice-variant diagnostic helper.
- 2026-05-25: ternary, branch, loop, and guard async/thread origin diagnostics now reuse focused await/return helpers.
- 2026-05-25: early await and async/thread return forwarding semantics smoke fixtures now share focused diagnostic helpers.
- 2026-05-25: task/thread value-return success semantics smoke fixtures now use focused assertion helpers alongside the shared value-boundary diagnostic helper.
- 2026-05-25: thread/task result safety success semantics smoke fixtures now use focused Transferable/Shareable assertion helpers.
- 2026-05-25: thread capture Transferable failure semantics smoke fixtures now share a focused diagnostic helper.
- 2026-05-25: task placement, invalid join receiver, and thread-return forwarding semantics smoke fixtures now share focused diagnostic helpers.
- 2026-05-25: address binding and address return semantics smoke fixtures now share focused structural address diagnostic helpers.
- 2026-05-25: pointer-constructor and raw_offset helper volatile_write mismatch semantics smoke fixtures now reuse the shared value/pointee diagnostic helper.
- 2026-05-25: nested, cast, and recovered volatile_write mismatch semantics smoke fixtures now reuse the shared value/pointee diagnostic helper.
- 2026-05-25: computed and indexed volatile_write mismatch semantics smoke fixtures now reuse the shared value/pointee diagnostic helper.
- 2026-05-25: volatile_read result and direct volatile_write value mismatch semantics smoke fixtures now share focused diagnostic helpers.
- 2026-05-25: pointer-constructor and raw_offset helper raw_write mismatch semantics smoke fixtures now reuse the shared value/pointee diagnostic helper.
- 2026-05-25: integer-cast and recovered raw_read raw_write mismatch semantics smoke fixtures now reuse the shared value/pointee diagnostic helper.
- 2026-05-25: indexed and nested-field raw_write mismatch semantics smoke fixtures now reuse the shared value/pointee diagnostic helper.
- 2026-05-25: computed raw_write pointer-sized mismatch semantics smoke fixtures now reuse the shared value/pointee diagnostic helper.
- 2026-05-25: raw_read return and raw_write value semantics smoke fixtures now share focused mismatch diagnostic helpers.
- 2026-05-25: address-return and address_of pointer-construction semantics smoke fixtures now share focused diagnostic helpers.
- 2026-05-24: generic raw_write pointer semantics smoke fixtures now share focused value/pointee diagnostic helpers.
- 2026-05-24: pointer-return and raw_offset pointer mismatch semantics smoke fixtures now share focused diagnostic helpers.
- 2026-05-24: pointer construction and pointer-typed binding semantics smoke fixtures now share focused diagnostic helpers.
- 2026-05-24: receiver `This` type semantics smoke fixtures now share focused context diagnostic assertion helpers.
- 2026-05-24: switch default mutation semantics smoke fixtures now share focused default diagnostic assertion helpers.
- 2026-05-24: payload choice switch exhaustiveness success semantics smoke fixtures now reuse the shared semantic success helper.
- 2026-05-24: nested switch-pattern success semantics smoke fixtures now reuse the shared semantic success helper.
- 2026-05-23: switch unknown-constructor/name-pattern semantics smoke fixtures now reuse shared fixture writers plus semantic diagnostic helpers.
- 2026-05-23: guard failure-path async-origin semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-23: while/repeat/for async/thread origin semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-23: if/switch branch async/thread origin semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-23: assignment and ternary async/thread origin semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-23: task-inside-async semantics smoke fixture now reuses the shared source writer plus semantic success helper.
- 2026-05-23: receiver-capture concurrency semantics smoke fixture now uses the focused receiver capture diagnostic helper.
- 2026-05-23: task/thread value-boundary and mutable-capture semantics smoke fixtures now share focused diagnostic helpers.
- 2026-05-23: thread/task result marker semantics smoke fixtures now share a thread-result Transferable diagnostic helper.
- 2026-05-23: thread/task capture marker semantics smoke fixtures now share a richer capture metadata assertion helper.
- 2026-05-23: concurrency capture classification semantics smoke fixture now reuses the shared source writer plus semantic analysis helper while preserving capture metadata assertions.
- 2026-05-23: task/thread consume-site semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-23: address-returning function semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-23: address-typed binding semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-23: non-integer cast and pointer-constructor volatile_write semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic helpers.
- 2026-05-22: recovered volatile_read value/cast volatile_write semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: integer literal and integer-cast volatile_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: nested scalar-field and member-container volatile_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: array and container-indexed volatile_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: computed volatile_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: direct volatile_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: volatile_read return and typed-binding semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: record and indexed field raw_write pointer semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: raw_offset helper raw_write pointer-type semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: non-integer cast and helper pointer-constructor raw_write semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-22: recovered raw_read value/cast raw_write semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-21: integer literal and direct integer-cast raw_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-21: method-returned container and member-container mismatch raw_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-21: nested scalar-field raw_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-21: helper-returned and member-container indexed raw_write value semantics smoke fixtures now reuse the shared source writer plus semantic success helpers.
- 2026-05-21: array-indexed raw_write value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-21: switch-merged raw_write computed-value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-21: rebound and branch-merged raw_write computed-value semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers.
- 2026-05-20: computed raw_write expression semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for integer-sum, bitwise, and ternary pointer-sized value coverage.
- 2026-05-20: direct raw_write value-type semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for exact mismatch, exact match, same-width integer, and pointer-sized mismatch coverage.
- 2026-05-20: raw_read return-type semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for mismatch, exact match, same-width integer, and pointer-sized mismatch coverage.
- 2026-05-20: raw_read typed-binding semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for mismatch, exact match, same-width integer, and pointer-sized mismatch coverage.
- 2026-05-20: pointer typed-binding and pointer-return semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for address_of, nonpointer initializer, and raw_offset source coverage.
- 2026-05-20: pointer-construction semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for unsafe-boundary, arity, source-shape, and address_of success coverage.
- 2026-05-20: unsafe-call boundary semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for outside-unsafe rejection and unsafe-block success coverage.
- 2026-05-20: index-access semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for non-integer and integer index coverage.
- 2026-05-20: raw/address operand-shape semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers for address_of, raw_read, raw_offset, volatile_read, and nested raw_offset coverage.
- 2026-05-20: unsafe-intrinsic semantics smoke fixtures now reuse the shared source writer plus semantic diagnostic/success helpers instead of manual source parsing and analyzer plumbing.
- 2026-05-20: await-success semantics smoke fixtures now reuse the shared concurrency fixture writer and semantic success helper instead of manual source parsing and analyzer plumbing.
- 2026-05-20: switch duplicate/default no-cascade CLI smoke assertions now avoid remaining single-use parse-result locals while preserving primary-versus-secondary diagnostic checks.
- 2026-05-20: no-cascade switch constructor-pattern CLI smoke fixtures now avoid redundant parse-result locals when asserting primary diagnostics without secondary missing-switch diagnostics.
- 2026-05-20: switch name-pattern and unknown-constructor CLI smoke fixtures now reuse the shared source writer plus parse failure helpers for unresolved constructor-pattern diagnostics.
- 2026-05-20: switch/repeat/for thread-origin and guard async-missing-origin CLI smoke fixtures now reuse the shared source writer plus parse failure helpers for await provenance diagnostics.
- 2026-05-20: subject-specific and nested-payload wrong-choice switch CLI smoke fixtures now use shared parse failure helpers for constructor/type mismatch diagnostics instead of manual argv assertions.
- 2026-05-20: nested constructor-pattern switch CLI smoke fixtures now avoid redundant parse-result locals, using direct parse helper assertions for duplicate-overlap and disjoint-success coverage.
- 2026-05-20: async/task provenance CLI smoke fixtures now reuse the shared source writer plus parse helpers for non-async task rejection, assignment/ternary/if/loop-preserved async-call provenance, ternary thread-await rejection, and guard failure-path isolation coverage.
- 2026-05-20: recovered `volatile_read` and helper-returned pointer `volatile_write` failure CLI smoke fixtures now reuse the shared source writer plus parse helpers for direct/member read recovery and direct/member/raw-offset helper pointer mismatch diagnostics.
- 2026-05-20: integer literal and integer-cast `volatile_write` CLI smoke fixtures now reuse the shared source writer plus parse helpers for literal, exact-cast, same-width-cast, wider-cast mismatch, and pointer-sized-cast mismatch coverage.
- 2026-05-20: array-indexed, member-container, nested-scalar-field, and helper-returned-container `volatile_write` CLI smoke fixtures now reuse the shared source writer plus parse helpers for shallow value recovery and pointer-sized mismatch coverage.
- 2026-05-20: direct and computed `volatile_write` CLI smoke fixtures now reuse the shared source writer plus parse helpers for exact mismatch, same-width success, pointer-sized mismatch, rebound, branch-merged, and switch-merged value-type coverage.
- 2026-05-20: `volatile_read` return and typed-binding CLI smoke fixtures now reuse the shared source writer plus parse helpers for exact, same-width, and mismatch result-type coverage.
- 2026-05-20: record pointer-field and member-field address pointer-constructor CLI smoke fixtures now reuse the shared source writer plus parse helpers for direct, indexed, rebound, and return-forwarded pointer/address recovery coverage.
- 2026-05-20: recovered raw-read and helper-returned pointer raw-write failure CLI smoke fixtures now reuse the shared source writer plus parse helpers for direct/member raw-read recovery and direct/member helper pointer mismatch diagnostics.
- 2026-05-20: integer literal and integer-cast raw-write CLI smoke fixtures now reuse the shared source writer plus parse helpers for literal, exact-cast, same-width-cast, wider-cast mismatch, and pointer-sized-cast mismatch coverage.
- 2026-05-20: array-indexed, member-container, nested-scalar-field, and helper-returned-container raw-write CLI smoke fixtures now reuse the shared source writer plus parse helpers for shallow value recovery and pointer-sized mismatch coverage.
- 2026-05-19: computed raw-write CLI smoke fixtures now reuse the shared source writer plus parse helpers for composed, rebound, branch-merged, and switch-merged value-type coverage.
- 2026-04-29: preserved explicit pointee types for `Pointer(...)` in annotated binding and pointer-return contexts, carried those declared return types plus `raw_offset(...)`-preserved pointer types through both top-level helper calls and receiver-aware method calls, recover explicit `Address`/`Pointer<T>` field types from simple member access on known record values, peel one pointee layer through straightforward indexed access on known pointer-typed values, and now also recover shallow element types from uniform array literals plus indexed `Array<T, N>`, `View<T>`, and `DynamicArray<T>` values so array-backed integer staging expressions can participate in the same low-level checks, with that same element recovery now covered through member-backed container fields, helper-returned container values, and nested scalar record-field access like `device.registers.status` too, while locking those shallow low-level types through simple local rebinding plus return forwarding and enforcing typed low-level operand/result rules by requiring inferable integer types for `raw_offset(...)` offsets and pointer-style index expressions, requiring `raw_read`/`volatile_read` results to match explicit expected binding or function-return types when those are known, requiring both `Pointer(address_of(...))` and `raw_offset(...)` in explicit `Pointer<T>` contexts to match shallowly known source storage/pointee types against `T` when those are recoverable from the expression, now using the same same-width fixed-size integer compatibility there as the read/write path already used so cases like `Int32` storage feeding `Pointer<UInt32>` no longer fail spuriously while pointer-sized and alias-style integer mismatches still do, extending that explicit pointee compatibility check to ordinary inferred pointer expressions too so typed pointer bindings/returns built from names, record fields, indexes, and helper-returned pointer values can no longer silently erase pointee mismatches, and now also specializing both generic callable/generic receiver-method returns and generic record field recovery from inferred argument, receiver, and record-instance types so helpers like `id_ptr<T>(Pointer<T>) -> Pointer<T>`, `id<T>(T) -> T`, `device.ptr(base)` on `Device<Int32>`, and direct member access like `box.value` on `Box<Int32>` preserve shallow low-level `Pointer<T>`, scalar, and `Address` information across call sites and field access instead of collapsing to unresolved generic names, with the same receiver-aware matching also feeding generic async-method awaitability and generic unsafe-method boundary checks, while the parallel `Address` path is explicitly locked in through field-, index-, and helper-returned success coverage rather than only bare `address_of(...)` examples, recovering explicit pointee information through nested `Pointer(address_of(...))` plus `raw_offset(...)` chains so downstream `raw_read`/`volatile_read` and write-site checks can fire on direct read-transform-write expressions, locking that same nested MMIO/read-transform-write behavior through helper-returned and receiver-method pointer chains rather than only top-level local rebinding, classifying built-in integer types enough to allow same-width fixed-size reinterpret-style compatibility across read expectations, direct write values, and integer-cast writes like `Int32` into `UInt32` while still requiring exact matches for pointer-sized integers and alias-style types such as `IntSize`, `UIntSize`, `Byte`, and `Char`, preserving that same shallow integer classification through small computed expressions like integer-preserving unary/binary forms, compatible integer ternaries, and computed nested field expressions, and explicitly covering local rebinding plus both `if`- and `switch`-branch merge paths so those computed integer types remain available to downstream `raw_write`/`volatile_write` checks after simple statement-level shaping.
- 2026-04-30: generic `choice` constructor-pattern payload names now specialize from the switched value type during semantic binding, so cases like `Some(value)` over `Maybe<Int32>` preserve a concrete payload type for downstream checks instead of binding `value` as untyped, and this is now covered through raw low-level write success/failure regressions that prove the same-width integer compatibility and pointee mismatch diagnostics fire correctly from switch-bound payload names.
- 2026-04-30: top-level switch constructor-pattern head and arity validation is now also subject-type-aware, so a `switch` over `Result<Int64>` rejects `Some(value)` even if `Some` exists on some other declared `choice`, and same-name variants across different `choice` types no longer collide through the old global arity map when validating patterns like `Some` versus `Some(value)`.
- 2026-04-30: nested call-shaped constructor subpatterns now inherit payload `choice` types during semantic validation too, so forms like `Wrap(Some(value))` are checked against the concrete payload type instead of only validating payload shape, and same-name nested constructors no longer collide with unrelated top-level `choice` variants when their payload arities differ; wrapped nested payload-name bindings through shapes like `Node(head, Node(next, tail))` are also now explicitly covered by low-level write regressions so the `Box<...>` path keeps carrying concrete payload types into downstream checks.
- 2026-04-30: ordinary non-constructor switch value patterns now also produce a first shallow type-consistency diagnostic against the switched expression, reusing the existing integer compatibility policy so `1 as Int32` can still match `UInt32` while obvious mismatches like `Text` against `Bool` are rejected explicitly.
- 2026-04-30: switch value-pattern semantics now also reject exact repeated literal arms for booleans, integers, and strings, which gives the first overlap-style diagnostic on ordinary value switches without claiming broader exhaustiveness or symbolic equivalence yet.
- 2026-05-07: switch value-pattern duplicate detection now recognizes small integer constant-equivalence cases, including integer literals repeated through same-width cast forms like `1` and `1 as Int32`, while still avoiding broader constant evaluation.
- 2026-05-07: boolean value switches now reject redundant `default` arms when both `true` and `false` value patterns are present, while the no-default exhaustive form remains valid.
- 2026-05-07: enum-like `choice` switches now reject redundant `default` arms when every declared zero-payload variant is covered by a bare constructor pattern, while the same exhaustive constructor set without `default` remains valid; payload-bearing choices are intentionally outside this first rule.
- 2026-05-07: enum-like `choice` switches now also reject duplicate bare constructor arms for valid zero-payload variants, matching the existing duplicate-value-pattern behavior without extending the rule to payload-bearing constructor patterns yet.
- 2026-05-07: enum-like `choice` switches without `default` now report the first missing zero-payload variant when otherwise-valid bare constructor arms do not cover every declared variant; this remains intentionally narrower than full payload-pattern exhaustiveness.
- 2026-05-07: boolean switches without `default` now report the missing `true` or `false` value arm when exactly one boolean literal arm is present, reusing the same narrow no-cascade policy as enum-like choice exhaustiveness.
- 2026-05-07: switch exhaustiveness diagnostics now have explicit no-cascade regression coverage for duplicate boolean arms without `default`, so overlap errors remain primary and do not also emit missing-arm diagnostics.
- 2026-05-07: enum-like `choice` switch exhaustiveness now has the same explicit no-cascade regression coverage for duplicate bare constructor arms without `default`, keeping the duplicate-constructor diagnostic primary over missing-variant reporting.
- 2026-05-07: payload-bearing `choice` switch overlap now has a first narrow rule: duplicate top-level constructor arms with valid name-only payload patterns such as `Some(value)` and `Some(other)` are rejected without attempting broader payload-pattern exhaustiveness.
- 2026-05-07: payload-bearing constructor overlap now has explicit no-cascade regression coverage, so a duplicate constructor arm like `Both(value, value)` reports the constructor overlap rather than also reporting duplicate payload binding names.
- 2026-05-08: payload-bearing `choice` switch overlap now also rejects exact duplicate all-literal payload constructor arms such as `Int(1)` followed by `Int(1)`, while mixed name/literal and nested payload overlap remain out of scope.
- 2026-05-08: literal payload constructor overlap now has explicit no-cascade CLI coverage too, ensuring nested duplicate literal payloads remain constructor-overlap diagnostics rather than leaking top-level value-pattern duplicate wording.
- 2026-05-08: payload literal constructor overlap now reuses the same small integer constant-equivalence rule as top-level value patterns, so `Int(1)` followed by `Int(1 as Int64)` is rejected without introducing a general constant evaluator.
- 2026-05-08: simple payload constructor overlap now treats name-binding payloads as wildcards against literal payloads, so `Int(value)` followed by `Int(1)` is rejected while nested payload-pattern overlap remains deferred.
- 2026-05-08: wildcard/literal payload overlap now has explicit CLI no-cascade coverage, keeping mixed simple-payload overlap diagnostics separate from top-level value-pattern duplicate wording.
- 2026-05-08: multi-payload simple constructor overlap is now covered explicitly for partial wildcard/literal matches such as `Both(left, 1)` followed by `Both(other, 1)`, still without attempting nested pattern reasoning.
- 2026-05-08: multi-payload simple constructor overlap now also has disjoint-literal regression coverage, so `Both(left, 1)` followed by `Both(other, 2)` remains valid because the literal-constrained positions do not overlap.
- 2026-05-08: wildcard/literal payload overlap now has order-reversed regression coverage too, so `Int(1)` followed by `Int(value)` is rejected symmetrically with `Int(value)` followed by `Int(1)`.
- 2026-05-08: mixed simple payload non-overlap now has leading-literal regression coverage too, so `Both(1, left)` followed by `Both(2, right)` remains valid when the first constrained payload differs.
- 2026-05-08: nested payload constructor overlap now has a first narrow implementation for identical nested simple constructor shapes, so `Wrap(Some(value))` followed by `Wrap(Some(other))` is rejected without attempting broader nested pattern algebra.
- 2026-05-08: nested payload constructor overlap now also has literal boundary coverage, so `Wrap(Some(1))` followed by `Wrap(Some(1))` is rejected while `Wrap(Some(1))` followed by `Wrap(Some(2))` remains valid.
- 2026-05-08: nested payload constructor overlap now recurses through rendered simple constructor keys for wildcard/literal checks too, so both `Wrap(Some(value))` followed by `Wrap(Some(1))` and the reverse order are rejected.
- 2026-05-08: nested multi-payload constructor overlap now has boundary coverage too, so `Wrap(PairSome(left, 1))` followed by `Wrap(PairSome(other, 1))` is rejected while `Wrap(PairSome(left, 1))` followed by `Wrap(PairSome(other, 2))` remains valid.
- 2026-05-08: nested zero-payload constructor names are now subject-aware instead of wildcard bindings, so `Wrap(Some(value))` and `Wrap(Empty)` remain distinct while duplicate `Wrap(Empty)` arms are rejected.
- 2026-05-08: duplicate nested zero-payload constructor overlap now has no-default no-cascade coverage, keeping duplicate `Wrap(Empty)` as the only diagnostic when future exhaustiveness checks grow around payload-bearing choices.
- 2026-05-08: nested switch-pattern smoke fixtures now share small local helpers for common `Boxed<Maybe<T>>` and `Boxed<PairMaybe<T>>` source generation, reducing repeated fixture boilerplate while preserving the same parity cases.
- 2026-05-08: nested switch-pattern duplicate diagnostic assertions now use focused local helpers in the semantics and CLI smoke suites, keeping the repeated `Wrap(...)` overlap expectation in one place per suite.
- 2026-05-08: nested switch-pattern CLI success assertions now use a local parse-success helper too, keeping the repeated parsed-output contract centralized for nearby positive smoke cases.
- 2026-05-08: deeper nested constructor overlap now has semantics and CLI coverage, so `Wrap(Hold(Some(value)))` followed by `Wrap(Hold(Some(other)))` is rejected through recursive payload-key comparison beyond one nested constructor level.
- 2026-05-08: deeper nested constructor non-overlap now has complementary coverage too, so `Wrap(Hold(Some(1)))` followed by `Wrap(Hold(Some(2)))` remains valid when the innermost literal payloads differ.
- 2026-05-08: deeper nested wildcard/literal constructor overlap now has bidirectional coverage, so `Wrap(Hold(Some(value)))` and `Wrap(Hold(Some(1)))` collide in either order through the recursive payload-key path.
- 2026-05-08: deeper nested zero-payload constructor distinction now has coverage too, so `Wrap(Hold(Some(value)))` and `Wrap(Hold(Empty))` remain distinct while duplicate `Wrap(Hold(Empty))` arms are rejected.
- 2026-05-08: payload-bearing `choice` switch exhaustiveness now has a narrow full-variant coverage rule: constructor arms with only binding-name payloads, such as `Some(value)`, cover that variant for redundant-default detection, so a `Maybe<T>` switch covering `Some(value)` plus `Empty` rejects a trailing `default` while the same arm set without `default` remains valid; literal-constrained and nested constructor arms are not treated as exhaustive.
- 2026-05-09: payload-bearing `choice` switches without `default` now report the first missing variant when the same narrow full-variant coverage model can prove one is uncovered, while duplicate payload-constructor arms keep their overlap diagnostic primary and suppress the missing-variant follow-up.
- 2026-05-09: literal-constrained payload constructor arms now have explicit exhaustiveness boundary coverage: `Some(1)` plus `Empty` still accepts a trailing `default`, while the no-default form reports `Some` as missing because the literal arm does not cover the whole payload-bearing variant.
- 2026-05-09: nested payload constructor arms now have matching exhaustiveness boundary coverage: `Wrap(Some(value))` plus `Blank` still accepts a trailing `default`, while the no-default form reports `Wrap` as missing because the nested pattern does not cover the whole outer payload-bearing variant.
- 2026-05-09: partial multi-payload constructor arms now have the same exhaustiveness boundary coverage: `Both(left, 1)` plus `Empty` still accepts a trailing `default`, while the no-default form reports `Both` as missing because one payload position is literal-constrained.
- 2026-05-09: payload-bearing choice exhaustiveness smoke fixtures now share local fixture writers across semantic and CLI coverage, keeping the full-cover, literal-boundary, nested-boundary, multi-payload-boundary, and no-cascade cases aligned without repeating source text setup.
- 2026-05-09: payload-bearing choice exhaustiveness now has order-reversed smoke coverage too, so `Empty` before `Some(value)` remains exhaustive while `Empty` before literal-constrained `Some(1)` still reports `Some` as missing.
- 2026-05-09: payload-bearing choice exhaustiveness semantic smoke assertions now share a local single-diagnostic helper, keeping the repeated line/message checks consistent as new boundary cases are added.
- 2026-05-09: payload-bearing choice exhaustiveness now has multi-payload-variant missing-order coverage, so switches over choices with `First(value)`, `Second(value)`, and `Empty` report the first declared uncovered payload-bearing variant deterministically.
- 2026-05-09: multi-payload-variant choice exhaustiveness now also has positive coverage, so full-cover arms for `First(value)`, `Second(value)`, and `Empty` are accepted without a `default`.
- 2026-05-09: multi-payload-variant choice exhaustiveness now rejects redundant trailing defaults too, so full-cover arms for `First(value)`, `Second(value)`, and `Empty` followed by `default` report the same all-choice-variants-covered diagnostic.
- 2026-05-09: multi-payload-variant choice exhaustiveness now has no-cascade duplicate coverage, so duplicate `First(...)` arms report the constructor-overlap diagnostic without also emitting missing-variant follow-up errors.
- 2026-05-09: switch choice-coverage bookkeeping in the semantic analyzer now lives behind a small helper structure, centralizing remaining-variant tracking, full-coverage checks, and declaration-order first-missing queries without changing diagnostics.
- 2026-05-09: mixed value/constructor switch patterns now have no-default no-cascade coverage, so the pattern-kind mixing diagnostic stays primary and suppresses missing-variant follow-up errors.
- 2026-05-09: invalid constructor-pattern arity now has no-default no-cascade coverage too, so missing or extra payload diagnostics remain primary and suppress choice-exhaustiveness follow-up errors.
- 2026-05-09: invalid constructor-pattern heads now have no-default no-cascade coverage, so unknown variants and variants from the wrong switched choice type suppress choice-exhaustiveness follow-up diagnostics.
- 2026-05-09: invalid constructor payload shapes now have no-default no-cascade coverage, so unsupported payload-expression diagnostics remain primary and suppress choice-exhaustiveness follow-up diagnostics.
- 2026-05-09: duplicate constructor payload bindings now have direct and nested no-default no-cascade coverage, so duplicate-binding diagnostics remain primary and suppress choice-exhaustiveness follow-up diagnostics.
- 2026-05-09: redundant-default diagnostics are now gated on prior valid switch patterns, so earlier duplicate value-pattern errors suppress redundant-default follow-up diagnostics.
- 2026-05-09: redundant-default no-cascade coverage now also spans zero-payload and payload-bearing choice switches, keeping duplicate constructor diagnostics primary when an otherwise-exhaustive choice switch has a trailing default.
- 2026-05-09: default structural diagnostics now have branch-analysis no-cascade coverage, so non-final and duplicate semantic defaults suppress unrelated diagnostics from their branch bodies.
- 2026-05-09: CLI switch no-cascade assertions now share a parse-failure helper that checks both required primary diagnostics and forbidden cascade diagnostics.
- 2026-05-09: semantic switch fixture assertions now share a fixture-level single-diagnostic helper, reducing repeated analyze-then-assert boilerplate across no-cascade and exhaustiveness coverage.
- 2026-05-09: `List<T>` constructor-pattern smoke fixtures now share local source writers across semantic and CLI tests, keeping invalid payload shape, duplicate binding, and arity diagnostics aligned.
- 2026-05-09: wrong-choice constructor smoke fixtures now share local `Maybe<T>`/`Result<T>` source writers across semantic and CLI tests, keeping top-level and nested wrong-choice diagnostics aligned.
- 2026-05-09: subject-specific constructor arity smoke fixtures now share local same-name `Some` source writers across semantic and CLI tests, keeping top-level zero-payload and nested multi-payload success coverage aligned.
- 2026-05-09: generic constructor payload raw-write smoke fixtures now share local `Maybe<T>` source writers across semantic and CLI tests, keeping payload type-binding success and pointer-element mismatch diagnostics aligned.
- 2026-05-09: nested list constructor raw-write smoke fixtures now share local `List<T>` source writers across semantic and CLI tests, keeping wrapped payload type-binding success and mismatch diagnostics aligned.
- 2026-05-09: nested list async-capture smoke fixtures now share local `List<T>` source writers across semantic and CLI tests, keeping `Node(head, Node(next, tail))` binding and `next` task-capture coverage aligned.
- 2026-05-09: top-level list async-capture smoke fixtures now share local `List<T>` source writers across semantic and CLI tests, keeping `Node(head, tail)` binding and immutable outer-local capture coverage aligned.
- 2026-05-09: unknown constructor no-cascade smoke fixtures now share local `Maybe<T>` source writers across semantic and CLI tests, keeping the primary `Missing(...)` diagnostic aligned.
- 2026-05-09: invalid constructor payload-shape smoke assertions now share the existing focused semantic and CLI failure helpers, keeping the `Node(head + 1, tail)` diagnostic checks aligned with the shared `List<T>` fixture writer.
- 2026-05-09: duplicate constructor payload-binding smoke assertions now share the existing focused semantic and CLI failure helpers, keeping direct and nested `head` rebinding diagnostics aligned with the shared `List<T>` fixture writer.
- 2026-05-09: constructor arity smoke assertions now share the existing focused semantic and CLI failure helpers, keeping missing-payload `Node(head)` and extra-payload `Empty(value)` diagnostics aligned with the shared `List<T>` fixture writer.
- 2026-05-09: constructor/value pattern-mixing smoke fixtures now share local source writers and focused failure helpers across semantic and CLI tests, keeping both ordering diagnostics and the no-default no-cascade check aligned.
- 2026-05-17: mismatched value-pattern type smoke fixtures now share local `Bool` switch source writers and focused failure helpers across semantic and CLI tests, keeping the `Text` arm diagnostic aligned.
- 2026-05-17: same-width integer value-pattern smoke fixtures now share local `UInt32` switch source writers and focused success helpers across semantic and CLI tests, keeping the `1 as Int32` acceptance case aligned.
- 2026-05-17: duplicate boolean value-pattern smoke fixtures now share local `Bool` switch source writers and focused failure helpers across semantic and CLI tests, keeping the direct duplicate and no-default no-cascade diagnostics aligned.
- 2026-05-17: duplicate string value-pattern smoke fixtures now share local `Text` switch source writers and focused failure helpers across semantic and CLI tests, keeping the `"ready"` duplicate diagnostic aligned.
- 2026-05-17: duplicate integer-cast value-pattern smoke fixtures now share local `UInt32` switch source writers and focused failure helpers across semantic and CLI tests, keeping the `1` versus `1 as Int32` duplicate diagnostic aligned.
- 2026-05-17: redundant boolean default smoke fixtures now share local arm-list `Bool` switch source writers and focused helpers across semantic and CLI tests, keeping the redundant-default, duplicate-suppression, and exhaustive-without-default cases aligned.
- 2026-05-17: missing boolean value-pattern smoke fixtures now share the arm-list `Bool` switch source writer and focused failure helpers across semantic and CLI tests, keeping the one-arm missing-`false` diagnostic aligned.
- 2026-05-17: zero-payload choice redundant-default smoke fixtures now share local arm-list `IOError` switch source writers and focused helpers across semantic and CLI tests, keeping the redundant-default, duplicate-suppression, and exhaustive-without-default cases aligned.
- 2026-05-17: missing zero-payload choice variant smoke fixtures now share the arm-list `IOError` switch source writer and focused failure helpers across semantic and CLI tests, keeping the missing-`PermissionDenied` diagnostic aligned.
- 2026-05-17: duplicate zero-payload choice constructor smoke fixtures now share the arm-list `IOError` switch source writer and focused failure helpers across semantic and CLI tests, keeping the duplicate-`Closed` and no-missing-variant diagnostics aligned.
- 2026-05-17: duplicate payload choice constructor smoke fixtures now reuse the shared `Maybe<T>` and `PairChoice` switch source writers plus focused helpers across semantic and CLI tests, keeping the duplicate-`Some(...)`, duplicate-`Both(...)`, and no-binding-cascade diagnostics aligned.
- 2026-05-17: literal payload constructor overlap smoke fixtures now share a local arm-list `Number` switch source writer and focused helpers across semantic and CLI tests, keeping exact literal, equivalent integer-cast, and wildcard/literal ordering diagnostics aligned.
- 2026-05-17: multi-payload constructor overlap smoke fixtures now reuse the shared `PairChoice` switch source writer and focused helpers across semantic and CLI tests, keeping partial-overlap failure and disjoint-literal success diagnostics aligned.
- 2026-05-17: structural default semantic smoke fixtures now reuse the arm-list `Bool` switch source writer and a shared mutated-switch analyzer helper, keeping multiple-default and nonfinal-default AST-invariant diagnostics aligned without repeated parser/analyzer setup.
- 2026-05-17: structural default CLI no-cascade coverage now reuses the arm-list `Bool` switch source writer too, keeping the nonfinal-default parse failure fixture aligned with the semantic smoke fixture shape.
- 2026-05-18: loop-control smoke fixtures now share local source writers and existing semantic/CLI assertion helpers, keeping `break`/`continue` inside/outside loop coverage aligned without repeated parser or compiler-app setup.
- 2026-05-18: receiver and `This` diagnostic smoke fixtures now share local source writers plus existing semantic/CLI assertion helpers, keeping ordinary-function, local-annotation, and receiver-parameter coverage aligned with less parser/compiler-app boilerplate.
- 2026-05-18: structural concurrency smoke fixtures now share local package-level source writers plus existing semantic/CLI assertion helpers for `task`/`thread` body-boundary, mutable-capture, and receiver-capture diagnostics.
- 2026-05-18: typed thread-capture marker smoke fixtures now reuse the shared concurrency source writer and existing semantic/CLI assertion helpers across owned, generic, `Transferable`, and `Shareable` coverage.
- 2026-05-18: task-shareable and thread/task result marker smoke fixtures now reuse the shared concurrency source writer and existing semantic/CLI assertion helpers, keeping marker result coverage aligned without repeated parser/compiler-app setup.
- 2026-05-18: async join/thread-value CLI smoke fixtures now reuse the shared concurrency source writer and parse-failure helper, keeping task-join, async-call-join, and bare-thread return diagnostics aligned without direct compiler-app setup.
- 2026-05-18: early async/provenance semantic smoke fixtures now reuse the shared concurrency source writer and single-diagnostic helper for await misuse and task/thread return-forwarding diagnostics.
- 2026-05-18: early async/provenance CLI smoke fixtures now reuse the shared concurrency source writer plus parse-failure helper for await misuse and task/thread return-forwarding diagnostics, matching the semantic fixture cleanup.
- 2026-05-19: first unsafe/MMIO CLI smoke fixture cluster now reuses the shared source writer plus parse success/failure helpers for unsafe intrinsic boundaries, operand-shape checks, index checks, and unsafe-call boundary coverage.
- 2026-05-19: pointer-construction CLI smoke fixtures now reuse the shared source writer plus parse success/failure helpers for unsafe-boundary, typed address-of, arity, source-shape, and direct address-of success coverage.
- 2026-05-19: pointer-typed binding and pointer-return CLI smoke fixtures now reuse the shared source writer plus parse success/failure helpers for structural pointer checks and shallow pointee compatibility coverage.
- 2026-05-19: generic helper-returned and receiver-method pointer CLI smoke fixtures now reuse the shared source writer plus parse helpers for raw-write same-width success and pointee mismatch diagnostics.
- 2026-05-19: pointer raw-offset and address binding/return CLI smoke fixtures now reuse the shared source writer plus parse helpers for typed pointer offsets, address-shape diagnostics, and field/index/helper address success coverage.
- 2026-05-19: generic record pointer/scalar field CLI smoke fixtures now reuse the shared source writer plus parse helpers for raw-write pointee checks and address-return field recovery coverage.
- 2026-05-19: raw-read typed binding and return CLI smoke fixtures now reuse the shared source writer plus parse helpers for exact, same-width, and mismatch result-type coverage.
- 2026-05-19: direct raw-write value-type CLI smoke fixtures now reuse the shared source writer plus parse helpers for exact mismatch, same-width integer success, and pointer-sized integer failure coverage.
- 2026-06-04: final and return-position ternary expressions now propagate declared expected types into each arm before relying on whole-ternary inference, so ordinary mismatches, generic choice-constructor payload mismatches, and `raw_read`/`volatile_read` result mismatches are diagnosed in both semantic and CLI smoke coverage.
- 2026-06-04: nested final-container ternary smoke coverage now pins the same expected-type propagation through trailing `unsafe`, final `if ... else`, and final `switch` containers for both `raw_read` and `volatile_read` success and mismatch paths.
- 2026-06-04: generic choice-constructor ternary smoke coverage now spans trailing `unsafe`, final `if ... else`, and final `switch` containers, keeping `Maybe<UInt32>` expected context available to `Some(...)` and `Empty` arms in semantic and CLI paths.
- 2026-06-04: pointer/address-producing ternary smoke coverage now pushes expected return context into `raw_offset(...)`, `Pointer(address_of(...))`, and `address_of(...)` arms across direct return/final expressions and nested final containers.
- 2026-06-04: typed binding and assignment ternary smoke coverage now preserves expected context for `Maybe<UInt32>` constructor arms and `Pointer<T>` raw-offset arms, including assignment-time pointer/address validation.
- 2026-06-04: function and method call argument ternary smoke coverage now preserves expected context for `Maybe<UInt32>` constructor arms and `Pointer<T>` raw-offset arms, including pointer-typed method arguments.
- 2026-06-11: final LLVM `if`/`switch` lowering now records structured first-failure categories for malformed shapes, conditions/subjects, arms/cases, patterns, and merge mismatches; nested expression failure details remain visible in final control-flow diagnostics, with direct malformed-`if` and end-to-end `if`/`switch` regression coverage.
- 2026-06-11: function-local LLVM lowering state now lives in a dedicated neutral `function_lowering_state.hpp`; expression, control-flow, and function emitters share `FunctionLoweringState` for bindings, SSA/CFG counters, and distinct structured failures without assigning ownership to the expression emitter.
- 2026-06-12: LLVM lowering failure text now lives in a dedicated `lowering_diagnostics` component; exhaustive direct smoke coverage pins every expression/control-flow failure category and detail composition while emitter integration diagnostics remain unchanged.
- 2026-06-12: LLVM lowering failure enums and records now live in `lowering_failures.hpp`, and `LoweringFailures` is passed separately from `FunctionLoweringState`; mutable bindings/SSA/CFG state no longer owns diagnostic lifecycle data.
- 2026-06-12: expression and control-flow lowering now pass a non-owning `FunctionLoweringSession` through public and recursive emitter APIs; it bundles references to separately owned state and failures, reducing parameter growth without merging ownership.
- 2026-06-12: immutable module lowering data and string constants now live behind neutral `LoweringEmissionContext`; expression, control-flow, and function emission no longer depend on an expression-emitter-owned context type.
- 2026-06-12: lowered scalar expression and inferred-type records now live in neutral `lowered_value.hpp`; function state, expression emission, and control-flow emission consume the shared model directly, and the control-flow public API no longer depends on the expression-emitter header.
- 2026-06-12: value-bearing statement extraction and immutable `let` lowering now live in a dedicated `statement_emitter` component; function prefixes and control-flow arm/case prefixes share it, with direct success, statement-shape, SSA-binding, and initializer-diagnostic smoke coverage.
- 2026-06-12: value-producing statement-block traversal now lives in `statement_emitter`; ordinary branch vectors and pointer-owned switch-case vectors normalize into one prefix/final-value policy, while nested `if`/`switch` CFG lowering remains an explicit callback owned by control-flow emission.
- 2026-06-12: final `if`/`switch` lowering now isolates visible immutable bindings through `ImmutableBindingScope`; sibling paths reset to one snapshot and all early failure/success exits restore it automatically, while deterministic local-name, temporary, block, and current-block state deliberately remains function-global.
- 2026-06-12: LLVM CFG text formatting now lives in the stateless `llvm_cfg` component; ternary, final `if`, and final `switch` lowering share golden-covered label, branch, switch-table, unreachable, and phi syntax while retaining semantic and predecessor-state ownership.
- 2026-06-12: final scalar switch case/default validation, literal/cast pattern lowering, and deterministic case/default/merge/fallback block naming now live in `switch_plan`; direct coverage pins explicit/no-default plans plus duplicate-default and unsupported-pattern failures before CFG text emission.
- 2026-06-12: ternary, final `if`, and final `switch` result compatibility and non-owning phi-input assembly now live in `merge_plan`; direct coverage pins common type/signedness propagation, ordered predecessors, empty/null rejection, and type/signedness mismatch details.
- 2026-06-12: ternary and final `if` deterministic then/else/merge block naming now lives in `conditional_plan`; direct coverage pins both naming families while emitters retain block-index allocation, branch lowering, predecessor tracking, and merge emission.
- 2026-06-12: ternary and final `if` branch-label execution, predecessor capture, merge planning, phi emission, and current-block transitions now live in `conditional_emitter`; non-owning callback coverage pins success, nested exits, between-arm policy, arm failures, and merge mismatch metadata while callers retain lowering and diagnostics.
- 2026-06-12: final switch dispatch, case-label execution, predecessor capture, no-default fallback, merge planning, phi emission, and current-block transitions now live in `switch_emitter`; direct callback coverage pins explicit/no-default success, nested exits, per-case policy, empty/case failures, and mismatch metadata.
- 2026-06-12: successful conditional and switch merge finalization now lives in `merge_emitter`; direct coverage pins merge labels, current-block transitions, deterministic temporary allocation, phi text, signedness propagation, and the returned lowered value.
- 2026-06-12: straight-line scalar `var` declarations, mutable-local assignments, and mutable reads now lower through explicit LLVM `alloca`, `store`, and `load` instructions; direct statement and function-emitter coverage pins typed storage, deterministic names, reassignment, and returned values.
- 2026-06-12: final value-producing `if` and `switch` arms now accept leading scalar `var` declarations and name-target assignments; branch scope snapshots both immutable and mutable visibility while stores to inherited outer slots remain runtime-visible across the selected path.
- 2026-06-12: initial LLVM `while` lowering now emits deterministic condition/body/exit blocks, reevaluates boolean conditions against current mutable scalar storage, and supports assignment-only loop bodies before continuing into the function's final value.
- 2026-06-12: LLVM `while` bodies now also lower direct call expression statements with supported scalar results, preserving deterministic SSA numbering while intentionally discarding the result; `Unit` and member-call statements remain deferred.
- 2026-06-12: direct terminal `break` and `continue` now lower inside LLVM `while` bodies through a nearest-loop target stack; statements after a loop-control terminator remain rejected until nested conditional statement lowering can model multiple body exits.
- 2026-06-12: statement-level `if` now lowers recursively inside LLVM `while` bodies, tracking fallthrough versus terminated branches so conditional `break`/`continue` emit only valid live merge edges; fully terminating `if ... else` blocks omit unreachable merge blocks.
- 2026-06-15: LLVM `while` bodies now support scoped loop-local `let` and scalar `var` declarations; binding visibility is restored after the loop and between nested `if` arms while runtime stores to outer mutable slots remain visible.
- 2026-06-15: direct `Unit` call statements now lower in LLVM `while` bodies as `call void` without inventing an unused SSA result, while scalar-result call statements continue to use the existing discarded-value path.
- 2026-06-15: LLVM call argument lowering and call-instruction formatting now live in a shared `call_emitter` component, so value-producing calls and direct `Unit` statement calls use one signature/operand/C ABI adapter emission policy.
- 2026-06-15: direct `Unit` call statement diagnostics now distinguish arity mismatches from argument-lowering failures, preserving targeted unknown-name/type details instead of falling back to a generic unsupported-call message.
- 2026-06-15: member-call statement lowering now fails with an explicit unsupported-member-call diagnostic for both `receiver.method()` and `receiver?.method()` call shapes, instead of falling through to generic result-type inference failure.
- 2026-06-15: `LoweringContext` now collects receiver-qualified method signatures from `implements` and `extend` blocks into a dedicated method model, preserving receiver type names and lowered parameter/return metadata without enabling member-call emission yet.
- 2026-06-15: lowered method lookup now resolves `(receiver type, method name)` pairs with explicit `found`, `not_found`, and `ambiguous` outcomes, avoiding source-order selection when duplicate method signatures are present.
- 2026-06-15: function lowering state now keeps source-level type-name bindings for parameters and annotated locals, and a query-only member-call receiver helper infers direct-name receiver type plus method name for `receiver.method()` and `receiver?.method()` without enabling member-call emission.
- 2026-06-15: member-call statement diagnostics now compose receiver inference with lowered method lookup, distinguishing unsupported receiver shapes, unknown receiver types, unknown methods, ambiguous methods, and non-lowerable targets.
- 2026-06-15: direct scalar member-call statements now lower through explicit receiver arguments and the shared call emitter; null-safe member-call statements remain diagnostic-only.
- 2026-06-15: ordinary function bodies now dispatch non-terminal call statements through the same call-statement helper as while bodies, so direct and member call statements can appear before the final return expression.
- 2026-06-15: `Unit`-returning function bodies now lower supported leading statements and explicit naked returns through a dedicated void path that emits `ret void` instead of short-circuiting the whole body.
- 2026-06-15: lowered method signatures now get deterministic internal symbol names such as `method.Device.read` and `method.Box_UInt32_.reset`, giving future receiver-aware emission a stable LLVM target while member-call emission remains disabled.
- 2026-06-15: scalar-subset method definitions now emit after top-level functions using stable lowered method symbols, and direct member-call expressions on scalar receivers now call those methods through explicit receiver arguments while member-call statements and null-safe member calls remain diagnostic-only.
- 2026-06-15: receiver-self method parameters now lower through the concrete receiver type for supported scalar receivers, so `extend UInt32` methods with `this: shared This` emit an `i32 %this` parameter while aggregate receiver layout remains unsupported.
- 2026-06-15: `Unit`-returning function bodies now recurse through supported statement blocks instead of short-circuiting, covering statement-level `if`/`switch`, `repeat`, provisional array-literal `for`, `unsafe`, `defer`, direct call/member-call statements, and naked returns before emitting `ret void`; unsafe-function declarations and raw MMIO lowering remain future work.
- 2026-06-15: scoped `defer` lowering now records cleanup blocks per lexical scope and replays them on fallthrough, explicit `return`, and loop-control exits in the existing function-body and loop-body lowering paths; cleanup blocks can now schedule additional defers that are replayed recursively in LIFO order before scope exit, while cleanup blocks themselves still must fall through.
- 2026-06-15: `Address` now maps to LLVM `i64` in lowering, and `Pointer(...)` now lowers through `inttoptr` from an `i64` source so the new unsafe-block smoke path can compile without implementing raw memory intrinsics yet.
- 2026-06-15: `guard ... else` now lowers as an explicit early-exit branch in both void and non-void function bodies; failure blocks can emit direct `return` statements, and non-void statement-level `if` bodies can now lower early-return branches before a later final expression or final control-flow statement.
- 2026-06-15: non-final `switch` statements in non-`Unit` function bodies now lower through the same early-exit path as `guard` and `if`, so individual cases can return early while other cases fall through to later statements after the switch.
- 2026-06-15: order-sensitive regressions now pin multiple nested defers within the same cleanup block.
- 2026-06-15: order-sensitive regressions now also pin recursive defer replay on loop-control exits.
- 2026-06-15: order-sensitive regressions now also pin recursive defer replay on `for` continue exits and `repeat` break exits.
- 2026-06-15: order-sensitive regressions now also pin recursive defer replay through nested `unsafe` blocks that contain loop-control exits.
- 2026-06-15: LLVM `while`, `repeat`, and `for` bodies now also accept nested `unsafe` blocks, reusing the existing cleanup replay path so `defer` blocks inside `unsafe` continue to run before `break` and `continue`.
- 2026-07-19: `CompilePipelineOptions` now exposes `source_drop_lowering_enabled` as the production-facing,
  disabled-by-default source Drop lowering gate. The previous `test_only_enable_source_drop_lowering` option remains an
  internal compatibility alias, while dynamic-array cleanup audit/report paths now use the production-facing gate.
- 2026-07-19: dynamic-array cleanup audit/report paths now use production-facing descriptor cleanup planning and cleanup
  emission gates, so reports can show missing element cleanup proof instead of stopping at signature rejection.
- 2026-07-19: dynamic-array cleanup audit/report paths now run metadata-only emission and mark parameter-origin
  descriptor cleanup plans as `audit`, removing the need to lower parameter descriptor signatures just to inspect
  cleanup capability.
- 2026-07-19: dynamic-array cleanup audit/report commands now use a dedicated pipeline metadata collection path rather
  than a normal LLVM emission option flag, keeping report collection separate from full IR generation.
- 2026-07-24: dynamic-array `for` lowering now consults an internal descriptor-ownership plan that distinguishes named
  bound descriptor owners, named bindings without descriptor storage, and computed owned iterables. Computed owned
  `DynamicArray<T>` iteration remains rejected until a single cleanup owner can be proven.
- 2026-07-24: the dynamic-array iterable descriptor plan now reports cleanup-owner proof independently from descriptor
  readability. Lowered local and bound parameter descriptors are proven owners; predicted semantic origins and
  audit-only descriptors remain metadata-only blockers for computed owned iteration.
- 2026-07-24: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the descriptor-ownership plan
  report, including the proven-single-owner requirement and cleanup-owner proof status.
- 2026-07-24: computed owned `DynamicArray<T>` ternary iterable planning now models branch ownership transfer without
  enabling lowering. Differing branch owners remain blocked; same-owner ternaries can be classified as proven or
  unproven based on cleanup-owner proof metadata.
- 2026-07-25: computed owned `DynamicArray<T>` `for` rejection diagnostics now include the computed ownership-transfer
  report, distinguishing ternary branch owner mismatches from missing cleanup-owner proof while leaving lowering
  disabled.
- 2026-07-25: same-owner computed owned `DynamicArray<T>` ternary iterables are now pinned as a separate rejection
  path: `flag ? items : items` reports an accepted ownership join but remains blocked by missing cleanup-owner proof.
- 2026-07-25: local same-owner computed owned `DynamicArray<T>` ternary iterables now pin the positive metadata-only
  proof path: a locally constructed descriptor reports ownership join ok and cleanup owner proven, while computed
  owned `for` lowering remains disabled.
- 2026-07-25: computed owned `DynamicArray<T>` cleanup-call insertion now has a final test-only seam. The lowered IR
  receives `__orison_dynamic_array_deallocate` only when computed lowering, cleanup-call authorization, and explicit
  insertion are enabled together; default and authorization-only paths remain audit-only.
- 2026-07-27: the test-only computed cleanup-call insertion seam now feeds the existing dynamic-array runtime-request
  and prelude path. Inserted computed cleanup calls receive the ordinary deallocation declaration and are checked
  through LLVM object emission.
- 2026-07-27: test-only computed cleanup insertion now clears the owner descriptor after deallocation, so the normal
  function-exit cleanup observes an empty descriptor. A linked pipeline smoke now runs a non-empty same-owner computed
  `DynamicArray<UInt32>` loop and exits with the iterated value.
- 2026-07-27: inserted computed cleanup now surfaces a consumed-descriptor audit report, proving that the inserted
  deallocation call is followed by owner descriptor finalization before normal function-exit cleanup runs.
- 2026-07-27: consumed computed cleanup descriptor proof now also has a lowering-model report that records owner,
  descriptor storage, and cleanup-resumption operation before the IR-derived inserted-call/finalization verifier runs.
- 2026-07-27: dynamic-array descriptor finalization now uses a named lowering helper, keeping the emitted
  zeroinitializer descriptor clear unchanged while removing the computed-loop lowerer's inline finalization string.
- 2026-07-27: consumed descriptor finalization now has a generic lowering plan for owner, descriptor storage, cleanup
  operation, and readiness flags. Computed DynamicArray cleanup metadata now carries that plan directly.
- 2026-07-27: computed DynamicArray cleanup insertion now emits the descriptor finalization clear through that generic
  plan's readiness gate, aligning model proof and IR emission without changing the generated clear instruction.
- 2026-07-27: the generic consumed descriptor finalization plan is now exposed through pipeline and CLI audit reports
  before the DynamicArray-specific consumed-cleanup wrapper report.
- 2026-07-27: cleanup insertion smoke coverage now uses the generic consumed descriptor finalization report as the
  primary proof, with DynamicArray-specific consumed-cleanup reports retained as compatibility context.
- 2026-07-27: the DynamicArray consumed-cleanup model wrapper now references the generic finalization proof instead of
  duplicating the generic owner-consumed/finalization readiness flags.
- 2026-07-27: named local `DynamicArray<T>` cleanup emission now uses the generic consumed descriptor finalization
  plan and clears the owner descriptor after deallocation, pinning that finalization ordering in lowering smoke.
- 2026-07-27: bound `DynamicArray<T>` parameter cleanup now also clears the callee-local descriptor spill through the
  generic finalization plan after descriptor deallocation; smoke coverage pins scalar and authorized owned-element
  parameter cleanup paths.
- 2026-07-27: `--dynamic-array-cleanup-audit` now merges consumed descriptor finalization plans from a successful
  full-emission pass, so local and bound cleanup finalization proof reaches the CLI audit surface.
- 2026-07-27: report-mode dynamic-array cleanup planning now classifies source-confirmed local
  `DynamicArray<T> = DynamicArray()` descriptors as local storage, removing the false storage-missing blocker from
  CLI cleanup audit output.
- 2026-07-27: successful `--dynamic-array-cleanup-audit` full-emission probes now replace metadata cleanup
  obligation/sequence/gate/capability sections with emitted cleanup reports, aligning local cleanup operation names
  with consumed descriptor finalization lines.
- 2026-07-27: successful cleanup audit emitted cleanup sections now include function symbol context, so repeated
  function-local cleanup symbols such as `__orison_dynamic_array_cleanup.0` are distinguishable in multi-function CLI
  output without changing the emitted IR naming model.
- 2026-07-27: dynamic-array cleanup emission capability reports now list the cleanup operations summarized by the
  capability proof, keeping aggregate capability lines auditable when a function owns multiple descriptor cleanups.
- 2026-07-27: dynamic-array cleanup emission capability reports now also list the descriptor owners summarized by the
  proof, pairing the aggregate capability result with the owners whose cleanup operations it authorizes or blocks.
- 2026-07-27: dynamic-array cleanup emission capability reports now include compact owner/operation pairs, so audit
  consumers do not need to correlate separate owner and operation lists by position.
- 2026-07-27: dynamic-array cleanup emission capability reports now include owned-element drop pairs when element
  cleanup is required, identifying the cleanup owner, element capture, and authorized drop ABI symbol.
- 2026-07-27: CLI cleanup-audit smoke coverage now pins owned-element drop-pair reporting for the authorized
  `DynamicArray<Payload>` fixture, proving the report reaches `--dynamic-array-cleanup-audit`.
- 2026-07-27: CLI cleanup-audit smoke coverage now also pins the blocked owned-element path: missing cleanup proof
  reports `[element cleanup missing]` and does not emit `element-drop-pairs`.
- 2026-07-27: blocked dynamic-array cleanup capability reports now include `missing-element-drop-pairs`, identifying
  the cleanup owner, element capture, and drop ABI symbol that still needs semantic/source drop proof.
- 2026-07-27: dynamic-array cleanup production-readiness reports now propagate `missing-element-drop-pairs` from the
  cleanup capability report, so readiness blockers identify the exact owned-element cleanup proof still missing.
- 2026-07-27: dynamic-array cleanup production-readiness planning now consumes structured missing element-drop pairs
  from the pipeline result instead of parsing formatted cleanup capability report text.
- 2026-07-27: dynamic-array cleanup production-readiness planning now consumes structured sequence-verification and
  cleanup-capability booleans from the pipeline result instead of scanning formatted report strings.
- 2026-07-27: computed dynamic-array inserted cleanup handoffs are now analyzed once into structured verified pairs
  before transition, state-verification, cleanup-call gate, plan, render, insertion, and consumed-descriptor reports
  are formatted.
- 2026-07-27: executable computed dynamic-array `for` lowering now records structured cleanup-call operand metadata
  for the planned cleanup resumption, and pipeline cleanup-call reports prefer that metadata over IR instruction
  scraping when proving data pointer, element size, and capacity operands.
- 2026-07-27: compile-pipeline smoke coverage now suppresses structured computed cleanup-call operand metadata through
  an internal test seam and proves cleanup-call plan/render/insertion-gate reports still degrade through the IR-derived
  fallback path.
- 2026-07-27: structured computed cleanup-call metadata now records whether the deallocation call and descriptor
  finalization were emitted, so inserted cleanup-call and consumed-descriptor reports prefer structured proof before
  falling back to IR text matching.
- 2026-07-27: executable computed cleanup handoffs are now retained as structured metadata, so inserted cleanup
  transition/state verification and downstream cleanup-call reports prefer recorded handoff operations before falling
  back to inserted IR comments.
- 2026-07-27: computed cleanup-call report formatting now shares one analyzed cleanup-call operand set per verified
  inserted cleanup pair, with structured-vs-IR-fallback provenance counts pinned in pipeline smoke coverage.
- 2026-07-27: inserted cleanup-call and consumed-descriptor reports now expose structured-vs-IR-fallback proof counts,
  and pipeline smoke coverage pins both the normal metadata proof path and the deliberate suppressed-metadata fallback.
- 2026-07-27: inserted cleanup handoff verification now exposes structured-vs-IR-comment provenance counts, with an
  explicit test-only handoff-metadata suppression seam covering the fallback path.
- 2026-07-27: computed cleanup report population now consumes a typed internal cleanup-proof model that bundles
  inserted handoff verification with verified cleanup-call operands before formatting audit reports.
- 2026-08-01: pipeline results now expose DynamicArray construction-plan state as typed lowering records, so smoke
  coverage renders exact construction-plan audit text from state instead of reading a pipeline report vector.
- 2026-08-01: pipeline results now expose DynamicArray runtime-request state as typed runtime operation records, so
  smoke coverage renders exact runtime request audit text from state instead of reading a pipeline report vector.
- 2026-08-01: pipeline results now expose DynamicArray allocation-call emission state with rendered snippet counts and
  snippets, so smoke coverage no longer reads allocation-call IR from a direct pipeline vector.
- 2026-08-01: pipeline results now expose planned drop declarations as typed state, and driver planned/emitted drop
  reports render from that state instead of reading pre-rendered pipeline vectors.
- 2026-08-01: pipeline results now expose planned drop actions as typed state, and driver planned-drop action reports
  render from that state instead of reading a pre-rendered pipeline vector.
- 2026-08-01: pipeline results now expose drop cleanup authorization as typed state, and driver cleanup authorization
  reports render from that state instead of reading a pre-rendered pipeline vector.
- 2026-07-27: the computed cleanup-proof model is now a reusable private pipeline component instead of anonymous
  report-formatting state, so later lowering pipeline stages can consume the same typed proof bundle.
- 2026-08-12: generic `DynamicArray<Box<T>>` owned-element scalar projection now has checked-in CLI coverage.
  `first_value<T>(values: DynamicArray<Box<T>>) -> T` specializes to `first_value__UInt32`, lowers
  `values[0].value` through the owned-element projection path, and cleans up `Box<UInt32>` elements through the
  canonical `__orison_drop.Box_UInt32_` symbol.
- 2026-08-12: the matching missing-Drop boundary is pinned for generic `DynamicArray<Box<T>>` projection. Appending
  `Box<UInt32>` without concrete source Drop proof fails with the owned-element authorized-drop diagnostic before any
  unchecked cleanup path can lower.
- 2026-08-12: nested generic owned-element scalar projection now has checked-in CLI coverage.
  `first_inner_value<T>(values: DynamicArray<Outer<T>>) -> T` specializes to `first_inner_value__UInt32`, lowers
  `values[0].inner.value`, and emits concrete `%record.Outer_UInt32_` fields using `%record.Box_UInt32_` rather than
  an open generic record layout.
- 2026-08-12: the matching nested missing-Drop boundary is pinned for `DynamicArray<Outer<T>>`. Appending
  `Outer<UInt32>` without concrete source Drop proof fails with the owned-element authorized-drop diagnostic before
  parameter cleanup or nested projection lowering can proceed unchecked.
- 2026-08-12: nested generic fixed-array projection now has checked-in CLI coverage.
  `second_inner_item<T>(values: DynamicArray<Outer<T>>) -> T` lowers `values[0].inner.items[1]` through concrete
  generic record layouts and fixed-array indexing, then cleans up `Outer<UInt32>` elements with source Drop proof.
- 2026-08-12: the matching nested fixed-array missing-Drop boundary is pinned. Appending `Outer<UInt32>` without
  concrete source Drop proof fails before `values[0].inner.items[1]` can lower through an unchecked cleanup path.
- 2026-08-12: nested generic fixed-array projection now also covers direct call-result arguments.
  `second_inner_item(make_values())` specializes `DynamicArray<Outer<T>>` from the helper's concrete
  `DynamicArray<Outer<UInt32>>` return descriptor and preserves the same fixed-array projection plus Drop proof path.
- 2026-08-12: the direct call-result nested fixed-array missing-Drop boundary is pinned. A helper returning
  `DynamicArray<Outer<UInt32>>` without concrete source Drop proof fails during owned-element append before the
  returned descriptor can feed `second_inner_item(make_values())`.
- 2026-08-12: nested generic fixed-array projection now covers inferred locals initialized from call results.
  `let values = make_values(); second_inner_item(values)` records the helper return descriptor's concrete source type
  before specializing `DynamicArray<Outer<T>>` and lowering the fixed-array projection.
- 2026-08-12: the inferred-local call-result missing-Drop boundary is pinned. `let values = make_values()` cannot feed
  `second_inner_item(values)` when the helper appends `Outer<UInt32>` without concrete source Drop proof.
- 2026-08-12: nested generic fixed-array projection now covers same-source-type ternary helper results.
  `second_inner_item(true ? make_left() : make_right())` specializes from matching
  `DynamicArray<Outer<UInt32>>` return descriptors and lowers the fixed-array projection with source Drop proof.
- 2026-08-12: the mismatched ternary helper-result boundary is pinned. `DynamicArray<Outer<UInt32>>` versus
  `DynamicArray<Outer<UInt64>>` arms fail source-type matching before `second_inner_item<T>` specialization.
- 2026-08-12: same-source-type ternary helper results now also work through inferred locals. `let values =
  flag ? make_left() : make_right(); second_inner_item(values)` preserves the concrete descriptor source type for
  nested fixed-array projection specialization.
- 2026-08-12: the inferred-local mismatched ternary helper-result boundary is pinned. `DynamicArray<Outer<UInt32>>`
  versus `DynamicArray<Outer<UInt64>>` arms now report a local initializer source-type mismatch before projection use.
- 2026-08-12: README gap analysis now marks the nested generic fixed-array projection source-type matrix as covered.
  Remaining DynamicArray work should move back to computed-index and partial-ownership cleanup gaps.
- 2026-08-12: DynamicArray runtime-index constructor-move coverage now pins the gated move path, sibling read
  preservation, and same computed-index reuse rejection for `items[index]` without enabling ordinary compilation.
- 2026-08-12: Runtime-index cleanup function integration now retargets DynamicArray constructor moves to the
  post-move block, so emitted cleanup runs after the moved value is loaded and stored. Driver smoke coverage pins
  emitted IR order plus object/link/run for the DynamicArray cleanup fixture.
- 2026-08-13: Runtime-index member cleanup now reports a verified function-rewrite candidate for rendered member
  cleanup CFG slices. The candidate remains report-only, with production module mutation still blocked.
- 2026-08-13: Runtime-index member cleanup now derives a report-only function rewrite edit-script plan from verified
  member cleanup candidates, covering branch replacement, CFG append, and PHI predecessor retarget readiness.
- 2026-08-13: Runtime-index member cleanup now validates the report-only function rewrite edit-script plan and reports
  finite blockers before any production module mutation path is considered.
- 2026-08-13: Runtime-index member cleanup module-mutation gates now explicitly require edit-script validation
  readiness, while still reporting production mutation as disabled.
- 2026-08-13: Runtime-index member cleanup validation diagnostics now distinguish structural edit-script blockers from
  structurally validated plans that remain production-disabled.
- 2026-08-13: Runtime-index member cleanup now derives a report-only staged-apply plan from edit-script validation,
  separating planned branch replacement, CFG append, and PHI retarget from disabled IR mutation.
- 2026-08-13: Runtime-index member cleanup module-mutation gates now require staged-apply readiness explicitly,
  keeping staged planning separate from disabled IR mutation.
- 2026-08-13: Runtime-index member cleanup staged-apply blockers now emit report-only diagnostics, separating
  validation-blocked staged planning from ready staged plans held at the disabled production mutation gate.
- 2026-08-13: Runtime-index member cleanup module-mutation blockers now emit report-only diagnostics for missing CFG
  slices, edit-script validation, staged apply readiness, disabled mutation, and disabled production cleanup.
- 2026-08-13: Runtime-index member cleanup now emits a final report-only promotion checklist summarizing rewrite
  candidate, edit script, validation, staged apply, module mutation, and production readiness status.
- 2026-08-13: Runtime-index member cleanup now emits a disabled report-only promotion seam that selects the real IR
  mutation path only after structural checklist readiness, while production IR mutation remains blocked.
- 2026-08-13: Runtime-index member cleanup promotion seams now feed a report-only mutation operation plan with explicit
  branch replacement, CFG append, and PHI retarget operation records that remain unapplied.
- 2026-08-13: Runtime-index member cleanup mutation operation plans now have report-only validation for operation
  count, order, required fields, readiness, and unapplied status.
- 2026-08-13: Runtime-index member cleanup mutation validation now feeds report-only conflict detection for unique
  branch anchor, closing anchor, and PHI predecessor matches before any apply step is allowed.
- 2026-08-13: Runtime-index member cleanup conflict detection now feeds a report-only apply-authorization gate that
  remains blocked while production IR mutation is disabled.
- 2026-08-13: Runtime-index member cleanup apply authorization now feeds a report-only ordered apply preview for the
  future branch replacement, CFG append, and PHI retarget actions without mutating IR.
- 2026-08-13: Runtime-index member cleanup apply previews now feed report-only post-apply verification targets for
  branch target, CFG append, and PHI predecessor checks.
- 2026-08-13: Runtime-index member cleanup mutation post-apply verification now feeds a final report-only promotion
  summary across operation planning, validation, conflict detection, apply authorization, apply preview, and post-apply
  verification.
- 2026-08-13: Runtime-index member cleanup mutation promotion summaries now feed a final report-only production
  readiness gate that separates prerequisite readiness from disabled IR mutation and disabled production gates.
- 2026-08-13: Runtime-index member cleanup mutation production-readiness gates now emit stable per-blocker diagnostic
  lines for prerequisite, authorization, post-apply verification, disabled IR mutation, and disabled production gates.
- 2026-08-13: Runtime-index member cleanup mutation production-readiness diagnostics now end with a compact
  report-only verdict line summarizing readiness, guarded rewrite status, blocker count, and diagnostic count.
- 2026-08-13: Runtime-index member cleanup mutation readiness verdicts now feed a report-only guarded rewrite
  authorization record that remains blocked until readiness and guarded rewrite prerequisites are met.
- 2026-08-13: Runtime-index member cleanup mutation guarded rewrite authorization now emits stable per-blocker
  diagnostics for blocked readiness verdicts and blocked guarded rewrite authorization.
- 2026-08-13: Runtime-index member cleanup mutation guarded rewrite authorization now feeds a report-only rewrite
  execution plan that keeps actual IR mutation disabled until rewrite authorization is ready.
- 2026-08-13: Runtime-index member cleanup mutation rewrite execution plans now emit stable per-blocker diagnostics
  for blocked rewrite authorization and unauthenticated rewrite execution.
- 2026-08-13: Runtime-index member cleanup mutation rewrite execution diagnostics now end with a compact report-only
  verdict summarizing execution readiness, blocker count, diagnostic count, and disabled production execution.
- 2026-08-13: Runtime-index member cleanup mutation rewrite execution verdicts now feed a final report-only promotion
  status line that aggregates authorization, execution-plan, execution-verdict, and production-disabled readiness.
- 2026-08-13: Runtime-index member cleanup mutation rewrite authorization and execution planning now have explicit
  internal opt-in gates that can reach promotion-ready status while default audit paths and production mutation remain
  disabled.
- 2026-08-13: Runtime-index member cleanup rewrite execution requests now thread from a test-only pipeline option into
  lowering audit state. Reports expose `rewrite-requested` and `execution-requested` while blocked prerequisites still
  prevent authorization, execution, and IR mutation.
- 2026-08-13: Pipeline smoke coverage now pins the source-backed computed DynamicArray member-transfer audit path for
  `items[index + zero].item`, including member Drop metadata readiness, production-readiness blockers, mutation
  operation validation readiness, and guarded rewrite authorization remaining blocked by default.
- 2026-08-13: Runtime-index member cleanup mutation apply authorization now accepts an internal test-only IR mutation
  request. The request is observable in audit output, while the production gate remains disabled and apply
  authorization stays blocked.
- 2026-08-13: Runtime-index member cleanup mutation apply authorization now also accepts an internal production-gate
  request. With both internal request bits set, authorization can report ready while `apply-authorized` remains false
  and no mutation actions are applied.
- 2026-08-14: Runtime-index member cleanup mutation apply authorization now has a separate internal apply request.
  When validation, conflict detection, IR mutation, production gate, and apply request all align, the audit reports
  applied actions, ready post-apply verification, and production-ready mutation status.
- 2026-08-14: Runtime-index member cleanup rewrite execution now reaches gated function-IR mutation. The pipeline
  records verifier-backed mutation state, appends the member cleanup CFG, and falls back to the function final return
  anchor for fixtures that do not yet contain an abstract cleanup branch.
- 2026-08-14: Runtime-index member cleanup rewrite plans now carry the synthetic member cleanup target through to the
  pipeline mutation helper. The fully gated emitted CFG routes through the sibling-cleanup block and records the target
  symbol, while real Drop/deallocation call lowering remains the next backend step.
- 2026-08-14: Runtime-index member cleanup rewrite mutation now emits a verifier-clean synthetic cleanup helper
  declaration and call. The call uses the selected runtime-indexed element address; helper body expansion into concrete
  sibling-member Drop/deallocation calls remains the next backend step.
- 2026-08-14: Runtime-index member cleanup helper generation now emits a verifier-clean helper body for the current
  no-sibling `Box.except.item` fixture instead of leaving a declaration-only helper. A sibling-owned-field fixture is
  still needed to drive concrete Drop/deallocation expansion.
- 2026-08-15: Runtime-index member cleanup promotion now has a typed internal gate between the promotion checklist and
  mutation seam. The gate records checklist readiness, explicit test-only IR mutation and production-gate requests,
  derived enablement, and remaining blockers before apply authorization consumes those same derived fields.
- 2026-08-15: Runtime-index member cleanup typed promotion gates now flow through function emission, LLVM emission,
  pipeline results, and the driver production-readiness headline. The headline reports `member-gate-records` and
  evaluates promotion readiness from those gate records plus the existing downstream typed readiness records.
- 2026-08-15: The executable `--test-only-runtime-indexed-member-cleanup-run` seam now emits the same typed promotion
  gate report used by readiness reporting after successful host execution, keeping run-path observability aligned with
  the production-readiness path.
