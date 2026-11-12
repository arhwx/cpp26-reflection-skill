#include "solution.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <print>
#include <string>
#include <vector>

enum class Level { debug, info, warn, error };

constexpr const char* level_name(Level l) {
    constexpr const char* names[] = {"debug", "info", "warn", "error"};
    return names[static_cast<int>(l)];
}

struct Db {
    std::string host = "localhost";
    std::uint16_t port = 5432;
    [[=env::required]] std::string password;
    [[=env::name("POOL")]] int max_connections = 4;
};

struct Cache {
    bool enabled = false;
    double ttl = 1.5;
    std::optional<int> shards;
};

struct Config {
    std::string name = "demo";
    Level log_level = Level::info;
    Db db;
    [[=env::name("REDIS")]] Cache cache;
    std::vector<std::string> hosts;
    std::vector<int> ports{80};
};

struct Pointer {
    int* p;
};

struct HasPointer {
    int id;
    Pointer inner;
};

struct Flags {
    std::optional<bool> debug;
    std::vector<Level> levels;
};

template <class T>
concept loadable = requires(T& config) { env::load(config, "X"); };

void print(const Config& c) {
    std::println("  name={} log_level={} db={}:{} password={} pool={}", c.name, level_name(c.log_level), c.db.host,
                 c.db.port, c.db.password, c.db.max_connections);
    std::println("  cache enabled={} ttl={} shards={} hosts={} ports={}", c.cache.enabled, c.cache.ttl,
                 c.cache.shards ? std::to_string(*c.cache.shards) : "none", c.hosts, c.ports);
}

void report(const std::vector<std::string>& errors) {
    std::println("  {} error(s)", errors.size());
    for (const std::string& e : errors)
        std::println("  {}", e);
}

// so that the outer environment does not change the output
template <class T>
void clear(const char* prefix) {
    for (const std::string& var : env::variables<T>(prefix))
        unsetenv(var.c_str());
}

int main() {
    clear<Config>("MYAPP");
    clear<Db>("PG");
    clear<Flags>("F");
    std::println("variables:");
    for (const std::string& var : env::variables<Config>("MYAPP"))
        std::println("  {}", var);

    std::println("nothing set:");
    Config config;
    report(env::load(config, "MYAPP"));
    print(config);

    setenv("MYAPP_LOG_LEVEL", "warn", 1);
    setenv("MYAPP_DB_HOST", "db.internal", 1);
    setenv("MYAPP_DB_PORT", "6543", 1);
    setenv("MYAPP_DB_PASSWORD", "swordfish", 1);
    setenv("MYAPP_DB_POOL", "16", 1);
    setenv("MYAPP_DB_MAX_CONNECTIONS", "99", 1);
    setenv("MYAPP_REDIS_ENABLED", "yes", 1);
    setenv("MYAPP_REDIS_TTL", "0.25", 1);
    setenv("MYAPP_REDIS_SHARDS", "3", 1);
    setenv("MYAPP_CACHE_TTL", "7", 1);
    setenv("MYAPP_HOSTS", "a.example,b.example", 1);
    setenv("MYAPP_PORTS", "", 1);
    std::println("all valid:");
    report(env::load(config, "MYAPP"));
    print(config);

    setenv("MYAPP_NAME", "", 1);
    setenv("MYAPP_LOG_LEVEL", "Warn", 1);
    setenv("MYAPP_DB_PORT", "70000", 1);
    setenv("MYAPP_DB_POOL", "-3", 1);
    setenv("MYAPP_REDIS_ENABLED", "maybe", 1);
    setenv("MYAPP_REDIS_TTL", "1e3", 1);
    setenv("MYAPP_REDIS_SHARDS", "two", 1);
    setenv("MYAPP_HOSTS", "c.example", 1);
    setenv("MYAPP_PORTS", "8080,x", 1);
    std::println("some invalid:");
    report(env::load(config, "MYAPP"));
    print(config);

    setenv("PG_HOST", "pg.local", 1);
    setenv("PG_PORT", " 15", 1);
    std::println("db alone:");
    Db db;
    report(env::load(db, "PG"));
    std::println("  {}:{} pool={}", db.host, db.port, db.max_connections);

    setenv("F_DEBUG", "off", 1);
    setenv("F_LEVELS", "error,debug,,info", 1);
    Flags flags;
    std::println("flags:");
    report(env::load(flags, "F"));
    std::println("  debug={} levels={}", flags.debug.has_value() ? (*flags.debug ? "true" : "false") : "none",
                 flags.levels.size());
    setenv("F_LEVELS", "error,debug,info", 1);
    report(env::load(flags, "F"));
    std::println("  debug={} levels={}", flags.debug.has_value() ? (*flags.debug ? "true" : "false") : "none",
                 flags.levels.size());

    std::println("loadable Config: {}", loadable<Config>);
    std::println("loadable Db: {}", loadable<Db>);
    std::println("loadable Pointer: {}", loadable<Pointer>);
    std::println("loadable HasPointer: {}", loadable<HasPointer>);
}
