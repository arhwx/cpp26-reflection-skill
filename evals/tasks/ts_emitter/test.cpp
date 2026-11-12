#include "solution.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace shop {

enum class Currency { usd, eur, jpy };

using Cents = std::int64_t;

struct Money {
    Cents amount;
    Currency currency;
};

struct Entity {
    std::uint32_t id;
};

struct Audited {
    std::string created_by;
};

struct Address {
    std::string street;
    std::optional<std::string> zip;
};

struct Customer : Entity {
    std::string name;
    std::vector<Address> addresses;
    std::map<std::string, std::string> tags;
    bool vip;
};

struct Item {
    std::string sku;
    const unsigned short quantity;
    Money price;
};

struct Card {
    std::string last4;
};

struct Transfer {
    std::string iban;
    Money fee;
};

struct Order : Entity, Audited {
    enum Status { open, paid, shipped };

    Customer customer;
    std::vector<Item> items;
    std::variant<std::monostate, Card, Transfer> payment;
    std::vector<std::optional<double>> discounts;
    Status status;
    std::pair<double, double> location;
    std::array<float, 3> box;
    std::optional<Money> refund;
    static constexpr int version = 2;

private:
    int secret = 0;
};

}  // namespace shop

// a self-referential type and two types that refer to each other
struct Comment {
    std::string text;
    std::vector<Comment> replies;
};

struct Team;

struct Person {
    std::string name;
    std::optional<std::vector<Team>> teams;
};

struct Team {
    std::string title;
    std::vector<Person> members;
    std::map<std::string, Team> subteams;
};

int main() {
    std::print("{}", ts::emit<shop::Order>());
    std::println("---");
    std::print("{}", ts::emit<Comment>());
    std::println("---");
    std::print("{}", ts::emit<Team>());
    std::println("---");
    std::print("{}", ts::emit<shop::Currency>());
    std::println("---");
    std::println("{}", ts::type_expr<std::map<std::string, std::vector<std::optional<int>>>>());
    std::println("{}", ts::type_expr<std::variant<char, std::string_view, std::vector<shop::Card>>>());
    std::println("{}", ts::type_expr<std::tuple<bool, shop::Cents, shop::Currency>>());
    std::println("{}", ts::type_expr<std::optional<std::variant<int, bool>>>());
    std::println("{}", ts::type_expr<std::vector<std::variant<std::monostate, long long>>>());
    std::println("{}", ts::type_expr<std::vector<std::variant<shop::Money>>>());
    std::println("{}", ts::type_expr<std::array<std::vector<std::pair<int, std::string>>, 2>>());
}
