I want typed queries over vectors of plain structs, written as short SQL-like text. The query text is checked at compile time against the struct: the result rows get a generated type with only the selected columns, each `?` placeholder becomes a parameter whose type is the type of the column it is compared with, and a misspelled column gives an error with a did-you-mean suggestion.

Everything goes in `namespace sql`. The columns of a table struct `T` are its non-static data members, in declaration order. All test structs are aggregates with only public members and no base classes.

Query language:
- The text is split into tokens. Whitespace (space, tab, newline) separates tokens and is otherwise ignored. A word is a longest run of letters, digits and `_`. An operator token is a longest run of the characters `=`, `!`, `<`, `>`. The characters `,`, `?` and `*` are tokens of one character each. Any other character is an error, so `name,salary` and `age<?` are three tokens each.
- Grammar, with keywords in lowercase only and optional parts in brackets:
  `select COLUMNS [where CONDITION {and CONDITION}] [order by COLUMN [asc | desc]] [limit NUMBER]`
- `COLUMNS` is `*`, meaning every column of `T` in declaration order, or one or more column names separated by `,`.
- `CONDITION` is `COLUMN OPERATOR ?`, where `OPERATOR` is one of `=`, `!=`, `<`, `<=`, `>`, `>=`.
- `COLUMN` is a word naming a column of `T`. `NUMBER` is a word made only of decimal digits.

API:
- `sql::query<T, "...">`: a class type (an alias is fine) whose template arguments are the table struct and the query text, given as a string literal (raw string literals included). A query that `problem` rejects must not compile, but the test does not do that.
- `query<T, Text>::row`: an aggregate struct with one public non-static data member per selected column, with the column's name and type, in the order of the `select` list, and no other non-static data members.
- `query<T, Text>::run(const std::vector<T>& table, args...)`: a static member function returning `std::vector<row>`. It takes exactly one argument after `table` per `?`, in the order the `?` appear, and the type of each is the type of the column compared with that `?`. It must accept exactly what a function with those parameter types accepts (so `"eng"` works for a `std::string` column and `100000` for a `double` column) and nothing else: a requires-expression with arguments of the wrong number or of unconvertible types is false, as the test checks.
- `template <class T> std::string_view problem(std::string_view text)`: called as `sql::problem<T>("...")` (it may be `consteval`; the test calls it only with string literals). Returns `ok` for a valid query of `T`, otherwise the first error, as below.

What `run` does:
1. Keeps the rows of `table`, in table order, for which every condition holds. A condition compares the row's column (left) with the argument (right) using the operator.
2. With `order by`, sorts the kept rows by that column, ascending with `<` (also for `asc`, the default) or descending with `desc`. The sort is stable: rows with equal keys keep their table order.
3. With `limit n`, keeps only the first `n` rows.
4. Returns, for each remaining row, a `row` with the selected columns copied.

Errors: the whole text is tokenized first, then read from left to right, and the first error is reported, in exactly these forms:
- `unexpected character '<c>'`: the first character that cannot be part of any token.
- `expected 'select'`: the first token is not `select`, or there is none.
- `expected a column`: a column is required but the next token is not a word, or the text ends.
- `unknown column '<w>'`: the word `<w>` is not a column of `T`. If some column is within Levenshtein distance 2 of `<w>` (single-character insertions, deletions and substitutions each count 1), the message continues with `, did you mean '<col>'?`, naming the column with the smallest distance, and among equally close columns the one declared first.
- `duplicate column '<w>'`: a column appears twice in the `select` list.
- `expected an operator after '<col>'`: in a condition, the token after the column is not an operator token, or the text ends.
- `unknown operator '<op>'`: an operator token that is not one of the six operators.
- `expected '?'`: in a condition, the token after the operator is not `?`.
- `expected 'by'`: `order` is not followed by `by`.
- `expected a number after 'limit'`: `limit` is not followed by a `NUMBER`.
- `unexpected '<token>'`: a token is left over where the grammar allows nothing more, for example `from`, or `,` after `*`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>
#include <vector>

struct Employee {
    int id;
    std::string name;
    std::string dept;
    double salary;
    int age;
    bool remote;
};

using ByPay = sql::query<Employee, "select name, salary where dept = ? and salary >= ? order by salary desc">;
using Remote = sql::query<Employee, "select * where remote = ? limit 2">;
using Roster = sql::query<Employee, "select id,name order by age">;
using Band = sql::query<Employee, R"(
    select dept, name, age
    where age<? and age>=? and dept!=?
    order by dept asc
    limit 3
)">;

template <class T, class U>
constexpr bool same = std::is_same_v<T, U>;

template <class Q, class... A>
constexpr bool takes = requires(const std::vector<Employee>& table, A... args) { Q::run(table, args...); };

// problem is called with a literal, so it can be evaluated at compile time
#define CHECK(text) std::println("{:<38} {}", text, sql::problem<Employee>(text))

int main() {
    const std::vector<Employee> staff{
        {1, "ada", "eng", 120000, 36, true},  {2, "bob", "ops", 70000, 45, false},  {3, "cy", "eng", 95000, 29, false},
        {4, "dee", "eng", 120000, 41, true},  {5, "eve", "sales", 60000, 23, true}, {6, "fay", "eng", 150000, 52, false},
        {7, "gus", "ops", 82000.5, 33, true},
    };

    std::println("ByPay row: {} {}", same<decltype(ByPay::row::name), std::string>, same<decltype(ByPay::row::salary), double>);
    std::println("Remote row: {} {}", same<decltype(Remote::row::id), int>, same<decltype(Remote::row::remote), bool>);
    std::println("Band row is aggregate: {}", std::is_aggregate_v<Band::row>);
    std::println("ByPay takes text, number: {}", takes<ByPay, std::string, double>);
    std::println("ByPay takes a literal and an int: {}", takes<ByPay, const char*, int>);
    std::println("ByPay takes two numbers: {}", takes<ByPay, double, double>);
    std::println("ByPay takes one argument: {}", takes<ByPay, const char*>);
    std::println("Roster takes none: {}", takes<Roster>);
    std::println("Roster takes one: {}", takes<Roster, int>);

    std::println("eng paid at least 100000:");
    for (const auto& r : ByPay::run(staff, "eng", 100000)) std::println("  {} {}", r.name, r.salary);
    std::println("eng paid at least 200000: {} rows", ByPay::run(staff, "eng", 200000).size());

    std::println("remote, first two:");
    for (const auto& r : Remote::run(staff, true)) std::println("  {} {} {} {} {} {}", r.id, r.name, r.dept, r.salary, r.age, r.remote);

    std::println("roster by age:");
    for (const auto& r : Roster::run(staff)) std::println("  {} {}", r.id, r.name);

    std::println("age in [30, 50), not sales:");
    for (const auto& r : Band::run(staff, 50, 30, "sales")) std::println("  {} {} {}", r.dept, r.name, r.age);
    std::println("age in [20, 40), not eng:");
    for (const auto& r : Band::run(staff, 40, 20, "eng")) std::println("  {} {} {}", r.dept, r.name, r.age);

    CHECK("select name, salary where dept = ?");
    CHECK("select name, salry");
    CHECK("select nmae");
    CHECK("select ame");
    CHECK("select title");
    CHECK("select name where dpt = ?");
    CHECK("select name where dept == ?");
    CHECK("select name where age > 30");
    CHECK("select name where age ?");
    CHECK("select name, name");
    CHECK("selec name");
    CHECK("select name from staff");
    CHECK("select name order salary");
    CHECK("select * limit ten");
    CHECK("select name;");
    CHECK("select");
    CHECK("select * where remote = ? and");
    CHECK("select *, name");
    CHECK("select name order by salry desc");
}
```

It must print exactly:

```
ByPay row: true true
Remote row: true true
Band row is aggregate: true
ByPay takes text, number: true
ByPay takes a literal and an int: true
ByPay takes two numbers: false
ByPay takes one argument: false
Roster takes none: true
Roster takes one: false
eng paid at least 100000:
  fay 150000
  ada 120000
  dee 120000
eng paid at least 200000: 0 rows
remote, first two:
  1 ada eng 120000 36 true
  4 dee eng 120000 41 true
roster by age:
  5 eve
  3 cy
  7 gus
  1 ada
  4 dee
  2 bob
  6 fay
age in [30, 50), not sales:
  eng ada 36
  eng dee 41
  ops bob 45
age in [20, 40), not eng:
  ops gus 33
  sales eve 23
select name, salary where dept = ?     ok
select name, salry                     unknown column 'salry', did you mean 'salary'?
select nmae                            unknown column 'nmae', did you mean 'name'?
select ame                             unknown column 'ame', did you mean 'name'?
select title                           unknown column 'title'
select name where dpt = ?              unknown column 'dpt', did you mean 'dept'?
select name where dept == ?            unknown operator '=='
select name where age > 30             expected '?'
select name where age ?                expected an operator after 'age'
select name, name                      duplicate column 'name'
selec name                             expected 'select'
select name from staff                 unexpected 'from'
select name order salary               expected 'by'
select * limit ten                     expected a number after 'limit'
select name;                           unexpected character ';'
select                                 expected a column
select * where remote = ? and          expected a column
select *, name                         unexpected ','
select name order by salry desc        unknown column 'salry', did you mean 'salary'?
```

Write the complete header to `solution.hpp` in the current directory.
