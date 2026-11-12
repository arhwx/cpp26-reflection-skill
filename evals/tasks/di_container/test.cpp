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
