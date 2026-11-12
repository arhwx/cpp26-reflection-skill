#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <vector>

struct AddressV1 {
    std::string street;
    std::string city;
};

struct UserV1 {
    int id;
    std::string name;
    std::string mail;
    AddressV1 address;
    int age;
    bool legacy;
};

struct AddressV2 {
    std::string street;
    [[=ver::from("city")]] std::string town;
    std::string country = "NL";
};

struct UserV2 {
    long long id;
    [[=ver::from("name")]] std::string full_name;
    [[=ver::from("mail")]] std::string email;
    AddressV2 address;
    std::optional<int> age;
    std::vector<std::string> tags{"new"};
    double score = 1.5;
};

// v3 can be made from v1 or from v2
struct UserV3 {
    long long id;
    [[=ver::from("full_name")]] [[=ver::from("name")]] std::string display_name;
    [[=ver::from("email")]] [[=ver::from("mail")]] std::string contact;
    AddressV2 address;
    std::vector<std::string> tags{"v3"};
};

// has both a full_name and a name, and its own display_name
struct Badge {
    long long id;
    std::string display_name;
    std::string full_name;
    std::string name;
    std::string mail;
    AddressV1 address;
};

// has a name before its full_name, and both mail and email
struct Profile {
    long long id;
    std::string name;
    std::string full_name;
    std::string mail;
    std::string email;
    AddressV2 address;
    std::vector<std::string> tags;
};

struct Point {
    int x;
    int y;
};

struct Vec3 {
    [[=ver::from("x")]] double a;
    [[=ver::from("y")]] double b;
    double c = 9;
};

static_assert(ver::convert<Vec3>(Point{1, 2}).b == 2);
static_assert(ver::convert<Vec3>(Point{1, 2}).c == 9);

struct NeedsPhone {
    long long id;
    std::string phone;
};

struct AddressZip {
    std::string street;
    std::string zip;
};

struct NeedsZip {
    int id;
    AddressZip address;
};

struct NumericName {
    int name;
};

struct ListAddress {
    std::vector<std::string> address;
};

template <class To, class From>
concept convertible = requires(const From& from) { ver::convert<To>(from); };

void print(const UserV2& u) {
    std::println("v2 id={} full_name={} email={} address={}/{}/{} age={} tags={} score={}", u.id, u.full_name,
                 u.email, u.address.street, u.address.town, u.address.country,
                 u.age ? std::to_string(*u.age) : "none", u.tags, u.score);
}

void print(const UserV3& u) {
    std::println("v3 id={} display_name={} contact={} address={}/{}/{} tags={}", u.id, u.display_name, u.contact,
                 u.address.street, u.address.town, u.address.country, u.tags);
}

int main() {
    UserV1 v1{7, "Ada Lovelace", "ada@example.org", {"1 Main St", "London"}, 36, true};

    UserV2 v2 = ver::convert<UserV2>(v1);
    print(v2);
    v2.tags.push_back("vip");
    v2.address.country = "UK";
    v2.age.reset();
    print(ver::convert<UserV2>(v2));

    print(ver::convert<UserV3>(v1));
    print(ver::convert<UserV3>(v2));

    Badge badge{3, "ada", "Augusta Ada King", "Ada", "ada@badge.org", {"2 Side St", "Paris"}};
    print(ver::convert<UserV3>(badge));
    Profile profile{4, "Ada", "Ada King", "old@example.org", "new@example.org", {"3 High St", "Rome", "IT"}, {}};
    print(ver::convert<UserV3>(profile));

    Vec3 v = ver::convert<Vec3>(Point{4, -5});
    std::println("vec3 {} {} {}", v.a, v.b, v.c);

    std::println("UserV2 from UserV1: {}", ver::check<UserV2, UserV1>());
    std::println("UserV1 from UserV2: {}", ver::check<UserV1, UserV2>());
    std::println("NeedsPhone from UserV1: {}", ver::check<NeedsPhone, UserV1>());
    std::println("NeedsZip from UserV1: {}", ver::check<NeedsZip, UserV1>());
    std::println("NumericName from UserV1: {}", ver::check<NumericName, UserV1>());
    std::println("ListAddress from UserV1: {}", ver::check<ListAddress, UserV1>());
    std::println("Point from Vec3: {}", ver::check<Point, Vec3>());

    std::println("convertible UserV3 <- Badge: {}", convertible<UserV3, Badge>);
    std::println("convertible UserV1 <- UserV2: {}", convertible<UserV1, UserV2>);
    std::println("convertible NeedsZip <- UserV1: {}", convertible<NeedsZip, UserV1>);
    std::println("convertible NumericName <- UserV1: {}", convertible<NumericName, UserV1>);
}
