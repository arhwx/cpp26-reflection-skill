#include "solution.hpp"

#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct Account {
    std::string name;
    int age;
    double balance;
    bool vip;
};

// a template that ignores its argument, like haskell's Const
template <class>
using text = std::string;
template <class>
using flag = bool;
// a user-defined wrapper
template <class M>
struct versioned {
    M value{};
    int version = 0;
};

static_assert(std::is_same_v<decltype(hkd::map<Account, std::optional>::age), std::optional<int>>);
static_assert(std::is_same_v<decltype(hkd::map<Account, std::vector>::vip), std::vector<bool>>);
static_assert(std::is_same_v<decltype(hkd::map<Account, std::reference_wrapper>::name),
                             std::reference_wrapper<std::string>>);
static_assert(std::is_same_v<decltype(hkd::map<Account, std::unique_ptr>::balance), std::unique_ptr<double>>);
static_assert(std::is_same_v<decltype(hkd::map<Account, text>::balance), std::string>);
static_assert(std::is_same_v<decltype(hkd::map<Account, versioned>::name), versioned<std::string>>);
static_assert(!std::is_same_v<hkd::map<Account, text>, hkd::map<Account, std::optional>>);

std::string show(const auto& v) {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(v)>, std::string>) return std::format("\"{}\"", v);
    else return std::format("{}", v);
}

void print_account(const Account& a) {
    std::println("  Account{{{}, {}, {}, {}}}", show(a.name), a.age, a.balance, a.vip);
}

int main() {
    std::println("draft:");
    hkd::map<Account, std::optional> draft{};
    draft.name = "ada";
    draft.balance = 12.5;
    hkd::map<Account, text> status = hkd::transform<text>(draft, [](const auto& o) {
        return o ? "set to " + show(*o) : std::string("missing");
    });
    hkd::for_each(status, [](std::string_view name, const std::string& s) { std::println("  {}: {}", name, s); });
    std::println("complete: {}", hkd::sequence(draft).has_value());
    draft.age = 36;
    draft.vip = false;
    std::optional<Account> done = hkd::sequence(draft);
    std::println("complete after age and vip: {}", done.has_value());
    if (done) print_account(*done);

    std::println("columns:");
    hkd::map<Account, std::vector> cols{};
    cols.name = {"ada", "bob", "cy"};
    cols.age = {36, 41, 29};
    cols.balance = {12.5, 0, -3};
    cols.vip = {false, true, false};
    print_account(hkd::at(cols, 1));
    print_account(hkd::at(cols, 2));
    cols.vip.pop_back();
    try {
        print_account(hkd::at(cols, 2));
    } catch (const std::out_of_range&) {
        std::println("  row 2 is out of range");
    }
    hkd::map<Account, flag> nonempty = hkd::transform<flag>(cols, [](const auto& c) { return c.size() == 3; });
    hkd::for_each(nonempty, [](std::string_view name, bool full) { std::println("  {} has 3: {}", name, full); });
    hkd::map<Account, std::optional> last = hkd::transform<std::optional>(cols, [](const auto& c) {
        return c.empty() ? std::nullopt : std::optional(c.back());
    });
    print_account(hkd::sequence(last).value());

    std::println("references:");
    Account first = hkd::at(cols, 0);
    hkd::map<Account, std::reference_wrapper> view = hkd::refs(first);
    view.age.get() += 1;
    view.name.get() += " lovelace";
    hkd::for_each(view, [](std::string_view name, auto& ref) {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(ref.get())>, double>) ref.get() *= 2;
        std::println("  {} = {}", name, show(ref.get()));
    });
    print_account(first);

    std::println("versions:");
    hkd::map<Account, versioned> history = hkd::transform<versioned>(view, [](const auto& ref) {
        return versioned<std::remove_cvref_t<decltype(ref.get())>>{ref.get(), 1};
    });
    hkd::for_each(history, [](std::string_view, auto& v) { ++v.version; });
    history.age.value = 99;
    hkd::for_each(std::as_const(history), [](std::string_view name, const auto& v) {
        std::println("  {} v{}: {}", name, v.version, show(v.value));
    });

    std::println("owned:");
    hkd::map<Account, std::unique_ptr> owned = hkd::transform<std::unique_ptr>(last, [](const auto& o) {
        return std::make_unique<std::remove_cvref_t<decltype(*o)>>(*o);
    });
    hkd::for_each(owned, [](std::string_view name, const auto& p) { std::println("  {} -> {}", name, show(*p)); });
}
