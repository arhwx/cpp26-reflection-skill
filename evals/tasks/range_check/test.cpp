#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

struct Player {
    std::string name;
    [[=check::range{0, 150}]] int age;
    [[=check::range{1, 10}]] int level;
    int score;
    [[=check::range{-50, 50}]] int balance;
};

struct Plain {
    int x;
    int y;
};

void report(const std::vector<std::string>& errors) {
    if (errors.empty()) {
        std::println("valid");
        return;
    }
    for (const std::string& e : errors)
        std::println("{}", e);
}

int main() {
    report(validate(Player{"ann", 30, 5, -999, 0}));
    std::println("--");
    report(validate(Player{"bob", 151, 0, 7, 50}));
    std::println("--");
    report(validate(Player{"cid", -1, 10, 0, -51}));
    std::println("--");
    report(validate(Player{"dee", 0, 1, 0, -50}));
    std::println("--");
    report(validate(Plain{-1000, 1000}));
}
