// meta_pipe: type transformations written like shell pipes, as in "int& | remove_reference | add_const | add_pointer"
// no word is defined here: each one is a function of std::meta, found by reflecting on std::meta itself
#include <algorithm>
#include <format>
#include <meta>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace metapipe {
using std::meta::info;

template <std::size_t N>
struct text {  // lets a string literal be a template argument
    char s[N]{};
    consteval text(const char (&v)[N]) { std::ranges::copy(v, s); }
    constexpr std::string_view view() const { return {s, N - 1}; }
};

// the words: every function in namespace std::meta that takes one reflection and returns one
consteval bool is_word(info f) {
    if (!is_function(f) || !has_identifier(f) || parameters_of(f).size() != 1) return false;
    return is_same_type(type_of(parameters_of(f)[0]), ^^std::meta::info) && is_same_type(return_type_of(f), ^^std::meta::info);
}
inline constexpr auto words = std::define_static_array(
    members_of(^^std::meta, std::meta::access_context::unprivileged()) | std::views::filter(is_word));

consteval std::optional<info> find(std::string_view name) {
    for (info w : words)
        if (identifier_of(w) == name) return w;
    return {};
}

// calls the word w on x; empty when std::meta rejects x
consteval std::optional<info> call(info w, info x) {
    template for (constexpr info f : words) {
        if (f == w) {
            try {
                return dealias([:f:](x));
            } catch (const std::meta::exception&) {
                return {};
            }
        }
    }
    return {};
}

consteval std::vector<std::string_view> split(std::string_view s) {  // words separated by spaces or '|'
    std::vector<std::string_view> out;
    while (!s.empty()) {
        std::size_t n = std::min(s.find_first_of(" |"), s.size());
        if (n) out.push_back(s.substr(0, n));
        s.remove_prefix(std::min(n + 1, s.size()));
    }
    return out;
}

// runs the pipeline on x; returns an error message, or "" with the result left in x
consteval std::string run(std::string_view pipeline, info& x) {
    for (std::string_view name : split(pipeline)) {
        std::optional<info> w = find(name);
        if (!w) return "unknown word '" + std::string(name) + "'";
        std::optional<info> next = call(*w, x);
        if (!next) return "'" + std::string(name) + "' rejects " + std::string(display_string_of(x));
        x = *next;
    }
    return "";
}
consteval std::string_view check(info x, std::string_view pipeline) { return std::define_static_string(run(pipeline, x)); }
consteval info apply(info x, std::string_view pipeline) {
    run(pipeline, x);
    return x;
}

template <class T, text P>
    requires(check(^^T, P.view()).empty())
using piped = [:apply(^^T, P.view()):];

// the shortest pipeline from one reflection to another, using every word. breadth first, so pipelines of the
// same length are tried in vocabulary order, and a type already reached by a shorter or earlier pipeline is skipped
consteval std::string_view search(info from, info to, std::size_t max_steps) {
    if (dealias(from) == dealias(to)) return "";
    struct node { info x; std::string path; };
    std::vector<node> level{{dealias(from), ""}};
    std::vector<info> seen{dealias(from)};
    for (std::size_t n = 0; n < max_steps; ++n) {
        std::vector<node> next;
        for (const node& at : level) {
            for (info w : words) {
                std::optional<info> y = call(w, at.x);
                if (!y || std::ranges::contains(seen, *y)) continue;
                std::string path = at.path + (n ? " | " : "") + std::string(identifier_of(w));
                if (*y == dealias(to)) return std::define_static_string(path);
                seen.push_back(*y);
                next.push_back({*y, path});
            }
        }
        level = std::move(next);
    }
    return "no pipeline";
}
}  // namespace metapipe

// ---- user code. no analogue: the words of this little language are the functions of std::meta itself
enum class Color : unsigned char { red, green };
using Grid = const int[2][3];
using Callback = void(int);

static_assert(std::is_same_v<metapipe::piped<int&, "remove_reference | add_const | add_pointer">, const int*>);

template <class T, metapipe::text P>
concept compiles = requires { typename metapipe::piped<T, P>; };
static_assert(!compiles<int, "add_pointr">);       // not a word
static_assert(!compiles<int, "is_const">);         // in std::meta, but returns bool
static_assert(!compiles<int, "underlying_type">);  // int is not an enum

template <std::meta::info X, metapipe::text P>
void show(std::string_view name = display_string_of(dealias(X))) {  // prints "X | P -> result", or the error
    constexpr std::string_view error = metapipe::check(X, P.view());
    std::string line = std::format("{} | {}", name, P.view());
    if constexpr (error.empty()) std::println("{:<50} -> {}", line, display_string_of(metapipe::apply(X, P.view())));
    else std::println("{:<50} -> error: {}", line, error);
}

int main() {
    std::println("words found in std::meta:");
    int i = 0;
    template for (constexpr std::meta::info w : metapipe::words) std::print("{}{}", identifier_of(w), ++i % 7 ? " " : "\n");

    std::println();
    show<^^int&, "remove_reference | add_const | add_pointer">();
    show<^^Grid, "decay">();
    show<^^Callback, "decay">();
    show<^^Color, "underlying_type | make_signed | add_const">();
    show<^^std::meta::decay, "type_of">("std::meta::decay");  // the words work on the words too
    show<^^std::meta::decay, "parent_of">("std::meta::decay");
    show<^^int, "add_pointr">();
    show<^^int, "add_const | is_const">();
    show<^^const double, "remove_const | make_signed">();

    metapipe::piped<const int&, "remove_cvref | make_unsigned"> big = 4'000'000'000u;  // a real type
    std::println("\nconst int& | remove_cvref | make_unsigned holds {}", big);

    std::println("\nshortest pipelines, searched among all the words:");
    std::println("const int& to int*: {}", metapipe::search(^^const int&, ^^int*, 3));
    std::println("Color& to unsigned char: {}", metapipe::search(^^Color&, ^^unsigned char, 3));
    std::println("int to const int* const*: {}", metapipe::search(^^int, ^^const int* const*, 4));
    std::println("int to long: {}", metapipe::search(^^int, ^^long, 3));
    std::println("std::meta::decay to std: {}", metapipe::search(^^std::meta::decay, ^^std, 3));
}

// output:
// words found in std::meta:
// type_of object_of constant_of parent_of dealias template_of variable_of
// return_type_of remove_const remove_volatile remove_cv add_const add_volatile add_cv
// remove_reference add_lvalue_reference add_rvalue_reference make_signed make_unsigned remove_extent remove_all_extents
// remove_pointer add_pointer remove_cvref decay underlying_type unwrap_reference unwrap_ref_decay
//
// int& | remove_reference | add_const | add_pointer  -> const int*
// const int [2][3] | decay                           -> const int (*)[3]
// void(int) | decay                                  -> void (*)(int)
// Color | underlying_type | make_signed | add_const  -> const signed char
// std::meta::decay | type_of                         -> std::meta::info(std::meta::info)
// std::meta::decay | parent_of                       -> std::meta
// int | add_pointr                                   -> error: unknown word 'add_pointr'
// int | add_const | is_const                         -> error: unknown word 'is_const'
// const double | remove_const | make_signed          -> error: 'make_signed' rejects double
//
// const int& | remove_cvref | make_unsigned holds 4000000000
//
// shortest pipelines, searched among all the words:
// const int& to int*: remove_cvref | add_pointer
// Color& to unsigned char: remove_reference | make_unsigned
// int to const int* const*: add_const | add_pointer | add_const | add_pointer
// int to long: no pipeline
// std::meta::decay to std: parent_of | parent_of
