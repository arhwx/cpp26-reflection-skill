#include "solution.hpp"

#include <array>
#include <cstddef>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

using Meters = double;

struct Body {
    std::string name;
    double mass;
    int id;
    Meters height;
    const double density = 1.0;
    int layer : 4;
    static inline double gravity = 9.81;
    double width;
    int count;
};

class Account {
public:
    Account(double balance, double limit) : balance(balance), limit_(limit) {}
    double headroom() const { return limit_ - balance - fee_; }

    double balance;
    int rank = 1;

private:
    double limit_;
    double fee_ = 0.5;
};

struct Empty {};

static_assert(std::is_same_v<decltype(member_pointers_of<Body, double>), const std::array<double Body::*, 3>>);
static_assert(member_pointers_of<Body, double>[0] == &Body::mass);
static_assert(member_pointers_of<Body, double>[1] == &Body::height);
static_assert(member_pointers_of<Body, double>[2] == &Body::width);
static_assert(std::is_same_v<decltype(member_names_of<Body, double>), const std::array<std::string_view, 3>>);
static_assert(member_names_of<Body, double>[0] == "mass" && member_names_of<Body, double>[2] == "width");

static_assert(member_pointers_of<Body, const double>.size() == 1);
static_assert(member_pointers_of<Body, const double>[0] == &Body::density);
static_assert(member_pointers_of<Body, int>.size() == 2 && member_pointers_of<Body, int>[1] == &Body::count);
static_assert(member_pointers_of<Body, std::string>[0] == &Body::name);
static_assert(std::is_same_v<decltype(member_pointers_of<Body, float>), const std::array<float Body::*, 0>>);
static_assert(member_names_of<Body, float>.empty());

static_assert(member_pointers_of<Account, double>.size() == 1);
static_assert(member_pointers_of<Account, double>[0] == &Account::balance);
static_assert(member_pointers_of<Empty, int>.empty());

template <class T>
double sum_doubles(const T& obj) {
    double total = 0;
    for (double T::* p : member_pointers_of<T, double>)
        total += obj.*p;
    return total;
}

template <class T>
void scale_doubles(T& obj, double factor) {
    for (double T::* p : member_pointers_of<T, double>)
        obj.*p *= factor;
}

template <class M, class T>
void print_members(std::string_view label, const T& obj) {
    std::println("{}:", label);
    std::size_t i = 0;
    for (M T::* p : member_pointers_of<T, M>)
        std::println("  {} = {}", member_names_of<T, M>[i++], obj.*p);
}

int main() {
    Body b{"probe", 2.5, 7, 1.75, 4.0, 3, 0.5, 12};
    std::println("sum {}", sum_doubles(b));
    scale_doubles(b, 2);
    std::println("scaled {}", sum_doubles(b));
    print_members<double>("double", b);
    print_members<int>("int", b);
    print_members<const double>("const double", b);
    print_members<std::string>("string", b);
    print_members<float>("float", b);

    Account acc(120.5, 500);
    std::println("account {} {}", sum_doubles(acc), acc.headroom());
    print_members<int>("account int", acc);

    std::println("empty {}", sum_doubles(Empty{}));
}
