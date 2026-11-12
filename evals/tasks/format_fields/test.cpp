#include "solution.hpp"

#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct Person {
    std::string name;
    int age;
    std::string_view city;
    long id;
    std::vector<int> scores;
};

struct Pet {
    int legs;
    std::string name;
};

struct Item {
    int count;
    int count2;
    std::string count_label;
};

class Account {
public:
    Account(std::string o, unsigned n, const char* c) : owner(std::move(o)), number(n), currency(c) {}
    bool check(int code) const { return code == pin; }

    std::string owner;
    unsigned number;
    const char* currency;

private:
    int pin = 1234;
};

template <class T>
concept valid_field = requires(const T& v) { format_fields<"{name}">(v); };
template <class T>
concept unknown_field = requires(const T& v) { format_fields<"{nope}">(v); };
template <class T>
concept vector_field = requires(const T& v) { format_fields<"{scores}">(v); };
template <class T>
concept private_field = requires(const T& v) { format_fields<"{pin}">(v); };
template <class T>
concept unmatched_open = requires(const T& v) { format_fields<"{name">(v); };
template <class T>
concept stray_close = requires(const T& v) { format_fields<"name}">(v); };

int main() {
    Person p{"Ann", 31, "Oslo", -7, {90, 85}};
    std::println("[{}]", format_fields<"{name} is {age} years">(p));
    std::println("[{}]", format_fields<"{name} and {name}">(p));
    std::println("[{}]", format_fields<"{name}{age}{city}">(p));
    std::println("[{}]", format_fields<"{{{name}}}">(p));
    std::println("[{}]", format_fields<"{{name}} = {name}, a}}b">(p));
    std::println("[{}]", format_fields<"#{id} from {city}">(p));
    std::println("[{}]", format_fields<"{age}">(p));
    std::println("[{}]", format_fields<"no fields">(p));
    std::println("[{}]", format_fields<"">(p));

    Pet pet{4, "Rex"};
    std::println("[{}]", format_fields<"{name} has {legs} legs">(pet));

    Item item{3, 12, "boxes"};
    std::println("[{}]", format_fields<"{count2} {count_label}, {count} left">(item));

    Account acc("Bob", 42u, "EUR");
    std::println("[{}]", format_fields<"{owner}: account {number} in {currency}">(acc));

    std::println("valid field: {}", valid_field<Person>);
    std::println("unknown field: {}", unknown_field<Person>);
    std::println("vector field: {}", vector_field<Person>);
    std::println("private field: {}", private_field<Account>);
    std::println("unmatched open brace: {}", unmatched_open<Person>);
    std::println("stray close brace: {}", stray_close<Person>);
}
