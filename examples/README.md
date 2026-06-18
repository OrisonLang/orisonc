# Orison Examples

`minimal.or` is the stable backend demo and must compile, link, and run with exit status `0`.
`ffi_fixed_parameters.or` demonstrates a finite C FFI contract with two explicit `Pointer<Byte>` parameters.
`nested_pointer_aggregate_assignment.or` demonstrates nested pointer-backed aggregate assignment on records and fixed arrays.
`pointer_array_nested_assignment.or` demonstrates pointer-backed nested fixed-array assignment.
`pointer_record_field_assignment.or` demonstrates pointer-backed array-of-record field assignment.
`pointer_record_nested_addressing.or` demonstrates pointer-backed nested record and fixed-array addressing.
`local_record_aggregate_reassignment.or` demonstrates local record and fixed-array whole-value reassignment.
`local_record_nested_addressing.or` demonstrates record-value-backed nested field and index addressing.
`local_record_nested_record_addressing.or` demonstrates nested record-field addressing on local values.
`local_record_nested_record_assignment.or` demonstrates nested record-field and fixed-array assignment on a local value.
`local_aggregate_let.or` demonstrates immutable aggregate `let` bindings for records and fixed arrays.
`local_nested_aggregate_let.or` demonstrates immutable aggregate `let` bindings for nested records and arrays.
`local_array_for.or` demonstrates non-literal fixed-array iteration over a local array value.
`local_record_array_for.or` demonstrates non-literal fixed-array iteration over a nested record-backed array.
`local_record_index_for.or` demonstrates non-literal fixed-array iteration over an indexed nested array source.
`local_record_index_field_for.or` demonstrates non-literal fixed-array iteration over an array in an indexed record field.
`local_helper_array_for.or` demonstrates non-literal fixed-array iteration over an array returned by a helper function.
`local_method_array_for.or` demonstrates non-literal fixed-array iteration over an array returned by a method.
`local_record_method_array_for.or` demonstrates non-literal fixed-array iteration over an array returned by a record method.
`local_member_receiver_method_array_for.or` demonstrates method-returned array iteration through member and index receivers.

The numbered `tour_*.or` files split `ORISON_TOUR.md` into focused examples:

| Example | Demonstrates | Current validation |
| --- | --- | --- |
| `tour_01_packages_imports.or` | `package`, `import`, `from`, `as`, `type` | frontend |
| `tour_02_records_choices.or` | visibility, `record`, `choice`, constructors, generics | frontend |
| `tour_03_interfaces_methods.or` | `interface`, `implements`, `extend`, `this`, `This` | frontend |
| `tour_04_generics_ownership.or` | generics, `where`, `shared`, `exclusive` | frontend |
| `tour_05_bindings_operators.or` | `let`, `var`, assignment, boolean, ternary, bitwise, shifts | frontend |
| `tour_06_control_flow.or` | `guard`, `else`, `if`, `switch`, `default`, loops, `break`, `continue`, `defer` | frontend |
| `tour_07_recursion.or` | recursion, explicit and implicit `return` | frontend |
| `tour_08_collections.or` | arrays, views, dynamic arrays, indexing | frontend |
| `tour_09_ffi_printf.or` | C FFI, explicit `library "m"`, and `printf("Hello world from Orison!\\n")` | backend |
| `tour_10_unsafe_memory.or` | `const`, `unsafe`, pointers, raw and volatile access | frontend |
| `tour_11_concurrency.or` | `async`, `await`, `task`, `thread` | frontend |
| `nested_pointer_aggregate_assignment.or` | nested pointer-backed aggregate assignment | backend |
| `pointer_array_nested_assignment.or` | pointer-backed nested fixed-array assignment | backend |
| `pointer_record_field_assignment.or` | pointer-backed array-of-record field assignment | backend |
| `pointer_record_nested_addressing.or` | pointer-backed nested record and array addressing | backend |
| `local_record_aggregate_reassignment.or` | local record and fixed-array reassignment | backend |
| `local_record_nested_addressing.or` | record-value-backed nested addressing | backend |
| `local_record_nested_record_addressing.or` | nested record-field addressing | backend |
| `local_record_nested_record_assignment.or` | nested record-field and array assignment | backend |
| `local_aggregate_let.or` | immutable aggregate `let` bindings | backend |
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

Run the backend demo with:

```sh
build/tools/orisonc/orisonc run examples/minimal.or
```

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
