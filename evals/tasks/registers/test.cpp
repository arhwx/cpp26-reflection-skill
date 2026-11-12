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
