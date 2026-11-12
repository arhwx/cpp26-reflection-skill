#include "solution.hpp"

#include <array>
#include <map>
#include <print>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

template <class T>
struct Box {
    T value;
};

template <class A, class B>
struct Pair {
    A first;
    B second;
};

template <class T, int N>
struct Fixed {
    T data[N];
};

template <template <class> class C, class T>
struct Holder {
    C<T> inner;
};

template <class... Ts>
struct List {};

struct Plain {
    int x;
};

using Int = int;
template <class T>
using Ptr = T*;

// aliases
static_assert(std::is_same_v<replace_t<Box<Int>, int, long>, Box<long>>);
static_assert(std::is_same_v<replace_t<Box<int>, Int, long>, Box<long>>);
static_assert(std::is_same_v<replace_t<Ptr<Int>, int, char>, char*>);

// T itself
static_assert(std::is_same_v<replace_t<int, int, long>, long>);
static_assert(std::is_same_v<replace_t<double, int, long>, double>);
static_assert(std::is_same_v<replace_t<Plain, int, long>, Plain>);

// template arguments at any depth
static_assert(std::is_same_v<replace_t<Pair<int, Box<int>>, int, long>, Pair<long, Box<long>>>);
static_assert(std::is_same_v<replace_t<Box<Box<Box<char>>>, char, bool>, Box<Box<Box<bool>>>>);
static_assert(std::is_same_v<replace_t<Pair<Plain, double>, int, long>, Pair<Plain, double>>);
static_assert(std::is_same_v<replace_t<List<int, char, int>, int, double>, List<double, char, double>>);
static_assert(std::is_same_v<replace_t<List<>, int, long>, List<>>);

// other template arguments are kept
static_assert(std::is_same_v<replace_t<Fixed<int, 3>, int, long>, Fixed<long, 3>>);
static_assert(std::is_same_v<replace_t<Holder<Box, int>, int, long>, Holder<Box, long>>);
static_assert(std::is_same_v<replace_t<std::array<Box<int>, 4>, int, char>, std::array<Box<char>, 4>>);

// const and pointers
static_assert(std::is_same_v<replace_t<std::tuple<int, const int*>, int, long>, std::tuple<long, const long*>>);
static_assert(std::is_same_v<replace_t<const Box<int>* const, int, long>, const Box<long>* const>);
static_assert(std::is_same_v<replace_t<int**, int, char>, char**>);
static_assert(std::is_same_v<replace_t<Box<const int>, int, Plain>, Box<const Plain>>);
static_assert(std::is_same_v<replace_t<const int*, const int, char>, char*>);

// From and To can be any type
static_assert(std::is_same_v<replace_t<Pair<Box<int>, int>, Box<int>, char>, Pair<char, int>>);
static_assert(std::is_same_v<replace_t<Pair<int, char>, int, Box<int>>, Pair<Box<int>, char>>);
static_assert(std::is_same_v<replace_t<Box<int*>, int*, const char*>, Box<const char*>>);

// standard templates, including their default arguments
static_assert(std::is_same_v<replace_t<std::string, char, wchar_t>, std::wstring>);
static_assert(std::is_same_v<replace_t<std::vector<int>, int, long>, std::vector<long>>);
static_assert(std::is_same_v<replace_t<std::map<int, Box<int>>, int, long>, std::map<long, Box<long>>>);

int main() {
    replace_t<std::map<int, std::vector<int>>, int, std::string> m;
    m["a"].push_back("x");
    m["a"].push_back("y");
    m["b"] = {"z"};
    for (const auto& [key, values] : m)
        std::println("{} {}", key, values);

    replace_t<Pair<int, Fixed<int, 2>>, int, double> p{0.5, {{1.25, 2.5}}};
    std::println("{} {} {}", p.first, p.second.data[0], p.second.data[1]);
}
