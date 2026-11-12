#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>
#include <vector>

struct Employee {
    int id;
    std::string name;
    std::string dept;
    double salary;
    int age;
    bool remote;
};

using ByPay = sql::query<Employee, "select name, salary where dept = ? and salary >= ? order by salary desc">;
using Remote = sql::query<Employee, "select * where remote = ? limit 2">;
using Roster = sql::query<Employee, "select id,name order by age">;
using Band = sql::query<Employee, R"(
    select dept, name, age
    where age<? and age>=? and dept!=?
    order by dept asc
    limit 3
)">;

template <class T, class U>
constexpr bool same = std::is_same_v<T, U>;

template <class Q, class... A>
constexpr bool takes = requires(const std::vector<Employee>& table, A... args) { Q::run(table, args...); };

// problem is called with a literal, so it can be evaluated at compile time
#define CHECK(text) std::println("{:<38} {}", text, sql::problem<Employee>(text))

int main() {
    const std::vector<Employee> staff{
        {1, "ada", "eng", 120000, 36, true},  {2, "bob", "ops", 70000, 45, false},  {3, "cy", "eng", 95000, 29, false},
        {4, "dee", "eng", 120000, 41, true},  {5, "eve", "sales", 60000, 23, true}, {6, "fay", "eng", 150000, 52, false},
        {7, "gus", "ops", 82000.5, 33, true},
    };

    std::println("ByPay row: {} {}", same<decltype(ByPay::row::name), std::string>, same<decltype(ByPay::row::salary), double>);
    std::println("Remote row: {} {}", same<decltype(Remote::row::id), int>, same<decltype(Remote::row::remote), bool>);
    std::println("Band row is aggregate: {}", std::is_aggregate_v<Band::row>);
    std::println("ByPay takes text, number: {}", takes<ByPay, std::string, double>);
    std::println("ByPay takes a literal and an int: {}", takes<ByPay, const char*, int>);
    std::println("ByPay takes two numbers: {}", takes<ByPay, double, double>);
    std::println("ByPay takes one argument: {}", takes<ByPay, const char*>);
    std::println("Roster takes none: {}", takes<Roster>);
    std::println("Roster takes one: {}", takes<Roster, int>);

    std::println("eng paid at least 100000:");
    for (const auto& r : ByPay::run(staff, "eng", 100000)) std::println("  {} {}", r.name, r.salary);
    std::println("eng paid at least 200000: {} rows", ByPay::run(staff, "eng", 200000).size());

    std::println("remote, first two:");
    for (const auto& r : Remote::run(staff, true)) std::println("  {} {} {} {} {} {}", r.id, r.name, r.dept, r.salary, r.age, r.remote);

    std::println("roster by age:");
    for (const auto& r : Roster::run(staff)) std::println("  {} {}", r.id, r.name);

    std::println("age in [30, 50), not sales:");
    for (const auto& r : Band::run(staff, 50, 30, "sales")) std::println("  {} {} {}", r.dept, r.name, r.age);
    std::println("age in [20, 40), not eng:");
    for (const auto& r : Band::run(staff, 40, 20, "eng")) std::println("  {} {} {}", r.dept, r.name, r.age);

    CHECK("select name, salary where dept = ?");
    CHECK("select name, salry");
    CHECK("select nmae");
    CHECK("select ame");
    CHECK("select title");
    CHECK("select name where dpt = ?");
    CHECK("select name where dept == ?");
    CHECK("select name where age > 30");
    CHECK("select name where age ?");
    CHECK("select name, name");
    CHECK("selec name");
    CHECK("select name from staff");
    CHECK("select name order salary");
    CHECK("select * limit ten");
    CHECK("select name;");
    CHECK("select");
    CHECK("select * where remote = ? and");
    CHECK("select *, name");
    CHECK("select name order by salry desc");
}
