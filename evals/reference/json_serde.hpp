#pragma once

#include <algorithm>
#include <format>
#include <meta>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace json {
using std::meta::info;

// ---- annotations
struct skip_t {};
inline constexpr skip_t skip;  // leave this member out
struct name {                  // use this key instead of the member name
    const char* text;
    consteval name(std::string_view t) : text(std::define_static_string(t)) {}
};
struct since {  // write this member only from that format version on
    int version;
};
enum class naming { as_written, camel };  // on the class: how member names become keys

// a class can befriend this to let the library see its private members
struct access {
    static consteval std::vector<info> fields(info type) { return nonstatic_data_members_of(type, std::meta::access_context::current()); }
};

template <class A>
consteval std::optional<A> annotation(info r) {
    for (info a : annotations_of(r))
        if (remove_const(type_of(a)) == ^^A) return extract<A>(a);
    return std::nullopt;
}

// a member's own name annotation wins over the class naming
consteval std::string key_of(info type, info member) {
    if (auto n = annotation<name>(member)) return n->text;
    std::string id(identifier_of(member)), out;
    if (annotation<naming>(type) != naming::camel) return id;
    for (bool upper = false; char c : id) {
        if (c == '_') upper = true;
        else out += upper ? char(c - 'a' + 'A') : c, upper = false;
    }
    return out;
}

// "ok", or why this class cannot be written
consteval std::string_view check(info type) {
    if (access::fields(type).size() != nonstatic_data_members_of(type, std::meta::access_context::unchecked()).size())
        return std::define_static_string(std::string(identifier_of(type)) + " has members the library cannot see; add: friend struct json::access;");
    std::vector<std::string> seen;
    for (info m : access::fields(type)) {
        if (annotation<skip_t>(m)) continue;
        std::string key = key_of(type, m);
        if (std::ranges::contains(seen, key))
            return std::define_static_string(std::string(identifier_of(type)) + ": two members map to the key '" + key + "'");
        seen.push_back(key);
    }
    return "ok";
}

// is there a member function with that name
consteval bool declares(info type, std::string_view function) {
    return std::ranges::any_of(members_of(type, std::meta::access_context::unchecked()),
                               [&](info m) { return is_function(m) && has_identifier(m) && identifier_of(m) == function; });
}

template <class T>
void write(std::string& out, const T& v, int version) {
    if constexpr (std::is_same_v<T, bool>) out += v ? "true" : "false";
    else if constexpr (std::is_arithmetic_v<T>) out += std::format("{}", v);
    else if constexpr (std::is_enum_v<T>) {
        template for (constexpr info e : std::define_static_array(enumerators_of(^^T)))
            if (v == [:e:]) out += std::format("\"{}\"", std::meta::identifier_of(e));
    } else if constexpr (std::is_convertible_v<const T&, std::string_view>) out += std::format("\"{}\"", std::string_view(v));
    else if constexpr (has_template_arguments(^^T) && template_of(^^T) == ^^std::optional) {
        if (v) write(out, *v, version);
        else out += "null";
    } else if constexpr (std::ranges::range<T>) {
        out += '[';
        for (bool first = true; const auto& item : v) {
            if (!std::exchange(first, false)) out += ',';
            write(out, item, version);
        }
        out += ']';
    } else if constexpr (declares(^^T, "to_json")) {
        v.to_json(out);
    } else {
        static_assert(check(^^T) == "ok", check(^^T));
        out += '{';
        template for (constexpr info m : std::define_static_array(access::fields(^^T))) {
            if constexpr (!annotation<skip_t>(m)) {
                constexpr std::string_view key = std::define_static_string(key_of(^^T, m));
                if (version >= annotation<since>(m).value_or(since{1}).version) {
                    out += std::format("{}\"{}\":", out.back() == '{' ? "" : ",", key);
                    write(out, v.[:m:], version);
                }
            }
        }
        out += '}';
    }
}

template <class T>
std::string to_string(const T& value, int version = 1) {
    std::string out;
    write(out, value, version);
    return out;
}
}  // namespace json
