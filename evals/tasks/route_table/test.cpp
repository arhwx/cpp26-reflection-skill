#include "solution.hpp"

#include <print>
#include <string_view>

namespace audit {
struct log {
    int level;
};
}  // namespace audit

class Api {
public:
    Api() = default;
    explicit Api(int base) : base_(base) {}
    Api(const Api&) = default;
    Api& operator=(const Api&) = default;
    ~Api() {}

    bool operator==(const Api&) const = default;

    [[=web::route("/")]] int operator()() const { return base_ * 2; }

    [[=web::route("/users")]] int list_users() { return base_ + 1; }
    [[=web::route("/users/count")]] int count_users() const { return 3; }
    [[=audit::log{2}]] [[=web::route("/health"), =web::route("/status")]] int health() {
        ++hits;
        return 200;
    }
    [[=audit::log{1}]] int audited() { return 7; }
    int unrouted() { return 99; }
    [[=web::route("/version")]] static int version() { return 2; }

    [[=web::route("/hits")]] int hits = 0;

private:
    [[=web::route("/secret")]] int secret() { return 42; }
    int base_ = 10;
};

struct Empty {
    int value = 5;
    int get() { return value; }
};

int main() {
    Api api(20);
    const std::string_view paths[] = {"/users", "/users/count", "/health", "/status", "/", "/users/",
                                      "/version", "/hits", "/secret", "audited", "", "/nope"};
    for (std::string_view path : paths)
        std::println("{} -> {}", path, web::handle(api, path));
    std::println("hits {}", api.hits);

    Api fresh;
    std::println("fresh / -> {}", web::handle(fresh, "/"));

    Empty e;
    std::println("empty -> {}", web::handle(e, "/users"));
}
