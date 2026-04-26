# Frontend Parity Audit

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, import, type, record, choice, interface, implements, extend, function, public, private, from, as, shared, exclusive, let, var, return, switch, default, guard, if, else, while, for, in, and defer keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, import blocks, type aliases, record declarations, choice declarations, interface declarations, `implements` declarations, `extend` declarations, and function headers
- parsed top-level visibility modifiers for supported declarations plus field visibility modifiers in records
- parsed choice variants including payload-bearing variants and generic choice parameter lists
- parsed interface method signatures including generic interface parameter lists, `implements Interface for Type` method blocks, and `extend Type` method blocks with method visibility
- parsed record fields, typed parameters, return types, nested generic type syntax, and `shared`/`exclusive`-qualified type names
- statement parsing for `let`, `var`, `return`, expression statements, inline and block-arm `switch`, `guard ... else`, `if` with an optional `else` block, `while`, `for ... in`, and block `defer`
- expression parsing for names, integer literals, unary `-`, calls, member access, and binary `+`, `-`, `*`, `%`, `/`, `==`, `!=`, `<`, `<=`, `>`, `>=`
- CLI parse output for import/type/choice/interface/implementation/extension counts, declaration visibility, implementation/extension targets, and first-statement nested/alternate/switch-arm counts

### Pending

- additional top-level declarations and blocks from the updated docs, including `where`
- richer expression grammar beyond the current narrow precedence table, including `and`, `or`, `not`, `?:`, `?.`, and named bitwise operators
- semantic analysis, type checking, ownership checking, lowering, and backend code generation

## Latest update

- 2026-04-26: completed parser support for `extend` declarations including visibility-qualified method blocks, extended smoke coverage, and synchronized the parity records.
