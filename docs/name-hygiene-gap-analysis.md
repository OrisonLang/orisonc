# Name Hygiene Gap Analysis

Date: 2026-08-02

## Scope

This note tracks semantic name hygiene and remaining lowering or LLVM symbol collision risks. It is an implementation
gap analysis only; it does not define new language syntax.

## Covered

- Import binding names reject duplicate visible bindings, including alias collisions.
- Foreign import local function names reject duplicate local callable names.
- Top-level `type`, `record`, `choice`, and `interface` declarations share one type namespace.
- Top-level imports, constants, source functions, foreign import local functions, and type declarations share a
  user-visible collision check.
- Source functions and foreign import/export ABI aliases using the compiler-reserved `__orison_` prefix are rejected
  before LLVM module-prelude runtime declarations are emitted.
- The lowering context has an initial module symbol registry that rejects foreign declaration symbols colliding with
  generated generic function specialization symbols or generated method symbols before LLVM IR text is emitted.
- The registry now also rejects generated generic function specialization symbols that collide with source function
  symbols, and foreign export aliases that collide with generated method symbols.
- The module symbol registry is now a reusable lowering component with constructed-state smoke coverage for generated
  concurrency thunk symbols, generated concurrency cleanup symbols, and planned drop declaration symbols colliding with
  already registered LLVM symbols.
- Same-category duplicate diagnostics remain specific, so `function`/`function`, `type`/`type`, `import`/`import`, and
  source/foreign function conflicts keep targeted messages instead of cascading into broad namespace diagnostics.

## Remaining Risks

- Lowered method symbols such as `method.Box_UInt32_.value` can still collide with source symbols only if future source
  syntax permits method-shaped global symbol names.
- Generated private concurrency thunk and cleanup symbols are deterministic and use reserved-looking names. Constructed
  collision states are now validated by the shared registry, while production emission still needs the registry threaded
  through every generated symbol site.
- DynamicArray cleanup helper symbols such as `__orison_dynamic_array_cleanup.0` are generated in lowering. The reserved
  prefix semantic guard blocks user source aliases from occupying that space, but direct lowerer tests can still create
  constructed collision states.
- Planned drop ABI symbols such as `__orison_drop.Payload` intentionally use compiler-owned names. Source-level
  conflicts are blocked through the broader `__orison_` guard, and lowerer-only constructed fixtures now validate that
  conflicting metadata is rejected by the shared registry.

## Next Implementation Slice

Thread the reusable lowering-level module symbol registry through every emitted or declared LLVM symbol before module
text is published. The registry should classify symbols as source function, foreign declaration, foreign export, method,
generic specialization, runtime prelude declaration, generated thunk, generated cleanup helper, or planned drop
declaration.

- generated concurrency thunk symbol colliding with any externally visible symbol in constructed lowering state
- planned drop declaration symbol colliding with any non-drop user-emitted symbol in constructed lowering state
