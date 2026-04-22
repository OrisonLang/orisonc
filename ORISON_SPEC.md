# ORISON_SPEC.md

## Overview

Orison is a statically typed systems programming language designed for readability, explicit ownership, and practical low-level control. The surface syntax aims to reduce unnecessary notation while preserving strong semantics around memory, safety, concurrency, and foreign interaction.

## Type system summary

Orison uses a **static, explicit, mostly nominal, ownership-aware systems type system**.

### Core properties

- **Static**: types are checked at compile time.
- **Explicit**: function parameters, return types, records, and interfaces are written directly in source.
- **Mostly nominal**: named types are distinct unless the language explicitly states otherwise.
- **Ownership-aware**: owned values, shared views, exclusive mutable views, and raw pointers are distinct in the type model.
- **Parametric**: generics are first-class.
- **Algebraic**: product types and sum types are core modeling tools.
- **Interface-based**: shared behavior is modeled with `interface` and `implements`.
- **Unsafe-aware**: raw memory and MMIO live behind explicit `unsafe` boundaries.

### One-paragraph description

Orison provides a static, mostly nominal type system for systems programming with explicit ownership and borrowing, generic abstraction, algebraic data types, and interface-based polymorphism. Ordinary values, shared views, exclusive mutable views, and raw pointers are represented distinctly. Safe typed code and unsafe raw-memory code are intentionally separated, while fixed-width numeric types, addresses, arrays, dynamic arrays, views, and memory-mapped I/O remain first-class concerns.

## Surface features and keywords

### `package`

Declares the package namespace for the current source file.

```text
package drivers.uart
```

### `import`

Imports named items from another package.

```text
import
    console from io
    Logger as Log from diagnostics.logger
    random_int from math.random
```

### `export`

Marks a declaration as visible outside the current package.

```text
export function parse_port(text: shared Text) -> Outcome<UInt16, ParseError>
    guard text.length() > 0 else
        return Error(ParseError.EmptyInput)

    Ok(parse_digits(text))
```

Also valid with foreign exports.

```text
export foreign "c"
function ffi_add(a: Int32, b: Int32) -> Int32
    a + b
```

### `type`

Defines a type alias.

```text
type UserId = UInt64
type Port = UInt16
```

### `record`

Defines a product type with named fields.

```text
record User
    id: UserId
    name: Text
    age: UInt8
```

Generic records are supported.

```text
record Pair<A, B>
    first: A
    second: B
```

### `choice`

Defines a tagged sum type.

```text
choice IOError
    Closed
    EndOfInput
    PermissionDenied
```

Variants may carry payloads.

```text
choice Expression
    Int(value: Int64)
    Add(left: Box<Expression>, right: Box<Expression>)
    Neg(inner: Box<Expression>)
```

### `interface`

Defines a behavioral abstraction.

```text
interface Display
    function display(this: shared This) -> Text
```

Generic interfaces are also supported.

```text
interface Iterator<T>
    function next(this: exclusive This) -> Maybe<T>
```

### `implements`

Attaches an interface implementation to a concrete type.

```text
implements Display for User
    function display(this: shared This) -> Text
        this.name
```

### `extend`

Adds inherent methods directly to a type.

```text
extend Buffer
    function length(this: shared This) -> IntSize
        this.data.length()

    function append(this: exclusive This, b: Byte) -> Unit
        this.data.push(b)
```

### `function`

Defines a function or method.

```text
function add(a: Int64, b: Int64) -> Int64
    a + b
```

### `where`

Adds generic constraints.

```text
function max<T>(left: T, right: T) -> T
where T: Ordered
    switch left >= right
        true => left
        false => right
```

### `this`

Refers to the current receiver value.

```text
extend User
    function display_name(this: shared This) -> Text
        this.name
```

### `This`

Refers to the receiver type inside `interface`, `implements`, or `extend`.

```text
interface Reader
    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<IntSize, IOError>
```

## Ownership and access

### Owned values

A bare type usually means owned value semantics.

```text
function consume(buf: Buffer) -> Unit
    sink(buf)
```

### `shared`

Marks a read-only non-owning access view.

```text
function first<T>(items: shared View<T>) -> Maybe<shared T>
    switch items.length()
        0 => Empty
        default => Some(items[0])
```

### `exclusive`

Marks a unique mutable non-owning access view.

```text
function fill<R>(source: exclusive R, into: exclusive View<Byte>) -> Outcome<IntSize, IOError>
where R: Reader
    source.read(into)
```

### `Pointer<T>`

Represents a raw typed pointer.

```text
unsafe function read_word(addr: Address) -> UInt32
    let p: Pointer<UInt32> = Pointer<UInt32>(addr)
    raw_read(p)
```

### `Address`

Represents a raw machine address.

```text
const UART0_BASE: Address = 0x4000_1000
```

## Built-in type families

### Fixed-width signed integers

- `Int8`
- `Int16`
- `Int32`
- `Int64`
- `Int128`
- `IntSize`

### Fixed-width unsigned integers

- `UInt8`
- `UInt16`
- `UInt32`
- `UInt64`
- `UInt128`
- `UIntSize`

### Floating-point types

- `Float32`
- `Float64`

### Other core built-ins

- `Bool`
- `Byte`
- `Char`
- `Text`
- `Unit`
- `Address`
- `Pointer<T>`

## Algebraic helpers

### `Maybe<T>`

Represents an optional value.

```text
function first<T>(items: shared View<T>) -> Maybe<shared T>
    switch items.length()
        0 => Empty
        default => Some(items[0])
```

Constructors:

- `Some(value)`
- `Empty`

### `Outcome<T, E>`

Represents success or failure.

```text
function parse_port(text: shared Text) -> Outcome<UInt16, ParseError>
    guard text.length() > 0 else
        return Error(ParseError.EmptyInput)

    Ok(parse_digits(text))
```

Constructors:

- `Ok(value)`
- `Error(reason)`

### `Box<T>`

Represents explicit owning heap indirection.

```text
record Box<T>
    value: T
```

Useful for recursive data.

```text
choice List<T>
    Empty
    Node(head: T, tail: Box<List<T>>)
```

## Collections and views

### `Array<T, N>`

Represents fixed-size contiguous inline storage.

```text
record Header
    magic: Array<Byte, 4>
    version: UInt16
```

### `View<T>`

Represents a non-owning contiguous view over data.

```text
function checksum(data: shared View<Byte>) -> UInt32
    var total: UInt32 = 0

    for b in data
        total = total + b as UInt32

    total
```

### `DynamicArray<T>`

Represents growable owned contiguous storage.

```text
record Buffer
    data: DynamicArray<Byte>
```

### `List<T>`

Represents a recursive algebraic sequence type.

```text
choice List<T>
    Empty
    Node(head: T, tail: Box<List<T>>)
```

## Local bindings

### `let`

Introduces a fixed local binding.

```text
let label: Text = "sum"
```

### `var`

Introduces a reassignable local binding.

```text
var total = 0
total = total + 1
```

## Control flow

### `switch`

Primary branching construct for values and patterns.

Value-based example:

```text
function sign(x: Int64) -> Int64
    switch x
        0 => 0
        default => 1
```

Pattern-based example:

```text
function evaluate(expr: shared Expression) -> Int64
    switch expr
        Int(value) => value
        Add(left, right) => evaluate(left.value) + evaluate(right.value)
        Neg(inner) => -evaluate(inner.value)
```

### `default`

Fallback branch in a `switch`.

```text
switch code
    200 => "ok"
    default => "unexpected"
```

### `guard`

Checks a precondition and exits early if it fails.

```text
function parse_port(text: shared Text) -> Outcome<UInt16, ParseError>
    guard text.length() > 0 else
        return Error(ParseError.EmptyInput)

    Ok(parse_digits(text))
```

### `else`

Introduces the failure branch of a `guard`.

```text
guard file.exists() else
    return Error(IOError.Closed)
```

### `if`

Small boolean branch form.

```text
function clamp_to_zero(x: Int64) -> Int64
    if x < 0
        return 0

    x
```

### `while`

Condition-driven loop.

```text
function count_down(n: Int64) -> Unit
    var current = n

    while current > 0
        print(current)
        current = current - 1
```

### `for`

Iterates over a sequence or view.

```text
function sum(items: shared View<Int64>) -> Int64
    var total = 0

    for item in items
        total = total + item

    total
```

### `repeat`

Runs a post-test loop.

```text
function poll() -> Unit
    repeat
        step()
    while ready() == false
```

### `break`

Exits the nearest loop.

```text
function find_stop(items: shared View<Int64>) -> Unit
    for item in items
        if item == 0
            break

        print(item)
```

### `continue`

Skips to the next loop iteration.

```text
function first_even(items: shared View<Int64>) -> Maybe<Int64>
    for item in items
        if item % 2 != 0
            continue

        return Some(item)

    Empty
```

### `defer`

Schedules cleanup when the current scope exits.

```text
function use_file(path: Text) -> Outcome<Unit, IOError>
    let file = open(path)
    defer
        file.close()

    Ok(unit)
```

### `return`

Performs an early return.

```text
function clamp_to_zero(x: Int64) -> Int64
    if x < 0
        return 0

    x
```

A `Unit` function may use a bare `return`.

```text
function maybe_log(text: Text, enabled: Bool) -> Unit
    if enabled == false
        return

    print(text)
```

### Implicit final result

A non-`Unit` function returns its final expression.

```text
function add(a: Int64, b: Int64) -> Int64
    a + b
```

### Implicit `unit`

A `Unit` function may reach the end of its block without spelling `unit`.

```text
function log_message(text: Text) -> Unit
    print(text)
```

Explicit `unit` remains legal.

```text
function explicit_unit_demo() -> Unit
    side_effect()
    unit
```

## Recursion and tail forms

### Ordinary recursion

```text
function factorial(n: Int64) -> Int64
    switch n
        0 => 1
        default => n * factorial(n - 1)
```

### `recur`

Represents self-tail-recursive re-entry.

```text
function sum_list(xs: shared List<Int64>, acc: Int64) -> Int64
    switch xs
        Empty => acc
        Node(head, tail) => recur(tail.value, acc + head)
```

### `tail`

Marks a guaranteed general tail call when used in `return tail ...`.

```text
function dispatch(task: Task) -> Outcome<Int64, IOError>
    switch task
        Local(next) => return tail run(next)
        Remote(done) => done
```

## Foreign Function Interface

### `foreign "abi"`

Declares foreign imports for a specific ABI.

```text
foreign "c"
    function puts(text: Pointer<Byte>) -> Int32
    function strlen(text: Pointer<Byte>) -> UIntSize
```

### `library`

Narrows foreign imports to a specific linked library.

```text
foreign "c" library "m"
    function sin(x: Float64) -> Float64
    function cos(x: Float64) -> Float64
```

### `as` in foreign declarations

Renames an imported foreign symbol locally.

```text
foreign "c"
    function print_line(text: Pointer<Byte>) -> Int32 as "puts"
```

### `export foreign`

Exports a language-defined function to foreign callers.

```text
export foreign "c"
function ffi_add(a: Int32, b: Int32) -> Int32
    a + b
```

You may also export under a specific external symbol name.

```text
export foreign "c" as "device_init"
function initialize_device() -> Int32
    0
```

## Unsafe, raw memory, and MMIO

### `unsafe`

Enters the unsafe subset of the language.

Unsafe function:

```text
unsafe function read_byte(addr: Address) -> Byte
    let p: Pointer<Byte> = Pointer<Byte>(addr)
    raw_read(p)
```

Unsafe block:

```text
function scribble(addr: Address) -> Unit
    unsafe
        let p = Pointer<Byte>(addr)
        raw_write(p, 0xFF)
```

### `raw_read` / `raw_write`

Perform raw typed memory access.

```text
unsafe function read_word(addr: Address) -> UInt32
    let p: Pointer<UInt32> = Pointer<UInt32>(addr)
    raw_read(p)

unsafe function write_word(addr: Address, value: UInt32) -> Unit
    let p: Pointer<UInt32> = Pointer<UInt32>(addr)
    raw_write(p, value)
```

### `raw_offset`

Performs pointer arithmetic.

```text
unsafe function zero_bytes(start: Address, count: UInt64) -> Unit
    let p: Pointer<Byte> = Pointer<Byte>(start)
    var i = 0

    while i < count
        raw_write(raw_offset(p, i), 0)
        i = i + 1
```

### `address_of`

Takes the address of an existing object or field.

```text
unsafe function poke_first(buf: exclusive Buffer, value: Byte) -> Unit
    let p = address_of(buf.data[0])
    raw_write(p, value)
```

### `volatile_read` / `volatile_write`

Perform MMIO-safe volatile access.

```text
const UART0_BASE: Address    = 0x4000_1000
const UART0_DATA: Address    = UART0_BASE + 0x00
const UART0_STATUS: Address  = UART0_BASE + 0x04
const UART0_CONTROL: Address = UART0_BASE + 0x08

unsafe function uart_enable() -> Unit
    volatile_write<UInt32>(UART0_CONTROL, 0x01)

unsafe function uart_ready() -> Bool
    (volatile_read<UInt32>(UART0_STATUS) & 0x01) != 0

unsafe function uart_send(byte: Byte) -> Unit
    while uart_ready() == false
        ()

    volatile_write<UInt32>(UART0_DATA, byte as UInt32)
```

## Concurrency and tasks

### `thread`

Spawns an OS thread.

```text
function parallel_sum(data: shared View<Int64>) -> Int64
    let left = thread
        sum(data)

    let right = sum(data)
    let left_result = left.join()

    left_result + right
```

### `task`

Spawns a lightweight runtime-scheduled task.

```text
async function fetch_pair() -> Outcome<Pair<Text, Text>, IOError>
    let a = task
        fetch("https://a.example")

    let b = task
        fetch("https://b.example")

    let left = await a
    let right = await b

    Ok(Pair(left, right))
```

### `async function`

Declares a coroutine-enabled function that may suspend.

```text
async function fetch(url: Text) -> Outcome<Text, IOError>
    Ok(url)
```

### `await`

Suspends until an async result is ready.

```text
async function maybe_fetch(url: Text, enabled: Bool) -> Outcome<Text, IOError>
    if enabled == false
        return Error(IOError.Closed)

    await fetch(url)
```

### `Transferable`

Marks values safe to move across thread or task boundaries.

```text
interface Transferable
```

### `Shareable`

Marks values safe to share across thread or task boundaries.

```text
interface Shareable
```

## Summary

Orison currently has a:

- static type system
- mostly nominal type identity model
- ownership-aware access discipline
- generic abstraction model
- algebraic data-modeling core
- interface-based behavioral abstraction model
- explicit package/import and FFI surface
- explicit unsafe boundary for raw memory, pointers, and MMIO
- readable concurrency model spanning threads, tasks, and async functions

In one sentence:

> Orison is a static, mostly nominal, ownership-aware systems language with records, choices, generics, interfaces, explicit views and pointers, clear package and FFI boundaries, and a readable concurrency model.
