#pragma once

#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace plugin {
using std::meta::info;

namespace detail {

// public named non-static member functions, in declaration order
consteval std::vector<info> methods(info type) {
    std::vector<info> out;
    for (info m : members_of(type, std::meta::access_context::unprivileged()))
        if (is_function(m) && has_identifier(m) && !is_static_member(m) && !is_constructor(m) && !is_destructor(m) &&
            !is_operator_function(m))
            out.push_back(m);
    return out;
}

consteval std::string base_spelling(info t) {
    t = dealias(t);
    if (t == ^^void) return "void";
    if (t == ^^bool) return "bool";
    if (t == ^^char) return "char";
    if (t == ^^int) return "int";
    if (t == ^^unsigned) return "unsigned";
    if (t == ^^long) return "long";
    if (t == ^^float) return "float";
    if (t == ^^double) return "double";
    return "";
}

// the C spelling of a type, or an empty string when C has no such type
consteval std::string c_spelling(info t, bool is_return) {
    t = dealias(t);
    if (is_pointer_type(t)) {
        info p = remove_pointer(t);
        std::string base = base_spelling(remove_const(p));
        if (base.empty() || is_volatile_type(p)) return "";
        return (is_const_type(p) ? "const " : "") + base + "*";
    }
    if (!is_return && t == ^^void) return "";
    return base_spelling(t);
}

consteval std::string param_name(info p, std::size_t k) {
    if (has_identifier(p)) return std::string(identifier_of(p));
    std::string digits;
    for (std::size_t n = k + 1; n > 0; n /= 10) digits.insert(digits.begin(), char('0' + n % 10));
    return "arg" + digits;
}

consteval std::string why_not(info type) {
    std::string cls(identifier_of(type));
    if (!is_default_constructible_type(type) || !is_destructible_type(type))
        return cls + " has no public default constructor";
    std::vector<info> ms = methods(type);
    for (std::size_t i = 0; i < ms.size(); ++i) {
        std::string m = cls + "::" + std::string(identifier_of(ms[i]));
        for (std::size_t j = 0; j < ms.size(); ++j)
            if (j != i && identifier_of(ms[j]) == identifier_of(ms[i])) return m + " is overloaded";
        if (c_spelling(return_type_of(ms[i]), true).empty()) return m + " returns a non-C type";
        std::vector<info> ps = parameters_of(ms[i]);
        for (std::size_t k = 0; k < ps.size(); ++k)
            if (c_spelling(type_of(ps[k]), false).empty())
                return m + " parameter " + param_name(ps[k], k) + " has a non-C type";
    }
    return "";
}

template <class R, class... A>
using fn_ptr = R (*)(A...);

// return type, then self, then the parameter types
consteval std::vector<info> signature(info m) {
    std::vector<info> out{dealias(return_type_of(m)), is_const(m) ? ^^const void* : ^^void*};
    for (info p : parameters_of(m)) out.push_back(dealias(type_of(p)));
    return out;
}

template <class T>
struct table_type {
    struct type;
    consteval {
        std::vector<info> specs{data_member_spec(^^const char*, {.name = "name"}),
                                data_member_spec(^^fn_ptr<void*>, {.name = "create"}),
                                data_member_spec(^^fn_ptr<void, void*>, {.name = "destroy"})};
        for (info m : methods(^^T))
            specs.push_back(data_member_spec(substitute(^^fn_ptr, signature(m)), {.name = identifier_of(m)}));
        define_aggregate(^^type, specs);
    }
};

// calls method M on the object self points to
template <class T, info M, class R, class Self, class... A>
R thunk(Self self, A... a) {
    using object = std::conditional_t<std::is_const_v<std::remove_pointer_t<Self>>, const T, T>;
    return static_cast<object*>(self)->[:M:](a...);
}

template <class T>
void* create() {
    return new T();
}

template <class T>
void destroy(void* self) {
    delete static_cast<T*>(self);
}

consteval info thunk_for(info type, info m) {
    std::vector<info> args{type, std::meta::reflect_constant(m)};
    args.append_range(signature(m));
    return substitute(^^thunk, args);
}

}  // namespace detail

template <class T>
consteval std::string_view why_not() {
    return std::define_static_string(detail::why_not(^^T));
}

template <class T>
concept exportable = why_not<T>().empty();

template <exportable T>
using vtable = detail::table_type<T>::type;

template <exportable T>
const vtable<T>& table() {
    static constexpr auto ms = std::define_static_array(detail::methods(^^T));
    static constexpr vtable<T> t = [] {
        return [&]<std::size_t... K>(std::index_sequence<K...>) {
            return vtable<T>{std::define_static_string(identifier_of(^^T)), &detail::create<T>, &detail::destroy<T>,
                             &[:detail::thunk_for(^^T, ms[K]):]...};
        }(std::make_index_sequence<ms.size()>{});
    }();
    return t;
}

template <exportable T>
consteval std::string_view c_header() {
    std::string cls(identifier_of(^^T));
    std::string out = "typedef struct " + cls + "_vtable {\n";
    out += "    const char* name;\n";
    out += "    void* (*create)(void);\n";
    out += "    void (*destroy)(void* self);\n";
    for (info m : detail::methods(^^T)) {
        out += "    " + detail::c_spelling(return_type_of(m), true) + " (*" + std::string(identifier_of(m)) + ")(" +
               (is_const(m) ? "const void* self" : "void* self");
        std::vector<info> ps = parameters_of(m);
        for (std::size_t k = 0; k < ps.size(); ++k)
            out += ", " + detail::c_spelling(type_of(ps[k]), false) + " " + detail::param_name(ps[k], k);
        out += ");\n";
    }
    out += "} " + cls + "_vtable;";
    return std::define_static_string(out);
}

}  // namespace plugin
