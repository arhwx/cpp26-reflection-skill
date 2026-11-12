// registers: a register map for a microcontroller. a layout string becomes a real bit-field struct, annotated
// functions become the interrupt vector table, and pin annotations on a board struct are checked for conflicts
#include <algorithm>
#include <array>
#include <cstdint>
#include <meta>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace reg {
using std::meta::info;

template <std::size_t N>
struct text {  // a string literal as a template argument
    char s[N]{};
    consteval text(const char (&a)[N]) { std::ranges::copy(a, s); }
};
struct irq { int number; };  // annotation on an interrupt handler
struct pin { char port; int number; bool operator==(const pin&) const = default; };  // annotation on a board member

consteval std::string num(int n) { return n < 0 ? "-" + num(-n) : (n > 9 ? num(n / 10) : "") + char('0' + n % 10); }
consteval std::string name(info r) { return std::string(identifier_of(r)); }
consteval std::string name(pin p) { return p.port + num(p.number); }

// layout text, from bit 0 upward: "EN:1 PARITY:2 _:1 BAUD:12". the name _ marks reserved bits
struct field { std::string name; int width; };
struct layout { std::vector<field> fields; std::string error; };
constexpr bool digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool id_char(char c) { return c == '_' || digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

consteval layout parse(std::string_view s) {
    layout l;
    int total = 0;
    for (auto word : s | std::views::split(' ')) {
        std::string_view f(word.begin(), word.end());
        if (f.empty()) continue;
        std::size_t colon = std::min(f.find(':'), f.size());
        std::string_view id = f.substr(0, colon), digits = f.substr(std::min(colon + 1, f.size()));
        int width = std::ranges::fold_left(digits, 0, [](int w, char c) { return digit(c) && w <= 32 ? w * 10 + c - '0' : 99; });
        if (id.empty() || digit(id[0]) || !std::ranges::all_of(id, id_char) || width < 1 || width > 32)
            return {{}, "bad field '" + std::string(f) + "'"};
        if (id != "_" && std::ranges::contains(l.fields, id, &field::name))
            return {{}, "duplicate field '" + std::string(id) + "'"};
        l.fields.push_back({std::string(id), width});
        total += width;
    }
    if (total > 32) return {{}, "layout has " + num(total) + " bits, more than 32"};
    return l;
}
consteval std::string_view check(std::string_view layout) {
    std::string e = parse(layout).error;
    return e.empty() ? "ok" : std::define_static_string(e);
}

template <text L>
struct bits_of {
    static_assert(check(L.s) == "ok", check(L.s));
    struct type;
    consteval {
        std::vector<info> specs;
        for (const field& f : parse(L.s).fields)  // reserved bits become unnamed bit-fields
            specs.push_back(f.name == "_" ? data_member_spec(^^unsigned, {.name = {}, .bit_width = f.width})
                                          : data_member_spec(^^unsigned, {.name = f.name, .bit_width = f.width}));
        define_aggregate(^^type, specs);
    }
};
template <text L>
using bits = bits_of<L>::type;

consteval std::vector<info> fields(info type) { return nonstatic_data_members_of(type, std::meta::access_context::unprivileged()); }
consteval int low_bit(info m) { return int(offset_of(m).bytes * 8 + offset_of(m).bits); }

template <class R>
constexpr std::uint32_t to_raw(const R& r) {
    std::uint32_t raw = 0;
    template for (constexpr info m : std::define_static_array(fields(^^R))) raw |= std::uint32_t(r.[:m:]) << low_bit(m);
    return raw;
}
template <class R>
consteval std::string_view describe() {  // named fields from the highest bit down, like a datasheet
    std::string out;
    for (info m : fields(^^R) | std::views::reverse) {
        int lo = low_bit(m), hi = lo + int(bit_size_of(m)) - 1;
        out += (out.empty() ? "" : " ") + name(m) + "[" + num(hi) + (hi == lo ? "" : ":" + num(lo)) + "]";
    }
    return std::define_static_string(out);
}

// interrupts: functions with an irq annotation. namespace members come in no fixed order, so sort them
consteval int irq_of(info f) { return extract<irq>(annotations_of_with_type(f, ^^irq)[0]).number; }
consteval std::vector<info> handlers(info ns) {
    std::vector<info> out;
    for (info f : members_of(ns, std::meta::access_context::current()))
        if (is_function(f) && !annotations_of_with_type(f, ^^irq).empty()) out.push_back(f);
    std::ranges::sort(out, {}, [](info f) { return std::pair(irq_of(f), identifier_of(f)); });
    return out;
}
template <info Ns, std::size_t N>
consteval std::string_view irq_check() {
    std::vector<info> hs = handlers(Ns);
    for (std::size_t i = 0; i < hs.size(); ++i) {
        int n = irq_of(hs[i]);
        if (n < 0 || n >= int(N)) return std::define_static_string("irq " + num(n) + " of " + name(hs[i]) + " is out of range");
        if (i > 0 && n == irq_of(hs[i - 1]))
            return std::define_static_string("irq " + num(n) + " used by " + name(hs[i - 1]) + " and " + name(hs[i]));
    }
    return "ok";
}
template <info Ns, std::size_t N>
constexpr std::array<void (*)(), N> vector_table(void (*fallback)()) {
    static_assert(irq_check<Ns, N>() == "ok", irq_check<Ns, N>());
    std::array<void (*)(), N> table;
    table.fill(fallback);
    template for (constexpr info f : std::define_static_array(handlers(Ns))) table[irq_of(f)] = &[:f:];
    return table;
}

// pins: annotated members in declaration order, walking into class members that have no pin of their own
struct placed { std::string path; pin at; };
consteval std::vector<placed> pins(info type, std::string prefix = "") {
    std::vector<placed> out;
    for (info m : fields(type)) {
        if (auto p = annotations_of_with_type(m, ^^pin); !p.empty()) out.push_back({prefix + name(m), extract<pin>(p[0])});
        else if (is_class_type(type_of(m))) out.append_range(pins(type_of(m), prefix + name(m) + "."));
    }
    return out;
}
template <class Board>
consteval std::string_view pin_check() {
    std::vector<placed> ps = pins(^^Board);
    for (std::size_t i = 0; i < ps.size(); ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (ps[i].at == ps[j].at)
                return std::define_static_string("pin " + name(ps[i].at) + " used by " + ps[j].path + " and " + ps[i].path);
    return "ok";
}
}  // namespace reg

// ---- user code. rust embedded: svd2rust register structs, #[interrupt] handlers, pins that can be used only once
// uart control register: bit 3 and bits 16 to 23 are reserved
using Ctrl = reg::bits<"EN:1 PARITY:2 _:1 BAUD:12 _:8 IRQ:1">;
constexpr Ctrl ctrl{.EN = 1, .PARITY = 2, .BAUD = 868, .IRQ = 1};
static_assert(sizeof(Ctrl) == 4 && reg::to_raw(ctrl) == 0x01003645);  // computed by the compiler
// reg::bits<"EN:1 EN:2"> does not compile: duplicate field 'EN'

void unhandled() { std::print(" unhandled"); }
namespace isr {
[[=reg::irq(5)]] void uart0() { std::print(" uart0"); }
[[=reg::irq(2)]] void timer() { std::print(" timer"); }
[[=reg::irq(0)]] void reset() { std::print(" reset"); }
void helper() {}  // no annotation, not a handler
}  // namespace isr
namespace clash {
[[=reg::irq(3)]] void uart() {}
[[=reg::irq(3)]] void dma() {}
}  // namespace clash
constexpr auto table = reg::vector_table<^^isr, 6>(&unhandled);  // a constant array of function pointers
// reg::vector_table<^^clash, 8>(&unhandled) does not compile: irq 3 used by dma and uart

struct Uart {
    [[=reg::pin('B', 6)]] bool tx;
    [[=reg::pin('B', 7)]] bool rx;
};
struct Board {
    Uart console;
    [[=reg::pin('C', 13)]] bool led;
    [[=reg::pin('A', 0)]] bool button;
};
struct Crowded {
    Uart console;
    [[=reg::pin('B', 7)]] bool led;  // B7 is console.rx
};
static_assert(reg::pin_check<Board>() == "ok");  // the same line for Crowded stops the build

int main() {
    std::println("ctrl {:#010x}: {}", reg::to_raw(ctrl), reg::describe<Ctrl>());
    template for (constexpr std::string_view bad : {"EN:1 EN:2", "A:20 B:13", "EN=1"})
        std::println("bits<\"{}\">: {}", bad, reg::check(bad));

    std::print("vector table:");
    for (auto handler : table) handler();
    std::println();
    std::println("isr in 4 slots: {}", reg::irq_check<^^isr, 4>());
    std::println("clash: {}", reg::irq_check<^^clash, 8>());
    std::println("crowded: {}", reg::pin_check<Crowded>());
}

// output:
// ctrl 0x01003645: IRQ[24] BAUD[15:4] PARITY[2:1] EN[0]
// bits<"EN:1 EN:2">: duplicate field 'EN'
// bits<"A:20 B:13">: layout has 33 bits, more than 32
// bits<"EN=1">: bad field 'EN=1'
// vector table: reset unhandled timer unhandled unhandled uart0
// isr in 4 slots: irq 5 of uart0 is out of range
// clash: irq 3 used by dma and uart
// crowded: pin B7 used by console.rx and led
