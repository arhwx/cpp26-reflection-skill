#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>
#include <vector>

using App = ini::schema<R"(; sample settings, also the defaults
name = demo
verbose = false
version = 1.2.3

[server]
host = localhost
port = 8080
timeout = 2.5
banner = "  welcome  "

  # the database
[ database ]
url = postgres://db/app?mode=ro
pool = 4
ratio = -0.25
code = "42"

[empty]
)">;

static_assert(std::is_same_v<decltype(App::type{}.name), std::string>);
static_assert(std::is_same_v<decltype(App::type{}.verbose), bool>);
static_assert(std::is_same_v<decltype(App::type{}.server.port), int>);
static_assert(std::is_same_v<decltype(App::type{}.server.timeout), double>);
static_assert(std::is_same_v<decltype(App::type{}.database.code), std::string>);
static_assert(std::is_aggregate_v<App::type> && std::is_aggregate_v<decltype(App::type{}.server)>);

template <class T>
concept has_port = requires(T t) { t.server.port; };
template <class T>
concept has_top_port = requires(T t) { t.port; };

void report(const std::vector<std::string>& errors) {
    std::println("{} errors", errors.size());
    for (const std::string& e : errors) std::println("  {}", e);
}

int main() {
    std::println("{}", App::describe());
    std::println("server.port: {}, port: {}", has_port<App::type>, has_top_port<App::type>);

    App::type cfg = App::defaults();
    std::println("defaults: port {} timeout {} banner [{}] code {} ratio {}", cfg.server.port, cfg.server.timeout,
                 cfg.server.banner, cfg.database.code, cfg.database.ratio);
    std::println("---");
    std::print("{}", App::write(cfg));
    std::println("---");

    std::vector<std::string> errors;
    App::type live = App::parse(R"(name = "production"
verbose = yes
[server]
port = 9090
hots = example.org
timeout = 30
port = 9091
verbose = true
[cache]
size = 10
this line is ignored
[database]
pool = many
ratio=0.5
url = sqlite://
broken line
code = 7
  ; the end
[empty]
)", errors);
    report(errors);
    std::println("---");
    std::print("{}", App::write(live));
    std::println("---");

    std::vector<std::string> again;
    App::type back = App::parse(App::write(live), again);
    std::println("round trip: {} errors, same text: {}", again.size(), App::write(back) == App::write(live));

    std::vector<std::string> more;
    App::type odd = App::parse("server = 1\n[server]\nport = -0\nport = 99999999999\nhost =\n", more);
    report(more);
    std::println("port {} host [{}]", odd.server.port, odd.server.host);

    std::println("{}", ini::problem("a = 1\n[s]\nb = true\n"));
    std::println("{}", ini::problem("a = 1\nb\n"));
    std::println("{}", ini::problem("[server]\nmax-conn = 4\n"));
    std::println("{}", ini::problem("[s]\nb = 1\n\nb = 2\n"));
    std::println("{}", ini::problem("[s]\nb = 2\n[t]\n[s]\n"));
    std::println("{}", ini::problem("a = 1\n[a]\n"));
    std::println("{}", ini::problem("[1st]\n"));
}
