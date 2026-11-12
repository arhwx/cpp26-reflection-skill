#include "solution.hpp"

#include <print>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

struct Order {
    int id;
    std::string item;
    double price;
};

class Account {
public:
    Account(long number, std::string owner) : number(number), owner(std::move(owner)) {}

    long number;
    std::string owner;
    static inline int created = 0;
    bool active = true;

    int pin() const { return pin_; }

private:
    int pin_ = 1234;
};

struct Empty {};

static_assert(std::is_same_v<tuple_type_t<Order>, std::tuple<int, std::string, double>>);
static_assert(std::is_same_v<tuple_type_t<Account>, std::tuple<long, std::string, bool>>);
static_assert(std::is_same_v<tuple_type_t<Empty>, std::tuple<>>);
static_assert(std::is_same_v<decltype(to_tuple(Order{})), tuple_type_t<Order>>);

int main() {
    Order o{7, "lamp", 19.5};
    auto t = to_tuple(o);
    o.item = "desk";
    std::println("{} {} {}", std::get<0>(t), std::get<1>(t), std::get<2>(t));

    const Account a(4021, "ann");
    auto [number, owner, active] = to_tuple(a);
    std::println("{} {} {}", number, owner, active);

    std::println("{}", std::tuple_size_v<decltype(to_tuple(Empty{}))>);
}
