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

## Follow-up work

- Revisit named field initializers if readability pressure outweighs the added syntax and AST complexity.
- Extend lowering once record layout and value materialization are implemented.
