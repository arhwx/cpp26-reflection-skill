// type_counter: compile-time counters and type lists that grow as the compiler reads the file. each change
// completes a class that later code can see. standard and well-formed, with no friend injection tricks
#include <array>
#include <cctype>
#include <cstddef>
#include <meta>
#include <print>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace ct {
using std::meta::info;

// the state is a row of classes that start out incomplete. bump completes tick<Tag, count>, add completes
// item<Tag, size> with one member that points to the new type. a complete class stays complete, so a query
// always answers for the code above it, in the same way every time
template <class Tag, std::size_t N>
struct tick;
template <class Tag, std::size_t N>
struct item;

consteval info slot(info tmpl, info tag, std::size_t n) { return substitute(tmpl, {tag, std::meta::reflect_constant(n)}); }
consteval std::size_t filled(info tmpl, info tag) {  // the number of complete slots at this point of the file
    std::size_t n = 0;
    while (is_complete_type(slot(tmpl, tag, n))) ++n;
    return n;
}

consteval std::size_t count(info tag) { return filled(^^tick, tag); }
consteval void bump(info tag) { define_aggregate(slot(^^tick, tag, count(tag)), {}); }

consteval std::vector<info> types(info tag) {
    std::vector<info> out;
    for (std::size_t i = 0; i < filled(^^item, tag); ++i) {
        auto members = nonstatic_data_members_of(slot(^^item, tag, i), std::meta::access_context::unprivileged());
        out.push_back(remove_pointer(type_of(members[0])));
    }
    return out;
}
consteval int index_of(info tag, info type) {
    std::vector<info> list = types(tag);
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i] == type) return int(i);
    return -1;
}
consteval void add(info tag, info type) {  // a type joins a list once
    if (index_of(tag, type) == -1)
        define_aggregate(slot(^^item, tag, types(tag).size()), {data_member_spec(add_pointer(type), {.name = "held"})});
}
}  // namespace ct

// ---- user code. c: __COUNTER__, rust: inventory::submit!
struct Counter;  // a tag is any class name. it is never defined

static_assert(ct::count(^^Counter) == 0);
consteval { ct::bump(^^Counter); }
static_assert(ct::count(^^Counter) == 1);  // the same expression, a new value
consteval {
    ct::bump(^^Counter);
    ct::bump(^^Counter);
}
std::array<int, ct::count(^^Counter)> buffer{};  // usable anywhere a constant is
// constexpr int n = (ct::bump(^^Counter), 1);  // does not compile: state only changes in a consteval block

// each filter joins the list right after its definition. nobody keeps a central list up to date
struct Filters;

struct Trim {
    static std::string apply(std::string s) {
        std::size_t b = s.find_first_not_of(' '), e = s.find_last_not_of(' ');
        return b == std::string::npos ? "" : s.substr(b, e - b + 1);
    }
};
consteval { ct::add(^^Filters, ^^Trim); }
constexpr std::size_t filters_early = ct::types(^^Filters).size();  // sees only what is above it

struct Upper {
    static std::string apply(std::string s) {
        for (char& c : s) c = char(std::toupper(c));
        return s;
    }
};
struct Exclaim {
    static std::string apply(std::string s) { return s + "!"; }
};
consteval {
    ct::add(^^Filters, ^^Upper);
    ct::add(^^Filters, ^^Exclaim);
    ct::add(^^Filters, ^^Trim);  // already in the list, so nothing happens
}
static_assert(ct::index_of(^^Filters, ^^Exclaim) == 2);  // a stable id for each type

// the list is a real type list, ready for substitute
using AllFilters = [:substitute(^^std::tuple, ct::types(^^Filters)):];
static_assert(std::is_same_v<AllFilters, std::tuple<Trim, Upper, Exclaim>>);

int main() {
    std::println("counter: {}, buffer size: {}", ct::count(^^Counter), buffer.size());
    constexpr std::size_t filters_now = ct::types(^^Filters).size();
    std::println("filters right after Trim: {}, filters now: {}", filters_early, filters_now);

    std::string text = "  hello, world  ";
    template for (constexpr std::meta::info f : std::define_static_array(ct::types(^^Filters))) {
        text = [:f:]::apply(text);
        std::println("{} {:<7} '{}'", ct::index_of(^^Filters, f), identifier_of(f), text);
    }
}

// output:
// counter: 3, buffer size: 3
// filters right after Trim: 1, filters now: 3
// 0 Trim    'hello, world'
// 1 Upper   'HELLO, WORLD'
// 2 Exclaim 'HELLO, WORLD!'
