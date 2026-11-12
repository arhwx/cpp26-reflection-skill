#include "solution.hpp"

#include <cstddef>
#include <print>
#include <string>
#include <tuple>
#include <type_traits>

static_assert(std::is_same_v<filter_integral_t<std::tuple<int, double, long, char, float>>, std::tuple<int, long, char>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<double, float, std::string>>, std::tuple<>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<>>, std::tuple<>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<bool, int&, const short, int*, std::size_t>>,
                             std::tuple<bool, const short, std::size_t>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<int, int, unsigned char>>, std::tuple<int, int, unsigned char>>);

template <class... Ts>
constexpr std::size_t count_integral() {
    return std::tuple_size_v<filter_integral_t<std::tuple<Ts...>>>;
}

static_assert(count_integral<char, double, long long, void*>() == 2);

int main() {
    std::println("OK");
}
