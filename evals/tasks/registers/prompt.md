On our microcontroller projects the register layouts, the interrupt vector table and the pin assignments are all maintained by hand, and they go wrong in quiet ways. I want a small embedded DSL: a register layout written as text becomes a bit-field struct, interrupt handlers are annotated functions in a namespace that become a vector table, and pin assignments are annotations on a board struct that are checked for conflicts at compile time.

Everything goes in `namespace reg`.

Registers:
- `reg::bits<"layout">`: a struct type generated from the layout text (a string literal as the only template argument). The layout lists fields from bit 0 upward, separated by one or more spaces, with optional spaces at both ends. Each field is `NAME:WIDTH`: a name made of letters, digits and `_` that does not start with a digit, and a width in decimal. The name `_` means reserved bits and may appear any number of times.
- The struct has one bit-field of type `unsigned int` with that width for every field, in layout order. Named fields are public members with the field's name; reserved fields are unnamed bit-fields of the same width, so they take up their bits but are not members. The struct is an aggregate with no other members, so it can be initialized with designated initializers. The test checks that `sizeof` is 4.
- The position of a field is the sum of the widths of all fields before it, reserved ones included.
- `template <class R> constexpr std::uint32_t reg::to_raw(const R& r)`: for every named field, its value shifted to its position, all combined with bitwise or. Reserved bits are 0.
- `template <class R> constexpr R reg::from_raw(std::uint32_t raw)`: every named field gets the bits of `raw` at its position and width. Reserved bits are ignored.
- `template <class R> consteval std::string_view reg::describe()`: the named fields from the highest position to the lowest, separated by single spaces, each written `NAME[hi:lo]` with its highest and lowest bit, or `NAME[bit]` when the width is 1. Reserved fields are not listed.
- `consteval std::string_view reg::check(std::string_view layout)`: `"ok"`, or the first error. The fields are checked one at a time from left to right, and for each field in this order: a field that does not have the form above is `bad field 'TEXT'`, with `TEXT` the whole field as written; a width outside 1 to 32 is `width of 'NAME' must be 1 to 32`; a name used by an earlier field (other than `_`) is `duplicate field 'NAME'`. When every field is fine but the widths add up to more than 32, the error is `layout has N bits, more than 32`. An empty layout is valid. `reg::bits` with an invalid layout must not compile.

Interrupts:
- `reg::irq(n)`: annotation on a function that handles interrupt number `n` (an `int`), as in `[[=reg::irq(5)]] void uart0();`. Handlers are `void()` functions.
- The handlers of a namespace are its functions with a `reg::irq` annotation. Other functions, variables and nested namespaces are ignored.
- `template <std::meta::info Ns, std::size_t N> consteval std::string_view reg::irq_check()`: checks the handlers of the namespace `Ns` for a table with `N` slots. When some handler's number is negative or at least `N`, the error is `irq N of NAME is out of range`, for the handler whose name comes first in alphabetical order (byte-wise comparison) among those. Otherwise, when two handlers have the same number, the error is `irq N used by A and B` for the smallest such number, where `A` and `B` are the two alphabetically first handler names with that number, in alphabetical order. Otherwise it returns `"ok"`. The result never depends on the order of declarations in the namespace.
- `template <std::meta::info Ns, std::size_t N> constexpr std::array<void (*)(), N> reg::vector_table(void (*fallback)())`: slot `n` holds the handler with number `n`, every other slot holds `fallback`. It is usable to initialize a `constexpr` variable, and it must not compile when `irq_check<Ns, N>()` is not `"ok"`.

Pins:
- `reg::pin(port, number)`: annotation on a data member, with a `char` port and an `int` number, as in `[[=reg::pin('A', 5)]] bool sck;`. It is written `A5`.
- The pins of a struct are found by walking its non-static data members in declaration order. A member with a `reg::pin` annotation uses that pin. A member without one whose type is a class is walked in turn, depth first, at that point. Other members are ignored. The path of a pin is the member names from the outer struct down, joined by `.`, as in `spi.sck`.
- `template <class Board> consteval std::string_view reg::pin_check()`: `"ok"`, or `pin P used by A and B` for the first pin in walk order that repeats a pin seen earlier, where `A` is the path of that earlier member and `B` the path of the repeating one.
- `template <class Board> consteval std::string_view reg::pin_list()`: every pin as `PIN=path`, separated by single spaces, sorted by port, then by number, then by walk order.

The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstdint>
#include <print>
#include <string_view>

// uart control register: bit 0 is EN, bit 3 and bits 16 to 23 are reserved
using Ctrl = reg::bits<"EN:1 PARITY:2 _:1 BAUD:12 _:8 IRQ:1">;
using Status = reg::bits<"  RXNE:1 TXE:1   _:6 ERR:3 ">;
using Word = reg::bits<"LO:16 HI:16">;

template <class R>
concept en_addressable = requires(R r) { &r.EN; };
static_assert(!en_addressable<Ctrl>);
static_assert(sizeof(Ctrl) == 4 && sizeof(Status) == 4 && sizeof(Word) == 4);
static_assert(reg::to_raw(Ctrl{.EN = 1, .IRQ = 1}) == 0x01000001);
static_assert(reg::from_raw<Word>(0x12345678).HI == 0x1234);

int ticks = 0;
void unhandled() { std::println("  unhandled"); }

namespace isr {
[[=reg::irq(5)]] void uart0() { std::println("  uart0"); }
[[=reg::irq(2)]] void timer() { std::println("  timer tick {}", ++ticks); }
[[=reg::irq(0)]] void reset() { std::println("  reset"); }
void helper() {}
int counter = 0;
}  // namespace isr

namespace clash {
[[=reg::irq(3)]] void uart() {}
[[=reg::irq(7)]] void spi() {}
[[=reg::irq(3)]] void dma() {}
[[=reg::irq(7)]] void adc() {}
}  // namespace clash

namespace far {
[[=reg::irq(1)]] void ok() {}
[[=reg::irq(-1)]] void nmi() {}
[[=reg::irq(12)]] void dog() {}
}  // namespace far

struct Spi {
    [[=reg::pin('A', 5)]] bool sck;
    [[=reg::pin('A', 6)]] bool miso;
    [[=reg::pin('A', 7)]] bool mosi;
};
struct Uart {
    [[=reg::pin('B', 6)]] bool tx;
    [[=reg::pin('B', 7)]] bool rx;
    int baud;
};
struct Board {
    Spi spi;
    [[=reg::pin('C', 13)]] bool led;
    Uart uart;
    [[=reg::pin('A', 0)]] bool button;
    [[=reg::pin('B', 10)]] Uart debug;
};
struct Crowded {
    Spi spi;
    Uart console;
    [[=reg::pin('B', 7)]] bool led;
    [[=reg::pin('A', 6)]] bool buzzer;
};
struct Twice {
    Uart a;
    Uart b;
};

constexpr auto table = reg::vector_table<^^isr, 6>(&unhandled);

int main() {
    // a value too wide for a bit-field keeps its low bits
    unsigned baud = 5000;
    Ctrl c{};
    c.EN = 1;
    c.PARITY = 2;
    c.BAUD = baud;
    c.IRQ = 1;
    std::println("ctrl raw {:#010x} baud {}", reg::to_raw(c), +c.BAUD);
    Ctrl all = reg::from_raw<Ctrl>(0xFFFFFFFF);
    std::println("all ones: EN={} PARITY={} BAUD={} IRQ={} raw {:#010x}", +all.EN, +all.PARITY, +all.BAUD, +all.IRQ,
                 reg::to_raw(all));
    Status s = reg::from_raw<Status>(0x00000502);
    std::println("status RXNE={} TXE={} ERR={} raw {:#x}", +s.RXNE, +s.TXE, +s.ERR, reg::to_raw(s));
    std::println("word {:#x}", reg::to_raw(reg::from_raw<Word>(0xDEADBEEF)));

    std::println("{}", reg::describe<Ctrl>());
    std::println("{}", reg::describe<Status>());
    std::println("{}", reg::describe<Word>());

    constexpr std::string_view checks[] = {
        reg::check("EN:1 PARITY:2 _:1 BAUD:12"), reg::check("_:4 _:4 X:1"), reg::check("EN:1 EN:2"),
        reg::check("A:20 B:13"),                 reg::check("EN:0"),        reg::check("EN:33"),
        reg::check("EN=1"),                      reg::check("1X:1"),        reg::check("A:1 B:"),
    };
    for (std::string_view e : checks) std::println("check: {}", e);

    for (std::size_t i = 0; i < table.size(); ++i) {
        std::println("irq {}", i);
        table[i]();
    }
    table[2]();
    std::println("irq isr: {}", reg::irq_check<^^isr, 6>());
    std::println("irq isr in 4 slots: {}", reg::irq_check<^^isr, 4>());
    std::println("irq clash: {}", reg::irq_check<^^clash, 8>());
    std::println("irq far: {}", reg::irq_check<^^far, 16>());
    std::println("irq far in 8 slots: {}", reg::irq_check<^^far, 8>());

    std::println("pins: {}", reg::pin_list<Board>());
    std::println("board: {}", reg::pin_check<Board>());
    std::println("crowded: {}", reg::pin_check<Crowded>());
    std::println("twice: {}", reg::pin_check<Twice>());
}
```

It must print exactly:

```
ctrl raw 0x01003885 baud 904
all ones: EN=1 PARITY=3 BAUD=4095 IRQ=1 raw 0x0100fff7
status RXNE=0 TXE=1 ERR=5 raw 0x502
word 0xdeadbeef
IRQ[24] BAUD[15:4] PARITY[2:1] EN[0]
ERR[10:8] TXE[1] RXNE[0]
HI[31:16] LO[15:0]
check: ok
check: ok
check: duplicate field 'EN'
check: layout has 33 bits, more than 32
check: width of 'EN' must be 1 to 32
check: width of 'EN' must be 1 to 32
check: bad field 'EN=1'
check: bad field '1X:1'
check: bad field 'B:'
irq 0
  reset
irq 1
  unhandled
irq 2
  timer tick 1
irq 3
  unhandled
irq 4
  unhandled
irq 5
  uart0
  timer tick 2
irq isr: ok
irq isr in 4 slots: irq 5 of uart0 is out of range
irq clash: irq 3 used by dma and uart
irq far: irq -1 of nmi is out of range
irq far in 8 slots: irq 12 of dog is out of range
pins: A0=button A5=spi.sck A6=spi.miso A7=spi.mosi B6=uart.tx B7=uart.rx B10=debug C13=led
board: ok
crowded: pin B7 used by console.rx and led
twice: pin B6 used by a.tx and b.tx
```

Write the complete header to `solution.hpp` in the current directory.
