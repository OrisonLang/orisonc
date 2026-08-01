# ADR-0005: Owned Drop Semantics and ABI Gating

## Status
Accepted

## Context

Concurrency cleanup lowering now records planned drop actions for owned aggregate captures, can inspect missing drop ABI
declarations, and has a test-only path that proves cleanup thunk calls are only emitted when every planned action has a
matching emitted declaration. That scaffolding is intentionally not language semantics yet.

Orison still needs a real ownership/drop model before generated cleanup thunks may call `__orison_drop.<Type>` in normal
compilation. Enabling calls from metadata alone would hide destruction cost, make ownership incomplete, and risk
inventing semantics outside the spec/tour.

## Decision

- Drop emission remains disabled in normal lowering until source-level ownership/drop semantics are explicitly accepted.
- A drop ABI declaration is necessary but not sufficient: cleanup calls require both an accepted source-level drop model
  and a proven emitted declaration for every action in the cleanup thunk.
- The compiler will treat drop cleanup authorization as all-or-nothing per cleanup thunk. If any captured field lacks an
  emitted declaration, no drop calls are emitted for that thunk.
- Drop planning remains finite and explicit. The compiler will not synthesize variadic, spread-like, or open-ended drop
  dispatch.
- Ordinary value reads of owned aggregate projections remain rejected until explicit borrow/clone/move semantics exist
  for that boundary. Passing the projection to a matching owned parameter is still the explicit transfer path.
- Lowering represents aggregate projection access intent internally as value read, explicit transfer, shared borrow,
  exclusive borrow, or clone value. Only explicit transfer is enabled for owned projections today; borrow and clone
  intents remain future-gated metadata.
- Call-argument ownership transfer and ordinary value-read rejection must use the same aggregate projection access plan
  so future borrow and clone gates have one lowering decision point.
- Aggregate projection diagnostics render from the access plan instead of emitter-local strings, keeping current
  value-read rejection text stable while giving future borrow and clone gates shared wording.
- Aggregate projection access plans have a deterministic internal report line containing intent, status, binding,
  source type, receiver status, and diagnostic text when present. This is debugging/audit metadata only.
- A lowering metadata option can collect aggregate projection access-plan records into function and module emission
  results. Normal compiler output remains unchanged.
- The driver exposes those aggregate projection access-plan records as report output only through the explicit
  `--test-only-aggregate-projection-access-plans` report command.
- That test-only report command preserves access-plan output on lowering failure, so rejection-boundary tests can assert
  the blocked plan and diagnostic without changing normal compiler output.
- Driver smoke coverage now pins receiver projection access reports, including `this.payload` as an allowed owned
  receiver projection.
- Pipeline results now expose typed aggregate projection access-plan state, including function symbols, intents,
  statuses, bindings, source types, diagnostics, receiver flags, and summary counts. Tests and future tooling should
  consume that state instead of parsing report strings.
- The driver aggregate projection access-plan report now renders from the typed pipeline state while preserving the
  previous report text.
- Pipeline results no longer expose aggregate projection access-plan report strings; aggregate projection access
  consumers use typed state at the pipeline boundary.
- Lowering metadata no longer exposes aggregate projection access-plan report strings; lower-level tests and pipeline
  state construction consume typed access-plan records.
- The internal aggregate projection access collection option is named for metadata collection, while the CLI command
  remains the explicit test-only report surface.
- Runtime concurrency cleanup callbacks remain untyped. The compiler owns typed environment layout, field addressing,
  drop ordering, and drop-call emission.
- The existing test-only drop declaration allowlist remains an internal backend seam only. It must not be exposed as CLI
  behavior or source-language surface.

## Consequences

- Planned drop metadata, per-cleanup-site actions, declaration reports, and authorization reports are valid scaffolding
  for future diagnostics and lowering.
- Direct formatter smoke tests own exact planned-drop metadata and semantic-drop report rendering. Pipeline and driver
  smoke tests should verify report wiring, counts, and representative symbols rather than duplicating formatter
  strings.
- Normal `orisonc --emit-llvm`, object emission, build, and run output must remain metadata-only for planned drops until
  this ADR is superseded by accepted source semantics.
- Future source-level drop work must define how a type becomes droppable, where drop bodies live, which values own
  resources, and how move/consume analysis prevents double drops.
- Cleanup thunk call emission can become production behavior only after semantic analysis proves ownership and the module
  prelude emits every required finite drop declaration.

## Follow-up work

- Decide the source-level syntax and placement for user-defined drop behavior.
- Add semantic ownership/drop analysis that identifies owned values, moved values, and deterministic drop sites.
- Add diagnostics for missing or invalid drop implementations before code generation.
- Replace the test-only declaration allowlist with declarations derived from accepted source-level drop implementations.
