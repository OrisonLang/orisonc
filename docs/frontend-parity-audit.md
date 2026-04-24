# Frontend Parity Audit

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, record, function, let, var, return, switch, default, guard, if, else, while, for, in, and defer keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, record declarations, and function headers
- parsed record fields, typed parameters, return types, and nested generic type syntax
- statement parsing for `let`, `var`, `return`, expression statements, inline and block-arm `switch`, `guard ... else`, `if` with an optional `else` block, `while`, `for ... in`, and block `defer`
- expression parsing for names, integer literals, calls, member access, and binary `+`, `-`, `*`, `%`, `/`, `==`, `!=`, `<`, `<=`, `>`, `>=`
- CLI parse output for first-statement nested, alternate, and switch-arm counts

### Pending

- richer expression grammar beyond the current narrow precedence table
- semantic analysis, type checking, ownership checking, lowering, and backend code generation

## Latest update

- 2026-04-24: completed parser support for `%`, extended smoke coverage, and synchronized the parity records.
