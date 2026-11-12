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
