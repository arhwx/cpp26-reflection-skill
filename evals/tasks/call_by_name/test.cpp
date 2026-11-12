#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <string_view>

class Counter {
public:
    Counter() = default;
    explicit Counter(int start) : value(start) {}
    Counter(const Counter&) = default;
    Counter& operator=(const Counter&) = default;
    ~Counter() {}

    bool operator==(const Counter&) const = default;
    Counter& operator++() {
        ++value;
        return *this;
    }
    int operator()() const { return -1; }

    int get() const { return value; }
    int doubled() const { return value * 2; }
    int bump() { return ++value; }
    int total() const { return value + step; }
    int total(int extra) const { return value + step + extra; }
    int add(int n) const { return value + n; }
    static int zero() { return 0; }
    void reset() { value = 0; }

    int value = 0;
    int step = 10;
    std::string label = "counter";

private:
    int secret() const { return 42; }
};

void show(Counter& c, std::string_view name) {
    std::optional<int> r = call_by_name(c, name);
    if (r)
        std::println("[{}] -> {}", name, *r);
    else
        std::println("[{}] -> nullopt", name);
}

int main() {
    Counter c(3);
    const char* names[] = {"get",   "doubled", "bump",    "bump",     "get",        "total",
                           "add",   "secret",  "zero",    "value",    "label",      "Counter",
                           "~Counter", "operator()", "Get", "",        "missing",    "reset"};
    for (const char* n : names)
        show(c, n);
}
