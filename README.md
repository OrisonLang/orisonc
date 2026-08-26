# Orison

Orison is a systems programming language project. This repository contains the current `orisonc` compiler prototype.

## Dependencies

- C++23 compiler
- CMake
- LLVM development tools and libraries
- POSIX shell tools
- `dos2unix`

## Build

```sh
cmake -S . -B build
cmake --build build -j 16
```

## Test

```sh
ctest --test-dir build -j 16 --output-on-failure
```

Run the canonical compiler pipeline smoke test:

```sh
ctest --test-dir build -j 16 --output-on-failure -L canonical_pipeline
```

## Run

Run the minimal example:

```sh
build/tools/orisonc/orisonc run examples/minimal.or
```

Emit LLVM IR:

```sh
build/tools/orisonc/orisonc --emit-llvm examples/minimal.or
```

Emit an object file:

```sh
build/tools/orisonc/orisonc --emit-object examples/minimal.or -o build/minimal.o
```

Build an executable:

```sh
build/tools/orisonc/orisonc --build examples/minimal.or -o build/minimal
```

## Documents

- `ORISON_SPEC.md`
- `ORISON_TOUR.md`
- `OrisonV1.ebnf`
- `docs/adr/`
