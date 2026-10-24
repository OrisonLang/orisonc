# ADR-0002: Positional Record Construction

## Status
Accepted

## Context

Records define named fields and member access, but the frontend did not yet define how source code constructs record values. Field-shaped constant initializer coverage also needs constructible record values before it can validate record member access through constants.

## Decision

Record values are constructed with a call whose head is the record type name. Arguments are positional and must follow the record field declaration order.

```text
Header([0x7F, 0x45, 0x4C, 0x46], 1)
```

## Consequences

- The syntax reuses the existing call expression shape and avoids a parser AST change for named arguments.
- Semantics validate constructor arity and field value types in constant initializers and ordinary expressions.
- Declaration-order construction is compact but less self-documenting than named fields.
- Generic record layout discovery uses function parameter types and statement-ordered local binding types when
  inferring constructor specializations. Nested blocks receive copied binding environments so local evidence does
  not escape its source scope.
- Statement-ordered inference follows exact record member types, including substituted fields of concrete generic
  instantiations, and sequence element types reached through `Array`, `DynamicArray`, and view indexing.
- Unannotated aliases can inherit a fixed function's declared return type after exact argument matching. Generic
  function return aliases use the shared generic-call substitution resolver.
- Unannotated ternary aliases contribute constructor type evidence only when both result branches resolve to the
  same exact source type. Unresolved or incompatible branches contribute no layout evidence.
- Unannotated aliases can inherit source-method return types after a unique receiver and argument match. Generic
  receiver and method parameters must all resolve before their substitutions contribute constructor type evidence.
- Generic function specialization discovery follows unannotated aliases returned by exact ordinary method calls,
  allowing method results to flow through later aliases into generic record construction.
- Generic method and generic-receiver specializations can seed unannotated aliases when source arguments identify
  one exact specialization. Ambiguous or incomplete substitutions contribute no constructor evidence.
- Generic function and method specialization discovery carries statement-ordered inferred aliases through copied
  nested-block environments. Branch, loop, and switch-case bindings can drive later specializations in their scope
  without escaping into sibling or enclosing scopes.
- `for` bindings inherit the iterable element source type for generic function and method specialization discovery.
  Choice-case payload bindings inherit their declared payload types for generic record layout discovery.
- Lowering validates emitted record LLVM type identifiers through the shared module symbol registry's type-symbol
  namespace before generated LLVM IR is accepted.

## Follow-up work

- Revisit named field initializers if readability pressure outweighs the added syntax and AST complexity.
- Extend lowering once record layout and value materialization are implemented.
