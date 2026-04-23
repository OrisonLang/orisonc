# ORISON_TOUR.md

```text
# ------------------------------------------------------------
# package and imports
# ------------------------------------------------------------

package drivers.uart

import
    console from io
    Logger as Log from diagnostics.logger
    random_int from math.random
    Reader from io.streams
    UartRegisters from devices.uart.types

# ------------------------------------------------------------
# type aliases
# ------------------------------------------------------------

type UserId = UInt64
type Port = UInt16

# ------------------------------------------------------------
# records
# ------------------------------------------------------------

record User
    id: UserId
    name: Text
    age: UInt8

record Box<T>
    value: T

record Pair<A, B>
    first: A
    second: B

# growable owned contiguous storage
record Buffer
    data: DynamicArray<Byte>

record BufferReader
    data: DynamicArray<Byte>
    cursor: IntSize

# fixed-size contiguous inline storage
record Header
    magic: Array<Byte, 4>
    version: UInt16

# typed MMIO register block
record UartRegisters
    data: UInt32
    status: UInt32
    control: UInt32

# ------------------------------------------------------------
# sum types / tagged unions
# ------------------------------------------------------------

choice IOError
    Closed
    EndOfInput
    PermissionDenied

choice ParseError
    EmptyInput
    InvalidDigit

choice Expression
    Int(value: Int64)
    Add(left: Box<Expression>, right: Box<Expression>)
    Neg(inner: Box<Expression>)

# recursive algebraic data type
choice List<T>
    Empty
    Node(head: T, tail: Box<List<T>>)

choice Task
    Local(next: Int64)
    Remote(done: Outcome<Int64, IOError>)

# ------------------------------------------------------------
# interfaces
# ------------------------------------------------------------

interface Reader
    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<IntSize, IOError>

interface Display
    function display(this: shared This) -> Text

interface Iterator<T>
    function next(this: exclusive This) -> Maybe<T>

# concurrency safety marker interfaces
interface Transferable
interface Shareable

# ------------------------------------------------------------
# interface implementations
# ------------------------------------------------------------

implements Reader for BufferReader
    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<IntSize, IOError>
        switch this.cursor >= this.data.length()
            true => Error(IOError.EndOfInput)
            false => Ok(0)

implements Display for User
    function display(this: shared This) -> Text
        this.name

# ------------------------------------------------------------
# inherent methods
# ------------------------------------------------------------

extend Buffer
    function append(this: exclusive This, b: Byte) -> Unit
        this.data.push(b)

    function length(this: shared This) -> IntSize
        this.data.length()

    function first(this: shared This) -> Maybe<Byte>
        switch this.data.length()
            0 => Empty
            default => Some(this.data[0])

extend BufferReader
    function reset(this: exclusive This) -> Unit
        this.cursor = 0

    function remaining(this: shared This) -> IntSize
        this.data.length() - this.cursor

# ------------------------------------------------------------
# plain functions
# ------------------------------------------------------------

function add(a: Int64, b: Int64) -> Int64
    a + b

function consume(buf: Buffer) -> Unit
    sink(buf)

function first<T>(items: shared View<T>) -> Maybe<shared T>
    switch items.length()
        0 => Empty
        default => Some(items[0])

# ------------------------------------------------------------
# generic functions with constraints
# ------------------------------------------------------------

function max<T>(left: T, right: T) -> T
where T: Ordered
    switch left >= right
        true => left
        false => right

function sort_copy<T>(items: shared View<T>) -> DynamicArray<T>
where T: Ordered + Cloneable
    clone_and_sort(items)

function fill<R>(source: exclusive R, into: exclusive View<Byte>) -> Outcome<IntSize, IOError>
where R: Reader
    source.read(into)

# ------------------------------------------------------------
# local bindings
# ------------------------------------------------------------

function sum(items: shared View<Int64>) -> Int64
    let label: Text = "sum"
    var total = 0

    for item in items
        total = total + item

    total

# ------------------------------------------------------------
# control flow
# ------------------------------------------------------------

# guard with early return
function parse_port(text: shared Text) -> Outcome<Port, ParseError>
    guard text.length() > 0 else
        return Error(ParseError.EmptyInput)

    Ok(parse_digits(text))

# small boolean branch
function clamp_to_zero(x: Int64) -> Int64
    if x < 0
        return 0
    else
        return x

# switch over values
function sign(x: Int64) -> Int64
    switch x
        0 => 0
        default => 1

# switch over patterns
function evaluate(expr: shared Expression) -> Int64
    switch expr
        Int(value) => value
        Add(left, right) => evaluate(left.value) + evaluate(right.value)
        Neg(inner) => -evaluate(inner.value)

# while loop
function count_down(n: Int64) -> Unit
    var current = n

    while current > 0
        console.print(current)
        current = current - 1

# for loop, continue, early return
function first_even(items: shared View<Int64>) -> Maybe<Int64>
    for item in items
        if item % 2 != 0
            continue

        return Some(item)

    Empty

# repeat loop
function poll() -> Unit
    repeat
        step()
    while ready() == false

# defer
function use_file(path: Text) -> Outcome<Unit, IOError>
    let file = open(path)
    defer
        file.close()

    Ok(unit)

# ------------------------------------------------------------
# implicit Unit returns and naked return
# ------------------------------------------------------------

# implicit Unit at end of block
function log_message(text: Text) -> Unit
    console.print(text)

# naked return is allowed in Unit functions
function maybe_log(text: Text, enabled: Bool) -> Unit
    if enabled == false
        return

    console.print(text)

# explicit unit is still legal, though usually unnecessary
function explicit_unit_demo() -> Unit
    side_effect()
    unit

# ------------------------------------------------------------
# recursion and tail forms
# ------------------------------------------------------------

# ordinary recursion
function factorial(n: Int64) -> Int64
    switch n
        0 => 1
        default => n * factorial(n - 1)

# self tail recursion
function sum_list(xs: shared List<Int64>, acc: Int64) -> Int64
    switch xs
        Empty => acc
        Node(head, tail) => recur(tail.value, acc + head)

# general tail call
function dispatch(task: Task) -> Outcome<Int64, IOError>
    switch task
        Local(next) => return tail run(next)
        Remote(done) => done

# ------------------------------------------------------------
# built-in Maybe and Outcome constructors
# ------------------------------------------------------------

function demo_constructors() -> Outcome<Maybe<Int64>, IOError>
    Ok(Some(42))

# ------------------------------------------------------------
# arrays, views, and dynamic arrays
# ------------------------------------------------------------

# fixed-size inline array
function make_magic() -> Array<Byte, 4>
    [0x7F, 0x45, 0x4C, 0x46]

# borrowed view over contiguous data
function checksum(data: shared View<Byte>) -> UInt32
    var total: UInt32 = 0

    for b in data
        total = total + b as UInt32

    total

# growable owned contiguous storage
function collect_bytes() -> DynamicArray<Byte>
    let bytes = DynamicArray<Byte>()
    bytes.push(0x41)
    bytes.push(0x42)
    bytes.push(0x43)
    bytes

# ------------------------------------------------------------
# foreign function interface
# ------------------------------------------------------------

# import foreign symbols with an explicit ABI
foreign "c"
    function puts(text: Pointer<Byte>) -> Int32
    function strlen(text: Pointer<Byte>) -> UIntSize

# import foreign symbols from a specific library
foreign "c" library "m"
    function sin(x: Float64) -> Float64
    function cos(x: Float64) -> Float64

# rename a foreign symbol locally
foreign "c"
    function print_line(text: Pointer<Byte>) -> Int32 as "puts"

# export a language function to foreign callers
export foreign "c"
function ffi_add(a: Int32, b: Int32) -> Int32
    a + b

# export with an explicit external symbol name
export foreign "c" as "device_init"
function initialize_device() -> Int32
    0

# ------------------------------------------------------------
# unsafe and raw memory
# ------------------------------------------------------------

# first-class raw addresses
const SRAM_BASE: Address = 0x2000_0000
const SRAM_END: Address  = 0x2001_0000

# raw pointer construction from an address
unsafe function read_byte(addr: Address) -> Byte
    let p: Pointer<Byte> = Pointer<Byte>(addr)
    raw_read(p)

unsafe function write_byte(addr: Address, value: Byte) -> Unit
    let p: Pointer<Byte> = Pointer<Byte>(addr)
    raw_write(p, value)

# typed raw memory access
unsafe function read_word(addr: Address) -> UInt32
    let p: Pointer<UInt32> = Pointer<UInt32>(addr)
    raw_read(p)

unsafe function write_word(addr: Address, value: UInt32) -> Unit
    let p: Pointer<UInt32> = Pointer<UInt32>(addr)
    raw_write(p, value)

# pointer arithmetic
unsafe function zero_bytes(start: Address, count: UInt64) -> Unit
    let p: Pointer<Byte> = Pointer<Byte>(start)
    var i = 0

    while i < count
        raw_write(raw_offset(p, i), 0)
        i = i + 1

# explicit unsafe block
function scribble(addr: Address) -> Unit
    unsafe
        let p = Pointer<Byte>(addr)
        raw_write(p, 0xFF)

# address_of for existing storage
unsafe function poke_first(buf: exclusive Buffer, value: Byte) -> Unit
    let p = address_of(buf.data[0])
    raw_write(p, value)

# ------------------------------------------------------------
# embedded / MMIO
# ------------------------------------------------------------

# memory-mapped register addresses
const UART0_BASE: Address    = 0x4000_1000
const UART0_DATA: Address    = UART0_BASE + 0x00
const UART0_STATUS: Address  = UART0_BASE + 0x04
const UART0_CONTROL: Address = UART0_BASE + 0x08

const GPIO_BASE: Address = 0x4800_0000
const GPIO_OUT: Address  = GPIO_BASE + 0x14
const GPIO_IN: Address   = GPIO_BASE + 0x10

# MMIO should use volatile operations
unsafe function uart_enable() -> Unit
    volatile_write<UInt32>(UART0_CONTROL, 0x01)

unsafe function uart_ready() -> Bool
    (volatile_read<UInt32>(UART0_STATUS) & 0x01) != 0

unsafe function uart_send(byte: Byte) -> Unit
    while uart_ready() == false
        ()

    volatile_write<UInt32>(UART0_DATA, byte as UInt32)

unsafe function gpio_set(mask: UInt32) -> Unit
    volatile_write<UInt32>(GPIO_OUT, mask)

unsafe function gpio_read() -> UInt32
    volatile_read<UInt32>(GPIO_IN)

# typed register block view
const UART0: Pointer<UartRegisters> = Pointer<UartRegisters>(UART0_BASE)

unsafe function uart_status() -> UInt32
    volatile_read(address_of(UART0.status))

unsafe function uart_enable_via_block() -> Unit
    volatile_write(address_of(UART0.control), 0x01)

# ------------------------------------------------------------
# native threads
# ------------------------------------------------------------

# spawn an OS thread and join it later
function parallel_sum(data: shared View<Int64>) -> Int64
    let left = thread
        sum(data)

    let right = sum(data)
    let left_result = left.join()

    left_result + right

# moving owned data into a thread requires concurrency-safe transfer
function launch_processing(buffer: Buffer) -> Unit
    let worker = thread
        process(buffer)

    let result = worker.join()
    consume(result)

# ------------------------------------------------------------
# async tasks and coroutines
# ------------------------------------------------------------

# async functions may suspend
async function fetch(url: Text) -> Outcome<Text, IOError>
    Ok(url)

# lightweight runtime-scheduled tasks
async function fetch_pair() -> Outcome<Pair<Text, Text>, IOError>
    let a = task
        fetch("https://a.example")

    let b = task
        fetch("https://b.example")

    let left = await a
    let right = await b

    Ok(Pair(left, right))

# async code with an early return
async function maybe_fetch(url: Text, enabled: Bool) -> Outcome<Text, IOError>
    if enabled == false
        return Error(IOError.Closed)

    await fetch(url)
```
