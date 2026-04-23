# Frontend Parity Audit

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, record, function, let, var, return, if, and else keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, record declarations, and function headers
- parsed record fields, typed parameters, return types, and nested generic type syntax
- statement parsing for `let`, `var`, `return`, expression statements, and `if` with an optional `else` block
- expression parsing for names, integer literals, calls, member access, and binary `+`, `-`, `*`, `/`
- CLI parse output for first-statement nested and alternate branch counts

### Pending

- `guard`, `switch`, `while`, `for`, and `defer` statement parsing
- richer expression grammar beyond the current narrow precedence table
- semantic analysis, type checking, ownership checking, lowering, and backend code generation

## Latest update

- 2026-04-23: completed parser support for `if ... else`, added lexer/parser diagnostics for stray `else`, extended smoke coverage, and synchronized the public docs.
