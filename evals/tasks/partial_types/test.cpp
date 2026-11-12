#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <type_traits>

struct User {
    int id;
    std::string name;
    std::string email;
    std::string password_hash;
};

using UserPatch = ts::partial<User>;
using PublicUser = ts::omit<User, "password_hash">;
using Login = ts::pick<User, "email password_hash">;
using Contact = ts::pick<User, "email id">;
using IdOnly = ts::omit<User, "password_hash name email">;

static_assert(std::is_same_v<decltype(UserPatch::id), std::optional<int>>);
static_assert(std::is_same_v<decltype(UserPatch::name), std::optional<std::string>>);
static_assert(std::is_same_v<ts::partial<User>, UserPatch>);
static_assert(std::is_same_v<ts::pick<User, "email password_hash">, Login>);
static_assert(std::is_aggregate_v<UserPatch> && std::is_aggregate_v<Login>);
static_assert(sizeof(Login) == 2 * sizeof(std::string));
static_assert(sizeof(IdOnly) == sizeof(int));
static_assert(std::is_same_v<decltype(PublicUser::email), std::string>);

template <class T>
concept has_name_key = requires(T& t) { ts::get<"name">(t); };
template <class T>
concept has_nme_key = requires(T& t) { ts::get<"nme">(t); };

int main() {
    User ada{1, "ada", "ada@example.org", "x9f"};
    User renamed = ts::with(ada, {.name = "countess", .email = "al@example.org"});
    std::println("{}", ts::show(renamed));
    std::println("{}", ts::show(ts::as<PublicUser>(renamed)));
    std::println("{}", ts::show(ts::as<Login>(ada)));
    std::println("{}", ts::show(ts::as<Contact>(ada)));
    std::println("{}", ts::show(ts::as<User>(ts::as<Login>(ada))));

    UserPatch patch{.id = 7};
    std::println("{}", ts::show(patch));
    ts::assign(ada, patch);
    ts::get<"name">(ada) += " lovelace";
    std::println("{}", ts::show(ada));

    UserPatch more{.email = "ada@lovelace.org"};
    ts::assign(patch, more);
    std::println("{}", ts::show(patch));
    std::println("{}", ts::show(ts::with(ada, patch)));
    std::println("{}", ts::show(ts::as<UserPatch>(ts::as<Contact>(ada))));

    std::println("get name on User: {}, on Login: {}", has_name_key<User>, has_name_key<Login>);
    std::println("get nme on User: {}", has_nme_key<User>);
    std::println("{}", ts::problem(^^User, "pasword_hash"));
    std::println("{}", ts::problem(^^Login, "email"));
}
