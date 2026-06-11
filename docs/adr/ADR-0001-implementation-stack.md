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
- Development builds may use the platform's monolithic shared LLVM target when component archives are unavailable;
  release packaging must use a static LLVM distribution to preserve statically linked tool executables.
- Future ADRs should define the lowering pipeline, incremental compilation architecture, and runtime boundary.

## Follow-up work

- Add the first shared foundation/source/diagnostics libraries.
- Define the initial frontend pipeline from source text to parsed syntax trees.
- Extend the linked LLVM boundary from verification into target-machine and object emission support.
