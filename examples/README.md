# Orison Examples

`minimal.or` is the stable backend demo and must compile, link, and run with exit status `0`.
`ffi_fixed_parameters.or` demonstrates a finite C FFI contract with two explicit `Pointer<Byte>` parameters.
`ffi_aggregate_scalar_parameters.or` demonstrates a finite C FFI contract with an aggregate-derived scalar parameter.
`nested_pointer_aggregate_assignment.or` demonstrates nested pointer-backed aggregate assignment on records and fixed arrays.
`pointer_array_nested_assignment.or` demonstrates pointer-backed nested fixed-array assignment.
`pointer_record_field_assignment.or` demonstrates pointer-backed array-of-record field assignment.
`pointer_record_nested_addressing.or` demonstrates pointer-backed nested record and fixed-array addressing.
`local_record_aggregate_reassignment.or` demonstrates local record and fixed-array whole-value reassignment.
`local_record_field_assignment.or` demonstrates local array-of-record field assignment.
`local_record_nested_addressing.or` demonstrates record-value-backed nested field and index addressing.
`local_record_nested_record_addressing.or` demonstrates nested record-field addressing on local values.
`local_record_nested_record_assignment.or` demonstrates nested record-field and fixed-array assignment on a local value.
`local_aggregate_let.or` demonstrates immutable aggregate `let` bindings for records and fixed arrays.
`local_inferred_record_let.or` demonstrates an immutable record `let` inferred from a constructor and read through a field.
`local_inferred_nested_record_let.or` demonstrates nested immutable record field access from an inferred constructor binding.
`local_inferred_record_array_let.or` demonstrates array-field index access from an inferred record constructor binding.
`local_inferred_array_let.or` demonstrates an immutable fixed-array `let` whose type is inferred from explicit elements.
`local_inferred_nested_array_let.or` demonstrates a nested immutable fixed-array `let` inferred from explicit elements.
`local_inferred_array_record_let.or` demonstrates record-field access from an inferred fixed array of records.
`local_inferred_nested_mixed_let.or` demonstrates record-field access through an inferred record's fixed-array field.
`local_inferred_aggregate_reassignment.or` demonstrates whole-value reassignment of an inferred mutable aggregate.
`local_branch_aggregate_reassignment.or` demonstrates branch-local whole-value reassignment of a mutable aggregate.
`local_switch_aggregate_reassignment.or` demonstrates switch-local whole-value reassignment of a mutable aggregate.
`local_mutable_aggregate_path_read.or` demonstrates address-backed reads from mutable aggregate storage.
`local_branch_aggregate_field_assignment.or` demonstrates branch-local nested field assignment on a mutable aggregate.
`local_switch_aggregate_field_assignment.or` demonstrates switch-local nested field assignment on a mutable aggregate.
`local_branch_nested_array_assignment.or` demonstrates branch-local nested fixed-array element assignment.
`local_switch_nested_array_assignment.or` demonstrates switch-local nested fixed-array element assignment.
`local_helper_aggregate_access.or` demonstrates field and index access from helper-returned aggregates.
`local_aggregate_parameter_access.or` demonstrates field and index access from aggregate parameters.
`local_call_argument_aggregate_scalar.or` demonstrates aggregate-derived scalar operands passed as call arguments.
`local_return_container_aggregate_scalar.or` demonstrates aggregate-derived scalar operands used to build returned containers.
`local_nested_return_container_aggregate_scalar.or` demonstrates aggregate-derived scalar operands used to build nested returned containers.
`local_branch_return_container_aggregate_scalar.or` demonstrates branch-local returned containers built from aggregate-derived scalars.
`local_loop_return_container_aggregate_scalar.or` demonstrates loop-built returned containers from aggregate-derived scalars.
`local_control_flow_aggregate_scalar.or` demonstrates aggregate-derived scalar operands through final control flow.
`local_loop_aggregate_scalar.or` demonstrates aggregate-derived scalar operands through loop accumulation.
`local_guard_aggregate_scalar.or` demonstrates aggregate-derived scalar operands through guard early returns.
`local_defer_aggregate_scalar.or` demonstrates aggregate-derived scalar operands through deferred cleanup and returns.
`local_unsafe_aggregate_scalar.or` demonstrates aggregate-derived scalar operands from pointer-backed unsafe reads.
`local_method_aggregate_access.or` demonstrates field and index access from method-returned aggregates.
`local_record_method_aggregate_access.or` demonstrates field and index access from record-method-returned aggregates.
`local_member_receiver_method_aggregate_access.or` demonstrates aggregate access from member/index receiver methods.
`local_branch_inferred_aggregate_let.or` demonstrates branch-local inferred aggregate bindings in final `if` arms.
`local_nested_aggregate_let.or` demonstrates immutable aggregate `let` bindings for nested records and arrays.
`local_array_for.or` demonstrates non-literal fixed-array iteration over a local array value.
`local_record_array_for.or` demonstrates non-literal fixed-array iteration over a nested record-backed array.
`local_record_index_for.or` demonstrates non-literal fixed-array iteration over an indexed nested array source.
`local_record_index_field_for.or` demonstrates non-literal fixed-array iteration over an array in an indexed record field.
`local_helper_array_for.or` demonstrates non-literal fixed-array iteration over an array returned by a helper function.
`local_method_array_for.or` demonstrates non-literal fixed-array iteration over an array returned by a method.
`local_record_method_array_for.or` demonstrates non-literal fixed-array iteration over an array returned by a record method.
`local_member_receiver_method_array_for.or` demonstrates method-returned array iteration through member and index receivers.
`concurrency_task_main.or` demonstrates a runnable `async main` that spawns and awaits a scalar task.
`concurrency_thread_main.or` demonstrates a runnable `main` that spawns and joins a scalar thread.

The numbered `tour_*.or` files split `ORISON_TOUR.md` into focused examples:

| Example | Demonstrates | Current validation |
| --- | --- | --- |
| `ffi_aggregate_scalar_parameters.or` | C FFI with an aggregate-derived scalar fixed parameter | backend |
| `tour_01_packages_imports.or` | `package`, `import`, `from`, `as`, `type` | backend |
| `tour_02_records_choices.or` | visibility, `record`, `choice`, constructors, generics | backend |
| `tour_03_interfaces_methods.or` | `interface`, `implements`, `extend`, `this`, `This` | backend |
| `tour_04_generics_ownership.or` | generics, `where`, `shared`, `exclusive` | backend |
| `tour_05_bindings_operators.or` | `let`, `var`, assignment, boolean, ternary, bitwise, shifts | backend |
| `tour_06_control_flow.or` | `guard`, `else`, `if`, `switch`, `default`, loops, `break`, `continue`, `defer` | backend |
| `tour_07_recursion.or` | recursion, explicit and implicit `return` | backend |
| `tour_08_collections.or` | arrays, views, dynamic arrays, indexing | backend |
| `tour_09_ffi_printf.or` | C FFI, explicit `library "m"`, and `printf("Hello world from Orison!\\n")` | backend |
| `tour_10_unsafe_memory.or` | `const`, `unsafe`, pointers, raw and volatile access | backend |
| `tour_11_concurrency.or` | `async`, `await`, `task`, `thread` | backend |
| `concurrency_task_main.or` | runnable `async main`, `task`, and `await` | backend |
| `concurrency_thread_main.or` | runnable `main`, `thread`, and `.join()` | backend |
| `nested_pointer_aggregate_assignment.or` | nested pointer-backed aggregate assignment | backend |
| `pointer_array_nested_assignment.or` | pointer-backed nested fixed-array assignment | backend |
| `pointer_record_field_assignment.or` | pointer-backed array-of-record field assignment | backend |
| `pointer_record_nested_addressing.or` | pointer-backed nested record and array addressing | backend |
| `local_record_aggregate_reassignment.or` | local record and fixed-array reassignment | backend |
| `local_record_field_assignment.or` | local array-of-record field assignment | backend |
| `local_record_nested_addressing.or` | record-value-backed nested addressing | backend |
| `local_record_nested_record_addressing.or` | nested record-field addressing | backend |
| `local_record_nested_record_assignment.or` | nested record-field and array assignment | backend |
| `local_aggregate_let.or` | immutable aggregate `let` bindings | backend |
| `local_inferred_record_let.or` | inferred immutable record `let` field access | backend |
| `local_inferred_nested_record_let.or` | inferred nested immutable record `let` field access | backend |
| `local_inferred_record_array_let.or` | inferred immutable record array-field index access | backend |
| `local_inferred_array_let.or` | inferred immutable fixed-array `let` binding | backend |
| `local_inferred_nested_array_let.or` | inferred nested immutable fixed-array `let` binding | backend |
| `local_inferred_array_record_let.or` | inferred immutable array-of-record field access | backend |
| `local_inferred_nested_mixed_let.or` | inferred immutable record-field/array-index/record-field access | backend |
| `local_inferred_aggregate_reassignment.or` | inferred mutable aggregate reassignment followed by field/index access | backend |
| `local_branch_aggregate_reassignment.or` | branch-local mutable aggregate reassignment followed by field/index access | backend |
| `local_switch_aggregate_reassignment.or` | switch-local mutable aggregate reassignment followed by field/index access | backend |
| `local_mutable_aggregate_path_read.or` | address-backed reads from mutable aggregate storage | backend |
| `local_branch_aggregate_field_assignment.or` | branch-local mutable aggregate field assignment followed by field/index access | backend |
| `local_switch_aggregate_field_assignment.or` | switch-local mutable aggregate field assignment followed by field/index access | backend |
| `local_branch_nested_array_assignment.or` | branch-local nested fixed-array element assignment followed by index access | backend |
| `local_switch_nested_array_assignment.or` | switch-local nested fixed-array element assignment followed by index access | backend |
| `local_helper_aggregate_access.or` | helper-returned aggregate field/index access | backend |
| `local_aggregate_parameter_access.or` | aggregate parameter field/index access | backend |
| `local_call_argument_aggregate_scalar.or` | aggregate-derived scalar operands passed as call arguments | backend |
| `local_return_container_aggregate_scalar.or` | aggregate-derived scalar operands used to build returned containers | backend |
| `local_nested_return_container_aggregate_scalar.or` | aggregate-derived scalar operands used to build nested returned containers | backend |
| `local_branch_return_container_aggregate_scalar.or` | branch-local returned containers built from aggregate-derived scalars | backend |
| `local_loop_return_container_aggregate_scalar.or` | loop-built returned containers from aggregate-derived scalars | backend |
| `local_control_flow_aggregate_scalar.or` | aggregate-derived scalar operands through final control flow | backend |
| `local_loop_aggregate_scalar.or` | aggregate-derived scalar operands through loop accumulation | backend |
| `local_guard_aggregate_scalar.or` | aggregate-derived scalar operands through guard early returns | backend |
| `local_defer_aggregate_scalar.or` | aggregate-derived scalar operands through deferred cleanup and returns | backend |
| `local_unsafe_aggregate_scalar.or` | aggregate-derived scalar operands from pointer-backed unsafe reads | backend |
| `local_method_aggregate_access.or` | method-returned aggregate field/index access | backend |
| `local_record_method_aggregate_access.or` | record-method-returned aggregate field/index access | backend |
| `local_member_receiver_method_aggregate_access.or` | member/index receiver method-returned aggregate access | backend |
| `local_branch_inferred_aggregate_let.or` | branch-local inferred immutable aggregate `let` bindings | backend |
| `local_nested_aggregate_let.or` | immutable nested aggregate `let` bindings | backend |
| `local_array_for.or` | non-literal fixed-array iteration | backend |
| `local_record_array_for.or` | nested record-backed fixed-array iteration | backend |
| `local_record_index_for.or` | indexed nested fixed-array iteration | backend |
| `local_record_index_field_for.or` | indexed record-field fixed-array iteration | backend |
| `local_helper_array_for.or` | helper-returned fixed-array iteration | backend |
| `local_method_array_for.or` | method-returned fixed-array iteration | backend |
| `local_record_method_array_for.or` | record-method-returned fixed-array iteration | backend |
| `local_member_receiver_method_array_for.or` | member/index receiver method-returned fixed-array iteration | backend |

"Frontend" means the source must parse and pass the current semantic checks. It does not imply LLVM lowering support.
The example smoke test enforces these levels so an example cannot silently drift out of sync with the compiler.

## Canonical Compiler Pipeline Demos

Use `minimal.or` for the smallest compile/link/run demonstration.

Use `local_record_field_assignment.or` and `pointer_record_field_assignment.or` for aggregate-assignment pipeline
coverage. CLI smoke tests currently pin both files through `--emit-llvm`, `--emit-object`, `run`, and retained
`--build` executable paths.

Run only these canonical pipeline demos with:

```sh
ctest --test-dir build --output-on-failure -L canonical_pipeline
```

Run the backend demo with:

```sh
build/tools/orisonc/orisonc run examples/minimal.or
```

Run the concurrency runtime demo with:

```sh
build/tools/orisonc/orisonc run examples/concurrency_task_main.or
build/tools/orisonc/orisonc run examples/concurrency_thread_main.or
```

The task example spawns and awaits a scalar runtime task. The thread example spawns and joins a scalar OS thread.
Both exit with status `0`.

Run the C FFI demo with:

```sh
build/tools/orisonc/orisonc run examples/tour_09_ffi_printf.or
```

It prints `Hello world from Orison!`. Its exit status is the value returned by `printf`.

Run the fixed-parameter FFI demo with:

```sh
build/tools/orisonc/orisonc run examples/ffi_fixed_parameters.or
```

It calls `strcmp` with exactly two statically checked arguments and exits with status `0`.

Run the aggregate-derived scalar FFI demo with:

```sh
build/tools/orisonc/orisonc run examples/ffi_aggregate_scalar_parameters.or
```

It calls a fixed-parameter `printf` adapter where the value argument comes from an aggregate.

Run the nested aggregate assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/nested_pointer_aggregate_assignment.or
```

It returns `0` after exercising nested pointer-backed aggregate assignment through a small local record demo.

Run the pointer-backed nested fixed-array assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/pointer_array_nested_assignment.or
```

It returns `0` after lowering `pointer.rows[index][inner] = value` through a pointer-backed aggregate path.

Run the pointer-backed array-of-record field assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/pointer_record_field_assignment.or
```

It returns `0` after lowering `pointer.entries[index].status = value` through a pointer-backed aggregate path.

Run the pointer-backed nested addressing demo with:

```sh
build/tools/orisonc/orisonc run examples/pointer_record_nested_addressing.or
```

It returns `0` after lowering `address_of(pointer.entries[index].status)` and
`address_of(pointer.rows[index][inner])` through pointer-backed aggregate paths.

Run the local aggregate reassignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_aggregate_reassignment.or
```

It returns `0` after exercising whole-value record and fixed-array reassignment.

Run the local array-of-record field assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_field_assignment.or
```

It returns `0` after lowering `local.entries[index].status = value` through a mutable-local aggregate path.

Run the local nested addressing demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_nested_addressing.or
```

It returns `0` after compiling local record-backed nested address calculations.

Run the local nested record demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_nested_record_addressing.or
```

It returns `0` after compiling local nested record-field addresses.

Run the local nested record assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_nested_record_assignment.or
```

It returns `0` after exercising nested record-field and fixed-array assignment on a local value.

Run the immutable aggregate `let` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_aggregate_let.or
```

It returns `0` after lowering immutable aggregate bindings for a record and a fixed array.

Run the inferred immutable record `let` field-access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_record_let.or
```

It returns `0` after lowering an unannotated record-constructor `let` and extracting one of its fields.

Run the inferred nested immutable record `let` field-access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_nested_record_let.or
```

It returns `0` after lowering an unannotated nested record-constructor `let` and extracting a nested field.

Run the inferred immutable record array-field index demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_record_array_let.or
```

It returns `0` after lowering an unannotated record-constructor `let` and extracting an indexed array field element.

Run the inferred immutable fixed-array `let` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_array_let.or
```

It returns `0` after lowering an unannotated fixed-array `let` whose element types are explicit.

Run the inferred nested immutable fixed-array `let` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_nested_array_let.or
```

It returns `0` after lowering an unannotated nested fixed-array `let` whose leaf element types are explicit.

Run the inferred immutable array-of-record field-access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_array_record_let.or
```

It returns `0` after lowering an unannotated fixed array of records and extracting a field from an indexed element.

Run the inferred nested mixed aggregate `let` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_nested_mixed_let.or
```

It returns `0` after lowering an unannotated record with a fixed array of records and extracting a nested field.

Run the inferred mutable aggregate reassignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_inferred_aggregate_reassignment.or
```

It returns `0` after whole-value reassignment of an inferred mutable aggregate and later nested field/index access.

Run the branch-local mutable aggregate reassignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_branch_aggregate_reassignment.or
```

It returns `0` after branch-local whole-value reassignment and later nested field/index access.

Run the switch-local mutable aggregate reassignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_switch_aggregate_reassignment.or
```

It returns `0` after switch-local whole-value reassignment and later nested field/index access.

Run the mutable aggregate path-read demo with:

```sh
build/tools/orisonc/orisonc run examples/local_mutable_aggregate_path_read.or
```

It returns `0` after lowering nested mutable aggregate reads through storage-derived element addresses.

Run the branch-local mutable aggregate field-assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_branch_aggregate_field_assignment.or
```

It returns `0` after branch-local nested field assignment and later nested field/index access.

Run the switch-local mutable aggregate field-assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_switch_aggregate_field_assignment.or
```

It returns `0` after switch-local nested field assignment and later nested field/index access.

Run the branch-local nested fixed-array assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_branch_nested_array_assignment.or
```

It returns `0` after branch-local nested fixed-array element assignment and later nested index access.

Run the switch-local nested fixed-array assignment demo with:

```sh
build/tools/orisonc/orisonc run examples/local_switch_nested_array_assignment.or
```

It returns `0` after switch-local nested fixed-array element assignment and later nested index access.

Run the helper-returned aggregate access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_helper_aggregate_access.or
```

It returns `0` after field and index access from helper-returned aggregates.

Run the aggregate parameter access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_aggregate_parameter_access.or
```

It returns `0` after field and index access from aggregate parameters in functions and methods.

Run the aggregate-derived scalar call-argument demo with:

```sh
build/tools/orisonc/orisonc run examples/local_call_argument_aggregate_scalar.or
```

It returns `0` after passing aggregate-derived scalar operands directly to plain and member calls.

Run the aggregate-derived scalar return-container demo with:

```sh
build/tools/orisonc/orisonc run examples/local_return_container_aggregate_scalar.or
```

It returns `0` after building returned record and fixed-array containers from aggregate-derived scalar operands.

Run the aggregate-derived scalar nested return-container demo with:

```sh
build/tools/orisonc/orisonc run examples/local_nested_return_container_aggregate_scalar.or
```

It returns `0` after building returned record-with-array and nested fixed-array containers from aggregate-derived scalars.

Run the aggregate-derived scalar branch return-container demo with:

```sh
build/tools/orisonc/orisonc run examples/local_branch_return_container_aggregate_scalar.or
```

It returns `0` after final `if` and `switch` arms build returned containers from aggregate-derived scalars.

Run the aggregate-derived scalar loop return-container demo with:

```sh
build/tools/orisonc/orisonc run examples/local_loop_return_container_aggregate_scalar.or
```

It returns `0` after dynamic-index `while` and `for` accumulation feed returned record and fixed-array containers.

Run the aggregate-derived scalar control-flow demo with:

```sh
build/tools/orisonc/orisonc run examples/local_control_flow_aggregate_scalar.or
```

It returns `0` after merging aggregate-derived scalar arithmetic through final `if` and `switch` expressions.

Run the aggregate-derived scalar loop demo with:

```sh
build/tools/orisonc/orisonc run examples/local_loop_aggregate_scalar.or
```

It returns `0` after accumulating aggregate-derived scalar values through `while` and `for` loop bodies.

Run the aggregate-derived scalar guard demo with:

```sh
build/tools/orisonc/orisonc run examples/local_guard_aggregate_scalar.or
```

It returns `0` after using aggregate-derived scalar values in guard conditions, early returns, and the final expression.

Run the aggregate-derived scalar defer demo with:

```sh
build/tools/orisonc/orisonc run examples/local_defer_aggregate_scalar.or
```

It returns `0` after replaying deferred cleanup that consumes aggregate-derived scalar values before returns.

Run the aggregate-derived scalar unsafe-read demo with:

```sh
build/tools/orisonc/orisonc run examples/local_unsafe_aggregate_scalar.or
```

It returns `0` after an `unsafe` block reads pointer-backed aggregate fields and combines the scalar values.

Run the method-returned aggregate access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_method_aggregate_access.or
```

It returns `0` after field and index access from method-returned aggregates.

Run the record-method-returned aggregate access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_method_aggregate_access.or
```

It returns `0` after field and index access from record-method-returned aggregates.

Run the member/index receiver method-returned aggregate access demo with:

```sh
build/tools/orisonc/orisonc run examples/local_member_receiver_method_aggregate_access.or
```

It returns `0` after aggregate access from methods called through member and index receivers.

Run the branch-local inferred aggregate `let` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_branch_inferred_aggregate_let.or
```

It returns `0` after lowering branch-local inferred aggregate bindings in final `if` arms.

Run the nested immutable aggregate `let` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_nested_aggregate_let.or
```

It returns `0` after lowering immutable aggregate bindings for nested records and arrays.

Run the named-array `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_array_for.or
```

It returns `0` after lowering a `for` loop over a local fixed-size array value.

Run the nested record-backed array `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_array_for.or
```

It returns `0` after lowering a `for` loop over a fixed-size array stored inside a local record value.

Run the indexed nested array `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_index_for.or
```

It returns `0` after lowering a `for` loop over a fixed-size array reached through an array index.

Run the indexed record-field `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_index_field_for.or
```

It returns `0` after lowering a `for` loop over a fixed-size array reached through an indexed record field.

Run the helper-returned array `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_helper_array_for.or
```

It returns `0` after lowering a `for` loop over a fixed-size array returned by a helper function.

Run the method-returned array `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_method_array_for.or
```

It returns `0` after lowering a `for` loop over a fixed-size array returned by a method.

Run the record-method-returned array `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_record_method_array_for.or
```

It returns `0` after lowering a `for` loop over a fixed-size array returned by a record receiver method.

Run the member/index receiver method-returned array `for` demo with:

```sh
build/tools/orisonc/orisonc run examples/local_member_receiver_method_array_for.or
```

It returns `0` after lowering `for` loops over fixed-size arrays returned by methods called through member and index receivers.
