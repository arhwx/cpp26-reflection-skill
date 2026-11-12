#include "solution.hpp"

#include <format>
#include <optional>
#include <print>
#include <string>
#include <vector>

enum class Role { admin, guest };

struct Money {  // brings its own format
    long cents;
    void to_json(std::string& out) const { out += std::format("\"{}.{:02}\"", cents / 100, cents % 100); }
};

struct [[=json::naming::camel]] Address {
    std::string street_name;
    int house_number;
};

struct Tag {  // no class annotation, so keys are written as declared
    std::string tag_name;
    [[=json::since(2)]] double weight;
};

class [[=json::naming::camel]] Account {
    friend struct json::access;
    int account_id = 7;
    std::string owner_name = "ada";
    [[=json::skip]] std::string password = "swordfish";
    [[=json::name("role")]] Role access_role = Role::admin;
    Money balance{12050};
    std::optional<Address> home_address = Address{"Main St", 3};
    std::optional<std::string> nick_name;
    std::vector<int> recent_logins{3, 5, 8};
    std::vector<Tag> tags{{"vip", 1.5}, {"beta", 0.25}};
    [[=json::since(2)]] bool two_factor = true;
};

struct Clash {
    int user_id;
    [[=json::name("user_id")]] int id;
};

struct [[=json::naming::camel]] CamelClash {
    int item_count;
    [[=json::skip]] int itemCount;
    int itemCount2;
    [[=json::name("item_count2")]] int other;
    int item_count2;
};

class Hidden {
    int secret = 1;

public:
    int shown = 2;
};

struct [[=json::naming::as_written]] Point {
    int x_pos;
    int y_pos;
    const char* label;
};

int main() {
    Account account;
    std::println("{}", json::to_string(account));
    std::println("{}", json::to_string(account, 2));
    std::println("{}", json::to_string(std::vector<Role>{Role::guest, Role::admin}));
    std::println("{}", json::to_string(std::vector<std::optional<Point>>{Point{1, -2, "a"}, std::nullopt}));
    std::println("{}", json::to_string(Money{5}));
    std::println("{}", json::check(^^Account));
    std::println("{}", json::check(^^Clash));
    std::println("{}", json::check(^^CamelClash));
    std::println("{}", json::check(^^Hidden));
}
