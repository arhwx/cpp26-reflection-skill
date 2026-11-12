Each table in our database is described by a C++ struct, one column per member. When the struct changes between releases, someone writes the `ALTER TABLE` migration by hand, and it often misses a column. I want the migration generated from the old and the new version of the struct.

Everything goes in `namespace sql`. All structs involved are aggregates whose members are all public, and a value-initialized `New{}` is valid.

Annotations, both written on members:
- `[[=sql::varchar(n)]]`, with an `int` length `n`: the column is `VARCHAR(n)` instead of `TEXT`. Only used on `std::string` and `std::optional<std::string>` members.
- `[[=sql::renamed_from("old_name")]]`: the member continues the old column `old_name` (see matching below).

Columns:
- Every non-static data member is a column with the member's name.
- A member of type `std::optional<V>` is a nullable column of the SQL type of `V`. Any other member is a `NOT NULL` column of the SQL type of its own type.
- SQL types: `bool` is `BOOLEAN`. `short`, `int`, `long` and `long long` are `SMALLINT`, `INTEGER` or `BIGINT` when their size is 2, 4 or 8 bytes. `float` is `REAL`, `double` is `DOUBLE PRECISION`, and `std::string` is `TEXT`, or `VARCHAR(n)` with the annotation. No other type has an SQL type: not unsigned types, not `char`, not containers.

API:
- `template <class Old, class New> std::vector<std::string> migration(std::string_view table)`: the statements, in the order below, that turn table `table` with the columns of `Old` into one with the columns of `New`. It returns an empty vector when nothing changes.
- `migration<Old, New>` must not compile when a member of `Old` or `New` has no SQL type, or when two members of `New` match the same member of `Old`. It must fail as part of overload resolution, so that a requires-expression containing the call is `false`. The test checks this with a concept.

Matching, for each member `n` of `New`:
- If `Old` has a member with `n`'s name, `n` matches it.
- Otherwise, if `n` has a `renamed_from` annotation and `Old` has a member with that name, `n` matches it.
- Otherwise `n` is a new column.
- A member of `Old` that no member of `New` matches is dropped. Annotations on `Old`'s members play no part.

Statements, each a separate string ending in `;`, with `<t>` the table name:
1. For each member of `New` matched through `renamed_from`, in `New`'s declaration order: `ALTER TABLE <t> RENAME COLUMN <old> TO <new>;`
2. For each dropped member of `Old`, in `Old`'s declaration order: `ALTER TABLE <t> DROP COLUMN <old>;`
3. For each matched member of `New`, in `New`'s declaration order: if the SQL type changed, `ALTER TABLE <t> ALTER COLUMN <new> TYPE <type>;` with the new type. Then, if the column went from nullable to `NOT NULL`, `ALTER TABLE <t> ALTER COLUMN <new> SET NOT NULL;`, or if it went from `NOT NULL` to nullable, `ALTER TABLE <t> ALTER COLUMN <new> DROP NOT NULL;`
4. For each new column, in `New`'s declaration order: for a nullable column `ALTER TABLE <t> ADD COLUMN <new> <type>;`, otherwise `ALTER TABLE <t> ADD COLUMN <new> <type> NOT NULL DEFAULT <literal>;`

All statements of step 1 come before all of step 2, and so on. `<old>` and `<new>` are the member names in `Old` and `New`, and `<type>` is an SQL type spelled as above.

The `DEFAULT` literal is the value that member has in a value-initialized `New{}`:
- `bool`: `TRUE` or `FALSE`.
- integers and floating-point values: as `std::format("{}", value)` writes them.
- strings: between single quotes, with each `'` inside doubled.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

struct UserV1 {
    int id;
    std::string name;
    [[=sql::varchar(64)]] std::string email;
    int age;
    std::string fax;
    std::optional<std::string> phone;
};

struct UserV2 {
    long long id;
    [[=sql::renamed_from("name")]] std::string full_name;
    [[=sql::varchar(128)]] std::string email;
    std::optional<int> age;
    bool active = true;
    std::string country = "Cote d'Ivoire";
    std::optional<double> score;
    int logins;
    short level = -2;
    [[=sql::renamed_from("phone")]] std::optional<std::string> mobile;
};

struct UserV3 {
    long long id;
    [[=sql::renamed_from("name")]] std::string full_name;
    std::string email;
    int age = 18;
    bool active = true;
    std::string country = "Cote d'Ivoire";
    [[=sql::renamed_from("score")]] float rating = 0.5f;
    int logins;
    [[=sql::renamed_from("phone")]] [[=sql::varchar(20)]] std::string mobile;
};

// both members would continue name
struct Twice {
    int id;
    std::string name;
    [[=sql::renamed_from("name")]] std::string nick;
};

struct WithTags {
    int id;
    std::vector<std::string> tags;
};

struct Unsigned {
    unsigned id;
};

struct Tiny {
    std::optional<long long> id;
    bool flag;
    double ratio = 0.25;
};

template <class Old, class New>
concept migratable = requires { sql::migration<Old, New>("t"); };

template <class Old, class New>
void show(std::string_view title, std::string_view table) {
    std::println("-- {}", title);
    std::vector<std::string> steps = sql::migration<Old, New>(table);
    if (steps.empty())
        std::println("(no changes)");
    for (const std::string& step : steps)
        std::println("{}", step);
}

int main() {
    show<UserV1, UserV2>("v1 to v2", "users");
    show<UserV2, UserV3>("v2 to v3", "users");
    show<UserV1, UserV3>("v1 to v3", "accounts");
    show<UserV3, UserV3>("v3 to v3", "users");
    show<UserV3, UserV1>("v3 to v1", "users");
    show<UserV1, Tiny>("v1 to tiny", "t");

    std::println("v1 to v2: {}", migratable<UserV1, UserV2>);
    std::println("v1 to twice: {}", migratable<UserV1, Twice>);
    std::println("twice to v1: {}", migratable<Twice, UserV1>);
    std::println("v1 to tags: {}", migratable<UserV1, WithTags>);
    std::println("tags to v1: {}", migratable<WithTags, UserV1>);
    std::println("v1 to unsigned: {}", migratable<UserV1, Unsigned>);
}
```

It must print exactly:

```
-- v1 to v2
ALTER TABLE users RENAME COLUMN name TO full_name;
ALTER TABLE users RENAME COLUMN phone TO mobile;
ALTER TABLE users DROP COLUMN fax;
ALTER TABLE users ALTER COLUMN id TYPE BIGINT;
ALTER TABLE users ALTER COLUMN email TYPE VARCHAR(128);
ALTER TABLE users ALTER COLUMN age DROP NOT NULL;
ALTER TABLE users ADD COLUMN active BOOLEAN NOT NULL DEFAULT TRUE;
ALTER TABLE users ADD COLUMN country TEXT NOT NULL DEFAULT 'Cote d''Ivoire';
ALTER TABLE users ADD COLUMN score DOUBLE PRECISION;
ALTER TABLE users ADD COLUMN logins INTEGER NOT NULL DEFAULT 0;
ALTER TABLE users ADD COLUMN level SMALLINT NOT NULL DEFAULT -2;
-- v2 to v3
ALTER TABLE users RENAME COLUMN score TO rating;
ALTER TABLE users DROP COLUMN level;
ALTER TABLE users ALTER COLUMN email TYPE TEXT;
ALTER TABLE users ALTER COLUMN age SET NOT NULL;
ALTER TABLE users ALTER COLUMN rating TYPE REAL;
ALTER TABLE users ALTER COLUMN rating SET NOT NULL;
ALTER TABLE users ALTER COLUMN mobile TYPE VARCHAR(20);
ALTER TABLE users ALTER COLUMN mobile SET NOT NULL;
-- v1 to v3
ALTER TABLE accounts RENAME COLUMN name TO full_name;
ALTER TABLE accounts RENAME COLUMN phone TO mobile;
ALTER TABLE accounts DROP COLUMN fax;
ALTER TABLE accounts ALTER COLUMN id TYPE BIGINT;
ALTER TABLE accounts ALTER COLUMN email TYPE TEXT;
ALTER TABLE accounts ALTER COLUMN mobile TYPE VARCHAR(20);
ALTER TABLE accounts ALTER COLUMN mobile SET NOT NULL;
ALTER TABLE accounts ADD COLUMN active BOOLEAN NOT NULL DEFAULT TRUE;
ALTER TABLE accounts ADD COLUMN country TEXT NOT NULL DEFAULT 'Cote d''Ivoire';
ALTER TABLE accounts ADD COLUMN rating REAL NOT NULL DEFAULT 0.5;
ALTER TABLE accounts ADD COLUMN logins INTEGER NOT NULL DEFAULT 0;
-- v3 to v3
(no changes)
-- v3 to v1
ALTER TABLE users DROP COLUMN full_name;
ALTER TABLE users DROP COLUMN active;
ALTER TABLE users DROP COLUMN country;
ALTER TABLE users DROP COLUMN rating;
ALTER TABLE users DROP COLUMN logins;
ALTER TABLE users DROP COLUMN mobile;
ALTER TABLE users ALTER COLUMN id TYPE INTEGER;
ALTER TABLE users ALTER COLUMN email TYPE VARCHAR(64);
ALTER TABLE users ADD COLUMN name TEXT NOT NULL DEFAULT '';
ALTER TABLE users ADD COLUMN fax TEXT NOT NULL DEFAULT '';
ALTER TABLE users ADD COLUMN phone TEXT;
-- v1 to tiny
ALTER TABLE t DROP COLUMN name;
ALTER TABLE t DROP COLUMN email;
ALTER TABLE t DROP COLUMN age;
ALTER TABLE t DROP COLUMN fax;
ALTER TABLE t DROP COLUMN phone;
ALTER TABLE t ALTER COLUMN id TYPE BIGINT;
ALTER TABLE t ALTER COLUMN id DROP NOT NULL;
ALTER TABLE t ADD COLUMN flag BOOLEAN NOT NULL DEFAULT FALSE;
ALTER TABLE t ADD COLUMN ratio DOUBLE PRECISION NOT NULL DEFAULT 0.25;
v1 to v2: true
v1 to twice: false
twice to v1: true
v1 to tags: false
tags to v1: false
v1 to unsigned: false
```

Write the complete header to `solution.hpp` in the current directory.
