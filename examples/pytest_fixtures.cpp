// pytest_fixtures: a test asks for what it needs by parameter name, like pytest. every function of the suite
// namespace whose name starts with test_ is a test; every other function is a fixture that others can ask for
#include <algorithm>
#include <meta>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace fixtures {
using std::meta::info;

consteval std::vector<info> functions(info suite, bool tests) {  // sorted by name
    std::vector<info> out;
    for (info m : members_of(suite, std::meta::access_context::unprivileged()))
        if (is_function(m) && has_identifier(m) && identifier_of(m).starts_with("test_") == tests) out.push_back(m);
    std::ranges::sort(out, {}, [](info f) { return identifier_of(f); });
    return out;
}
consteval info fixture(info suite, info user, info param) {  // the fixture that a parameter of user asks for
    for (info f : functions(suite, false))
        if (identifier_of(f) == identifier_of(param)) return f;
    throw std::meta::exception(std::string(identifier_of(user)) + " asks for '" + std::string(identifier_of(param)) +
                               "', which is not a fixture", ^^fixture);
}
// appends everything fn needs and then fn itself, so dependencies always come first
consteval void visit(info suite, info fn, std::vector<info>& path, std::vector<info>& out) {
    if (std::ranges::contains(out, fn)) return;
    if (auto it = std::ranges::find(path, fn); it != path.end()) {
        std::string cycle = "fixture cycle: ";
        for (; it != path.end(); ++it) cycle += std::string(identifier_of(*it)) + " -> ";
        throw std::meta::exception(cycle + std::string(identifier_of(fn)), ^^visit);
    }
    path.push_back(fn);
    for (info p : parameters_of(fn)) visit(suite, fixture(suite, fn, p), path, out);
    path.pop_back();
    out.push_back(fn);
}
consteval std::vector<info> plan(info suite, std::vector<info> roots) {
    std::vector<info> path, out;
    for (info r : roots) visit(suite, r, path, out);
    return out;
}
consteval std::string_view check(info suite) {  // what is wrong with the suite, or "ok"
    try {
        plan(suite, functions(suite, false));
        plan(suite, functions(suite, true));
        return "ok";
    } catch (const std::meta::exception& e) {
        return std::define_static_string(std::string_view(e.what()));
    }
}

template <info Suite>
struct cache_of {
    struct type;  // one optional per fixture, in dependency order, so teardown runs in reverse
    consteval {
        std::vector<info> specs;
        for (info f : plan(Suite, functions(Suite, false)))
            specs.push_back(data_member_spec(substitute(^^std::optional, {return_type_of(f)}), {.name = identifier_of(f)}));
        define_aggregate(^^type, specs);
    }
};
consteval info slot(info cache, info f) {  // the cache member that holds fixture f
    for (info m : nonstatic_data_members_of(cache, std::meta::access_context::unprivileged()))
        if (identifier_of(m) == identifier_of(f)) return m;
    throw std::meta::exception("no slot", ^^slot);
}

// calls F with the cached fixtures it asked for
template <class Cache, info F, info... Deps>
struct call {
    static decltype(auto) run(Cache& c) { return [:F:](*c.[:Deps:]...); }
};
consteval info caller(info cache, info suite, info f) {
    std::vector<info> args{cache, std::meta::reflect_constant(f)};
    for (info p : parameters_of(f)) args.push_back(std::meta::reflect_constant(slot(cache, fixture(suite, f, p))));
    return substitute(^^call, args);
}
template <class F>
struct lazy {  // lets optional::emplace build the fixture in place, so it never has to move
    F make;
    operator std::invoke_result_t<F&>() { return make(); }
};

template <info Suite>
int run() {
    static_assert(check(Suite) == "ok", check(Suite));
    int passed = 0, failed = 0;
    if constexpr (check(Suite) == "ok") {  // a broken suite stops at the assertion above, with that one error
        using cache_t = typename cache_of<Suite>::type;
        template for (constexpr info t : std::define_static_array(functions(Suite, true))) {
            cache_t cache{};  // fresh fixtures for every test
            template for (constexpr info f : std::define_static_array(plan(Suite, {t}))) {
                if constexpr (f != t) {
                    using make = [:caller(dealias(^^cache_t), Suite, f):];
                    cache.[:slot(dealias(^^cache_t), f):].emplace(lazy{[&] { return make::run(cache); }});
                }
            }
            using test = [:caller(dealias(^^cache_t), Suite, t):];
            bool ok = test::run(cache);
            std::println("{} {}", ok ? "PASS" : "FAIL", std::meta::identifier_of(t));
            ++(ok ? passed : failed);
        }
    }
    std::println("{} passed, {} failed", passed, failed);
    return failed;
}
}  // namespace fixtures

// ---- user code
namespace suite {
struct Config {
    std::string url;
};
struct Database {
    std::string url;
    int queries = 0;
    explicit Database(std::string u) : url(std::move(u)) { std::println("  open {}", url); }
    Database(const Database&) = delete;
    ~Database() { std::println("  close {}", url); }
};
struct Client {
    Database& db;
    std::string user;
};

Config config() { return {"mem://test"}; }
Database database(const Config& config) { return Database(config.url); }
Client admin(Database& database) { return {database, "root"}; }

bool test_config_is_in_memory(const Config& config) { return config.url.starts_with("mem:"); }
bool test_database_starts_empty(const Database& database) { return database.queries == 0; }
bool test_admin_shares_the_database(Client& admin, Database& database) {
    admin.db.queries++;
    return database.queries == 1 && admin.user == "root";
}
bool test_that_fails(const Config& config) { return config.url.empty(); }
}  // namespace suite

namespace typo {
int clock();
bool test_time(int clokc);
}  // namespace typo
namespace loop {
int a(int c);
int b(int a);
int c(int b);
}  // namespace loop

int main() {
    int failed = fixtures::run<^^suite>();
    std::println("{}", fixtures::check(^^typo));
    std::println("{}", fixtures::check(^^loop));
    return failed == 1 ? 0 : 1;  // test_that_fails is there on purpose, so exactly one failure is the good outcome
}

// output:
//   open mem://test
// PASS test_admin_shares_the_database
//   close mem://test
// PASS test_config_is_in_memory
//   open mem://test
// PASS test_database_starts_empty
//   close mem://test
// FAIL test_that_fails
// 3 passed, 1 failed
// test_time asks for 'clokc', which is not a fixture
// fixture cycle: a -> c -> b -> a
