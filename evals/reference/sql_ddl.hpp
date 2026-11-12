#pragma once

#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orm {

// annotation values; strings live in static storage so the values stay structural
struct table {
    const char* name;
    consteval table(std::string_view n) : name(std::define_static_string(n)) {}
};

struct column {
    const char* name;
    consteval column(std::string_view n) : name(std::define_static_string(n)) {}
};

struct max_length {
    int length;
};

struct primary_key_t {};
inline constexpr primary_key_t primary_key{};

struct unique_t {};
inline constexpr unique_t unique{};

// a foreign key holds a reflection of the referenced data member
struct references {
    std::meta::info target;
};

namespace detail {

using std::meta::info;

constexpr auto ctx = std::meta::access_context::unprivileged();

template <class A>
consteval std::optional<A> find(info r) {
    for (info a : std::meta::annotations_of(r))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A)
            return std::meta::extract<A>(a);
    return std::nullopt;
}

template <class A>
consteval bool has(info r) {
    return find<A>(r).has_value();
}

consteval std::string table_name(info type) {
    if (auto t = find<table>(type))
        return t->name;
    return std::string(std::meta::identifier_of(type));
}

consteval std::string column_name(info m) {
    if (auto c = find<column>(m))
        return c->name;
    return std::string(std::meta::identifier_of(m));
}

consteval bool is_optional(info type) {
    type = std::meta::dealias(type);
    return std::meta::has_template_arguments(type) && std::meta::template_of(type) == ^^std::optional;
}

// the member type with one level of std::optional removed
consteval info value_type(info m) {
    info t = std::meta::dealias(std::meta::type_of(m));
    if (is_optional(t))
        return std::meta::dealias(std::meta::template_arguments_of(t)[0]);
    return t;
}

// empty when the member type has no sql type
consteval std::string decimal(int n) {
    std::string digits;
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + n % 10));
        n /= 10;
    } while (n > 0);
    return digits;
}

consteval std::string sql_type(info m) {
    info t = value_type(m);
    if (t == ^^bool)
        return "BOOLEAN";
    if (std::meta::is_integral_type(t) && std::meta::size_of(t) <= 4)
        return "INTEGER";
    if (std::meta::is_integral_type(t) && std::meta::size_of(t) == 8)
        return "BIGINT";
    if (std::meta::is_floating_point_type(t))
        return "REAL";
    if (t == std::meta::dealias(^^std::string)) {
        if (auto len = find<max_length>(m))
            return "VARCHAR(" + decimal(len->length) + ")";
        return "TEXT";
    }
    return "";
}

consteval std::string qualified_column(info m) {
    return table_name(std::meta::parent_of(m)) + "." + column_name(m);
}

consteval std::string check(info type) {
    bool seen_key = false;
    for (info m : std::meta::nonstatic_data_members_of(type, ctx)) {
        std::string prefix = table_name(type) + "." + column_name(m) + ": ";
        std::string sql = sql_type(m);
        if (sql.empty())
            return prefix + "unsupported type";
        if (has<primary_key_t>(m)) {
            if (seen_key)
                return prefix + "second primary key";
            seen_key = true;
        }
        if (auto ref = find<references>(m)) {
            info target = ref->target;
            if (!has<primary_key_t>(target) && !has<unique_t>(target))
                return prefix + qualified_column(target) + " is not a primary key or unique";
            std::string target_sql = sql_type(target);
            if (target_sql != sql)
                return prefix + "type " + sql + " differs from " + qualified_column(target) + " type " + target_sql;
        }
    }
    return "";
}

consteval std::string ddl(info type) {
    std::string out = "CREATE TABLE " + table_name(type) + " (\n";
    bool first = true;
    for (info m : std::meta::nonstatic_data_members_of(type, ctx)) {
        if (!first)
            out += ",\n";
        first = false;
        out += "  " + column_name(m) + " " + sql_type(m);
        if (!is_optional(std::meta::type_of(m)))
            out += " NOT NULL";
        if (has<primary_key_t>(m))
            out += " PRIMARY KEY";
        if (has<unique_t>(m))
            out += " UNIQUE";
        if (auto ref = find<references>(m))
            out += " REFERENCES " + table_name(std::meta::parent_of(ref->target)) + "(" + column_name(ref->target) + ")";
    }
    return out + "\n);";
}

// tables that type references, other than itself
consteval std::vector<info> dependencies(info type) {
    std::vector<info> out;
    for (info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (auto ref = find<references>(m)) {
            info owner = std::meta::parent_of(ref->target);
            if (owner != type)
                out.push_back(owner);
        }
    return out;
}

consteval bool contains(const std::vector<info>& list, info r) {
    for (info x : list)
        if (x == r)
            return true;
    return false;
}

consteval std::string schema(std::vector<info> remaining) {
    std::vector<info> all = remaining;
    std::vector<info> done;
    std::string out;
    while (!remaining.empty()) {
        std::size_t pick = remaining.size();
        for (std::size_t i = 0; i < remaining.size() && pick == remaining.size(); ++i) {
            bool ready = true;
            for (info dep : dependencies(remaining[i]))
                if (contains(all, dep) && !contains(done, dep))
                    ready = false;
            if (ready)
                pick = i;
        }
        if (pick == remaining.size())
            throw "foreign keys form a cycle";
        if (!out.empty())
            out += "\n\n";
        out += ddl(remaining[pick]);
        done.push_back(remaining[pick]);
        remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(pick));
    }
    return out;
}

template <class T>
consteval std::string_view make_table() {
    static_assert(check(^^T).empty(), "the table has a schema error");
    return std::define_static_string(ddl(^^T));
}

template <class... Ts>
consteval std::string_view make_schema() {
    static_assert((check(^^Ts).empty() && ...), "a table has a schema error");
    return std::define_static_string(schema({^^Ts...}));
}

}  // namespace detail

template <class T>
consteval std::string_view schema_error() {
    return std::define_static_string(detail::check(^^T));
}

template <class T>
inline constexpr std::string_view create_table = detail::make_table<T>();

template <class... Ts>
inline constexpr std::string_view create_schema = detail::make_schema<Ts...>();

}  // namespace orm
