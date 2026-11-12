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
