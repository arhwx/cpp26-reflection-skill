#include "solution.hpp"

#include <array>
#include <print>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct Address {
    std::string street;
    std::string city;
    int zip;
};

struct Item {
    std::string name;
    int qty;
};

struct Person {
    std::string name;
    int age;
    Address address;
    std::array<Item, 2> favorites;
    std::vector<Address> past;
    int scores[3];
};

class Vault {
public:
    int id = 1;

private:
    int code = 1234;
};

static_assert(std::is_same_v<kp::type<Person, "address.zip">, int>);
static_assert(std::is_same_v<kp::type<Person, "favorites[1]">, Item>);
static_assert(std::is_same_v<kp::type<Person, "past[9].city">, std::string>);
static_assert(std::is_same_v<decltype(kp::get<"scores[0]">(std::declval<const Person&>())), const int&>);
static_assert(std::is_same_v<decltype(kp::get<"address">(std::declval<Person&>())), Address&>);

template <class T>
concept zip_path = requires(T& t) { kp::get<"address.zip">(t); };
template <class T>
concept typo_path = requires(T& t) { kp::get<"address.zpi">(t); };
template <class T>
concept far_score = requires(T& t) { kp::set<"scores[3]">(t, 1); };

std::string upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}

void show(const Person& p) {
    std::println("  {} {} at {}, {} {} | {} x{}, {} x{} | past {} | scores {} {} {}", p.name, p.age, p.address.street,
                 p.address.zip, p.address.city, p.favorites[0].name, p.favorites[0].qty, p.favorites[1].name,
                 p.favorites[1].qty, p.past.size(), p.scores[0], p.scores[1], p.scores[2]);
}

int main() {
    Person ada{"ada", 36, {"main st", "london", 1815}, {{{"tea", 1}, {"cake", 2}}}, {{"old rd", "paris", 75}}, {7, 8, 9}};
    show(ada);

    std::println("city: {}", kp::get<"address.city">(ada));
    std::println("second favorite: {}", kp::get<"favorites[1].name">(ada));
    std::println("last score: {}", kp::get<"scores[2]">(ada));
    std::println("past street: {}", kp::get<"past[0].street">(ada));

    kp::set<"address.zip">(ada, 1852);
    kp::get<"favorites[1].qty">(ada) += 5;
    kp::set<"scores[0]">(ada, 10);
    kp::set<"past[0].city">(ada, std::string("rome"));
    std::println("after set:");
    show(ada);
    std::println("  past city: {}", ada.past[0].city);

    const Person& view = ada;
    Person shouted = kp::over<"address.street">(view, upper);
    Person older = kp::over<"age">(kp::over<"name">(ada, upper), [](int a) { return a + 1; });
    std::println("over results:");
    show(shouted);
    show(older);
    std::println("original:");
    show(view);

    try {
        kp::set<"past[3].zip">(ada, 1);
    } catch (const std::out_of_range&) {
        std::println("past[3] is out of range at run time");
    }

    std::println("checks:");
    std::println("  {}", kp::check<Person, "address.city">());
    std::println("  {}", kp::check<Person, "past[12].zip">());
    std::println("  {}", kp::check<Person, "nmae">());
    std::println("  {}", kp::check<Person, "address.stret">());
    std::println("  {}", kp::check<Person, "age.value">());
    std::println("  {}", kp::check<Person, "name[0]">());
    std::println("  {}", kp::check<Person, "address.city[0]">());
    std::println("  {}", kp::check<Person, "scores[3]">());
    std::println("  {}", kp::check<Person, "favorites[2].qty">());
    std::println("  {}", kp::check<Person, "favorites[1].price">());
    std::println("  {}", kp::check<Person, "favorites.name">());
    std::println("  {}", kp::check<Person, "address..city">());
    std::println("  {}", kp::check<Person, "">());
    std::println("  {}", kp::check<Person, "scores[x]">());
    std::println("  {}", kp::check<Person, "past[1">());
    std::println("  {}", kp::check<Person, "[0]">());
    std::println("  {}", kp::check<Person, "address.">());
    std::println("  {}", kp::check<Vault, "code">());
    std::println("  {}", kp::check<Vault, "id">());

    std::println("zip path: {}", zip_path<Person>);
    std::println("typo path: {}", typo_path<Person>);
    std::println("far score: {}", far_score<Person>);
    std::println("zip path on Address: {}", zip_path<Address>);
}
