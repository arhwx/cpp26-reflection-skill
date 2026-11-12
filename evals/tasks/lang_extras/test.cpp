#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

enum class Weekday { mon, tue, wed, thu, fri, sat, sun };
enum class Permission { read = 4, write = 2, execute = 1 };
enum class Level { low = 1, normal = 2, standard = 2, high = 3 };

namespace audit {
struct reviewed {
    int year;
};
}  // namespace audit

struct Signup {
    [[=lang::not_empty, =lang::tag("form", "user-name"), =lang::tag("json", "user")]] std::string user;
    [[=lang::between{13, 120}]] int age;
    [[=audit::reviewed{2024}, =lang::between{1, 12}, =lang::tag("form", "birth-month")]] int month;
    [[=lang::tag("json", "points"), =lang::not_empty]] std::vector<int> scores;
    Level level;
    bool admin;
};

struct Point {
    int x;
    long y;
    std::string label;
};

int main() {
    for (Weekday day : lang::cases<Weekday>) std::print("{} ", lang::name(day));
    constexpr std::size_t days = lang::cases<Weekday>.size();
    std::println("are {} days", days);
    for (Level level : lang::cases<Level>) std::print("{} ", lang::name(level));
    std::println("are {} levels", lang::cases<Level>.size());
    std::println("first permission: {}", lang::name(lang::cases<Permission>[0]));
    std::println("name of Permission 3: {}", lang::name(static_cast<Permission>(3)));

    constexpr std::optional<Weekday> thu = lang::parse<Weekday>("thu");
    std::println("parse thu: {}", thu.has_value() && *thu == Weekday::thu);
    std::println("parse someday: {}", lang::parse<Weekday>("someday").has_value());
    std::println("parse Read: {}", lang::parse<Permission>("Read").has_value());
    std::println("parse standard is normal: {}", lang::parse<Level>("standard") == Level::normal);

    lang::set<Permission> allowed{Permission::write, Permission::read};
    std::println("{} contains execute: {}", allowed.text(), allowed.contains(Permission::execute));
    constexpr bool reads = lang::set<Permission>{Permission::read}.contains(Permission::read);
    std::println("{{read}} contains read: {}", reads);
    lang::set<Weekday> weekend{Weekday::sun, Weekday::sat, Weekday::sun};
    std::println("weekend: {}", weekend.text());
    lang::set<Weekday> none{};
    std::println("none: {}, contains mon: {}", none.text(), none.contains(Weekday::mon));

    for (const std::string& problem : lang::violations(Signup{"", 9, 14, {}, Level::low, false}))
        std::println("{}", problem);
    std::println("problems with a good signup: {}", lang::violations(Signup{"ada", 36, 12, {1}, Level::high, true}).size());
    std::println("problems at the limits: {}", lang::violations(Signup{"x", 120, 1, {0}, Level::low, false}).size());

    constexpr std::string_view month_form = lang::tag_of(^^Signup::month, "form");
    std::println("month is sent as {}", month_form);
    std::println("user is sent as form [{}], json [{}]", lang::tag_of(^^Signup::user, "form"),
                 lang::tag_of(^^Signup::user, "json"));
    std::println("age has form tag [{}]", lang::tag_of(^^Signup::age, "form"));
    std::println("scores has json tag [{}]", lang::tag_of(^^Signup::scores, "json"));

    Signup ada{"ada", 36, 12, {90, 85}, Level::standard, true};
    std::println("{}", lang::repr(ada));
    std::println("{}", lang::repr(Point{3, -4, "corner"}));

    Point p{1, 2, "a"};
    std::println("equal values, equal hashes: {}", lang::hash(p) == lang::hash(Point{1, 2, "a"}));
    std::println("other label, other hash: {}", lang::hash(p) != lang::hash(Point{1, 2, "b"}));
}
