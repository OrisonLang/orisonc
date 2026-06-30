# ADR-0005: Owned Drop Semantics and ABI Gating

## Status
Proposed

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
- Runtime concurrency cleanup callbacks remain untyped. The compiler owns typed environment layout, field addressing,
  drop ordering, and drop-call emission.
- The existing test-only drop declaration allowlist remains an internal backend seam only. It must not be exposed as CLI
  behavior or source-language surface.

## Consequences

- Planned drop metadata, per-cleanup-site actions, declaration reports, and authorization reports are valid scaffolding
  for future diagnostics and lowering.
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
