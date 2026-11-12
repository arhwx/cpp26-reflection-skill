#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <vector>

namespace commands {
int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int neg(int a) { return -a; }
}  // namespace commands

#include "solution.hpp"

void run(std::string_view name, std::vector<int> args) {
    std::optional<int> r = dispatch(name, args);
    if (r)
        std::println("{} -> {}", name, *r);
    else
        std::println("{} -> nullopt", name);
}

int main() {
    run("add", {2, 3});
    run("mul", {4, 5});
    run("neg", {7});
    run("add", {1});
    run("neg", {1, 2});
    run("mul", {});
    run("sub", {1, 2});
    run("commands", {});

    const int fixed[] = {-6, 7};
    std::optional<int> r = dispatch("mul", std::span<const int>(fixed));
    std::println("fixed -> {}", r.value_or(0));
}
