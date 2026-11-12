I want Erlang-style actors for plain classes: every handler member function of a class becomes a message type, messages are queued in a mailbox, and the mailbox later delivers them to the object in order. Nobody should write the message structs or the dispatch code by hand.

Everything goes in `namespace actor`.

Handlers of a class `T`:
- The handlers are the public non-static member functions declared in `T` itself that return `void`, in declaration order.
- Constructors, the destructor, assignment operators and other operator functions, static member functions, member function templates, non-public member functions and member functions that return anything other than `void` are not handlers. Data members are not handlers either, even when they are callable.
- `const` member functions returning `void` are handlers. The test's handlers all have distinct names.

API:
- `actor::msg<T, "deposit">`: an alias template whose second template argument is a string literal naming a handler of `T`. It is the message type of that handler: an aggregate struct with one public non-static data member per parameter of the handler, named after the parameter, in parameter order, and no other non-static data members. Each member's type is the parameter's type with references and top-level `const` removed, so a message owns copies of its arguments. A handler without parameters gives an empty struct. Naming `msg` again with the same arguments gives the same type.
- `actor::has_message<T, "deposit">`: a `constexpr bool` variable template, `true` exactly when `T` has a handler with that name.
- `template <class T> using message_of = std::variant<...>;`: a `std::variant` with the message type of every handler of `T` as alternatives, in handler declaration order.
- `template <class T> std::string describe(const message_of<T>& m)`: called as `actor::describe<T>(m)`. Returns the handler name of the held message, then the stored values in parentheses: `name(param=value, ...)`, with entries joined by `, `, or `name()` for a message without values. A `std::string` value is written in double quotes with no escaping; every other value (the test uses `int`, `double` and `bool`) is written the way `std::format("{}", v)` writes it.
- `template <class T> class mailbox`:
  - `explicit mailbox(T& target)`: a mailbox that delivers to `target`, which outlives it.
  - `void send(message_of<T> m)`: appends `m` to the end of the queue. Nothing is called.
  - `std::size_t pending() const`: the number of queued messages.
  - `bool step()`: returns `false` if the queue is empty. Otherwise removes the oldest message from the queue, then calls its handler on the target with the stored values as arguments, in parameter order (they may be moved from), and returns `true`. A handler may call `send` on the same mailbox; those messages go to the end of the queue.
  - `std::size_t run()`: calls `step()` until the queue is empty, including messages sent while running, and returns the number of messages delivered.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <functional>
#include <print>
#include <string>
#include <type_traits>
#include <variant>

struct Account {
    std::string owner = "ada";
    int balance = 0;
    double rate = 0;
    bool frozen = false;
    std::function<void(int)> on_overdraft;

    void deposit(int amount, const std::string& memo) {
        balance += amount;
        std::println("  deposit {} ({}), balance {}", amount, memo, balance);
    }
    void withdraw(int amount) {
        if (frozen) {
            std::println("  withdraw {} refused, frozen", amount);
            return;
        }
        balance -= amount;
        std::println("  withdraw {}, balance {}", amount, balance);
        if (balance < 0 && on_overdraft) on_overdraft(balance);
    }
    void rename(std::string name) {
        std::println("  rename {} -> {}", owner, name);
        owner = std::move(name);
    }
    void freeze() {
        frozen = true;
        std::println("  frozen");
    }
    void set_rate(double percent, bool compound) {
        rate = percent;
        std::println("  rate {}{}", percent, compound ? " compound" : "");
    }
    void report() const { std::println("  report: {} has {}{}", owner, balance, frozen ? " (frozen)" : ""); }

    int total() const { return balance; }
    static Account make(std::string owner) { return Account{.owner = owner}; }
    template <class F>
    void each(F f) const { f(balance); }
    bool operator==(const Account&) const = default;

private:
    void audit(int code) { std::println("  audit {}", code); }
};

using Msg = actor::message_of<Account>;

void post(actor::mailbox<Account>& box, Msg m) {
    std::println("send {}", actor::describe<Account>(m));
    box.send(std::move(m));
}

int main() {
    std::println("messages: {}", std::variant_size_v<Msg>);
    std::println("has deposit: {}", actor::has_message<Account, "deposit">);
    std::println("has report: {}", actor::has_message<Account, "report">);
    std::println("has total: {}", actor::has_message<Account, "total">);
    std::println("has make: {}", actor::has_message<Account, "make">);
    std::println("has each: {}", actor::has_message<Account, "each">);
    std::println("has audit: {}", actor::has_message<Account, "audit">);
    std::println("has owner: {}", actor::has_message<Account, "owner">);

    using Deposit = actor::msg<Account, "deposit">;
    using Freeze = actor::msg<Account, "freeze">;
    std::println("memo is std::string: {}", std::is_same_v<decltype(Deposit::memo), std::string>);
    std::println("amount is int: {}", std::is_same_v<decltype(Deposit::amount), int>);
    std::println("freeze is empty: {}", std::is_empty_v<Freeze>);
    std::println("rename index: {}", Msg{actor::msg<Account, "rename">{"bob"}}.index());
    std::println("report index: {}", Msg{actor::msg<Account, "report">{}}.index());

    Account acct;
    actor::mailbox<Account> box(acct);
    bool alerted = false;
    acct.on_overdraft = [&](int balance) {
        std::println("  overdraft at {}{}", balance, alerted ? "" : ", queue freeze");
        if (alerted) return;
        alerted = true;
        post(box, Freeze{});
        post(box, actor::msg<Account, "report">{});
    };

    std::string memo = "salary";
    post(box, Deposit{100, memo});
    memo = "changed later";
    post(box, actor::msg<Account, "set_rate">{.percent = 2.5, .compound = true});
    post(box, actor::msg<Account, "withdraw">{.amount = 30});
    post(box, actor::msg<Account, "rename">{.name = "ada lovelace"});
    post(box, actor::msg<Account, "withdraw">{.amount = 100});
    post(box, actor::msg<Account, "withdraw">{.amount = 5});
    std::println("pending: {}, balance still {}", box.pending(), acct.balance);

    std::println("step");
    std::println("stepped: {}", box.step());
    std::println("pending: {}", box.pending());

    std::println("run");
    std::size_t done = box.run();
    std::println("processed: {}, pending: {}", done, box.pending());
    std::println("step on empty: {}", box.step());

    post(box, actor::msg<Account, "withdraw">{10});
    post(box, actor::msg<Account, "deposit">{.amount = 50, .memo = "refund"});
    std::println("run again");
    std::println("processed: {}", box.run());
    std::println("final: {} {} {} {}", acct.owner, acct.balance, acct.rate, acct.frozen);
}
```

It must print exactly:

```
messages: 6
has deposit: true
has report: true
has total: false
has make: false
has each: false
has audit: false
has owner: false
memo is std::string: true
amount is int: true
freeze is empty: true
rename index: 2
report index: 5
send deposit(amount=100, memo="salary")
send set_rate(percent=2.5, compound=true)
send withdraw(amount=30)
send rename(name="ada lovelace")
send withdraw(amount=100)
send withdraw(amount=5)
pending: 6, balance still 0
step
  deposit 100 (salary), balance 100
stepped: true
pending: 5
run
  rate 2.5 compound
  withdraw 30, balance 70
  rename ada -> ada lovelace
  withdraw 100, balance -30
  overdraft at -30, queue freeze
send freeze()
send report()
  withdraw 5, balance -35
  overdraft at -35
  frozen
  report: ada lovelace has -35 (frozen)
processed: 7, pending: 0
step on empty: false
send withdraw(amount=10)
send deposit(amount=50, memo="refund")
run again
  withdraw 10 refused, frozen
  deposit 50 (refund), balance 15
processed: 2
final: ada lovelace 15 2.5 true
```

Write the complete header to `solution.hpp` in the current directory.
