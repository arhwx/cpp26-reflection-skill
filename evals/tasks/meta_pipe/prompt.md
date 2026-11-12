I want a tiny language of type transformations whose vocabulary is not written by me but found in the reflection library itself: a pipeline such as `"remove_reference add_const add_pointer"` applied to `int&` gives `const int*`, and every word is the name of a function in namespace `std::meta`. I also want a search that finds the shortest pipeline from one type to another using a given list of words.

Everything goes in `namespace metapipe`:
- `metapipe::text`: a class template usable as the type of a template parameter, so that `template <metapipe::text P>` accepts a string literal as its argument, like `row<"add_const", int>()` in the test. `P.view()` returns the text without the terminating null as a `std::string_view` and can be called in constant expressions.
- `consteval std::string_view metapipe::check(std::string_view pipeline, std::meta::info type)`: an empty string when `pipeline` can be applied to the type `type`, otherwise a message, as described below.
- `consteval std::meta::info metapipe::run(std::string_view pipeline, std::meta::info type)`: the type that results from applying `pipeline` to `type`. Its result is a reflection of a type, with no type aliases. It is only called with pipelines that `check` accepts.
- `template <metapipe::text P, class T> using metapipe::piped`: the type that results from applying the pipeline `P` to `T`.
- `consteval std::string_view metapipe::search(std::string_view vocabulary, std::meta::info from, std::meta::info to, std::size_t max_steps)`: the shortest pipeline made of words of `vocabulary` that turns `from` into `to`, as described below.

Words:
- A pipeline or a vocabulary is a list of words separated by one or more spaces. Leading and trailing spaces are allowed. A pipeline with no words leaves the type unchanged.
- A word is known when namespace `std::meta` has a function with exactly that name that takes a single `std::meta::info` and returns `std::meta::info`. Any other word is unknown, including names of `std::meta` functions with other signatures, such as functions that return `bool`.
- Applying a known word to a type calls that function of `std::meta` with the reflection of the type. Words are applied left to right, each to the result of the previous one. The first word is step 1.
- A step fails when the function rejects its argument by throwing `std::meta::exception`, like `underlying_type` for a type that is not an enum.

What `check` returns, for the first word that has a problem:
- `unknown word '<word>' at step <k>` when the word at step `<k>` is unknown.
- `'<word>' failed at step <k>` when the word at step `<k>` is known but its step fails.
- An empty string when every step succeeds.

What `search` returns:
- If some word of the vocabulary is unknown: `unknown word '<word>'` for the first such word.
- Otherwise, if `from` and `to` are the same type (a type alias and the type it names are the same type): an empty string.
- Otherwise, the first pipeline of 1 to `max_steps` words, each taken from the vocabulary (a word may be used several times), that applied to `from` succeeds at every step and gives the same type as `to`. Pipelines are tried shortest first. Among pipelines of the same length, they are tried in lexicographic order of the positions of their words in the vocabulary: with vocabulary `a b`, the order is `a`, `b`, `a a`, `a b`, `b a`, `b b`, `a a a`, and so on. The result is the words of that pipeline joined with single spaces.
- `no pipeline` when no such pipeline exists.

The test only uses the standard type transformation functions of `std::meta` as known words.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
remove_reference add_const add_pointer     int&                           -> int const*
  remove_cvref   add_volatile              long const&                    -> long volatile
                                           int const&                     -> int const&
add_lvalue_reference add_rvalue_reference  int                            -> int&
add_rvalue_reference                       int&                           -> int&
remove_extent add_pointer                  int const[2][3]                -> (int const[3])*
remove_all_extents remove_const            int const[2][3]                -> int
decay                                      int const[2][3]                -> (int const[3])*
decay                                      function                       -> function*
underlying_type make_signed add_const      Color                          -> signed char const
make_unsigned add_pointer add_pointer      long                           -> unsigned long**
remove_pointer remove_pointer remove_cv    char const* volatile*          -> char
unwrap_reference                           reference_wrapper<int>         -> int&
unwrap_ref_decay                           reference_wrapper<int> const&  -> int&
unwrap_reference                           int                            -> int
runtime value: 4000000000
checks:
add_const remove_const                     int          -> ok
add_pointr                                 int          -> unknown word 'add_pointr' at step 1
remove_reference remove_reference_t        int&         -> unknown word 'remove_reference_t' at step 2
add_const is_const                         int          -> unknown word 'is_const' at step 2
underlying_type add_const                  int          -> 'underlying_type' failed at step 1
remove_const  make_signed                  double const -> 'make_signed' failed at step 2
add_pointer make_unsigned                  Color        -> 'make_unsigned' failed at step 2
search:
int const& to int* in 3 steps with [remove_reference remove_const add_pointer add_const]: 'remove_reference remove_const add_pointer'
int const& to int* in 3 steps with [add_pointer remove_cv decay add_const]: 'decay add_pointer'
int to int const* const* in 4 steps with [add_pointer add_const remove_pointer]: 'add_const add_pointer add_const add_pointer'
int to int const* const* in 3 steps with [add_pointer add_const]: 'no pipeline'
Color& to unsigned char in 2 steps with [underlying_type decay make_unsigned]: 'decay underlying_type'
unsigned long to unsigned long in 2 steps with [add_pointer]: ''
int to long in 3 steps with [remove_pointer remove_extent]: 'no pipeline'
int* to int in 1 steps with [remove_pointer add_ponter]: 'unknown word 'add_ponter''
```

Write the complete header to `solution.hpp` in the current directory.
