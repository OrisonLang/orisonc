# Orison Examples

`minimal.or` is the stable backend demo and must compile, link, and run with exit status `0`.
`ffi_fixed_parameters.or` demonstrates a finite C FFI contract with two explicit `Pointer<Byte>` parameters.
`nested_pointer_aggregate_assignment.or` demonstrates nested pointer-backed aggregate assignment on records and fixed arrays.
`local_record_aggregate_reassignment.or` demonstrates local record and fixed-array whole-value reassignment.
`local_record_nested_addressing.or` demonstrates record-value-backed nested field and index addressing.
`local_record_nested_record_addressing.or` demonstrates nested record-field addressing on local values.

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
| `local_record_aggregate_reassignment.or` | local record and fixed-array reassignment | backend |
| `local_record_nested_addressing.or` | record-value-backed nested addressing | backend |
| `local_record_nested_record_addressing.or` | nested record-field addressing | backend |

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
