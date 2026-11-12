#include "solution.hpp"

#include <cstdint>
#include <optional>
#include <print>
#include <string>
#include <string_view>

namespace app {

struct [[=orm::table("users")]] User {
    [[=orm::primary_key]] std::int64_t id;
    [[=orm::column("user_name")]] [[=orm::unique]] [[=orm::max_length(32)]] std::string name;
    std::optional<std::string> email;
    bool active;
    static constexpr int version = 3;  // not a column
};

struct [[=orm::table("posts")]] Post {
    [[=orm::primary_key]] std::int64_t id;
    [[=orm::references(^^User::id)]] std::int64_t author;
    std::string body;
    double score;
    std::optional<short> views;
};

// no table annotation: the table is named after the struct
struct Comment {
    [[=orm::primary_key]] int id;
    [[=orm::references(^^Post::id)]] std::int64_t post;
    [[=orm::references(^^Comment::id)]] std::optional<int> reply_to;
    [[=orm::references(^^User::name)]] [[=orm::max_length(32)]] std::string by;
    float weight;
};

struct [[=orm::table("tags")]] Tag {
    [[=orm::column("tag_id")]] [[=orm::primary_key]] unsigned char id;
    [[=orm::unique]] std::string label;
};

}  // namespace app

// tables with schema errors
struct Pointer {
    [[=orm::primary_key]] int id;
    const char* name;
};

struct [[=orm::table("pairs")]] TwoKeys {
    [[=orm::primary_key]] int left;
    [[=orm::primary_key]] int right;
};

struct NotKey {
    [[=orm::primary_key]] int id;
    [[=orm::references(^^app::Post::body)]] std::string body;
};

struct Narrow {
    [[=orm::references(^^app::User::id)]] int user;
};

struct LongName {
    [[=orm::references(^^app::User::name)]] std::string user;
};

static_assert(orm::create_table<app::Tag>.starts_with("CREATE TABLE tags (\n"));
static_assert(orm::create_table<app::Tag>.ends_with("\n);"));
static_assert(orm::schema_error<app::Comment>().empty());

template <class T>
void report(std::string_view name) {
    constexpr std::string_view error = orm::schema_error<T>();
    std::println("{}: {}", name, error.empty() ? "ok" : error);
}

int main() {
    std::println("{}", orm::create_table<app::Post>);
    std::println("--- schema");
    std::println("{}", orm::create_schema<app::Comment, app::Tag, app::Post, app::User>);
    std::println("--- checks");
    report<app::User>("User");
    report<app::Comment>("Comment");
    report<Pointer>("Pointer");
    report<TwoKeys>("TwoKeys");
    report<NotKey>("NotKey");
    report<Narrow>("Narrow");
    report<LongName>("LongName");
}
