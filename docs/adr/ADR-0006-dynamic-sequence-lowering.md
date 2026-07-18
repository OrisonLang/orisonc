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
- Dynamic-array runtime ABI entry points are finite and internal: `__orison_dynamic_array_allocate(i64, i64)`,
  `__orison_dynamic_array_grow({ ptr, i64, i64 }, i64, i64)`, and
  `__orison_dynamic_array_deallocate(ptr, i64, i64)`. The `i64` parameters are element size/capacity-style scalar
  values chosen by lowering, not source-level variadic or spread arguments.
- Shared/exclusive access is tracked as source-type metadata, not by inventing different LLVM pointer spellings.
- The current fixed-array-only `for` lowering diagnostic remains valid until loop lowering consumes this model and
  emits descriptor-aware indexing and bounds.

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

## Follow-up work

- Implement the dynamic-array runtime entry points, construction lowering, and element cleanup lowering.
- Enable `DynamicArray<T>` lowered signatures only after semantic ownership/drop analysis proves unique ownership,
  initialized length, capacity bounds, and deterministic cleanup.
- Extend `for ... in` lowering to consume dynamic sequence descriptors after descriptor ABI support is in place.
