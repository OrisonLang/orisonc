# ADR-0010: Interface Dispatch Metadata

## Status

Accepted

## Context

Semantic analysis validates `interface` and concrete `implements` declarations. Lowering already emits concrete
implementation methods, but it had no structured relationship between an interface method slot and its concrete
method symbol. Existential value storage and indirect calls require that relationship before an LLVM ABI is selected.

## Decision

`LoweringContext` records one dispatch table description for each supported non-generic interface implemented by a
concrete receiver type. Each description contains:

- the interface source type;
- the concrete receiver source type;
- a deterministic package-qualified table symbol;
- method slots in interface declaration order; and
- each slot's concrete method symbol and lowered signature.

This metadata is internal compiler state, not source syntax. This iteration does not emit table globals, erase concrete
values, generate receiver thunks, or emit indirect calls. Generic interfaces, generic receiver patterns, and generic
interface methods remain outside the initial representation boundary.

## Consequences

Future table emission and indirect-call lowering can consume one ordered source of truth. Concrete method signatures
remain visible, so receiver adaptation requirements are explicit rather than hidden behind an assumed fat-pointer ABI.

## Follow-up work

- Define the erased existential value and receiver thunk ABI.
- Emit one immutable dispatch table for the validated metadata slice.
- Lower one concrete-to-interface conversion and one indirect method call end to end.
