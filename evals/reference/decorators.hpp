#pragma once

#include <array>
#include <cstddef>
#include <exception>
#include <format>
#include <map>
#include <meta>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace deco {

// what a decorator learns about the function it wraps
struct frame {
    std::string_view name;
    std::span<const std::string_view> params;
};

namespace detail {

template <class T>
inline constexpr bool is_decorator = requires { typename T::decorator; };

// annotations of f whose type is a decorator, in the order written
consteval std::vector<std::meta::info> decorators_of(std::meta::info f) {
    std::vector<std::meta::info> out;
    for (std::meta::info a : std::meta::annotations_of(f)) {
        std::meta::info t = std::meta::remove_const(std::meta::type_of(a));
        if (std::meta::is_class_type(t) &&
            std::meta::extract<bool>(std::meta::substitute(^^is_decorator, {t})))
            out.push_back(a);
    }
    return out;
}

template <std::meta::info F>
struct facts {
    static constexpr auto decos = std::define_static_array(decorators_of(F));
    static constexpr auto params = std::define_static_array(std::meta::parameters_of(F));
    static constexpr std::array<std::string_view, params.size()> names = [] consteval {
        std::array<std::string_view, params.size()> out{};
        for (std::size_t i = 0; i < params.size(); ++i) out[i] = std::meta::identifier_of(params[i]);
        return out;
    }();
    static constexpr frame info{std::meta::identifier_of(F), names};
};

// decorator I wraps the rest of the chain; the first one written is the outermost
template <std::meta::info F, std::size_t I, class R, class... P>
R run(P... args) {
    if constexpr (I == facts<F>::decos.size()) {
        return [:F:](args...);
    } else {
        constexpr auto d = std::meta::extract<typename [:std::meta::remove_const(std::meta::type_of(facts<F>::decos[I])):]>(
            facts<F>::decos[I]);
        auto next = [](auto&&... a) -> R { return run<F, I + 1, R, P...>(std::forward<decltype(a)>(a)...); };
        if constexpr (std::is_void_v<R>)
            d.wrap(facts<F>::info, next, args...);
        else
            return d.wrap(facts<F>::info, next, args...);
    }
}

template <std::meta::info F, class Sig>
struct invoker;

template <std::meta::info F, class R, class... P>
struct invoker<F, R(P...)> {
    R operator()(P... args) const { return run<F, 0, R, P...>(args...); }
};

template <std::meta::info F, class R, class... P>
struct invoker<F, R(P...) noexcept> : invoker<F, R(P...)> {};

inline int trace_depth = 0;

}  // namespace detail

// the decorated function: has the same parameters and return type as F
template <std::meta::info F>
struct fn : detail::invoker<F, typename [:std::meta::type_of(F):]> {};

// prints the call and its result, indented by the traced calls in progress
struct trace {
    using decorator = void;

    template <class Next, class... A>
    auto wrap(const frame& f, Next next, A&... args) const -> decltype(next(args...)) {
        std::string indent(2 * detail::trace_depth, ' ');
        std::string call = std::format("{}-> {}(", indent, f.name);
        std::size_t i = 0;
        ((call += std::format("{}{}={}", i == 0 ? "" : ", ", f.params[i], args), ++i), ...);
        std::println("{})", call);
        ++detail::trace_depth;
        try {
            if constexpr (std::is_void_v<decltype(next(args...))>) {
                next(args...);
                --detail::trace_depth;
                std::println("{}<- {}", indent, f.name);
            } else {
                auto r = next(args...);
                --detail::trace_depth;
                std::println("{}<- {} = {}", indent, f.name, r);
                return r;
            }
        } catch (const std::exception& e) {
            --detail::trace_depth;
            std::println("{}<- {} threw {}", indent, f.name, e.what());
            throw;
        }
    }
};

// remembers results by argument values, one cache per decorated function
struct memo {
    using decorator = void;

    template <class Next, class... A>
    auto wrap(const frame&, Next next, A&... args) const -> decltype(next(args...)) {
        static std::map<std::tuple<std::remove_cvref_t<A>...>, decltype(next(args...))> cache;
        std::tuple<std::remove_cvref_t<A>...> key{args...};
        if (auto it = cache.find(key); it != cache.end()) return it->second;
        auto r = next(args...);
        cache.emplace(std::move(key), r);
        return r;
    }
};

// calls again after an exception, up to times attempts in total
struct retry {
    using decorator = void;
    int times;

    template <class Next, class... A>
    auto wrap(const frame& f, Next next, A&... args) const -> decltype(next(args...)) {
        for (int i = 1;; ++i) {
            try {
                return next(args...);
            } catch (const std::exception& e) {
                std::println("retry {}: attempt {} of {} failed: {}", f.name, i, times, e.what());
                if (i >= times) throw;
            }
        }
    }
};

}  // namespace deco
