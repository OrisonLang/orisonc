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

The thunk adapts erased storage to the concrete method receiver ABI. Shared scalar and record receivers load their
value from erased storage. Pointer-native receivers forward the erased pointer directly. Production coverage proves
this path for an exclusive `Pointer<UInt32>` implementation. Exclusive by-value receivers remain blocked until their
concrete method ABI preserves mutation through a pointer.

Lowering emits one private immutable array of thunk pointers for each table whose complete slot set is emit-ready.
Lowering also provides internal emitters that construct the borrowed pair from addressable concrete storage and a
known table, then extract the pair, load a validated slot, and issue a typed indirect call. LLVM verifier coverage
proves this composition. These emitters are not connected to user source: current semantic validation does not admit
interface declarations as ordinary value types, and no new source form is inferred here.

## Consequences

Shared and exclusive borrowing remain visible to later safety checks while every dispatch slot has one uniform erased
receiver position. Concrete values must have stable addressable storage for the duration of the existential borrow.

## Follow-up work

- Define and approve source semantics for borrowed interface values before production integration.
- Preserve the exclusive by-value gate until concrete method lowering uses a mutation-preserving pointer ABI.
