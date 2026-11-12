// checked_sql: query text checked against a struct at compile time. the result row is a generated struct with
// just the selected columns, each ? is a parameter with the type of its column, and a typo gets a did-you-mean
#include <algorithm>
#include <functional>
#include <meta>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace sql {
using std::meta::info;

consteval std::vector<info> columns(info type) { return nonstatic_data_members_of(type, std::meta::access_context::unprivileged()); }
consteval auto indices(std::size_t n) { return std::define_static_array(std::views::iota(0uz, n)); }

struct condition { info column, compare; };  // column op ?, with op as a type such as ^^std::less<>
struct plan {
    std::vector<info> select;      // columns of T, in the order of the select list
    std::vector<condition> where;  // one per ?
    info order{}, sort = ^^std::less<>;
    std::string error;
};

consteval std::size_t distance(std::string_view a, std::string_view b) {  // edits from a to b, for did-you-mean
    std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1])});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

constexpr bool is_word(char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
constexpr bool is_op(char c) { return c == '=' || c == '!' || c == '<' || c == '>'; }
struct parser {  // reads the text from left to right and keeps the first error
    info table;
    std::string_view s;  // the text not read yet
    plan out{};

    consteval void fail(std::string message) { if (out.error.empty()) out.error = message; }
    consteval std::string_view peek() {  // the next token: a word, a run of = ! < >, or one other character
        s.remove_prefix(std::min(s.find_first_not_of(" \n"), s.size()));
        auto run = [&](auto in) { return std::size_t(std::ranges::find_if_not(s, in) - s.begin()); };
        return s.substr(0, std::max({run(is_word), run(is_op), std::min(s.size(), 1uz)}));
    }
    consteval std::string next() { std::string_view t = peek(); s.remove_prefix(t.size()); return std::string(t); }
    consteval bool take(std::string_view t) { if (peek() != t) return false; next(); return true; }
    consteval void expect(std::string_view t) { if (!take(t)) fail("expected '" + std::string(t) + "'"); }

    consteval info column() {
        std::string word = next();
        if (word.empty() || !is_word(word[0])) fail("expected a column");
        std::string hint;
        for (std::size_t best = 3; info c : columns(table)) {
            std::string_view name = identifier_of(c);
            if (name == word) return c;
            if (std::size_t d = distance(word, name); d < best) best = d, hint = ", did you mean '" + std::string(name) + "'?";
        }
        fail("unknown column '" + word + "'" + hint);
        return {};
    }
    consteval void read() {  // select cols [where col op ? {and col op ?}] [order by col [desc]]
        expect("select");
        if (take("*")) out.select = columns(table);
        else do out.select.push_back(column()); while (take(","));
        if (take("where")) do {
            info c = column();
            std::string op = next();
            info compare = op == "=" ? ^^std::equal_to<> : op == "!=" ? ^^std::not_equal_to<> : op == "<" ? ^^std::less<>
                         : op == "<=" ? ^^std::less_equal<> : op == ">" ? ^^std::greater<> : op == ">=" ? ^^std::greater_equal<> : info{};
            if (compare == info{}) fail("unknown operator '" + op + "'");
            expect("?");
            out.where.push_back({c, compare});
        } while (take("and"));
        if (take("order")) {
            expect("by");
            out.order = column();
            if (take("desc")) out.sort = ^^std::greater<>;
        }
        if (!peek().empty()) fail("unexpected '" + next() + "'");
    }
};
consteval plan parse(info table, std::string_view text) {
    parser p{table, text};
    p.read();
    if (!p.out.error.empty()) { p.out.select.clear(); p.out.where.clear(); }  // a broken query has no columns
    return p.out;
}

template <class T>
consteval std::string_view problem(std::string_view text) {  // "ok", or the first error
    std::string error = parse(^^T, text).error;
    return error.empty() ? "ok" : std::define_static_string(error);
}

template <class T, fixed_string Text, class... Args>  // the Args are the column types of the ? placeholders, in order
struct prepared {
    static consteval plan get() { return parse(^^T, Text.view()); }
    static_assert(problem<T>(Text.view()) == "ok", problem<T>(Text.view()));

    struct row;  // one member per selected column, with its name and type
    consteval {
        std::vector<info> specs;
        for (info c : get().select) specs.push_back(data_member_spec(type_of(c), {.name = identifier_of(c)}));
        define_aggregate(^^row, specs);
    }
    static constexpr auto select = std::define_static_array(get().select);
    static constexpr auto fields = std::define_static_array(columns(^^row));
    static constexpr auto where = std::define_static_array(get().where);
    static constexpr info order = get().order, sort = get().sort;

    static std::vector<row> run(const std::vector<T>& table, Args... args) {
        std::vector<const T*> kept;
        for (const T& r : table) {
            bool keep = true;
            template for (constexpr std::size_t i : indices(where.size()))  // "age < ?" is std::less<>{}(r.age, arg)
                keep = keep && typename [:where[i].compare:]{}(r.[:where[i].column:], args...[i]);
            if (keep) kept.push_back(&r);
        }
        if constexpr (order != info{})
            std::ranges::stable_sort(kept, typename [:sort:]{}, [](const T* r) -> const auto& { return r->[:order:]; });
        std::vector<row> out(kept.size());
        for (std::size_t n = 0; n < kept.size(); ++n)
            template for (constexpr std::size_t i : indices(select.size())) out[n].[:fields[i]:] = kept[n]->[:select[i]:];
        return out;
    }
};

template <class T, fixed_string Text>
consteval info with_parameters() {  // prepared<T, Text, the type of the column of each ?...>
    std::vector<info> args{^^T, std::meta::reflect_constant(Text)};
    for (condition c : parse(^^T, Text.view()).where) args.push_back(type_of(c.column));
    return substitute(^^prepared, args);
}
template <class T, fixed_string Text>
using query = [:with_parameters<T, Text>():];
}  // namespace sql

// ---- user code. rust sqlx: query!("select name, salary from staff where dept = ? and salary >= ?", dept, pay)
struct Employee {
    int id;
    std::string name;
    std::string dept;
    double salary;
    int age;
};

using ByPay = sql::query<Employee, "select name, salary where dept = ? and salary >= ? order by salary desc">;
using Senior = sql::query<Employee, R"(
    select *
    where age >= ? and dept != ?
    order by age desc
)">;

// the row type has just the selected columns, and each ? is a parameter with the type of its column
template <class Row> constexpr bool has_age = requires(Row r) { r.age; };
template <class Q, class... A> constexpr bool takes = requires(std::vector<Employee> t, A... a) { Q::run(t, a...); };
static_assert(!has_age<ByPay::row> && has_age<Senior::row> && type_of(^^ByPay::row::salary) == ^^double);
static_assert(takes<ByPay, const char*, int>);  // "eng" becomes a std::string, 100000 a double
static_assert(!takes<ByPay, int, int>);         // dept is text
static_assert(!takes<ByPay, const char*>);      // one argument is missing

// sql::query<Employee, "select name, salry"> does not compile, with the message that problem() gives below

int main() {
    const std::vector<Employee> staff{
        {1, "ada", "eng", 120000, 36}, {2, "bob", "ops", 70000, 45},   {3, "cy", "eng", 95000, 29},
        {4, "dee", "eng", 120000, 41}, {5, "eve", "sales", 60000, 23}, {6, "fay", "eng", 150000, 52},
    };
    std::println("eng paid at least 100000:");
    for (const auto& r : ByPay::run(staff, "eng", 100000)) std::println("  {} {}", r.name, r.salary);
    std::println("30 or older, not in ops:");
    for (const auto& r : Senior::run(staff, 30, "ops")) std::println("  {} {} {} {} {}", r.id, r.name, r.dept, r.salary, r.age);
    template for (constexpr const char* text : {"select name, salry", "select name where dept == ?", "select name from staff"})
        std::println("{:<28} {}", text, sql::problem<Employee>(text));
}

// output:
// eng paid at least 100000:
//   fay 150000
//   ada 120000
//   dee 120000
// 30 or older, not in ops:
//   6 fay eng 150000 52
//   4 dee eng 120000 41
//   1 ada eng 120000 36
// select name, salry           unknown column 'salry', did you mean 'salary'?
// select name where dept == ?  unknown operator '=='
// select name from staff       unexpected 'from'
