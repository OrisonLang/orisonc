# ADR-0001: C++23 Frontend with LLVM Backend

## Status
Accepted

## Context

Orison is intended to ship as a serious native toolchain with a compiler driver, formatter,
and language server built from shared internal libraries. The implementation needs predictable
native performance, direct control over memory and data layout, and practical integration with
existing backend and object-generation infrastructure.

## Decision

The Orison implementation stack will use:

- `C++23` for the compiler, shared libraries, and supporting tools
- `LLVM` as the core backend and code generation stack

The repository scaffold should reflect a modular internal architecture so the compiler driver,
formatter, and language server can share source management, diagnostics, syntax, semantic
analysis, and lowering components.

## Consequences

- The build system should standardize on a `CMake`-based native workflow.
- Repository layout should separate reusable compiler libraries from tool entry points and tests.
- The lowering library links LLVM's core, assembly parser, and support components so emitted textual IR is parsed
  and verified in-process before it is exposed to callers.
- Host object emission uses LLVM target discovery, a host `TargetMachine`, and the target's object-file pass pipeline.
- Initial executable linking delegates to a CMake-discovered host Clang/C driver through an argument-vector process
  launch; no shell command construction is used.
- `orisonc run <file>` uses the same host object and link path, executes a uniquely named temporary executable with
  inherited standard streams, propagates its exit status, and removes the temporary artifact.
- Shared frontend, LLVM IR, and object compilation stages live behind `orison_pipeline`; CLI modes select an
  artifact or continue into linking without duplicating stage orchestration.
- Per-function LLVM signature, SSA expression, control-flow, and return emission live behind a dedicated lowering
  component; module emission owns only module assembly, shared context construction, and final LLVM verification.
- A dedicated neutral function-lowering state model owns lowered bindings, deterministic SSA names, block identities,
  and current-block tracking so expression and statement CFG lowering share mutable emission state without either
  emitter owning the model.
- Recursive expression lowering and scalar expression-type/literal utilities compile in their own translation unit;
  function statement/control-flow emission consumes that API instead of owning expression implementation details.
- Value-bearing statement extraction and immutable `let` emission compile in a dedicated statement component;
  function and control-flow emission share it without assigning ordinary statement policy to CFG lowering.
- Straight-line scalar mutable locals use explicit LLVM stack storage; statement lowering owns `var` allocation,
  initialization, and name-target assignment, while expression lowering emits visible loads from typed mutable bindings.
- Initial `while` lowering owns deterministic condition/body/exit CFG emission in a dedicated loop component and
  supports scoped loop-local `let`/`var` declarations, mutable-local assignments, and discarded scalar-result call
  statements plus direct `Unit` call statements; member calls remain explicit follow-up work. Direct and conditionally
  nested `break`/`continue` resolve through a function-local nearest-loop target stack, and statement-level `if` emits
  merges only for live branches.
- Shared LLVM call argument lowering and call-instruction formatting live in a dedicated call emitter; value-producing
  expression calls, direct `Unit` statement calls, and direct scalar member-call statements consume the same
  signature, operand, and C ABI adapter policy. Statement-side `Unit` and member-call calls still own call-statement
  diagnostics for unsupported shapes and translate arity/argument failures into the same structured lowering failure
  categories used by expression calls. Null-safe member-call lowering remains explicit follow-up work.
- Ordinary function-body lowering now dispatches non-terminal call statements through the same call-statement helper as
  while bodies, so direct and scalar member call statements can appear before the final return expression.
- Ordinary non-`Unit` function-body lowering now also dispatches leading `repeat`, `for`, and `unsafe` statements
  through the same recursive unit path before the final return expression, while still rejecting statements that
  follow a terminating non-`Unit` statement.
- `unsafe function` declarations now lower identically to ordinary functions; the unsafe marker remains a semantic
  boundary and does not affect emitted LLVM signatures.
- `Unit`-returning function bodies now lower supported leading statements and explicit naked returns through a
  dedicated void path that emits `ret void` instead of short-circuiting the whole body; binding statements in that
  path infer their own local types from annotations or initializers, while final value-producing control flow remains
  on the non-void path.
- `defer` statements now lower as scoped cleanup records that replay on lexical scope exit, explicit `return`
  statements, and loop-control exits through the existing function-body and loop-body emission paths; cleanup blocks
  may schedule additional defers that are replayed recursively in LIFO order before scope exit, while cleanup blocks
  themselves must still fall through, and order-sensitive regressions now pin multiple nested defers within the same
  cleanup block.
- order-sensitive regressions now also pin recursive defer replay on loop-control exits.
- order-sensitive regressions now also pin recursive defer replay on `for` continue exits and `repeat` break exits.
- order-sensitive regressions now also pin recursive defer replay through nested `unsafe` blocks that contain loop-control exits.
- LLVM `while`, `repeat`, and `for` bodies now also accept nested `unsafe` blocks, reusing the existing cleanup
  replay path so `defer` blocks inside `unsafe` continue to run before `break` and `continue`; `while` bodies also
  dispatch nested `while` loops through the shared while lowering path and nested `repeat` loops through the same
  while-body statement dispatcher. Nested `for` loops in while bodies support the same array-literal and fixed-size
  array iterable subset as top-level Unit `for` lowering.
- Loop statement-flow classification and loop-target stack scoping now live in shared lowering support, so function
  body and while-body emitters no longer carry duplicate loop-control infrastructure.
- Current `for` lowering now lives behind shared loop lowering support with caller-provided type inference and body
  emission, reducing duplication between Unit function-body and while-body array-literal and fixed-size array iterable
  loop paths.
- `repeat` lowering now follows the same shared-helper direction: common block construction, loop target scoping,
  condition lowering, and diagnostics live in one helper while callers supply the body-lowering callback.
- `while` lowering now follows the same helper split: common block construction, condition lowering, loop target
  scoping, and branch emission are centralized while the existing while-body dispatcher remains caller-provided.
- Unsafe block lowering now has a shared binding-scope helper used by Unit function bodies and while bodies while
  each caller still supplies its own nested statement lowering callback.
- Non-value statement-block traversal is shared for Unit function bodies and while bodies: the common helper owns
  defer-scope replay and post-termination checks while callers retain statement-kind dispatch policy.
- Common non-value statement handlers for local bindings, assignment, calls, defers, loop control, and nested `while`
  now share one helper with caller-provided binding type inference and diagnostics.
- Nested `repeat`, `for`, and `unsafe` dispatch in non-value statement bodies now flows through the same common
  helper using caller-provided callbacks, leaving Unit-only return/guard/switch/if behavior local.
- The shared non-value statement-body helpers are header-templated to avoid `std::function` type erasure and the
  extra out-of-line lowering component on this hot dispatch path.
- Unsafe block lowering now uses the same header-templated callback style, removing its `std::function` body wrapper
  and dedicated lowering source file while keeping the binding-scope behavior unchanged.
- Repeat loop lowering now also uses a header-templated body callback, removing its `std::function` wrapper and
  dedicated lowering source while preserving existing repeat block, condition, and loop-target emission.
- While loop lowering now uses a header-templated body callback as well, removing its `std::function` wrapper and
  dedicated lowering source while preserving condition, branch, and loop-target emission.
- `guard ... else` now lowers as an explicit early-exit branch in both void and non-void function bodies; failure
  blocks can emit direct `return` statements, and non-void statement-level `if` bodies can now lower early-return
  branches before a later final expression or final control-flow statement.
- non-final `switch` statements in non-`Unit` function bodies now lower through the same early-exit path as `guard`
  and `if`, so individual cases can return early while other cases fall through to later statements after the switch.
- Value-producing statement-block traversal also lives in the statement component and normalizes contiguous syntax
  statements plus pointer-owned switch-case statements through one policy; CFG recursion is supplied as a callback.
- Branch-local immutable and mutable binding visibility is isolated by a dedicated RAII scope that snapshots both maps,
  resets them between sibling paths, and restores them on every exit while storage effects and SSA/CFG counters remain global.
- Final value-producing `if`/`switch` CFG emission, nested value blocks, and branch-local immutable bindings compile
  in a dedicated control-flow component; function emission owns signatures, parameters, and final return assembly.
- LLVM local-value, temporary, and indexed block naming policy lives in a shared stateless utility; emitters retain
  explicit counters/maps and request deterministic names without duplicating formatting or increment behavior.
- LLVM CFG text formatting for labels, branches, switches, unreachable blocks, and phi nodes lives in a shared
  stateless utility; expression and statement CFG emitters retain semantic decisions and predecessor/state tracking.
- Ternary and final `if` block naming lives in a neutral conditional planner; expression and control-flow emitters
  allocate the shared block index and provide caller-specific arm lowering.
- Ternary and final `if` conditional CFG execution lives in a shared emitter over non-owning callbacks; it owns branch
  labels, predecessor tracking, merge planning, phi emission, and current-block transitions while callers retain arm
  lowering, binding-scope policy, and diagnostic translation.
- Final scalar switch case/default validation, pattern lowering, and deterministic block planning live in a dedicated
  component that returns a non-owning plan.
- Final switch CFG execution lives in a shared emitter over non-owning callbacks; it owns dispatch-table emission,
  case labels, predecessor tracking, fallback blocks, merge planning, phi emission, and current-block transitions while
  control-flow lowering retains subject validation, case-body lowering, binding-scope policy, and diagnostics.
- Ternary, final `if`, and final `switch` result compatibility plus phi-input assembly live in a neutral merge planner;
  control-flow executors retain caller-specific failure categories.
- Successful branch merge finalization lives in a shared merge emitter; it owns merge labels, current-block transitions,
  temporary naming, phi emission, and construction of the resulting lowered value.
- Ordinary function definitions retain their shared lowered signatures in the module context even when a type is
  unsupported; function emission consumes that model directly for declarations, bindings, signedness, and diagnostics.
- Expression lowering records a structured first failure reason and detail in an explicit failure model; callers retain
  optional value flow while producing targeted diagnostics for names, types, operators, calls, casts, and branches.
- Final control-flow lowering records a separate structured first failure in the same explicit failure model; `if` and `switch`
  diagnostics identify shape, condition/subject, arm/case, pattern, and merge failures while retaining nested expression
  failure details.
- Lowering failure rendering lives in a dedicated diagnostics component over the neutral failure records; expression,
  control-flow, and function emitters consume one stable text policy without owning diagnostic wording.
  Prefix/detail diagnostic composition now also lives in that diagnostics component so emitters do not duplicate
  conditional `": detail"` formatting.
- First-failure recording for expression/control-flow lowering lives in the neutral failure model, so emitters share
  the same "record only first failure" policy.
- Failure reset lifecycle helpers also live in the neutral failure model, keeping emitter entry points explicit about
  when a fresh expression or final-control-flow lowering attempt starts.
- Mutable function emission state and lowering failures are passed as separate objects; `FunctionLoweringState` cannot
  accumulate diagnostic policy or failure lifecycle concerns as new statement and backend lowering is added.
- Expression and control-flow emitters receive a non-owning `FunctionLoweringSession` that references the separately
  owned state and failure objects, reducing recursive parameter lists without recombining their responsibilities.
- Immutable module lowering data and string constants are exposed through a neutral `LoweringEmissionContext`;
  expression, control-flow, and function emission share it without assigning context ownership to an emitter.
- Lowering context retains receiver-qualified method signatures from `implements` and `extend` blocks in a separate
  method model; method lookup reports `found`, `not_found`, or `ambiguous` rather than silently selecting among
  duplicate receiver/name matches.
- Source-type recovery for lowering now lives in a shared query component covering generic argument splitting, fixed
  LLVM array parsing, record field lookup, lowered source-type mapping, pointer pointee extraction, expression source
  recovery, and function/method return recovery; member-call receiver inference and aggregate/array emission consume
  the same model instead of carrying parallel copies.
- Aggregate path collection and LLVM `getelementptr` cursor advancement now live in a shared lowering helper for
  member/index chains over records and fixed arrays; `address_of(...)` and aggregate assignment keep their own
  diagnostics and index-expression lowering, but no longer duplicate traversal state.
- Function lowering state keeps source-level type names in a side map for parameters and annotated locals; receiver
  inference for member-call expressions consumes that map plus lowering-context record/signature metadata, recovering
  receiver source types from direct names, record member access, fixed-array index access, and supported function or
  method call returns. Direct member-call expressions now lower through explicit receiver arguments for scalar and
  supported record receiver methods, and direct member-call statements now lower through the same receiver-aware path
  while null-safe member-call expressions remain future work.
  Member-call statement diagnostics compose receiver inference with lowered method lookup so unsupported receiver
  shapes, unknown receiver types, unknown methods, ambiguous methods, and non-lowerable targets are diagnosed
  explicitly.
  Lowered methods receive deterministic internal symbols of the form `method.<receiver>.<method>`, with non-identifier
  receiver characters sanitized to `_`.
- Method definitions that fit the existing scalar function subset are emitted after top-level functions using those
  stable symbols; direct member-call expressions and statements on scalar receivers now lower through the shared call
  emitter, while null-safe member calls remain future work.
- Direct scalar member-call statements now lower through explicit receiver arguments and the shared call emitter for
  supported scalar receivers; null-safe member-call statements remain future work.
- Receiver-self method parameters (`this: This`, `shared.This`, `exclusive.This`) are lowered as the concrete receiver
  type when that receiver has a supported LLVM representation, enabling scalar receiver method definitions plus
  non-generic record receiver method definitions for records with supported layouts. Record receiver methods can return
  fixed-size arrays that participate in `for` iterable recovery; broader aggregate receiver ABI policy remains future
  work.
- Raw/MMIO intrinsics now have an initial LLVM backend path for the pointer-proven subset: `raw_read` and
  `volatile_read` emit loads, `raw_write` and `volatile_write` emit stores, `volatile_*` marks the access volatile,
  and `raw_offset` emits `getelementptr` when the source is a known `Pointer<T>`. `Address` operands are converted to
  `ptr` at the use site with `inttoptr`.
- Lowering context now collects record field layout metadata from parsed records, preserving field order, source type
  names, supported LLVM field types, and canonical named LLVM struct type names. Non-generic records whose fields all
  have supported LLVM types are emitted as named LLVM structs, and `address_of(pointer.field)` plus chained record-field
  forms like `address_of(pointer.inner.field)` lower through struct `getelementptr` steps when the source is a known
  `Pointer<Record>`. Fixed `Array<T, N>` fields lower to LLVM array fields when `T` is supported, and terminal indexed
  forms like `address_of(pointer.array[index])` and `address_of(pointer.inner.array[index])` lower through an array
  element `getelementptr`; array-of-record element field paths like `address_of(pointer.items[index].field)` also lower
  through the same mixed member/index walker, which now also supports nested-array shapes like
  `address_of(pointer.rows[index][inner])`. Lowerable non-generic record constructor calls now materialize as LLVM
  aggregate values through `insertvalue`, mutable local `var` storage accepts those aggregate values, and
  `address_of(local.field)` lowers through the same record-layout metadata from mutable local record storage. Expected
  fixed-array literals now materialize through recursive `insertvalue` as well, extending mutable local aggregate
  coverage to array-backed local record fields, local array-of-record fields, nested local arrays, and direct annotated
  mutable local arrays. Immutable aggregate `let` lowering now uses annotated or inferred initializer types before
  falling back to the enclosing return type, so unannotated record-constructor and fixed-array literal bindings can
  materialize as SSA aggregate values and retain source-type metadata for later field or indexed use, including nested
  record field chains, record array-field element extraction, inferred arrays of records, and fixed-array literals
  whose leaf elements carry explicit types. Checked-in backend coverage now also pins nested inferred mixed extraction
  through record-field-to-array-index-to-record-field immutable metadata composition plus branch-local inferred
  aggregate bindings inside final `if` arms, with CLI coverage for LLVM branch/extraction shape, object emission, direct
  `run`, and retained `--build` execution. Whole-value reassignment for those same mutable local aggregate subsets now
  reuses direct typed aggregate stores, including inferred mutable aggregate bindings whose source-type metadata remains
  available for later nested field/index access and branch-local or switch-local whole-value stores that are read after
  the control-flow join, and supported mutable-local plus pointer-backed aggregate
  field/index assignment now lowers through direct field/element addresses plus typed stores for record fields and
  fixed-array elements, including nested mixed paths like `pointer.items[index].field = value`,
  `pointer.rows[index][inner] = value`, branch-local or switch-local nested field stores read after the control-flow
  join, and branch-local or switch-local nested fixed-array element stores read after the control-flow join. CLI smoke
  coverage pins user-facing LLVM extraction shapes for helper-returned, scalar-method-returned, record-method-returned,
  member/index-receiver method-returned, and function/method parameter record and fixed-array aggregates plus mixed
  inferred immutable aggregate paths in record-field-to-array-index, array-index-to-record-field, and nested
  record-field-to-array-index-to-record-field directions, and now covers LLVM emission, host object emission, direct
  `run`, and retained `--build` execution for all three mixed demos. Aggregate member/index expression inference now
  recovers lowered scalar types from source-type metadata, and record-field extraction preserves field signedness so
  binary lowering can combine aggregate-derived scalar values with ordinary receiver fields. Checked-in coverage now
  also pins aggregate-derived scalar arithmetic through final `if`/`switch` expression merges, `while`/`for`
  loop-body accumulation, guard early-return paths, deferred cleanup replay before early/final returns, plain/member
  call arguments, fixed source-level FFI adapter arguments, returned record/fixed-array containers, returned nested
  record-with-array and fixed-array containers, branch-local returned containers, dynamic-index `while`-built
  returned containers, `for`-built returned containers, address-backed reads from mutable aggregate storage,
  address-backed reads from aggregate parameter storage, temporary address-backed reads from helper/method-returned
  aggregate values, read-only storage-backed reads from immutable aggregate `let` bindings, and pointer-backed
  aggregate reads inside `unsafe` blocks. Aggregate `for` iteration values now become read-only addressable bindings
  when their element type is a lowered record or fixed array, so field/index reads inside loop bodies reuse the same
  address-backed path. Broader
  aggregate construction/assignment remains future work.
- Lowered scalar expression and inferred-type metadata live in a neutral `lowered_value.hpp`; function state and
  emitter APIs share these records without assigning representation ownership to state or expression emission.
  Read-only aggregate addressable binding setup is shared through a lowering helper so parameters, immutable aggregate
  `let` bindings, and aggregate `for` items use the same storage/allocation convention.
  Temporary aggregate path reads and runtime-index fixed-array value reads use the same helper module for their
  alloca/store spill shape without registering source-visible bindings.
  Named aggregate path reads resolve mutable storage and read-only addressable storage through the same helper module,
  keeping binding-map precedence outside expression emission.
  The same resolver now carries optional source-type metadata for named aggregate bases so expression emission can
  distinguish non-addressable paths from addressable paths that lack source metadata.
  Aggregate path collection now exposes named-base and temporary-base classifiers so expression emission does not
  duplicate path-shape checks before choosing storage-backed or temporary-spill lowering.
  Final aggregate cursor load emission is shared by the aggregate path helper while expression emission still owns
  expected-type validation, index-expression lowering, and diagnostic policy.
  Member-step temporary naming and cursor advancement are also shared by the aggregate path helper. Index-step temporary
  naming and cursor advancement are shared too, while index operand lowering remains in expression emission because it
  requires recursive expression lowering.
  Aggregate assignment target lowering also uses the shared member/index cursor-advancement helpers while retaining its
  assignment-specific diagnostics.
  Pointer/address conversion emission is centralized inside expression lowering so `address_of`, generic address
  operands, and `Pointer(...)` construction share the same `ptrtoint`/`inttoptr` instruction shape.
  For loop lowering now uses header-templated body and element-type callbacks while preserving array-literal and
  fixed-array iterable lowering for Unit and nested while bodies.
  The templated `for` lowering paths now share one local block-plan helper for exit and iteration block naming,
  keeping array-literal and fixed-array iterable CFG setup consistent.
  Non-callback-specific `for` block planning compiles out of line so the header keeps only the callable-dependent loop
  orchestration.
  Repeat loop block planning and loop-target construction also compile out of line while repeat body orchestration
  remains templated.
  While loop block planning and loop-target construction follow the same split while condition/body orchestration
  remains templated.
  Common non-value statement dispatch now keeps concrete built-in statement lowering out of line while preserving
  templated caller-provided binding inference and nested repeat/for/unsafe callbacks.
  Deferred cleanup scope and replay declarations now live behind a narrow cleanup header so shared statement-body
  traversal no longer depends on the full statement-emitter API.
  Deferred cleanup scope RAII and replay iteration also live behind that cleanup boundary; function emission keeps only
  the Unit block-lowering adapter for cleanup replay.
  Value statement-block lowering now requires callers to pass an explicit cleanup block lowerer instead of relying on
  compatibility overloads that selected the Unit cleanup adapter implicitly.
  Final value-producing control-flow lowering now passes the Unit cleanup block adapter explicitly when lowering
  nested value blocks.
  Value statement-block cleanup replay now routes directly through the caller-provided cleanup callback.
  Statement emitter smoke coverage now exercises the explicit value-block cleanup callback overload for no-cleanup
  value blocks.
  Non-value statement-block traversal, common built-in statement dispatch, and loop-control lowering now thread the
  cleanup block lowerer explicitly while preserving the existing Unit cleanup adapter behavior at current call sites.
  Deferred cleanup replay no longer exposes the compatibility wrapper that implicitly selected the Unit cleanup
  adapter; all current cleanup replay call sites now choose the cleanup-block lowerer explicitly.
  The explicit cleanup replay API now uses the canonical `emit_deferred_cleanup_to_depth` name with a required cleanup
  block lowerer parameter.
  The Unit cleanup-block adapter is declared from a narrow cleanup-specific header so while/control-flow lowering and
  statement smoke tests no longer include the full function-emitter API only to replay deferred cleanup.
  Abandoned concurrency-handle cleanup is now emitted from a dedicated lowering component instead of a function-emitter
  local helper.
  Join/await handle destruction and abandoned handle destruction now share the same concurrency cleanup emitter helper.
  Join/await result loading now shares the same helper that destroys the completed concurrency handle.
  Join/await runtime-call emission now lives with concurrency result completion helpers instead of inline expression
  lowering.
  Spawn-failure runtime-call emission now also lives behind the concurrency cleanup helper boundary.
  Spawn runtime-call emission now lives with the other concurrency operation emitters; statement lowering still owns
  capture storage, result storage, failure CFG labels, and binding registration.
  The broadened concurrency operation emission boundary is named `concurrency_emitter` instead of
  `concurrency_cleanup`.
  Concurrency cleanup thunk emission now lives under `concurrency_emitter` instead of the general statement-emitter
  API.
  Concurrency entry thunk emission now also lives under `concurrency_emitter`; statement lowering keeps capture
  storage, result storage, spawn failure CFG, and binding registration.
  Join/await result-type compatibility and mismatch failure recording now live with the concurrency completion
  emitters, while expression lowering still owns join/await syntax recognition and binding lookup.
  Concurrency environment capture store emission now lives under `concurrency_emitter`; statement lowering still owns
  environment allocation and maps narrow emission statuses to statement diagnostics.
  Spawn failure branch emission now lives under `concurrency_emitter` with caller-provided block labels; statement
  lowering still owns block naming and current-block state.
  Concurrency binding registration now lives under `concurrency_emitter`; statement lowering still chooses binding
  names and result storage names after successful spawn CFG emission.
  Pending concurrency function-definition queuing now lives under `concurrency_emitter`; statement lowering still
  decides when successful spawn lowering has reached the queuing point.
  Drop cleanup authorization option application now lives with concurrency drop cleanup planning; statement lowering
  no longer knows the allowlist-vs-semantic-authorization declaration mapping.
- Development builds may use the platform's monolithic shared LLVM target when component archives are unavailable;
  release packaging must use a static LLVM distribution to preserve statically linked tool executables.
- Future ADRs should define the lowering pipeline, incremental compilation architecture, and runtime boundary.

## Follow-up work

- Add the first shared foundation/source/diagnostics libraries.
- Define the initial frontend pipeline from source text to parsed syntax trees.
- Replace the provisional external host-link driver with a deliberate cross-platform linker strategy.
- Generalize C foreign binding lowering so code-complete `orisonc` can dynamically emit declarations and calls for
  arbitrary programmer-declared `foreign "c"` bindings instead of relying on hard-coded lowered LLVM IR shapes.
