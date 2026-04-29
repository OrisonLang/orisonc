# Frontend Parity Audit

## Scope

This file tracks which source-language frontend slices are reflected in the current native compiler scaffold.

## Current status

### Implemented

- lexing for package, import, type, record, choice, interface, implements, extend, function, public, private, from, as, shared, exclusive, let, var, return, switch, default, guard, if, else, while, for, in, defer, break, continue, repeat, where, `async`, `await`, `task`, `thread`, `and`, `or`, `not`, `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shift_left`, `shift_right`, `true`, and `false` keywords
- indentation-sensitive block structure with explicit `indent` and `dedent` tokens
- package declarations, import blocks, type aliases, record declarations, choice declarations, interface declarations, `implements` declarations, `extend` declarations, function headers, and single-line `where` clauses
- parsed top-level visibility modifiers for supported declarations plus field visibility modifiers in records
- parsed choice variants including payload-bearing variants and generic choice parameter lists
- parsed interface method signatures including generic interface parameter lists, `implements Interface for Type` method blocks, and `extend Type` method blocks with method visibility
- parsed record fields, typed parameters, return types, generic `where` constraints, nested generic type syntax, and `shared`/`exclusive`-qualified type names
- statement parsing for `let`, `var`, plain `=` assignment, `return`, `break`, `continue`, expression statements, inline and block-arm `switch`, `guard ... else`, `if` with an optional `else` block, `while`, `repeat ... while`, `for ... in`, block `unsafe`, and block `defer`
- expression parsing for names, decimal/hex/binary integer literals, string literals, boolean literals, array literals, `task` and `thread` block expressions, unary `-`, `not`, `bit_not`, and `await`, calls, member access, null-safe member access, index access, ternary `?:`, and binary `+`, `-`, `*`, `%`, `/`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `and`, `or`, `bit_and`, `bit_or`, `bit_xor`, `shift_left`, and `shift_right`
- first semantic validation for concurrency surface rules: `await` and `task` now produce frontend diagnostics when used outside `async` functions or methods, `await` now also requires a structural task value or a value produced by a declared `async` call, member-call awaitability no longer falls through the global top-level async-function name bucket, consume-site diagnostics now distinguish `thread`, `task`, and declared-async-call misuse (`await` on thread values, `join()` on task values, and `join()` on declared-async-call results), return sites now also reject forwarding bare thread values and task/declared-async-call values directly, plain-name assignments preserve local provenance, ternary expressions preserve provenance when both branches carry the same structural origin, `if`/`else` plus `switch` branch assignment shaping now merges same-origin provenance back into outer locals, `guard ... else` failure blocks now analyze for diagnostics without leaking failure-path provenance into the continuing path, `switch` constructor-pattern payload names now bind as case-local immutable names in semantics across nested constructor subpatterns while unsupported constructor payload shapes and duplicate payload bindings are rejected, `while` and `for` loop shaping now preserve only provenance that survives both the zero-iteration and body-executed paths, and `repeat` loop shaping now preserves provenance established by the guaranteed first body execution, while `task`/`thread` bodies must end in a value-producing statement shape, concurrency bodies reject captures of mutable outer locals and receiver `this`, and the semantics pass now classifies captured outer values with provisional type names plus split marker recognition so `thread` captures and result values require placeholder `Transferable` support while `task` captures and result values still accept placeholder `Transferable` or `Shareable` markers across both generic constraints and concrete implementations
- CLI parse output for import/type/choice/interface/implementation/extension counts, declaration visibility, implementation/extension targets, function `where` constraints, and first-statement nested/alternate/switch-arm counts

### Pending

- additional top-level forms and modifiers from the updated docs
- richer expression, literal, and pattern grammar beyond the current narrow subset
- semantic analysis, type checking, ownership checking, lowering, and backend code generation

## Latest update

- 2026-04-28: tightened constructor-pattern `switch` arm semantics so nested payload names still bind recursively, unsupported payload expressions still fail, and duplicate payload names like `Node(head, head)` now produce explicit semantic diagnostics.
