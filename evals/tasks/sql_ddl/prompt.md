I want my database schema to live next to my C++ structs: each struct is a table, each data member is a column, and annotations mark keys, column names and foreign keys. A foreign key annotation holds a reflection of the member it references, so it cannot drift away from the referenced column. The `CREATE TABLE` text should be built at compile time, and schema mistakes should be reported at compile time too.

Everything goes in `namespace orm`.

Annotations, used with `[[=...]]` exactly as in the test:
- `orm::table(name)`: on a struct, with a string literal. The table name.
- `orm::column(name)`: on a data member, with a string literal. The column name.
- `orm::primary_key` and `orm::unique`: constants that mark a data member.
- `orm::max_length(n)`: on a `std::string` member, with an `int`.
- `orm::references(r)`: on a data member, where `r` is a `std::meta::info` reflecting a non-static data member of a table struct, possibly the same struct. That member is the referenced column.

API:
- `template <class T> constexpr std::string_view orm::create_table`: a variable template holding the `CREATE TABLE` statement for `T`. It is usable in constant expressions. Naming it for a `T` with a schema error must not compile.
- `template <class... Ts> constexpr std::string_view orm::create_schema`: a variable template holding the statements of all `Ts` in dependency order, separated by one empty line (`"\n\n"`), with no newline at the end. Naming it when one of the `Ts` has a schema error must not compile.
- `template <class T> consteval std::string_view orm::schema_error()` (`constexpr` is fine too): an empty string when `T` has no schema error, otherwise the message for the first error, as described below.

Tables and columns:
- The columns of a table struct are its non-static data members, in declaration order. Static data members are not columns. The test only uses public members.
- The table name is the string of the `orm::table` annotation, or the struct's unqualified name if it has none. The column name is the string of the `orm::column` annotation, or the member's name if it has none.
- A member of type `std::optional<X>` is a nullable column whose SQL type is that of `X`. Every other column is not nullable.
- The SQL type of a column depends on its C++ type, after removing `std::optional` (aliases such as `std::int64_t` stand for the type they name):
  - `bool`: `BOOLEAN`
  - any other integral type of at most 4 bytes: `INTEGER`
  - an integral type of 8 bytes: `BIGINT`
  - a floating-point type: `REAL`
  - `std::string`: `VARCHAR(n)` when the member has `orm::max_length(n)`, otherwise `TEXT`
  - any other type is unsupported.

Statement format, for `create_table`:
- The first line is `CREATE TABLE <table> (`, then one line per column, then a last line `);`. There is no newline after `);`.
- A column line is two spaces, the column name, a space and the SQL type, followed by these parts in this order: ` NOT NULL` when the column is not nullable, ` PRIMARY KEY` when it has `orm::primary_key`, ` UNIQUE` when it has `orm::unique`, and ` REFERENCES <table>(<column>)` when it has `orm::references`, with the table name and column name of the referenced member.
- Every column line except the last ends with a comma.

Dependency order, for `create_schema`:
- Table `A` depends on table `B` when a column of `A` references a member of `B` and `B` is not `A`. Dependencies on tables that are not among the `Ts` are ignored.
- Start with the `Ts` in the given order. Repeatedly take out the first remaining table whose dependencies have all been emitted already, and emit it. When no remaining table can be taken, the program must not compile.

Schema errors, for `schema_error`:
- The columns are checked in declaration order, and for each column the checks below are made in this order. The first check that fails gives the message. Names in messages are the table and column names defined above.
  1. The C++ type is unsupported: `<table>.<column>: unsupported type`.
  2. The column has `orm::primary_key` and an earlier column has it too: `<table>.<column>: second primary key`.
  3. The column references a member that has neither `orm::primary_key` nor `orm::unique`: `<table>.<column>: <referenced table>.<referenced column> is not a primary key or unique`.
  4. The column references a member with a different SQL type: `<table>.<column>: type <its SQL type> differs from <referenced table>.<referenced column> type <referenced SQL type>`.
- Nullability is not part of the SQL type, so a nullable column may reference a column that is not nullable.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
CREATE TABLE posts (
  id BIGINT NOT NULL PRIMARY KEY,
  author BIGINT NOT NULL REFERENCES users(id),
  body TEXT NOT NULL,
  score REAL NOT NULL,
  views INTEGER
);
--- schema
CREATE TABLE tags (
  tag_id INTEGER NOT NULL PRIMARY KEY,
  label TEXT NOT NULL UNIQUE
);

CREATE TABLE users (
  id BIGINT NOT NULL PRIMARY KEY,
  user_name VARCHAR(32) NOT NULL UNIQUE,
  email TEXT,
  active BOOLEAN NOT NULL
);

CREATE TABLE posts (
  id BIGINT NOT NULL PRIMARY KEY,
  author BIGINT NOT NULL REFERENCES users(id),
  body TEXT NOT NULL,
  score REAL NOT NULL,
  views INTEGER
);

CREATE TABLE Comment (
  id INTEGER NOT NULL PRIMARY KEY,
  post BIGINT NOT NULL REFERENCES posts(id),
  reply_to INTEGER REFERENCES Comment(id),
  by VARCHAR(32) NOT NULL REFERENCES users(user_name),
  weight REAL NOT NULL
);
--- checks
User: ok
Comment: ok
Pointer: Pointer.name: unsupported type
TwoKeys: pairs.right: second primary key
NotKey: NotKey.body: posts.body is not a primary key or unique
Narrow: Narrow.user: type INTEGER differs from users.id type BIGINT
LongName: LongName.user: type TEXT differs from users.user_name type VARCHAR(32)
```

Write the complete header to `solution.hpp` in the current directory.
