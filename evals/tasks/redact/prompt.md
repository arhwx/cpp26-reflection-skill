Some of our structs hold secrets: passwords, card numbers, national ids. I want to mark those members once, have log output hide them, and have the real serializer refuse to compile for any type that holds a secret, anywhere inside it, unless the caller passes a policy that says how to write each secret.

Everything goes in `namespace sec`. All structs involved are aggregates whose members are all public.

Marking and classification:
- `sec::secret`: a constant used as the annotation `[[=sec::secret]]` on a non-static data member. That member is a secret.
- Member types are `std::string`, `bool`, other arithmetic types, and aggregate class types, which are nested structs. A member of a nested struct type that is not itself a secret is looked into; a secret member of a nested struct type is not looked into.
- The secret paths of a type `T` are, for each secret reachable that way, in declaration order and depth first: the names of the members leading to it, joined with `.`, as in `card.number`.
- `template <class T> consteval std::span<const char* const> secret_paths()`: the secret paths of `T`, each a null-terminated string. The span and its strings stay valid at run time.
- `template <class T> constexpr bool classified`: `true` when `T` has at least one secret path.

Text format, used by every function below:
- A struct is written as `{` then its members in declaration order, separated by `, `, then `}`. An empty struct is `{}`.
- Each member is written as its name, `: `, and its value.
- A `std::string` value is written between double quotes, with no escaping. A nested struct is written in the same struct format. Any other value, including `bool` and floating-point values, is written as `std::format("{}", value)` writes it.
- Secrets are written as each function below says, instead of their value.

Functions:
- `template <class T> std::string redacted(const T& value)`: every secret is written as `***`, without quotes. Works for every `T`.
- `template <class T> std::string serialize(const T& value)`: for a `T` that is not classified. For a classified `T`, the call must not compile.
- `template <class T, class P> std::string serialize(const T& value, const P& policy)`: every secret is written as the text returned by `policy(path, member_value)`, inserted as is, without quotes. `path` is a `std::string_view` holding the secret's path as defined above, and `member_value` is a `const` reference to the secret member, which may be a whole nested struct. The policy is called through a `const P&`. Works for classified and unclassified `T`, but only when, for every secret path of `T`, that call with the secret's member type is valid and returns something convertible to `std::string`. Otherwise the call must not compile.
- Each "must not compile" above must fail as part of overload resolution, so that a requires-expression containing the call is `false`. The test checks this with concepts.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <format>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

struct Card {
    [[=sec::secret]] std::string number;
    std::string expiry;
};

struct Patient {
    int id;
    std::string name;
    [[=sec::secret]] std::string ssn;
    Card card;
    [[=sec::secret]] Card spare;
    bool insured;
    double weight;
};

struct Login {
    std::string user;
    [[=sec::secret]] std::string password;
    [[=sec::secret]] int pin;
};

// classified only through a nested member
struct Account {
    std::string owner;
    Card card;
};

struct Visit {
    int patient;
    std::string ward;
    double hours;
};

struct Ward {
    std::string name;
    Visit last;
    bool open;
};

// shows the last four characters of a string, and a tag for anything else
inline constexpr auto last4 = [](std::string_view path, const auto& value) -> std::string {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, std::string>)
        return "\"..." + value.substr(value.size() - 4) + "\"";
    else
        return std::format("<{} hidden>", path);
};

// writes the path of each secret instead of its value
inline constexpr auto audit = [](std::string_view path, const auto&) -> std::string {
    return std::format("<{}>", path);
};

// can only write string secrets
inline constexpr auto only_strings = [](std::string_view, const std::string& value) -> std::string {
    return "'" + value + "'";
};

// a policy that returns no text
inline constexpr auto silent = [](std::string_view, const auto&) {};

template <class T>
concept plain = requires(const T& value) { sec::serialize(value); };

template <class T, class P>
concept with_policy = requires(const T& value, const P& policy) { sec::serialize(value, policy); };

template <class T>
void list_paths(std::string_view label) {
    std::string out;
    for (std::string_view path : sec::secret_paths<T>())
        out += std::format(" {}", path);
    std::println("{} secrets ({}):{}", label, sec::secret_paths<T>().size(), out);
}

static_assert(sec::secret_paths<Patient>().size() == 3);
static_assert(sec::classified<Account> && !sec::classified<Ward>);

int main() {
    Patient patient{12, "Ada", "123-45-6789", {"4111111111111111", "12/29"}, {"5500000000000004", "01/31"},
                    true, 61.5};
    Login login{"ada", "swordfish", 4321};
    Account account{"Bob", {"378282246310005", "07/27"}};
    Visit visit{12, "B2", 1.25};
    Ward ward{"Cardiology", visit, false};

    std::println("{}", sec::redacted(patient));
    std::println("{}", sec::redacted(login));
    std::println("{}", sec::redacted(account));
    std::println("{}", sec::redacted(ward));

    std::println("{}", sec::serialize(visit));
    std::println("{}", sec::serialize(ward));
    std::println("{}", sec::serialize(patient, last4));
    std::println("{}", sec::serialize(login, last4));
    std::println("{}", sec::serialize(patient, audit));
    std::println("{}", sec::serialize(account, only_strings));
    std::println("{}", sec::serialize(ward, only_strings));

    list_paths<Patient>("Patient");
    list_paths<Login>("Login");
    list_paths<Account>("Account");
    list_paths<Ward>("Ward");

    std::println("classified: Patient {} Login {} Account {} Visit {} Ward {}", sec::classified<Patient>,
                 sec::classified<Login>, sec::classified<Account>, sec::classified<Visit>, sec::classified<Ward>);

    std::println("plain Patient: {}", plain<Patient>);
    std::println("plain Account: {}", plain<Account>);
    std::println("plain Ward: {}", plain<Ward>);
    std::println("Patient with last4: {}", with_policy<Patient, decltype(last4)>);
    std::println("Patient with only_strings: {}", with_policy<Patient, decltype(only_strings)>);
    std::println("Login with only_strings: {}", with_policy<Login, decltype(only_strings)>);
    std::println("Account with only_strings: {}", with_policy<Account, decltype(only_strings)>);
    std::println("Ward with only_strings: {}", with_policy<Ward, decltype(only_strings)>);
    std::println("Login with silent: {}", with_policy<Login, decltype(silent)>);
    std::println("Ward with silent: {}", with_policy<Ward, decltype(silent)>);
}
```

It must print exactly:

```
{id: 12, name: "Ada", ssn: ***, card: {number: ***, expiry: "12/29"}, spare: ***, insured: true, weight: 61.5}
{user: "ada", password: ***, pin: ***}
{owner: "Bob", card: {number: ***, expiry: "07/27"}}
{name: "Cardiology", last: {patient: 12, ward: "B2", hours: 1.25}, open: false}
{patient: 12, ward: "B2", hours: 1.25}
{name: "Cardiology", last: {patient: 12, ward: "B2", hours: 1.25}, open: false}
{id: 12, name: "Ada", ssn: "...6789", card: {number: "...1111", expiry: "12/29"}, spare: <spare hidden>, insured: true, weight: 61.5}
{user: "ada", password: "...fish", pin: <pin hidden>}
{id: 12, name: "Ada", ssn: <ssn>, card: {number: <card.number>, expiry: "12/29"}, spare: <spare>, insured: true, weight: 61.5}
{owner: "Bob", card: {number: '378282246310005', expiry: "07/27"}}
{name: "Cardiology", last: {patient: 12, ward: "B2", hours: 1.25}, open: false}
Patient secrets (3): ssn card.number spare
Login secrets (2): password pin
Account secrets (1): card.number
Ward secrets (0):
classified: Patient true Login true Account true Visit false Ward false
plain Patient: false
plain Account: false
plain Ward: true
Patient with last4: true
Patient with only_strings: false
Login with only_strings: false
Account with only_strings: true
Ward with only_strings: true
Login with silent: false
Ward with silent: true
```

Write the complete header to `solution.hpp` in the current directory.
