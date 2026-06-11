# Orison Examples

`minimal.or` is the stable backend demo and must compile, link, and run with exit status `0`.

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
| `tour_09_ffi_printf.or` | C FFI and `printf("Hello world from Orison!\\n")` | frontend |
| `tour_10_unsafe_memory.or` | `const`, `unsafe`, pointers, raw and volatile access | frontend |
| `tour_11_concurrency.or` | `async`, `await`, `task`, `thread` | frontend |

"Frontend" means the source must parse and pass the current semantic checks. It does not imply LLVM lowering support.
The example smoke test enforces these levels so an example cannot silently drift out of sync with the compiler.

Run the backend demo with:

```sh
build/tools/orisonc/orisonc run examples/minimal.or
```

The `printf` example becomes runnable only when string literal storage, `Pointer<Byte>` FFI arguments, and foreign
symbol declarations are represented in LLVM IR. Until then, it intentionally documents the accepted language surface
without claiming backend support.
