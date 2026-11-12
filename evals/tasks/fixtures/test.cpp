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
