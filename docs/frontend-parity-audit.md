# Frontend Parity Audit

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, import, type, record, choice, function, public, private, from, as, let, var, return, switch, default, guard, if, else, while, for, in, and defer keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, import blocks, type aliases, record declarations, choice declarations, and function headers
- parsed top-level visibility modifiers for supported declarations plus field visibility modifiers in records
- parsed choice variants including payload-bearing variants and generic choice parameter lists
- parsed record fields, typed parameters, return types, and nested generic type syntax
- statement parsing for `let`, `var`, `return`, expression statements, inline and block-arm `switch`, `guard ... else`, `if` with an optional `else` block, `while`, `for ... in`, and block `defer`
- expression parsing for names, integer literals, unary `-`, calls, member access, and binary `+`, `-`, `*`, `%`, `/`, `==`, `!=`, `<`, `<=`, `>`, `>=`
- CLI parse output for import/type/choice counts, declaration visibility, and first-statement nested/alternate/switch-arm counts

### Pending

- additional top-level declarations and blocks from the updated docs, including `interface`, `implements`, `extend`, and `where`
- richer expression grammar beyond the current narrow precedence table, including `and`, `or`, `not`, `?:`, `?.`, and named bitwise operators
- semantic analysis, type checking, ownership checking, lowering, and backend code generation

## Latest update

- 2026-04-25: completed parser support for `choice` declarations including payload variants and generic parameter lists, extended smoke coverage, and synchronized the parity records.
