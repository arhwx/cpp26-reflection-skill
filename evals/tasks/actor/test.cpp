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
