#pragma once

#include <coroutine>
#include <cstddef>
#include <exception>
#include <meta>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace aio {
namespace detail {
using std::meta::info;

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

consteval bool has_public(info type, std::string_view name) {
    for (info m : std::meta::members_of(type, std::meta::access_context::unprivileged()))
        if (std::meta::has_identifier(m) && std::meta::identifier_of(m) == name) return true;
    return false;
}

consteval bool is_awaitable(info type) {
    type = std::meta::remove_cv(std::meta::dealias(type));
    return std::meta::is_class_type(type) && has_public(type, "await_ready") && has_public(type, "await_suspend") &&
           has_public(type, "await_resume");
}

template <class A>
using resume_t = std::remove_cvref_t<decltype(std::declval<A&>().await_resume())>;

template <class T>
constexpr auto indices = std::define_static_array(std::views::iota(0uz, fields(^^T).size()));

// a coroutine that starts at once and frees itself when it ends
struct starter {
    struct promise_type {
        starter get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

struct counter {
    std::size_t left;
    std::coroutine_handle<> waiter;
    void done() {
        if (--left == 0 && waiter) std::exchange(waiter, {}).resume();
    }
};

template <class A, class R>
starter run_one(A& job, R& out, counter& pending) {
    out = co_await job;
    pending.done();
}
}  // namespace detail

// "ok", or the first member that is not awaitable
template <class Tasks>
consteval std::string_view check() {
    for (std::meta::info m : detail::fields(^^Tasks))
        if (!detail::is_awaitable(std::meta::type_of(m)))
            return std::define_static_string(std::string(std::meta::identifier_of(^^Tasks)) + "::" +
                                             std::string(std::meta::identifier_of(m)) + " is not awaitable");
    return "ok";
}

template <class Tasks>
struct results_of {
    static_assert(check<Tasks>() == "ok", check<Tasks>());
    struct type;
    consteval {
        // members that are not awaitable are left out, so the assertion above is the only error
        std::vector<std::meta::info> specs;
        for (std::meta::info m : detail::fields(^^Tasks))
            if (detail::is_awaitable(std::meta::type_of(m)))
                specs.push_back(std::meta::data_member_spec(
                    std::meta::dealias(std::meta::substitute(^^detail::resume_t, {std::meta::type_of(m)})),
                    {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class Tasks>
using results = typename results_of<Tasks>::type;

template <class Tasks>
class all_of {
public:
    explicit all_of(Tasks jobs) : jobs_(std::move(jobs)) {}
    all_of(const all_of&) = delete;
    all_of& operator=(const all_of&) = delete;

    bool await_ready() const noexcept { return false; }

    // starts every member in declaration order, then sleeps unless all of them are already done
    bool await_suspend(std::coroutine_handle<> waiter) {
        template for (constexpr std::size_t i : detail::indices<Tasks>) {
            constexpr std::meta::info from = detail::fields(^^Tasks)[i];
            constexpr std::meta::info to = detail::fields(std::meta::dealias(^^results<Tasks>))[i];
            detail::run_one(jobs_.[:from:], out_.[:to:], pending_);
        }
        if (pending_.left == 0) return false;
        pending_.waiter = waiter;
        return true;
    }

    results<Tasks> await_resume() { return std::move(out_); }

private:
    Tasks jobs_;
    results<Tasks> out_{};
    detail::counter pending_{detail::indices<Tasks>.size(), {}};
};

template <class Tasks>
all_of<Tasks> all(Tasks jobs) {
    return all_of<Tasks>(std::move(jobs));
}
}  // namespace aio
