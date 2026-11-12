Our services read their configuration from environment variables, and every service has a hand-written list that maps names like `APP_DB_HOST` to `config.db.host`. I want that mapping derived from the config struct: the variable name comes from the path of member names, and the value is parsed according to the member's type.

Everything goes in `namespace env`. All structs involved are aggregates whose members are all public.

Annotations, written on members:
- `[[=env::name("TEXT")]]`: the member's segment (see below) is `TEXT`, used exactly as written.
- `[[=env::required]]`, where `env::required` is a constant: loading reports an error when the member's variable is not set. Only used on value members, not on nested structs.

Member types:
- Scalar types: `std::string`, `bool`, integer types other than `bool` and the character types, floating-point types, and enumeration types.
- Value members have a scalar type, `std::optional<S>` or `std::vector<S>` for a scalar type `S`.
- Any other aggregate class type is a nested struct, whose members are handled with the same rules.
- A struct is loadable when every member is a value member or a member of a loadable nested struct. For a type that is not loadable, `env::load` and `env::variables` must not compile. They must fail as part of overload resolution, so that a requires-expression containing the call is `false`. The test checks this with a concept.

Variable names:
- A member's segment is the text of its `env::name` annotation if it has one, and otherwise its name with the ASCII letters `a` to `z` turned into upper case.
- The variable of a value member is the prefix, then for each member on the path from the top-level struct down to that member, `_` followed by the segment. So with prefix `APP`, `config.db.host` reads `APP_DB_HOST`. Nested structs have no variable of their own.
- The variables of a struct are its value members' variables, in declaration order, depth first.

API:
- `template <class T> std::vector<std::string> variables(std::string_view prefix)`: the variables of `T`.
- `template <class T> std::vector<std::string> load(T& config, std::string_view prefix)`: for each variable of `T`, in the same order, reads the environment variable of that name and updates the member. It returns the error messages, in that order, and an empty vector when there are none. One error does not stop the others.

Loading one value member with variable `VAR`:
- Not set: the member keeps its value. If the member is `env::required`, the error is `VAR: required but not set`.
- Set, even to an empty string: the text is parsed by the rules below. On success the member is assigned the parsed value. On failure the member keeps its value and the error is `VAR: invalid value 'TEXT'`, with `TEXT` the variable's text exactly as set.

Parsing text for a scalar type:
- `std::string`: the text itself.
- `bool`: `true`, `1`, `yes` or `on` give `true`; `false`, `0`, `no` or `off` give `false`. Anything else fails. The comparison is case-sensitive.
- integer and floating-point types: `std::from_chars` into the member's type, in its default format, must succeed and use the whole text. So a leading space, a leading `+` or a value out of range fails.
- enumeration types: the name of one of its enumerators, exactly. Anything else fails.

`std::optional<S>` parses the text as `S` and becomes engaged on success. `std::vector<S>` splits the text at every `,` and parses each piece as `S`, without trimming; it fails if any piece fails. An empty text gives an empty vector.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
variables:
  MYAPP_NAME
  MYAPP_LOG_LEVEL
  MYAPP_DB_HOST
  MYAPP_DB_PORT
  MYAPP_DB_PASSWORD
  MYAPP_DB_POOL
  MYAPP_REDIS_ENABLED
  MYAPP_REDIS_TTL
  MYAPP_REDIS_SHARDS
  MYAPP_HOSTS
  MYAPP_PORTS
nothing set:
  1 error(s)
  MYAPP_DB_PASSWORD: required but not set
  name=demo log_level=info db=localhost:5432 password= pool=4
  cache enabled=false ttl=1.5 shards=none hosts=[] ports=[80]
all valid:
  0 error(s)
  name=demo log_level=warn db=db.internal:6543 password=swordfish pool=16
  cache enabled=true ttl=0.25 shards=3 hosts=["a.example", "b.example"] ports=[]
some invalid:
  5 error(s)
  MYAPP_LOG_LEVEL: invalid value 'Warn'
  MYAPP_DB_PORT: invalid value '70000'
  MYAPP_REDIS_ENABLED: invalid value 'maybe'
  MYAPP_REDIS_SHARDS: invalid value 'two'
  MYAPP_PORTS: invalid value '8080,x'
  name= log_level=warn db=db.internal:6543 password=swordfish pool=-3
  cache enabled=true ttl=1000 shards=3 hosts=["c.example"] ports=[]
db alone:
  2 error(s)
  PG_PORT: invalid value ' 15'
  PG_PASSWORD: required but not set
  pg.local:5432 pool=4
flags:
  1 error(s)
  F_LEVELS: invalid value 'error,debug,,info'
  debug=false levels=0
  0 error(s)
  debug=false levels=3
loadable Config: true
loadable Db: true
loadable Pointer: false
loadable HasPointer: false
```

Write the complete header to `solution.hpp` in the current directory.
