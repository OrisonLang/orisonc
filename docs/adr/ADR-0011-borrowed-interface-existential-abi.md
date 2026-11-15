# ADR-0011: Borrowed Interface Existential ABI

## Status

Accepted

## Context

ADR-0010 records interface method slots and concrete implementation signatures. Dispatch emission also needs a stable
erased value shape and a uniform callable signature for methods whose concrete receiver LLVM types differ.

## Decision

The first interface existential ABI is borrowed and contains two pointers:

```text
{ ptr concrete_storage, ptr dispatch_table }
```

Field zero points to addressable concrete storage. Field one points to the immutable dispatch table selected for the
interface/concrete-type pair. The existential does not own, allocate, copy, move, or finalize the concrete value.

Each dispatch slot names a thunk. Its LLVM signature preserves the concrete method return type and all non-receiver
parameters, while replacing the first concrete receiver parameter with `ptr`. Slot metadata records whether the source
receiver is `shared This` or `exclusive This`. Bare owned `This` receivers are outside this ABI slice.

The thunk will adapt erased storage to the concrete method receiver ABI. This decision defines metadata only; thunk
bodies, table globals, existential construction, and indirect calls are not emitted yet.

## Consequences

Shared and exclusive borrowing remain visible to later safety checks while every dispatch slot has one uniform erased
receiver position. Concrete values must have stable addressable storage for the duration of the existential borrow.

## Follow-up work

- Emit receiver thunk bodies for scalar and record receivers.
- Emit immutable dispatch tables containing thunk pointers.
- Lower one borrowed concrete-to-interface conversion and indirect method call.
