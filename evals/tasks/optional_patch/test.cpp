#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

struct Config {
    std::string host = "localhost";
    int port = 8080;
    bool verbose = false;
    double timeout = 1.5;
    std::vector<int> retries{1, 2};
};

struct Point {
    int x;
    int y;
};

static_assert(std::is_same_v<decltype(patch_of<Config>::port), std::optional<int>>);
static_assert(std::is_same_v<decltype(patch_of<Config>::host), std::optional<std::string>>);
static_assert(std::is_same_v<decltype(patch_of<Config>::retries), std::optional<std::vector<int>>>);
static_assert(std::is_same_v<decltype(patch_of<Point>::y), std::optional<int>>);
static_assert(!std::is_same_v<patch_of<Config>, patch_of<Point>>);

void print(const Config& c) {
    std::println("{}:{} verbose={} timeout={} retries={}", c.host, c.port, c.verbose, c.timeout, c.retries.size());
}

int main() {
    Config cfg;
    print(cfg);

    patch_of<Config> p;
    std::println("engaged {} {} {} {} {}", p.host.has_value(), p.port.has_value(), p.verbose.has_value(),
                 p.timeout.has_value(), p.retries.has_value());
    p.port = 9000;
    apply_patch(cfg, p);
    print(cfg);

    patch_of<Config> q;
    q.host = "example.org";
    q.verbose = true;
    q.retries = std::vector<int>{};
    apply_patch(cfg, q);
    print(cfg);

    apply_patch(cfg, patch_of<Config>{});
    print(cfg);

    Point pt{1, 2};
    apply_patch(pt, patch_of<Point>{.x = {}, .y = -7});
    std::println("{} {}", pt.x, pt.y);
    apply_patch(pt, patch_of<Point>{3, std::nullopt});
    std::println("{} {}", pt.x, pt.y);
}
