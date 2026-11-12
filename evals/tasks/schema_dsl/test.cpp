#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

using User = schema_dsl::schema<R"(
    id:int name:string
    tags:[string]
    address:{city:string zip:int}
    nick:string?
    pets:[{name:string age:int}]
    grid:[[int]]
    boss:{name:string}?
    score: double  active :bool
)">;

using Pair = schema_dsl::schema<"a:{} b:[bool]? c:[int?]">;
using Empty = schema_dsl::schema<"  ">;

template <class T, class U>
constexpr bool same = std::is_same_v<T, U>;

// problem is called with a literal, so it can be evaluated at compile time
#define CHECK(text) std::println("{:<28} {}", text, schema_dsl::problem(text))

int main() {
    std::println("id: {}", same<decltype(User::id), int>);
    std::println("name: {}", same<decltype(User::name), std::string>);
    std::println("tags: {}", same<decltype(User::tags), std::vector<std::string>>);
    std::println("nick: {}", same<decltype(User::nick), std::optional<std::string>>);
    std::println("grid: {}", same<decltype(User::grid), std::vector<std::vector<int>>>);
    using Pet = decltype(User::pets)::value_type;
    using Boss = decltype(User::boss)::value_type;
    std::println("pet age: {}", same<decltype(Pet::age), int>);
    std::println("boss name: {}", same<decltype(Boss::name), std::string>);
    std::println("score: {}", same<decltype(User::score), double>);
    std::println("c: {}", same<decltype(Pair::c), std::vector<std::optional<int>>>);
    std::println("aggregate: {}", std::is_aggregate_v<User> && std::is_aggregate_v<decltype(User::address)>);
    std::println("same text, same type: {}", same<schema_dsl::schema<"x:int y:[double]">, schema_dsl::schema<"x:int y:[double]">>);
    std::println("empty: {}", std::is_empty_v<Empty> && std::is_empty_v<decltype(Pair::a)>);

    User u{
        .id = 7,
        .name = "ada",
        .tags = {"admin", "dev"},
        .address = {.city = "Oslo", .zip = 150},
        .nick = std::nullopt,
        .pets = {{"rex", 3}, {.name = "tom", .age = 5}},
        .grid = {{1, 2}, {}, {3}},
        .boss = std::nullopt,
        .score = 4.5,
        .active = true,
    };
    std::println("{}", schema_dsl::to_text(u));
    u.nick = "countess";
    u.boss.emplace();
    u.boss->name = "grace";
    u.pets.pop_back();
    u.address.city += " west";
    std::println("{}", schema_dsl::to_text(u));
    std::println("{}", schema_dsl::to_text(u.address));

    Pair p{};
    std::println("{}", schema_dsl::to_text(p));
    p.b = std::vector<bool>{true, false};
    p.c = {1, std::nullopt, 3};
    std::println("{}", schema_dsl::to_text(p));
    std::println("{}", schema_dsl::to_text(Empty{}));

    CHECK("id:int name:string");
    CHECK("");
    CHECK("id:int name:strng");
    CHECK("id:int id:double");
    CHECK("a:{x:int} x:int");
    CHECK("a:{x:int x:int}");
    CHECK("id int");
    CHECK("tags:[string");
    CHECK("tags:[string}");
    CHECK("address:{city:string");
    CHECK("a:int?? b:int");
    CHECK("a:");
    CHECK("list:[]");
    CHECK("x:int}");
    CHECK("1a:int");
    CHECK("a:int b:{c:[{d:flaot}]}");
    CHECK("a:intb:int");
}
