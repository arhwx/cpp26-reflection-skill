I want string-to-enum and string-to-member lookups backed by a perfect hash that is built at compile time from the names themselves: one hash of the input picks a slot, and at most one name comparison decides the result. No name lists written by hand, no linear search, no `std::map` or `std::unordered_map`.

Keys: for an enumeration type `K`, the keys are the names of its enumerators. For a class type `K`, the keys are the names of its public non-static data members; private and protected members are not keys.

API, all in `namespace ph`:
- `template <class K> constexpr std::size_t table_size`: a variable template, the number of slots of the table for `K`. It is at most twice the number of keys of `K`, so it is 0 when `K` has no keys.
- `template <class K> constexpr std::size_t slot_of(std::string_view s)`: the slot that `s` hashes to. For every key of `K` the slot is less than `table_size<K>`, and two different keys never share a slot. The test only calls it with keys of `K`.
- `template <class E> constexpr std::optional<E> enum_from_string(std::string_view s)`: for an enumeration `E`, the enumerator whose name is exactly `s`, comparing the whole string including its length and any embedded NUL characters, case-sensitively; `std::nullopt` when there is none. Two enumerators may have the same value; each name still finds its own enumerator.
- `template <class T, class F> bool visit_field(T& obj, std::string_view name, F&& f)`: for a class type `T` (possibly const), if `name` is a key of `T`, calls `f` once with that member of `obj` as an lvalue (const when `obj` is const) and returns `true`; otherwise returns `false` without calling `f`.

The lookups in `enum_from_string` and `visit_field` hash the input once to find its slot and compare it with at most the one key in that slot. `table_size`, `slot_of` and `enum_from_string` must be usable in constant expressions, and the hash function itself is up to you: the test checks only the properties above, never particular slot numbers.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <array>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

enum class Color { red, green, blue };

// atomic numbers up to xenon
enum class Element {
    H = 1, He, Li, Be, B, C, N, O, F, Ne, Na, Mg, Al, Si, P, S, Cl, Ar,
    K, Ca, Sc, Ti, V, Cr, Mn, Fe, Co, Ni, Cu, Zn, Ga, Ge, As, Se, Br, Kr,
    Rb, Sr, Y, Zr, Nb, Mo, Tc, Ru, Rh, Pd, Ag, Cd, In, Sn, Sb, Te, I, Xe,
};
constexpr std::array<std::string_view, 54> element_names = {
    "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne", "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar",
    "K", "Ca", "Sc", "Ti", "V", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Ga", "Ge", "As", "Se", "Br", "Kr",
    "Rb", "Sr", "Y", "Zr", "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn", "Sb", "Te", "I", "Xe",
};

// same letters, different order
enum class Anagram { ab, ba, aab, aba, baa, abc, acb, bac, bca, cab, cba };
constexpr std::array<std::string_view, 11> anagram_names = {"ab", "ba", "aab", "aba", "baa", "abc", "acb", "bac", "bca", "cab", "cba"};

enum class Status { ok = 0, success = 0, failed = 2 };
enum class Single { only };
enum class Nothing {};

struct Config {
    int port = 8080;
    std::string host = "localhost";
    double ratio = 0.5;
    bool verbose = false;
    int verbosity = 1;

private:
    int secret_ = 42;
};

static_assert(ph::enum_from_string<Color>("green") == Color::green);
static_assert(!ph::enum_from_string<Color>("Green"));
static_assert(ph::enum_from_string<Element>("Kr") == Element::Kr);
static_assert(ph::slot_of<Element>("Xe") < ph::table_size<Element>);

template <class K, std::size_t N>
void check_slots(const char* what, const std::array<std::string_view, N>& keys) {
    std::vector<bool> used(ph::table_size<K>);
    bool distinct = true;
    for (std::string_view k : keys) {
        std::size_t s = ph::slot_of<K>(k);
        if (s >= used.size() || used[s]) distinct = false;
        else used[s] = true;
    }
    std::println("{}: {} keys, table at most twice the keys: {}, distinct slots: {}", what, N,
                 ph::table_size<K> <= 2 * N, distinct);
}

template <class E>
void find(std::string_view s) {
    if (auto e = ph::enum_from_string<E>(s)) std::println("\"{}\" -> {}", s, static_cast<int>(*e));
    else std::println("\"{}\" -> none", s);
}

void show_field(const Config& c, std::string_view name) {
    bool found = ph::visit_field(c, name, [&](const auto& v) { std::println("\"{}\" = {}", name, v); });
    if (!found) std::println("\"{}\": no such field", name);
}

int main() {
    check_slots<Element>("Element", element_names);
    check_slots<Anagram>("Anagram", anagram_names);
    check_slots<Color>("Color", std::array<std::string_view, 3>{"red", "green", "blue"});
    check_slots<Config>("Config", std::array<std::string_view, 5>{"port", "host", "ratio", "verbose", "verbosity"});
    std::println("Nothing table size: {}", ph::table_size<Nothing>);

    int sum = 0;
    bool in_order = true;
    for (std::size_t i = 0; i < element_names.size(); ++i) {
        auto e = ph::enum_from_string<Element>(element_names[i]);
        in_order = in_order && e && static_cast<std::size_t>(*e) == i + 1;
        sum += e ? static_cast<int>(*e) : 0;
    }
    std::println("all elements found in order: {}, sum {}", in_order, sum);
    bool anagrams = true;
    for (std::size_t i = 0; i < anagram_names.size(); ++i)
        anagrams = anagrams && ph::enum_from_string<Anagram>(anagram_names[i]) == static_cast<Anagram>(i);
    std::println("all anagrams found: {}", anagrams);

    for (std::string_view s : {"Fe", "I", "In", "Xe", "", "h", "HE", "Xe ", "Uuo", "Hee"})
        find<Element>(s);
    std::println("Fe with a trailing NUL found: {}", ph::enum_from_string<Element>(std::string_view("Fe\0", 3)).has_value());
    find<Anagram>("abb");
    find<Anagram>("cba");
    find<Status>("success");
    find<Status>("ok");
    find<Status>("failed");
    find<Status>("fail");
    find<Single>("only");
    find<Single>("");
    find<Nothing>("anything");
    find<Nothing>("");

    Config cfg;
    for (std::string_view name : {"port", "host", "ratio", "verbose", "verbosity", "secret_", "Port", "verb", ""})
        show_field(cfg, name);
    bool set = ph::visit_field(cfg, "port", [](auto& v) {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(v)>, int>) v = 9090;
    });
    std::println("set port: {}, port is now {}", set, cfg.port);
}
```

It must print exactly:

```
Element: 54 keys, table at most twice the keys: true, distinct slots: true
Anagram: 11 keys, table at most twice the keys: true, distinct slots: true
Color: 3 keys, table at most twice the keys: true, distinct slots: true
Config: 5 keys, table at most twice the keys: true, distinct slots: true
Nothing table size: 0
all elements found in order: true, sum 1485
all anagrams found: true
"Fe" -> 26
"I" -> 53
"In" -> 49
"Xe" -> 54
"" -> none
"h" -> none
"HE" -> none
"Xe " -> none
"Uuo" -> none
"Hee" -> none
Fe with a trailing NUL found: false
"abb" -> none
"cba" -> 10
"success" -> 0
"ok" -> 0
"failed" -> 2
"fail" -> none
"only" -> 0
"" -> none
"anything" -> none
"" -> none
"port" = 8080
"host" = localhost
"ratio" = 0.5
"verbose" = false
"verbosity" = 1
"secret_": no such field
"Port": no such field
"verb": no such field
"": no such field
set port: true, port is now 9090
```

Write the complete header to `solution.hpp` in the current directory.
