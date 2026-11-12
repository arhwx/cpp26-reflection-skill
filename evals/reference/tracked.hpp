#pragma once

#include <cstddef>
#include <format>
#include <functional>
#include <meta>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace track {

struct untracked_t {};
inline constexpr untracked_t untracked{};

// a string literal as a template argument
template <std::size_t N>
struct fixed_string {
    char s[N]{};
    consteval fixed_string(const char (&a)[N]) {
        for (std::size_t i = 0; i < N; ++i) s[i] = a[i];
    }
    constexpr std::string_view view() const { return {s, N - 1}; }
};

namespace detail {

consteval bool is_untracked(std::meta::info m) {
    for (std::meta::info a : std::meta::annotations_of(m))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^untracked_t) return true;
    return false;
}

consteval std::vector<std::meta::info> members(std::meta::info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unchecked());
}

consteval std::vector<std::meta::info> tracked_members(std::meta::info type) {
    std::vector<std::meta::info> out;
    for (std::meta::info m : members(type))
        if (!is_untracked(m)) out.push_back(m);
    return out;
}

// the member of type called name, or a null reflection
consteval std::meta::info find(std::meta::info type, std::string_view name) {
    for (std::meta::info m : members(type))
        if (std::meta::identifier_of(m) == name) return m;
    return {};
}

template <class T>
struct flags_of {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::meta::info m : tracked_members(^^T))
            specs.push_back(std::meta::data_member_spec(^^bool, {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class T, fixed_string Name>
inline constexpr std::meta::info member = find(^^T, Name.view());

template <class T, fixed_string Name>
concept has_member = member<T, Name> != std::meta::info{};

template <class T, fixed_string Name>
concept has_tracked_member = has_member<T, Name> && !is_untracked(member<T, Name>);

}  // namespace detail

template <class T>
using flags = typename detail::flags_of<T>::type;

template <class T>
class tracked {
    static constexpr auto all = std::define_static_array(detail::members(^^T));
    static constexpr auto watched = std::define_static_array(detail::tracked_members(^^T));
    static constexpr auto bits = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^flags<T>, std::meta::access_context::current()));

    template <fixed_string Name>
    static constexpr std::meta::info flag = [] consteval {
        for (std::meta::info b : bits)
            if (std::meta::identifier_of(b) == Name.view()) return b;
        return std::meta::info{};
    }();

public:
    explicit tracked(T initial) : value_(initial), committed_(std::move(initial)) {}

    const T& value() const { return value_; }
    const T* operator->() const { return &value_; }

    template <fixed_string Name>
        requires detail::has_member<T, Name>
    const auto& get() const {
        return value_.[:detail::member<T, Name>:];
    }

    template <fixed_string Name, class V>
        requires detail::has_member<T, Name> &&
                 std::is_convertible_v<V, typename [:std::meta::type_of(detail::member<T, Name>):]>
    bool set(V&& v) {
        using M = [:std::meta::type_of(detail::member<T, Name>):];
        M next(std::forward<V>(v));
        auto& slot = value_.[:detail::member<T, Name>:];
        if (slot == next) return false;
        slot = std::move(next);
        if constexpr (detail::has_tracked_member<T, Name>) {
            dirty_.[:flag<Name>:] = true;
            for (auto& f : observers_) f(Name.view());
        }
        return true;
    }

    const flags<T>& dirty() const { return dirty_; }

    template <fixed_string Name>
        requires detail::has_tracked_member<T, Name>
    bool is_dirty() const {
        return dirty_.[:flag<Name>:];
    }

    std::vector<std::string_view> changed() const {
        std::vector<std::string_view> out;
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, bits.size()))) {
            constexpr std::string_view name = std::meta::identifier_of(watched[i]);
            if (dirty_.[:bits[i]:]) out.push_back(name);
        }
        return out;
    }

    std::string summary() const {
        std::string out;
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, bits.size()))) {
            constexpr std::string_view name = std::meta::identifier_of(watched[i]);
            if (dirty_.[:bits[i]:]) {
                if (!out.empty()) out += ", ";
                out += std::format("{}: {} -> {}", name, committed_.[:watched[i]:], value_.[:watched[i]:]);
            }
        }
        return out.empty() ? "no changes" : out;
    }

    void commit() {
        committed_ = value_;
        dirty_ = {};
    }

    void revert() {
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, bits.size())))
            if (dirty_.[:bits[i]:]) value_.[:watched[i]:] = committed_.[:watched[i]:];
        dirty_ = {};
    }

    void on_change(std::function<void(std::string_view)> f) { observers_.push_back(std::move(f)); }

private:
    T value_;
    T committed_;
    flags<T> dirty_{};
    std::vector<std::function<void(std::string_view)>> observers_;
};

}  // namespace track
