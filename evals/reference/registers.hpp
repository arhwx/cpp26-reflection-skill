#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace reg {

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

// annotation on a handler function
struct irq {
    int number;
};

// annotation on a board member
struct pin {
    char port;
    int number;
};

namespace detail {
using std::meta::info;

consteval std::string number(long long n) {
    if (n < 0) return "-" + number(-n);
    return (n > 9 ? number(n / 10) : "") + char('0' + n % 10);
}

struct field {
    std::string name;  // "_" for reserved bits
    int width;
};

struct layout {
    std::vector<field> fields;
    std::string error;
};

consteval bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
consteval bool is_digit(char c) { return c >= '0' && c <= '9'; }

// fields from bit 0 upward, NAME:WIDTH separated by spaces
consteval layout parse(std::string_view text) {
    layout out;
    long long total = 0;
    std::size_t pos = 0;
    while (true) {
        while (pos < text.size() && text[pos] == ' ') ++pos;
        if (pos == text.size()) break;
        std::size_t start = pos;
        while (pos < text.size() && text[pos] != ' ') ++pos;
        std::string_view tok = text.substr(start, pos - start);
        std::size_t colon = tok.find(':');
        std::string_view name = tok.substr(0, colon), digits = colon == tok.npos ? "" : tok.substr(colon + 1);
        bool good = !name.empty() && is_alpha(name[0]) && !digits.empty() &&
                    std::ranges::all_of(name, [](char c) { return is_alpha(c) || is_digit(c); }) &&
                    std::ranges::all_of(digits, is_digit);
        if (!good) {
            out.error = "bad field '" + std::string(tok) + "'";
            return out;
        }
        long long width = 0;
        for (char c : digits) width = std::min(width * 10 + (c - '0'), 1000LL);
        if (width < 1 || width > 32) {
            out.error = "width of '" + std::string(name) + "' must be 1 to 32";
            return out;
        }
        if (name != "_")
            for (const field& f : out.fields)
                if (f.name == name) {
                    out.error = "duplicate field '" + std::string(name) + "'";
                    return out;
                }
        out.fields.push_back({std::string(name), int(width)});
        total += width;
    }
    if (total > 32) out.error = "layout has " + number(total) + " bits, more than 32";
    return out;
}

template <fixed_string L>
struct builder {
    struct type;
    consteval {
        layout l = parse(L.view());
        if (l.error.empty()) {
            std::vector<info> specs;
            for (const field& f : l.fields) {
                // reserved bits become unnamed bit-fields
                if (f.name == "_") specs.push_back(std::meta::data_member_spec(^^unsigned, {.bit_width = f.width}));
                else specs.push_back(std::meta::data_member_spec(^^unsigned, {.name = f.name, .bit_width = f.width}));
            }
            std::meta::define_aggregate(^^type, specs);
        }
    }
};

consteval std::vector<info> fields_of(info r) {
    return std::meta::nonstatic_data_members_of(r, std::meta::access_context::unprivileged());
}

consteval int position(info m) {
    std::meta::member_offset o = std::meta::offset_of(m);
    return int(o.bytes * 8 + o.bits);
}

template <class A>
consteval std::vector<A> annotations(info r) {
    std::vector<A> out;
    for (info a : std::meta::annotations_of(r))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A) out.push_back(std::meta::extract<A>(a));
    return out;
}

struct handler {
    std::string name;
    int number;
};

consteval std::vector<info> handler_functions(info ns) {
    std::vector<info> out;
    for (info m : std::meta::members_of(ns, std::meta::access_context::current()))
        if (std::meta::is_function(m) && !annotations<irq>(m).empty()) out.push_back(m);
    return out;
}

consteval std::string irq_problem(info ns, std::size_t size) {
    std::vector<handler> hs;
    for (info f : handler_functions(ns)) hs.push_back({std::string(std::meta::identifier_of(f)), annotations<irq>(f)[0].number});
    std::ranges::sort(hs, {}, &handler::name);
    for (const handler& h : hs)
        if (h.number < 0 || std::size_t(h.number) >= size)
            return "irq " + number(h.number) + " of " + h.name + " is out of range";
    std::ranges::stable_sort(hs, {}, &handler::number);
    for (std::size_t i = 1; i < hs.size(); ++i)
        if (hs[i].number == hs[i - 1].number)
            return "irq " + number(hs[i].number) + " used by " + hs[i - 1].name + " and " + hs[i].name;
    return "";
}

struct placed {
    std::string path;
    pin where;
};

// annotated members in declaration order, depth first through unannotated class members
consteval void collect_pins(info type, std::string prefix, std::vector<placed>& out) {
    for (info m : fields_of(type)) {
        std::string path = prefix + std::string(std::meta::identifier_of(m));
        std::vector<pin> ps = annotations<pin>(m);
        if (!ps.empty()) out.push_back({path, ps[0]});
        else if (std::meta::is_class_type(std::meta::type_of(m))) collect_pins(std::meta::type_of(m), path + ".", out);
    }
}

consteval std::string pin_name(pin p) { return std::string(1, p.port) + number(p.number); }

}  // namespace detail

template <fixed_string L>
using bits = typename detail::builder<L>::type;

consteval std::string_view check(std::string_view layout) {
    std::string error = detail::parse(layout).error;
    return error.empty() ? "ok" : std::define_static_string(error);
}

template <class R>
constexpr std::uint32_t to_raw(const R& r) {
    std::uint32_t out = 0;
    template for (constexpr auto m : std::define_static_array(detail::fields_of(^^R))) {
        constexpr int pos = detail::position(m);
        out |= std::uint32_t(r.[:m:]) << pos;
    }
    return out;
}

template <class R>
constexpr R from_raw(std::uint32_t raw) {
    R out{};
    template for (constexpr auto m : std::define_static_array(detail::fields_of(^^R))) {
        constexpr int pos = detail::position(m);
        constexpr std::uint64_t mask = (std::uint64_t(1) << std::meta::bit_size_of(m)) - 1;
        out.[:m:] = unsigned((raw >> pos) & mask);
    }
    return out;
}

// named fields from the highest bit down
template <class R>
consteval std::string_view describe() {
    std::vector<std::meta::info> ms = detail::fields_of(^^R);
    std::string out;
    for (std::size_t i = ms.size(); i-- > 0;) {
        int lo = detail::position(ms[i]), hi = lo + int(std::meta::bit_size_of(ms[i])) - 1;
        out += (out.empty() ? "" : " ") + std::string(std::meta::identifier_of(ms[i])) + "[" + detail::number(hi) +
               (hi == lo ? "" : ":" + detail::number(lo)) + "]";
    }
    return std::define_static_string(out);
}

template <std::meta::info Ns, std::size_t N>
consteval std::string_view irq_check() {
    std::string problem = detail::irq_problem(Ns, N);
    return problem.empty() ? "ok" : std::define_static_string(problem);
}

template <std::meta::info Ns, std::size_t N>
constexpr std::array<void (*)(), N> vector_table(void (*fallback)()) {
    static_assert(irq_check<Ns, N>() == "ok", "irq table conflict");
    std::array<void (*)(), N> table;
    table.fill(fallback);
    template for (constexpr auto f : std::define_static_array(detail::handler_functions(Ns))) {
        constexpr int n = detail::annotations<irq>(f)[0].number;
        table[n] = &[:f:];
    }
    return table;
}

template <class Board>
consteval std::string_view pin_check() {
    std::vector<detail::placed> ps;
    detail::collect_pins(^^Board, "", ps);
    for (std::size_t i = 0; i < ps.size(); ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (ps[i].where.port == ps[j].where.port && ps[i].where.number == ps[j].where.number)
                return std::define_static_string("pin " + detail::pin_name(ps[i].where) + " used by " + ps[j].path + " and " +
                                                 ps[i].path);
    return "ok";
}

template <class Board>
consteval std::string_view pin_list() {
    std::vector<detail::placed> ps;
    detail::collect_pins(^^Board, "", ps);
    std::ranges::stable_sort(ps, [](const detail::placed& a, const detail::placed& b) {
        return a.where.port != b.where.port ? a.where.port < b.where.port : a.where.number < b.where.number;
    });
    std::string out;
    for (const detail::placed& p : ps) out += (out.empty() ? "" : " ") + detail::pin_name(p.where) + "=" + p.path;
    return std::define_static_string(out);
}

}  // namespace reg
