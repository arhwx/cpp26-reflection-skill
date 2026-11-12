I want Haskell's higher-kinded data for plain structs. From one struct `T` and one wrapper template `F`, I want a struct with the same member names whose member types are all wrapped in `F`. With `std::optional` it is a form being filled in, with `std::vector` it is a table of columns, with `std::reference_wrapper` it is a view of an object, and with a template that ignores its argument it is one label or flag per member. A few generic functions work on all of them.

Everything goes in `namespace hkd`. Below, "the members of `T`" means the non-static data members of `T`, in declaration order. The test structs are aggregates with only public members and no base classes. `F` and `G` stand for templates that take a type and can be passed as a `template <class> class` template argument: class templates such as `std::optional`, also ones with defaulted extra parameters such as `std::vector` and `std::unique_ptr`, and alias templates, including ones whose result does not depend on the argument.

API:
- `hkd::map<T, F>`: a class template, not an alias template, so the functions below can deduce `T` and `F` from an argument of type `map<T, F>`. It is an aggregate. For each member of `T`, with type `M`, it has a public data member with the same name and type `F<M>`, in the same order. These members may be inherited from a base class, but `map<T, F>` has no other data members. `map<T, F>{}` value-initializes every member when every `F<M>` is default constructible.
- `template <class T, template <class> class F, class Fn> void hkd::for_each(map<T, F>& m, Fn fn)`, and the same for `const map<T, F>&`: for each member of `T`, in order, calls `fn(name, x)`, where `name` is a `std::string_view` holding the member name and `x` is the member of `m` with that name, as an lvalue that is `const` exactly when `m` is.
- `template <template <class> class G, class T, template <class> class F, class Fn> map<T, G> hkd::transform(const map<T, F>& m, Fn fn)`: starts from a value-initialized `map<T, G>`, then for each member of `T`, in order, assigns `fn(x)` to the result's member of that name, where `x` is the member of `m` with that name as a `const` lvalue. `fn` is called exactly once per member. Returns the result.
- `template <class T> std::optional<T> hkd::sequence(const map<T, std::optional>& m)`: if every member of `m` holds a value, returns a value-initialized `T` whose members are then assigned the held values, in order. Otherwise returns `std::nullopt`.
- `template <class T> T hkd::at(const map<T, std::vector>& columns, std::size_t i)`: a value-initialized `T` whose members are then assigned, in order, the element `i` of the column of the same name. If `i` is not less than the size of some column, it throws `std::out_of_range`.
- `template <class T> map<T, std::reference_wrapper> hkd::refs(T& obj)`: each member refers to the member of `obj` with the same name.

Nothing may be written per struct type or per wrapper.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
draft:
  name: set to "ada"
  age: missing
  balance: set to 12.5
  vip: missing
complete: false
complete after age and vip: true
  Account{"ada", 36, 12.5, false}
columns:
  Account{"bob", 41, 0, true}
  Account{"cy", 29, -3, false}
  row 2 is out of range
  name has 3: true
  age has 3: true
  balance has 3: true
  vip has 3: false
  Account{"cy", 29, -3, true}
references:
  name = "ada lovelace"
  age = 37
  balance = 25
  vip = false
  Account{"ada lovelace", 37, 25, false}
versions:
  name v2: "ada lovelace"
  age v2: 99
  balance v2: 25
  vip v2: false
owned:
  name -> "cy"
  age -> 29
  balance -> -3
  vip -> true
```

Write the complete header to `solution.hpp` in the current directory.
