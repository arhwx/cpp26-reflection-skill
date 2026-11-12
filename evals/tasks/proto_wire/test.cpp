#include "solution.hpp"

#include <cstdint>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

enum class Role { guest, member, admin = 7 };
enum Kind { home_office, branch = -1 };

struct Address {
    [[=pb::field(1)]] std::string city;
    [[=pb::field(2)]] std::uint32_t zip;
    [[=pb::field(3)]] Kind kind;
};

struct User {
    [[=pb::field(1)]] std::string name;
    [[=pb::field(2)]] std::int32_t id;
    [[=pb::field(3)]] [[=pb::zigzag]] std::int64_t balance;
    [[=pb::field(4)]] Role role;
    [[=pb::field(5)]] std::vector<std::int32_t> scores;
    [[=pb::field(6)]] std::vector<std::string> tags;
    [[=pb::field(7)]] Address home;
    [[=pb::field(8)]] std::vector<Address> offices;
    [[=pb::field(9)]] std::optional<std::int32_t> age;
    [[=pb::field(15)]] double ratio;
    [[=pb::field(16)]] float weight;
    [[=pb::field(20)]] [[=pb::zigzag]] std::vector<std::int32_t> deltas;
    int cache = 0;
};

struct Dup { [[=pb::field(1)]] int a; [[=pb::field(2)]] int b; [[=pb::field(1)]] int c; };
struct Zero { [[=pb::field(0)]] int a; };
struct Huge { [[=pb::field(536870911)]] int a; [[=pb::field(536870912)]] int b; };
struct Zig { [[=pb::field(1)]] [[=pb::zigzag]] std::uint32_t a; };
struct Odd { [[=pb::field(1)]] int a; [[=pb::field(2)]] char c; };
struct Grid { [[=pb::field(1)]] std::vector<std::vector<int>> rows; };

std::string hex(std::string_view bytes) {
    std::string out;
    for (char c : bytes) out += std::format("{:02x}", static_cast<unsigned char>(c));
    return out;
}

std::string bytes(std::string_view hex) {
    std::string out;
    for (std::size_t i = 0; i < hex.size(); i += 2) out += static_cast<char>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16));
    return out;
}

void show(std::string_view label, const std::optional<Address>& a) {
    if (!a) std::println("{}: malformed", label);
    else std::println("{}: city={} zip={} kind={}", label, a->city, a->zip, static_cast<int>(a->kind));
}

void show(const User& u) {
    std::print("name={} id={} balance={} role={} scores=[", u.name, u.id, u.balance, static_cast<int>(u.role));
    for (int s : u.scores) std::print(" {}", s);
    std::print(" ] tags=[");
    for (const std::string& t : u.tags) std::print(" {}", t);
    std::print(" ] home={}/{} offices=[", u.home.city, u.home.zip);
    for (const Address& a : u.offices) std::print(" {}/{}/{}", a.city, a.zip, static_cast<int>(a.kind));
    std::print(" ] age={} ratio={} weight={} deltas=[", u.age ? std::to_string(*u.age) : "none", u.ratio, u.weight);
    for (int d : u.deltas) std::print(" {}", d);
    std::println(" ] cache={}", u.cache);
}

int main() {
    std::print("{}", pb::schema<User>());
    std::println("---");
    std::println("User: {}", pb::check(^^User));
    std::println("{}", pb::check(^^Dup));
    std::println("{}", pb::check(^^Zero));
    std::println("{}", pb::check(^^Huge));
    std::println("{}", pb::check(^^Zig));
    std::println("{}", pb::check(^^Odd));
    std::println("{}", pb::check(^^Grid));
    std::println("---");

    std::println("address: {}", hex(pb::encode(Address{"Oslo", 150, branch})));
    std::println("empty user: {}", hex(pb::encode(User{})));
    User ada{.name = "ada", .id = -2, .balance = -3, .role = Role::admin, .scores = {1, 300}, .tags = {"x", ""},
             .home = {"Paris", 0, home_office}, .offices = {{}, {"Rome", 7, branch}}, .age = 0, .ratio = 0.5,
             .weight = 1.5f, .deltas = {-1, 1, -64}, .cache = 99};
    std::string wire = pb::encode(ada);
    std::println("ada: {} bytes", wire.size());
    std::println("{}", hex(wire));
    std::optional<User> back = pb::decode<User>(wire);
    std::print("decoded: ");
    show(*back);
    std::println("same bytes again: {}", pb::encode(*back) == wire);
    std::println("---");

    std::string mixed = bytes("a006010a03616e6e28052807"
                              "2a020102ad06010203043a060a04526f6d65"
                              "3a02102a0a02626fb20603787878b906010203040506070848002007");
    std::optional<User> m = pb::decode<User>(mixed);
    std::print("mixed: ");
    if (m) show(*m);
    else std::println("malformed");

    show("truncated", pb::decode<Address>(bytes("0a044f736c6f1096")));
    show("wrong wire type", pb::decode<Address>(bytes("1201ff")));
    show("group", pb::decode<Address>(bytes("1b")));
    show("field zero", pb::decode<Address>(bytes("0001")));
    show("length past end", pb::decode<Address>(bytes("0a054f736c6f")));
    show("long varint", pb::decode<Address>(bytes("10ffffffffffffffffffff01")));
    show("nothing", pb::decode<Address>(""));
    show("negative kind", pb::decode<Address>(bytes("18ffffffff0f")));
}
