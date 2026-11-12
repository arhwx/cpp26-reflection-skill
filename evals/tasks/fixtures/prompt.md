I want pytest-style fixtures for plain C++ test functions. A test asks for what it needs by naming its parameters after fixture functions, and the runner builds those objects, passes them in and tears them down again. Nothing is registered by hand: the runner finds the tests and fixtures by looking at a namespace.

Everything goes in `namespace fixtures`:
- `template <std::meta::info Suite> int fixtures::run()`: `Suite` is a reflection of a namespace. Runs every test of that namespace as described below and returns the number of failed tests.
- `consteval std::string_view fixtures::check(std::meta::info suite)` (`constexpr` is fine too): `"ok"` when the namespace `suite` is a valid suite, otherwise a message that describes the first problem, as described below.

Tests and fixtures:
- Every function declared directly in the suite namespace is either a test, when its name starts with `test_`, or a fixture, when it does not. Everything else in the namespace, such as classes, is ignored. Function names in a suite are never overloaded.
- A test returns `bool`: `true` means it passed.
- Each parameter of a test or of a fixture asks for the fixture whose name is the parameter's name. When the function is called, that parameter receives the object returned by that fixture. In a suite given to `run`, each parameter type is `U&` or `const U&`, where `U` is the return type of that fixture.

What `run` does:
- It runs the tests in ascending order of their names, compared byte by byte.
- Each test gets fresh fixtures: nothing is kept from one test to the next. A test gets only the fixtures it needs directly or through other fixtures, and each of those is created at most once for that test, so all parameters that ask for the same fixture get the same object.
- Creation order: before a function (test or fixture) is called, its parameters are handled left to right. For each parameter whose fixture was not created yet for this test, the runner first handles that fixture's own parameters in the same way and then calls the fixture. When all parameters of the test are handled, the test is called.
- The runner keeps the object that a fixture returns. Fixture return types may be neither copyable nor movable, like `Database` and `Session` in the test, so the returned object must be stored without being copied or moved.
- When the test returns, the runner prints `PASS <test name>` or `FAIL <test name>` on its own line, and only then destroys that test's fixture objects, in the reverse of the order in which they were created.
- After the last test it prints `<passed> passed, <failed> failed` on its own line and returns the number of failed tests.
- `run` for a suite where `check` does not return `"ok"` must not compile.

What `check` reports:
- It walks the suite like this: visit every fixture in ascending order of name, then every test in ascending order of name. Visiting a function that was already visited to the end does nothing. Otherwise, for each parameter from left to right, visit the fixture that it asks for, and then the function's visit ends.
- The first problem found on this walk is reported, and the walk stops there.
- A parameter whose name is not the name of a fixture gives `<function> asks for '<parameter>', which is not a fixture`.
- Visiting a function whose visit has started and not ended is a cycle. It gives `fixture cycle: ` followed by the names of the functions on the current visit path, starting with the function that is reached again, joined by ` -> `, and then ` -> ` and that function's name again. So `a` asks for `c`, `c` asks for `b`, and `b` asks for `a` gives `fixture cycle: a -> c -> b -> a`.
- `check` only looks at names, so parameter types do not matter to it, and functions given to `check` alone need no definitions.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <utility>

namespace suite {
struct Config {
    std::string url;
};

struct Database {
    explicit Database(std::string url) : url(std::move(url)) { std::println("  open {}", this->url); }
    Database(const Database&) = delete;
    ~Database() { std::println("  close {}", url); }

    std::string url;
    int queries = 0;
};

struct Session {
    Session(Database& db, std::string user) : db(db), user(std::move(user)) { std::println("  login {}", this->user); }
    Session(const Session&) = delete;
    ~Session() { std::println("  logout {}", user); }

    Database& db;
    std::string user;
};

Config config() { return {"mem://test"}; }
Database database(const Config& config) { return Database(config.url); }
Session admin(Database& database) { return Session(database, "root"); }
Session guest(Database& database, const Config& config) { return Session(database, config.url.size() ? "guest" : "nobody"); }

bool test_config_is_in_memory(const Config& config) { return config.url.starts_with("mem:"); }
bool test_database_starts_empty(const Database& database) { return database.queries == 0; }
bool test_admin_shares_the_database(Session& admin, Database& database) {
    admin.db.queries++;
    return database.queries == 1 && admin.user == "root";
}
bool test_two_sessions(Session& guest, Session& admin) { return &guest.db == &admin.db; }
bool test_that_fails(const Config& config) { return config.url.empty(); }
bool test_without_fixtures() { return true; }
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
    std::println("run returned {}", failed);
    std::println("{}", fixtures::check(^^suite));
    std::println("{}", fixtures::check(^^typo));
    std::println("{}", fixtures::check(^^loop));
}
```

It must print exactly:

```
  open mem://test
  login root
PASS test_admin_shares_the_database
  logout root
  close mem://test
PASS test_config_is_in_memory
  open mem://test
PASS test_database_starts_empty
  close mem://test
FAIL test_that_fails
  open mem://test
  login guest
  login root
PASS test_two_sessions
  logout root
  logout guest
  close mem://test
PASS test_without_fixtures
5 passed, 1 failed
run returned 1
ok
test_time asks for 'clokc', which is not a fixture
fixture cycle: a -> c -> b -> a
```

Write the complete header to `solution.hpp` in the current directory.
