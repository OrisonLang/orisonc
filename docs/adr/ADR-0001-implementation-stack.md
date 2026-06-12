# ADR-0001: C++23 Frontend with LLVM Backend

## Status
Accepted

## Context

Orison is intended to ship as a serious native toolchain with a compiler driver, formatter,
and language server built from shared internal libraries. The implementation needs predictable
native performance, direct control over memory and data layout, and practical integration with
existing backend and object-generation infrastructure.

## Decision

The Orison implementation stack will use:

- `C++23` for the compiler, shared libraries, and supporting tools
- `LLVM` as the core backend and code generation stack

The repository scaffold should reflect a modular internal architecture so the compiler driver,
formatter, and language server can share source management, diagnostics, syntax, semantic
analysis, and lowering components.

## Consequences

- The build system should standardize on a `CMake`-based native workflow.
- Repository layout should separate reusable compiler libraries from tool entry points and tests.
- The lowering library links LLVM's core, assembly parser, and support components so emitted textual IR is parsed
  and verified in-process before it is exposed to callers.
- Host object emission uses LLVM target discovery, a host `TargetMachine`, and the target's object-file pass pipeline.
- Initial executable linking delegates to a CMake-discovered host Clang/C driver through an argument-vector process
  launch; no shell command construction is used.
- `orisonc run <file>` uses the same host object and link path, executes a uniquely named temporary executable with
  inherited standard streams, propagates its exit status, and removes the temporary artifact.
- Shared frontend, LLVM IR, and object compilation stages live behind `orison_pipeline`; CLI modes select an
  artifact or continue into linking without duplicating stage orchestration.
- Per-function LLVM signature, SSA expression, control-flow, and return emission live behind a dedicated lowering
  component; module emission owns only module assembly, shared context construction, and final LLVM verification.
- A dedicated neutral function-lowering state model owns lowered bindings, deterministic SSA names, block identities,
  current-block tracking, and structured emitter failures so expression and statement CFG lowering use one state
  without either emitter owning the shared model.
- Recursive expression lowering and scalar expression-type/literal utilities compile in their own translation unit;
  function statement/control-flow emission consumes that API instead of owning expression implementation details.
- Final value-producing `if`/`switch` CFG emission, nested value blocks, and branch-local immutable bindings compile
  in a dedicated control-flow component; function emission owns signatures, parameters, and final return assembly.
- LLVM local-value, temporary, and indexed block naming policy lives in a shared stateless utility; emitters retain
  explicit counters/maps and request deterministic names without duplicating formatting or increment behavior.
- Ordinary function definitions retain their shared lowered signatures in the module context even when a type is
  unsupported; function emission consumes that model directly for declarations, bindings, signedness, and diagnostics.
- Expression lowering records a structured first failure reason and detail in explicit function-lowering state; callers retain
  optional value flow while producing targeted diagnostics for names, types, operators, calls, casts, and branches.
- Final control-flow lowering records a separate structured first failure in the same explicit state; `if` and `switch`
  diagnostics identify shape, condition/subject, arm/case, pattern, and merge failures while retaining nested expression
  failure details.
- Development builds may use the platform's monolithic shared LLVM target when component archives are unavailable;
  release packaging must use a static LLVM distribution to preserve statically linked tool executables.
- Future ADRs should define the lowering pipeline, incremental compilation architecture, and runtime boundary.

## Follow-up work

- Add the first shared foundation/source/diagnostics libraries.
- Define the initial frontend pipeline from source text to parsed syntax trees.
- Replace the provisional external host-link driver with a deliberate cross-platform linker strategy.
