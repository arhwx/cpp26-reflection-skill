I want typed configuration without writing the config struct by hand. A sample INI file, kept in the source as a string literal, defines the settings: its sections and keys become the members of a generated struct, the sample values give the member types, and they are also the defaults. At run time the library reads real INI text into that struct, reports every problem with its line number, and writes the struct back as INI text.

Everything goes in `namespace ini`:
- `ini::schema<"...">`: a class template whose only template argument is the sample text, given as a string literal (raw string literals included, as in the test).
- `schema<Sample>::type`: an aggregate struct. It has one public non-static data member per top-level key of the sample, in sample order, followed by one per section, in sample order, and no other non-static data members. A member for a key has the key's name and the key's type (see below). A member for a section has the section's name, and its type is an aggregate struct with one public non-static data member per key of that section, in sample order, and no other non-static data members.
- `static consteval std::string_view describe()` (`constexpr` is fine too): one line per key, `<path>: <kind>`, joined with `"\n"` and with no newline at the end. The keys come in member order: the top-level keys, then the keys of each section. The path of a top-level key is its name, the path of a key in a section is `<section>.<key>`. The kind is `bool`, `int`, `double` or `string`. A section without keys adds no line.
- `static type defaults()`: every key holds its sample value.
- `static type parse(std::string_view text, std::vector<std::string>& errors)`: reads `text` as described below and appends one message per problem to `errors`, which it does not clear.
- `static std::string write(const type& value)`: the INI text of `value`, described below.
- `consteval std::string_view ini::problem(std::string_view sample)` (`constexpr` is fine too): `ok` if `sample` is a valid sample, otherwise its first problem.
- Using `schema` with a sample that `problem` rejects must not compile, but the test does not do that.

Lines, the same for the sample and for `parse`:
- The text is split at each `'\n'` into lines, numbered from 1. Each line is trimmed: spaces and tabs at both ends are removed.
- A line that is then empty, or starts with `;` or `#`, is ignored.
- A line that starts with `[` and ends with `]` is a section header. The section name is the text between the brackets, trimmed.
- Any other line that contains `=` is a key line. The key is the text before the first `=`, trimmed, and the value is the text after it, trimmed. The value may be empty and may contain more `=`.
- Any other line is malformed.
- Key lines before the first header are top-level keys. Every later key line belongs to the section of the nearest header above it.

The sample:
- Lines are examined in order, each with the checks below in this order, and the first problem found is reported, as `line <n>: ` followed by:
  - a malformed line: `expected 'key = value'`
  - a section name or key that is not an identifier (a letter or `_`, then letters, digits and `_`): `'<name>' is not an identifier`
  - a section name used by an earlier header: `section '<name>' appears twice`
  - a section name equal to a top-level key: `section '<name>' has the name of a key`
  - a key that appears earlier in the same section, or among the top-level keys for a top-level key: `key '<name>' appears twice`
- The type of a key comes from its sample value: `true` or `false` gives `bool`. An optional `-` followed by one or more digits gives `int`. An optional `-`, one or more digits, `.` and one or more digits gives `double`. Anything else gives `std::string`.
- The sample value of a `bool` is `true` for `true`. The value of a number is the number written. A string written in quotes, meaning at least 2 characters with `"` as the first and the last, holds the characters between the quotes, kept as they are. Any other string holds the value text as it is.

`parse`:
- It starts from `defaults()` and changes keys as the lines say, in order.
- A header whose name is a section of the schema starts that section. Any other header gives `line <n>: unknown section '<name>'`, and every line after it up to the next header is ignored without further messages.
- A malformed line gives `line <n>: expected 'key = value'`.
- A key line whose key is not a key of the current section (or not a top-level key, before the first header) gives `line <n>: unknown key '<path>'`, with the path as in `describe`. A section name is not a top-level key.
- Otherwise the value is converted to the member's type, and the member is set. A later line for the same key wins. If the conversion fails, the member keeps its value and the message is `line <n>: <path> expects <kind>, got '<value>'`.
  - `bool`: exactly `true` or `false`.
  - `int`: an optional `-` and one or more digits, whose value fits in an `int`.
  - `double`: an optional `-`, one or more digits, and optionally `.` followed by one or more digits. The result is the nearest `double`, as `std::from_chars` gives it.
  - `std::string`: never fails. Quotes are handled as for the sample.
- The messages are appended in line order.

`write`:
- First a line `<key> = <value>` for each top-level key, in member order. Then for each section, in member order: an empty line if the text written so far is not empty, a line `[<section>]`, and a line `<key> = <value>` for each of its keys. Every line ends with `"\n"`.
- A `bool` is written `true` or `false`, an `int` in decimal, a `double` as `std::format("{}", value)` writes it, and a string always between `"` quotes, with its characters unchanged.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>
#include <vector>

using App = ini::schema<R"(; sample settings, also the defaults
name = demo
verbose = false
version = 1.2.3

[server]
host = localhost
port = 8080
timeout = 2.5
banner = "  welcome  "

  # the database
[ database ]
url = postgres://db/app?mode=ro
pool = 4
ratio = -0.25
code = "42"

[empty]
)">;

static_assert(std::is_same_v<decltype(App::type{}.name), std::string>);
static_assert(std::is_same_v<decltype(App::type{}.verbose), bool>);
static_assert(std::is_same_v<decltype(App::type{}.server.port), int>);
static_assert(std::is_same_v<decltype(App::type{}.server.timeout), double>);
static_assert(std::is_same_v<decltype(App::type{}.database.code), std::string>);
static_assert(std::is_aggregate_v<App::type> && std::is_aggregate_v<decltype(App::type{}.server)>);

template <class T>
concept has_port = requires(T t) { t.server.port; };
template <class T>
concept has_top_port = requires(T t) { t.port; };

void report(const std::vector<std::string>& errors) {
    std::println("{} errors", errors.size());
    for (const std::string& e : errors) std::println("  {}", e);
}

int main() {
    std::println("{}", App::describe());
    std::println("server.port: {}, port: {}", has_port<App::type>, has_top_port<App::type>);

    App::type cfg = App::defaults();
    std::println("defaults: port {} timeout {} banner [{}] code {} ratio {}", cfg.server.port, cfg.server.timeout,
                 cfg.server.banner, cfg.database.code, cfg.database.ratio);
    std::println("---");
    std::print("{}", App::write(cfg));
    std::println("---");

    std::vector<std::string> errors;
    App::type live = App::parse(R"(name = "production"
verbose = yes
[server]
port = 9090
hots = example.org
timeout = 30
port = 9091
verbose = true
[cache]
size = 10
this line is ignored
[database]
pool = many
ratio=0.5
url = sqlite://
broken line
code = 7
  ; the end
[empty]
)", errors);
    report(errors);
    std::println("---");
    std::print("{}", App::write(live));
    std::println("---");

    std::vector<std::string> again;
    App::type back = App::parse(App::write(live), again);
    std::println("round trip: {} errors, same text: {}", again.size(), App::write(back) == App::write(live));

    std::vector<std::string> more;
    App::type odd = App::parse("server = 1\n[server]\nport = -0\nport = 99999999999\nhost =\n", more);
    report(more);
    std::println("port {} host [{}]", odd.server.port, odd.server.host);

    std::println("{}", ini::problem("a = 1\n[s]\nb = true\n"));
    std::println("{}", ini::problem("a = 1\nb\n"));
    std::println("{}", ini::problem("[server]\nmax-conn = 4\n"));
    std::println("{}", ini::problem("[s]\nb = 1\n\nb = 2\n"));
    std::println("{}", ini::problem("[s]\nb = 2\n[t]\n[s]\n"));
    std::println("{}", ini::problem("a = 1\n[a]\n"));
    std::println("{}", ini::problem("[1st]\n"));
}
```

It must print exactly:

```
name: string
verbose: bool
version: string
server.host: string
server.port: int
server.timeout: double
server.banner: string
database.url: string
database.pool: int
database.ratio: double
database.code: string
server.port: true, port: false
defaults: port 8080 timeout 2.5 banner [  welcome  ] code 42 ratio -0.25
---
name = "demo"
verbose = false
version = "1.2.3"

[server]
host = "localhost"
port = 8080
timeout = 2.5
banner = "  welcome  "

[database]
url = "postgres://db/app?mode=ro"
pool = 4
ratio = -0.25
code = "42"

[empty]
---
6 errors
  line 2: verbose expects bool, got 'yes'
  line 5: unknown key 'server.hots'
  line 8: unknown key 'server.verbose'
  line 9: unknown section 'cache'
  line 13: database.pool expects int, got 'many'
  line 16: expected 'key = value'
---
name = "production"
verbose = false
version = "1.2.3"

[server]
host = "localhost"
port = 9091
timeout = 30
banner = "  welcome  "

[database]
url = "sqlite://"
pool = 4
ratio = 0.5
code = "7"

[empty]
---
round trip: 0 errors, same text: true
2 errors
  line 1: unknown key 'server'
  line 4: server.port expects int, got '99999999999'
port 0 host []
ok
line 2: expected 'key = value'
line 2: 'max-conn' is not an identifier
line 4: key 'b' appears twice
line 4: section 's' appears twice
line 2: section 'a' has the name of a key
line 1: '1st' is not an identifier
```

Write the complete header to `solution.hpp` in the current directory.
