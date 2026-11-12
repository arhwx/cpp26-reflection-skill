#include "solution.hpp"

#include <cstddef>
#include <functional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

enum class Color : unsigned char { red, green };

// a small type printer for the types in this test, with const written after what it applies to
template <class T>
std::string show() {
    if constexpr (std::is_lvalue_reference_v<T>) return show<std::remove_reference_t<T>>() + "&";
    else if constexpr (std::is_rvalue_reference_v<T>) return show<std::remove_reference_t<T>>() + "&&";
    else if constexpr (std::is_array_v<T>) {
        std::string dims;
        if constexpr (std::rank_v<T> >= 1) dims += "[" + std::to_string(std::extent_v<T, 0>) + "]";
        if constexpr (std::rank_v<T> >= 2) dims += "[" + std::to_string(std::extent_v<T, 1>) + "]";
        return show<std::remove_all_extents_t<T>>() + dims;
    } else if constexpr (std::is_const_v<T> || std::is_volatile_v<T>)
        return show<std::remove_cv_t<T>>() + (std::is_const_v<T> ? " const" : "") + (std::is_volatile_v<T> ? " volatile" : "");
    else if constexpr (std::is_pointer_v<T> && std::is_array_v<std::remove_pointer_t<T>>)
        return "(" + show<std::remove_pointer_t<T>>() + ")*";
    else if constexpr (std::is_pointer_v<T>) return show<std::remove_pointer_t<T>>() + "*";
    else if constexpr (std::is_same_v<T, int>) return "int";
    else if constexpr (std::is_same_v<T, long>) return "long";
    else if constexpr (std::is_same_v<T, unsigned long>) return "unsigned long";
    else if constexpr (std::is_same_v<T, double>) return "double";
    else if constexpr (std::is_same_v<T, char>) return "char";
    else if constexpr (std::is_same_v<T, signed char>) return "signed char";
    else if constexpr (std::is_same_v<T, unsigned char>) return "unsigned char";
    else if constexpr (std::is_same_v<T, Color>) return "Color";
    else if constexpr (std::is_function_v<T>) return "function";
    else if constexpr (std::is_same_v<T, std::reference_wrapper<int>>) return "reference_wrapper<int>";
    else return "?";
}

template <metapipe::text P, class T>
void row() {
    std::println("{:<42} {:<30} -> {}", P.view(), show<T>(), show<metapipe::piped<P, T>>());
}

template <metapipe::text P, class T>
void check() {
    constexpr std::string_view message = metapipe::check(P.view(), ^^T);
    std::println("{:<42} {:<12} -> {}", P.view(), show<T>(), message.empty() ? "ok" : message);
}

template <metapipe::text Vocabulary, class From, class To, std::size_t Steps>
void search() {
    constexpr std::string_view found = metapipe::search(Vocabulary.view(), ^^From, ^^To, Steps);
    std::println("{} to {} in {} steps with [{}]: '{}'", show<From>(), show<To>(), Steps, Vocabulary.view(), found);
}

using Matrix = const int[2][3];
using Callback = void(int);
using Size = std::size_t;

static_assert(std::is_same_v<metapipe::piped<"remove_reference add_const add_pointer", int&>, const int*>);
static_assert(std::is_same_v<metapipe::piped<"", const int&>, const int&>);
static_assert(metapipe::run("decay", ^^Callback) == ^^Callback*);

int main() {
    row<"remove_reference add_const add_pointer", int&>();
    row<"  remove_cvref   add_volatile  ", const long&>();
    row<"", const int&>();
    row<"add_lvalue_reference add_rvalue_reference", int>();
    row<"add_rvalue_reference", int&>();
    row<"remove_extent add_pointer", Matrix>();
    row<"remove_all_extents remove_const", Matrix>();
    row<"decay", Matrix>();
    row<"decay", Callback>();
    row<"underlying_type make_signed add_const", Color>();
    row<"make_unsigned add_pointer add_pointer", long>();
    row<"remove_pointer remove_pointer remove_cv", const char* volatile*>();
    row<"unwrap_reference", std::reference_wrapper<int>>();
    row<"unwrap_ref_decay", const std::reference_wrapper<int>&>();
    row<"unwrap_reference", int>();

    metapipe::piped<"remove_cvref make_unsigned", const int&> big = 4'000'000'000u;
    std::println("runtime value: {}", big);

    std::println("checks:");
    check<"add_const remove_const", int>();
    check<"add_pointr", int>();
    check<"remove_reference remove_reference_t", int&>();
    check<"add_const is_const", int>();
    check<"underlying_type add_const", int>();
    check<"remove_const  make_signed", const double>();
    check<"add_pointer make_unsigned", Color>();

    std::println("search:");
    search<"remove_reference remove_const add_pointer add_const", const int&, int*, 3>();
    search<"add_pointer remove_cv decay add_const", const int&, int*, 3>();
    search<"add_pointer add_const remove_pointer", int, const int* const*, 4>();
    search<"add_pointer add_const", int, const int* const*, 3>();
    search<"underlying_type decay make_unsigned", Color&, unsigned char, 2>();
    search<"add_pointer", Size, unsigned long, 2>();
    search<"remove_pointer remove_extent", int, long, 3>();
    search<"remove_pointer add_ponter", int*, int, 1>();
}
