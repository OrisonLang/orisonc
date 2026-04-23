# Frontend Parity Audit

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, record, function, let, var, return, guard, if, and else keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, record declarations, and function headers
- parsed record fields, typed parameters, return types, and nested generic type syntax
- statement parsing for `let`, `var`, `return`, expression statements, `guard ... else`, and `if` with an optional `else` block
- expression parsing for names, integer literals, calls, member access, and binary `+`, `-`, `*`, `/`, `<`, `>`
- CLI parse output for first-statement nested and alternate branch counts

### Pending

- `switch`, `while`, `for`, and `defer` statement parsing
- richer expression grammar beyond the current narrow precedence table
- semantic analysis, type checking, ownership checking, lowering, and backend code generation

## Latest update

- 2026-04-23: completed parser support for `guard ... else`, added low-precedence `<`/`>` comparisons for documented conditions, extended smoke coverage, and synchronized the parity records.
