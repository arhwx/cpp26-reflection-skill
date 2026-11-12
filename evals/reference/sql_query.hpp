#pragma once

#include <algorithm>
#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sql {
using std::meta::info;

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

consteval std::vector<info> columns(info type) {
    return nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

enum class op { eq, ne, lt, le, gt, ge };

struct condition {
    std::size_t column;
    op compare;
};

struct plan {
    std::vector<std::size_t> select;
    std::vector<condition> where;
    bool ordered = false;
    std::size_t order = 0;
    bool descending = false;
    bool limited = false;
    std::size_t limit = 0;
    std::string error;
};

constexpr bool is_word_char(char c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
constexpr bool is_op_char(char c) { return c == '=' || c == '!' || c == '<' || c == '>'; }

consteval std::size_t distance(std::string_view a, std::string_view b) {
    std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

struct parser {
    info table;
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    plan out;

    consteval bool fail(std::string message) {
        out.error = message;
        return false;
    }
    consteval std::string_view peek() const { return pos < tokens.size() ? std::string_view(tokens[pos]) : std::string_view(); }
    consteval bool at_word() const { return pos < tokens.size() && is_word_char(tokens[pos][0]); }

    consteval bool tokenize(std::string_view s) {
        for (std::size_t i = 0; i < s.size();) {
            char c = s[i];
            std::size_t start = i;
            if (c == ' ' || c == '\t' || c == '\n') {
                ++i;
                continue;
            }
            if (is_word_char(c)) {
                while (i < s.size() && is_word_char(s[i])) ++i;
            } else if (is_op_char(c)) {
                while (i < s.size() && is_op_char(s[i])) ++i;
            } else if (c == ',' || c == '?' || c == '*') {
                ++i;
            } else {
                return fail(std::string("unexpected character '") + c + "'");
            }
            tokens.emplace_back(s.substr(start, i - start));
        }
        return true;
    }

    consteval bool column(std::size_t& index) {
        if (!at_word()) return fail("expected a column");
        std::string name = tokens[pos++];
        std::vector<info> cols = columns(table);
        for (std::size_t i = 0; i < cols.size(); ++i)
            if (identifier_of(cols[i]) == name) {
                index = i;
                return true;
            }
        std::string message = "unknown column '" + name + "'";
        std::size_t best = 3;
        std::string_view suggestion;
        for (info c : cols)
            if (std::size_t d = distance(name, identifier_of(c)); d < best) best = d, suggestion = identifier_of(c);
        if (!suggestion.empty()) message += ", did you mean '" + std::string(suggestion) + "'?";
        return fail(message);
    }

    consteval bool condition_() {
        condition c{};
        if (!column(c.column)) return false;
        std::string name = tokens[pos - 1];
        if (pos == tokens.size() || !is_op_char(tokens[pos][0])) return fail("expected an operator after '" + name + "'");
        std::string_view o = tokens[pos++];
        if (o == "=") c.compare = op::eq;
        else if (o == "!=") c.compare = op::ne;
        else if (o == "<") c.compare = op::lt;
        else if (o == "<=") c.compare = op::le;
        else if (o == ">") c.compare = op::gt;
        else if (o == ">=") c.compare = op::ge;
        else return fail("unknown operator '" + std::string(o) + "'");
        if (peek() != "?") return fail("expected '?'");
        ++pos;
        out.where.push_back(c);
        return true;
    }

    consteval bool query(std::string_view text) {
        if (!tokenize(text)) return false;
        if (peek() != "select") return fail("expected 'select'");
        ++pos;
        if (peek() == "*") {
            ++pos;
            for (std::size_t i = 0; i < columns(table).size(); ++i) out.select.push_back(i);
        } else {
            do {
                std::size_t index;
                if (!column(index)) return false;
                if (std::ranges::contains(out.select, index)) return fail("duplicate column '" + tokens[pos - 1] + "'");
                out.select.push_back(index);
            } while (peek() == "," && ++pos);
        }
        if (peek() == "where") {
            do {
                ++pos;
                if (!condition_()) return false;
            } while (peek() == "and");
        }
        if (peek() == "order") {
            ++pos;
            if (peek() != "by") return fail("expected 'by'");
            ++pos;
            out.ordered = true;
            if (!column(out.order)) return false;
            if (peek() == "asc") ++pos;
            else if (peek() == "desc") ++pos, out.descending = true;
        }
        if (peek() == "limit") {
            ++pos;
            std::string_view n = peek();
            if (n.empty() || !std::ranges::all_of(n, [](char c) { return c >= '0' && c <= '9'; }))
                return fail("expected a number after 'limit'");
            ++pos;
            out.limited = true;
            for (char c : n) out.limit = out.limit * 10 + std::size_t(c - '0');
        }
        if (pos < tokens.size()) return fail("unexpected '" + tokens[pos] + "'");
        return true;
    }
};

consteval plan parse(info table, std::string_view text) {
    parser p{table};
    p.query(text);
    return p.out;
}

template <class T>
consteval std::string_view problem(std::string_view text) {
    plan p = parse(^^T, text);
    return p.error.empty() ? std::string_view("ok") : std::string_view(std::define_static_string(p.error));
}

template <class T, fixed_string Text>
struct query_base {
    static_assert(problem<T>(Text.view()) == "ok", problem<T>(Text.view()));

    using table = T;
    static constexpr auto cols = std::define_static_array(columns(^^T));
    static constexpr auto conds = std::define_static_array(parse(^^T, Text.view()).where);
    static constexpr std::size_t params = conds.size();
    static constexpr bool ordered = parse(^^T, Text.view()).ordered;
    static constexpr std::size_t order = parse(^^T, Text.view()).order;
    static constexpr bool descending = parse(^^T, Text.view()).descending;
    static constexpr bool limited = parse(^^T, Text.view()).limited;
    static constexpr std::size_t limit = parse(^^T, Text.view()).limit;

    struct row;
    consteval {
        std::vector<info> specs;
        for (std::size_t i : parse(^^T, Text.view()).select) {
            info c = columns(^^T)[i];
            specs.push_back(data_member_spec(type_of(c), {.name = identifier_of(c)}));
        }
        define_aggregate(^^row, specs);
    }
};

template <class Base, class... P>
struct runner : Base {
    using T = typename Base::table;
    using row = typename Base::row;

    template <std::size_t I, class V>
    static bool holds(const T& r, const V& value) {
        constexpr condition c = Base::conds[I];
        const auto& field = r.[:Base::cols[c.column]:];
        if constexpr (c.compare == op::eq) return field == value;
        else if constexpr (c.compare == op::ne) return field != value;
        else if constexpr (c.compare == op::lt) return field < value;
        else if constexpr (c.compare == op::le) return field <= value;
        else if constexpr (c.compare == op::gt) return field > value;
        else return field >= value;
    }

    template <std::size_t... I>
    static bool matches(const T& r, const std::tuple<P...>& args, std::index_sequence<I...>) {
        return (holds<I>(r, std::get<I>(args)) && ...);
    }

    static std::vector<row> run(const std::vector<T>& table, P... p) {
        std::tuple<P...> args{std::move(p)...};
        std::vector<const T*> kept;
        for (const T& r : table)
            if (matches(r, args, std::index_sequence_for<P...>{})) kept.push_back(&r);
        if constexpr (Base::ordered) {
            std::ranges::stable_sort(kept, [](const T* a, const T* b) {
                constexpr info key = Base::cols[Base::order];
                return Base::descending ? b->[:key:] < a->[:key:] : a->[:key:] < b->[:key:];
            });
        }
        if constexpr (Base::limited)
            if (kept.size() > Base::limit) kept.resize(Base::limit);
        std::vector<row> out;
        for (const T* r : kept) {
            row& o = out.emplace_back();
            template for (constexpr info m :
                          std::define_static_array(nonstatic_data_members_of(^^row, std::meta::access_context::unprivileged()))) {
                constexpr std::string_view name = identifier_of(m);
                template for (constexpr info c : Base::cols)
                    if constexpr (identifier_of(c) == name) o.[:m:] = r->[:c:];
            }
        }
        return out;
    }
};

// runner<query_base<T, Text>, type of the column of each condition...>
template <class T, fixed_string Text>
consteval info runner_for() {
    std::vector<info> args{^^query_base<T, Text>};
    for (condition c : parse(^^T, Text.view()).where) args.push_back(remove_cv(type_of(columns(^^T)[c.column])));
    return substitute(^^runner, args);
}

template <class T, fixed_string Text>
using query = [:runner_for<T, Text>():];
}  // namespace sql
