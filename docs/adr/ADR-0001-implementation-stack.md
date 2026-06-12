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
  and current-block tracking so expression and statement CFG lowering share mutable emission state without either
  emitter owning the model.
- Recursive expression lowering and scalar expression-type/literal utilities compile in their own translation unit;
  function statement/control-flow emission consumes that API instead of owning expression implementation details.
- Value-bearing statement extraction and immutable `let` emission compile in a dedicated statement component;
  function and control-flow emission share it without assigning ordinary statement policy to CFG lowering.
- Value-producing statement-block traversal also lives in the statement component and normalizes contiguous syntax
  statements plus pointer-owned switch-case statements through one policy; CFG recursion is supplied as a callback.
- Branch-local immutable bindings are isolated by a dedicated RAII scope that snapshots only the visible binding map,
  resets it between sibling paths, and restores it on every success or failure exit while SSA/CFG counters remain global.
- Final value-producing `if`/`switch` CFG emission, nested value blocks, and branch-local immutable bindings compile
  in a dedicated control-flow component; function emission owns signatures, parameters, and final return assembly.
- LLVM local-value, temporary, and indexed block naming policy lives in a shared stateless utility; emitters retain
  explicit counters/maps and request deterministic names without duplicating formatting or increment behavior.
- Ordinary function definitions retain their shared lowered signatures in the module context even when a type is
  unsupported; function emission consumes that model directly for declarations, bindings, signedness, and diagnostics.
- Expression lowering records a structured first failure reason and detail in an explicit failure model; callers retain
  optional value flow while producing targeted diagnostics for names, types, operators, calls, casts, and branches.
- Final control-flow lowering records a separate structured first failure in the same explicit failure model; `if` and `switch`
  diagnostics identify shape, condition/subject, arm/case, pattern, and merge failures while retaining nested expression
  failure details.
- Lowering failure rendering lives in a dedicated diagnostics component over the neutral failure records; expression,
  control-flow, and function emitters consume one stable text policy without owning diagnostic wording.
- Mutable function emission state and lowering failures are passed as separate objects; `FunctionLoweringState` cannot
  accumulate diagnostic policy or failure lifecycle concerns as new statement and backend lowering is added.
- Expression and control-flow emitters receive a non-owning `FunctionLoweringSession` that references the separately
  owned state and failure objects, reducing recursive parameter lists without recombining their responsibilities.
- Immutable module lowering data and string constants are exposed through a neutral `LoweringEmissionContext`;
  expression, control-flow, and function emission share it without assigning context ownership to an emitter.
- Lowered scalar expression and inferred-type metadata live in a neutral `lowered_value.hpp`; function state and
  emitter APIs share these records without assigning representation ownership to state or expression emission.
- Development builds may use the platform's monolithic shared LLVM target when component archives are unavailable;
  release packaging must use a static LLVM distribution to preserve statically linked tool executables.
- Future ADRs should define the lowering pipeline, incremental compilation architecture, and runtime boundary.

## Follow-up work

- Add the first shared foundation/source/diagnostics libraries.
- Define the initial frontend pipeline from source text to parsed syntax trees.
- Replace the provisional external host-link driver with a deliberate cross-platform linker strategy.
