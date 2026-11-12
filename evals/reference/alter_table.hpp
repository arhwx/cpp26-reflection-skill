#pragma once

#include <format>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sql {

struct varchar {
    int length;
    consteval varchar(int n) : length(n) {}
};

// annotation values must be structural, so the name is copied into static storage
struct renamed_from {
    const char* name;
    consteval renamed_from(std::string_view n) : name(std::define_static_string(n)) {}
};

namespace detail {

using std::meta::info;

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

template <class A>
consteval info find_annotation(info member) {
    for (info a : std::meta::annotations_of(member))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A)
            return a;
    return {};
}

consteval bool is_optional(info type) {
    type = std::meta::dealias(type);
    return std::meta::has_template_arguments(type) && std::meta::template_of(type) == ^^std::optional;
}

// the value type of a column, without optional
consteval info value_type(info member) {
    info type = std::meta::dealias(std::meta::type_of(member));
    if (is_optional(type))
        type = std::meta::dealias(std::meta::template_arguments_of(type)[0]);
    return type;
}

consteval bool nullable(info member) { return is_optional(std::meta::type_of(member)); }

// the sql type of a member, or an empty string when it has none
consteval std::string column_type(info member) {
    info type = value_type(member);
    if (type == ^^bool)
        return "BOOLEAN";
    if (type == ^^short || type == ^^int || type == ^^long || type == ^^long long) {
        switch (std::meta::size_of(type)) {
        case 2: return "SMALLINT";
        case 4: return "INTEGER";
        case 8: return "BIGINT";
        }
        return "";
    }
    if (type == ^^float)
        return "REAL";
    if (type == ^^double)
        return "DOUBLE PRECISION";
    if (type == std::meta::dealias(^^std::string)) {
        if (info a = find_annotation<varchar>(member); a != info{}) {
            std::string digits;
            for (int n = std::meta::extract<varchar>(a).length; n > 0; n /= 10)
                digits.insert(digits.begin(), char('0' + n % 10));
            return "VARCHAR(" + digits + ")";
        }
        return "TEXT";
    }
    return "";
}

// the member of Old that a member of New continues, or a null reflection
consteval info match(info member, info old_type) {
    for (info o : fields(old_type))
        if (std::meta::identifier_of(o) == std::meta::identifier_of(member))
            return o;
    if (info a = find_annotation<renamed_from>(member); a != info{}) {
        std::string_view name = std::meta::extract<renamed_from>(a).name;
        for (info o : fields(old_type))
            if (std::meta::identifier_of(o) == name)
                return o;
    }
    return {};
}

consteval bool matched(info old_member, info new_type) {
    for (info n : fields(new_type))
        if (match(n, std::meta::parent_of(old_member)) == old_member)
            return true;
    return false;
}

consteval bool valid(info old_type, info new_type) {
    for (info t : {old_type, new_type})
        for (info m : fields(t))
            if (column_type(m).empty())
                return false;
    for (info o : fields(old_type)) {
        int uses = 0;
        for (info n : fields(new_type))
            if (match(n, old_type) == o)
                ++uses;
        if (uses > 1)
            return false;
    }
    return true;
}

template <class V>
std::string literal(const V& value) {
    if constexpr (std::is_same_v<V, bool>) {
        return value ? "TRUE" : "FALSE";
    } else if constexpr (std::is_same_v<V, std::string>) {
        std::string out = "'";
        for (char c : value) {
            if (c == '\'')
                out += '\'';
            out += c;
        }
        return out + "'";
    } else {
        return std::format("{}", value);
    }
}

}  // namespace detail

template <class Old, class New>
    requires(detail::valid(^^Old, ^^New))
std::vector<std::string> migration(std::string_view table) {
    std::vector<std::string> renames, drops, alters, adds;
    const std::string head = std::format("ALTER TABLE {} ", table);
    const New fresh{};
    template for (constexpr std::meta::info n : std::define_static_array(detail::fields(^^New))) {
        constexpr std::meta::info o = detail::match(n, ^^Old);
        constexpr std::string_view name = std::meta::identifier_of(n);
        constexpr std::string_view type = std::define_static_string(detail::column_type(n));
        if constexpr (o == std::meta::info{}) {
            if constexpr (detail::nullable(n))
                adds.push_back(std::format("{}ADD COLUMN {} {};", head, name, type));
            else
                adds.push_back(std::format("{}ADD COLUMN {} {} NOT NULL DEFAULT {};", head, name, type,
                                           detail::literal(fresh.[:n:])));
        } else {
            constexpr std::string_view old_name = std::meta::identifier_of(o);
            constexpr std::string_view old_type = std::define_static_string(detail::column_type(o));
            if constexpr (old_name != name)
                renames.push_back(std::format("{}RENAME COLUMN {} TO {};", head, old_name, name));
            if constexpr (old_type != type)
                alters.push_back(std::format("{}ALTER COLUMN {} TYPE {};", head, name, type));
            if constexpr (detail::nullable(o) && !detail::nullable(n))
                alters.push_back(std::format("{}ALTER COLUMN {} SET NOT NULL;", head, name));
            if constexpr (!detail::nullable(o) && detail::nullable(n))
                alters.push_back(std::format("{}ALTER COLUMN {} DROP NOT NULL;", head, name));
        }
    }
    template for (constexpr std::meta::info o : std::define_static_array(detail::fields(^^Old))) {
        if constexpr (!detail::matched(o, ^^New))
            drops.push_back(std::format("{}DROP COLUMN {};", head, std::meta::identifier_of(o)));
    }
    std::vector<std::string> out;
    for (auto* group : {&renames, &drops, &alters, &adds})
        out.insert(out.end(), group->begin(), group->end());
    return out;
}

}  // namespace sql
