#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <format>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace actor {
using std::meta::info;

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

// public non-static member functions declared in type that return void, in declaration order
consteval std::vector<info> handlers(info type) {
    std::vector<info> out;
    for (info m : members_of(type, std::meta::access_context::unprivileged()))
        if (is_function(m) && has_identifier(m) && !is_static_member(m) && !is_special_member_function(m) &&
            !is_operator_function(m) && !is_conversion_function(m) && dealias(return_type_of(m)) == ^^void)
            out.push_back(m);
    return out;
}

consteval std::size_t index_of(info type, std::string_view name) {
    std::vector<info> hs = handlers(type);
    for (std::size_t i = 0; i < hs.size(); ++i)
        if (identifier_of(hs[i]) == name) return i;
    return hs.size();
}

// one message struct per handler, with a member per parameter holding its value
template <class T, std::size_t I>
struct message_impl {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info p : parameters_of(handlers(^^T)[I]))
            specs.push_back(data_member_spec(remove_cvref(type_of(p)), {.name = identifier_of(p)}));
        define_aggregate(^^type, specs);
    }
};

template <class T, class Seq>
struct variant_impl;
template <class T, std::size_t... I>
struct variant_impl<T, std::index_sequence<I...>> {
    using type = std::variant<typename message_impl<T, I>::type...>;
};

template <class T>
inline constexpr std::size_t handler_count = handlers(^^T).size();

template <class T, fixed_string Name>
inline constexpr bool has_message = index_of(^^T, Name.view()) < handler_count<T>;

template <class T, fixed_string Name>
    requires has_message<T, Name>
using msg = typename message_impl<T, index_of(^^T, Name.view())>::type;

template <class T>
using message_of = typename variant_impl<T, std::make_index_sequence<handler_count<T>>>::type;

template <class V>
std::string show(const V& v) {
    if constexpr (std::is_same_v<V, std::string>) return "\"" + v + "\"";
    else return std::format("{}", v);
}

template <class T, std::size_t I>
struct handler {
    using message = typename message_impl<T, I>::type;
    static constexpr info fn = handlers(^^T)[I];
    static constexpr auto fields =
        std::define_static_array(nonstatic_data_members_of(^^message, std::meta::access_context::unprivileged()));

    template <std::size_t... J>
    static void call(T& target, message& m, std::index_sequence<J...>) {
        target.[:fn:](std::move(m.[:fields[J]:])...);
    }
    static void call(T& target, message& m) { call(target, m, std::make_index_sequence<fields.size()>{}); }

    static std::string describe(const message& m) {
        std::string out = std::string(identifier_of(fn)) + "(";
        bool first = true;
        template for (constexpr info f : fields) {
            out += std::format("{}{}={}", first ? "" : ", ", identifier_of(f), show(m.[:f:]));
            first = false;
        }
        return out + ")";
    }
};

template <class T, std::size_t... I>
std::string describe_at(const message_of<T>& m, std::index_sequence<I...>) {
    std::string out;
    ((m.index() == I ? void(out = handler<T, I>::describe(std::get<I>(m))) : void()), ...);
    return out;
}

// the handler name and the stored arguments, as name(param=value, ...)
template <class T>
std::string describe(const message_of<T>& m) {
    return describe_at<T>(m, std::make_index_sequence<handler_count<T>>{});
}

template <class T>
class mailbox {
public:
    explicit mailbox(T& target) : target_(target) {}

    void send(message_of<T> m) { queue_.push_back(std::move(m)); }
    std::size_t pending() const { return queue_.size(); }

    bool step() {
        if (queue_.empty()) return false;
        message_of<T> m = std::move(queue_.front());
        queue_.pop_front();
        dispatch(m, std::make_index_sequence<handler_count<T>>{});
        return true;
    }

    std::size_t run() {
        std::size_t n = 0;
        while (step()) ++n;
        return n;
    }

private:
    template <std::size_t... I>
    void dispatch(message_of<T>& m, std::index_sequence<I...>) {
        ((m.index() == I ? handler<T, I>::call(target_, std::get<I>(m)) : void()), ...);
    }

    T& target_;
    std::deque<message_of<T>> queue_;
};
}  // namespace actor
