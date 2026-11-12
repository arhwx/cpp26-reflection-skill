I want a tiny dependency injector: each class asks for what it needs in its constructor, and the injector creates every object once and hands out shared references, without registering any class by hand.

API:
- `class injector`: default constructible.
- `template <class T> T& get()`: returns this injector's `T`, creating it on the first call. Later calls return the same object.

Creating a `T`:
- `get` uses the public constructor of `T` that is not a copy or move constructor. The test only uses classes that have exactly one; for any other class, `get<T>` must not compile.
- Each parameter of that constructor is `U&` or `const U&`, and its argument is `get<U>()` of the same injector. The arguments are resolved in parameter order, left to right, and then `T` is constructed.

Ownership:
- Each injector owns the objects it creates, and two injectors never share an object.
- When the injector is destroyed, it destroys its objects in the reverse of the order in which their constructors finished, so every object is destroyed before the objects it depends on.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>

struct Logger {
    Logger() { std::println("  create Logger"); }
    ~Logger() { std::println("  destroy Logger"); }
};

struct Config {
    Config() { std::println("  create Config"); }
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    ~Config() { std::println("  destroy Config"); }

    int port = 8080;
};

struct Repo {
    explicit Repo(Logger& log) : log(log) { std::println("  create Repo"); }
    ~Repo() { std::println("  destroy Repo"); }

    Logger& log;
};

struct Service {
    Service(Repo& repo, Logger& log) : repo(repo), log(log) { std::println("  create Service"); }
    ~Service() { std::println("  destroy Service"); }

    Repo& repo;
    Logger& log;
};

struct App {
    App(Service& service, Repo& repo) : service(service), repo(repo) { std::println("  create App"); }
    ~App() { std::println("  destroy App"); }

    Service& service;
    Repo& repo;
};

struct Mailer {
    Mailer(const Config& config, Logger& log) : config(config), log(log) { std::println("  create Mailer"); }
    ~Mailer() { std::println("  destroy Mailer"); }

    const Config& config;
    Logger& log;
};

struct Clock {
    int ticks = 0;
};

class Cache {
public:
    explicit Cache(Clock& clock) : clock(&clock) { std::println("  create Cache"); }
    ~Cache() { std::println("  destroy Cache"); }
    static Cache detached() { return Cache(nullptr); }

    Clock* clock;

private:
    explicit Cache(Clock* clock) : clock(clock) {}
};

int main() {
    {
        injector inj;
        std::println("get App");
        App& app = inj.get<App>();
        std::println("get App again");
        App& again = inj.get<App>();
        std::println("same App: {}", &app == &again);
        std::println("App and Service share Repo: {}", &app.repo == &app.service.repo);
        std::println("Repo and Service share Logger: {}", &app.repo.log == &app.service.log);
        std::println("get Logger is shared: {}", &inj.get<Logger>() == &app.service.log);

        std::println("get Mailer");
        Mailer& mailer = inj.get<Mailer>();
        std::println("Mailer port: {}", mailer.config.port);
        std::println("Mailer shares Logger: {}", &mailer.log == &app.repo.log);

        std::println("get Cache");
        Cache& cache = inj.get<Cache>();
        inj.get<Clock>().ticks = 5;
        std::println("Cache sees ticks: {}", cache.clock->ticks);
        std::println("destroy injector");
    }
    {
        injector other;
        std::println("get Mailer from a new injector");
        other.get<Mailer>();
        std::println("destroy injector");
    }
}
```

It must print exactly:

```
get App
  create Logger
  create Repo
  create Service
  create App
get App again
same App: true
App and Service share Repo: true
Repo and Service share Logger: true
get Logger is shared: true
get Mailer
  create Config
  create Mailer
Mailer port: 8080
Mailer shares Logger: true
get Cache
  create Cache
Cache sees ticks: 5
destroy injector
  destroy Cache
  destroy Mailer
  destroy Config
  destroy App
  destroy Service
  destroy Repo
  destroy Logger
get Mailer from a new injector
  create Config
  create Logger
  create Mailer
destroy injector
  destroy Mailer
  destroy Logger
  destroy Config
```

Write the complete header to `solution.hpp` in the current directory.
