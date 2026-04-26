# Frontend Parity Audit

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, import, type, record, choice, interface, implements, extend, function, public, private, from, as, shared, exclusive, let, var, return, switch, default, guard, if, else, while, for, in, defer, break, continue, repeat, where, `and`, `or`, `not`, `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shift_left`, `shift_right`, `true`, and `false` keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, import blocks, type aliases, record declarations, choice declarations, interface declarations, `implements` declarations, `extend` declarations, function headers, and single-line `where` clauses
- parsed top-level visibility modifiers for supported declarations plus field visibility modifiers in records
- parsed choice variants including payload-bearing variants and generic choice parameter lists
- parsed interface method signatures including generic interface parameter lists, `implements Interface for Type` method blocks, and `extend Type` method blocks with method visibility
- parsed record fields, typed parameters, return types, generic `where` constraints, nested generic type syntax, and `shared`/`exclusive`-qualified type names
- statement parsing for `let`, `var`, plain `=` assignment, `return`, `break`, `continue`, expression statements, inline and block-arm `switch`, `guard ... else`, `if` with an optional `else` block, `while`, `repeat ... while`, `for ... in`, block `unsafe`, and block `defer`
- expression parsing for names, decimal/hex/binary integer literals, string literals, boolean literals, array literals, unary `-`, `not`, and `bit_not`, calls, member access, null-safe member access, index access, ternary `?:`, and binary `+`, `-`, `*`, `%`, `/`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `and`, `or`, `bit_and`, `bit_or`, `bit_xor`, `shift_left`, and `shift_right`
- CLI parse output for import/type/choice/interface/implementation/extension counts, declaration visibility, implementation/extension targets, function `where` constraints, and first-statement nested/alternate/switch-arm counts

### Pending

- additional top-level forms and modifiers from the updated docs, including `unsafe`/`async` function modifiers
- richer expression, literal, and pattern grammar beyond the current narrow subset
- concurrency expression forms from the updated docs, including `thread`, `task`, and `await`
- semantic analysis, type checking, ownership checking, lowering, and backend code generation

## Latest update

- 2026-04-26: completed parser support for foreign imports and exports, extended smoke coverage, and synchronized the parity records.
