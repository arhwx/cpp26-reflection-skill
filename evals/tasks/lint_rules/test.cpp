#include "solution.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <print>
#include <string>
#include <vector>

namespace app {

namespace ui {
struct Widget;
}

namespace db {
struct Row {
    std::int64_t id;
    double value;
};

class Connection {
public:
    Connection() = default;
    ~Connection() {}
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

private:
    int handle_ = 0;
};

struct row_cache {
    std::vector<Row> rows;
    int hitCount;
};

union Raw {
    int asInt;
    float asFloat;
};
}  // namespace db

namespace core {
struct [[=lint::max_padding{0}]] Order {
    char kind;
    double price;
    std::int32_t qty;
};

struct [[=lint::max_padding{4}]] Quote {
    double price;
    std::int32_t qty;
    char kind;
};

struct Session {
    ~Session() {}
    Session(const Session& other) = default;
    std::string user;
    int userId = 0;
};

class Mover {
public:
    Mover(Mover&&) = default;
    Mover& operator=(Mover&&) = default;
};

struct Cart {
    std::vector<Order> orders;
    db::Row* last;
    std::unique_ptr<ui::Widget> view;
    int count;
};

struct Cache {
    std::map<int, std::vector<const ui::Widget*>> by_id;
    std::map<int, db::Row> rows;
};

namespace detail {
struct Node {
    Node* next;
    ui::Widget* const* owners[2];
};
}  // namespace detail
}  // namespace core

namespace ui {
struct Widget {
    int width;
    core::Order* order;
};

struct Panel : Widget {
    Widget main;
    db::Row rows[4];
};
}  // namespace ui

namespace core {
struct Screen : ui::Panel {
    int Layer;
};
}  // namespace core

namespace tools {
struct Dump {
    ui::Widget widget;
    core::Cart cart;
};
struct HTTPClient2 {};
}  // namespace tools

struct Config {
    std::string db_host;
};

}  // namespace app

namespace clean {
struct [[=lint::max_padding{3}]] Point {
    int x;
    int y;
    char tag;
};
}  // namespace clean

int main() {
    std::println("app:");
    std::println("{}", lint::report(^^app, {^^app::db, ^^app::core, ^^app::ui}));
    std::println("app::ui with ui at the bottom:");
    std::println("{}", lint::report(^^app::ui, {^^app::ui, ^^app::db, ^^app::core}));
    std::println("app::tools as the lowest layer:");
    std::println("{}", lint::report(^^app::tools, {^^app::tools, ^^app::ui}));
    constexpr std::string_view none = lint::report(^^clean, {});
    std::println("clean: {}", none.empty() ? "no problems" : none);
}
