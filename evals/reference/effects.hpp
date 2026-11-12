#pragma once

#include <algorithm>
#include <coroutine>
#include <exception>
#include <format>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace eff {
using std::meta::info;

namespace detail {
struct context {  // what the running program is asking for right now
    const void* effect = nullptr;   // the request
    const void* kind = nullptr;     // tells its type
    void* answer = nullptr;         // where the result goes
    std::coroutine_handle<> asker;  // whom to resume once it is there
    std::vector<std::string>* log = nullptr;
};
template <class E>
inline constexpr char kind_of = 0;  // one address per effect type
template <class T>
using stored = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

consteval std::string names(const std::vector<info>& types) {
    std::string out;
    for (info t : types) out += (out.empty() ? "" : ", ") + std::string(std::meta::identifier_of(t));
    return out;
}

// empty when every type in wanted is in row
consteval std::string_view unlisted(std::vector<info> wanted, std::vector<info> row) {
    std::vector<info> extra;
    for (info e : wanted)
        if (!std::ranges::contains(row, e)) extra.push_back(e);
    if (extra.empty()) return "";
    return std::define_static_string(names(extra) + " is not among the effects this program lists: " + names(row));
}

// the public member functions of handler that take this effect, whatever they are called
consteval std::vector<info> methods_for(info handler, info effect) {
    std::vector<info> found;
    for (info m : std::meta::members_of(handler, std::meta::access_context::unprivileged())) {
        if (!std::meta::is_function(m) || std::meta::is_constructor(m)) continue;
        auto params = std::meta::parameters_of(m);
        if (params.size() == 1 &&
            std::meta::is_same_type(std::meta::remove_cvref(std::meta::type_of(params[0])), std::meta::dealias(effect)))
            found.push_back(m);
    }
    return found;
}
}  // namespace detail

// "ok", or what is wrong with this set of handlers for these effects
consteval std::string_view coverage(std::vector<info> effects, std::vector<info> handlers) {
    for (info h : handlers)
        for (info e : effects)
            if (detail::methods_for(h, e).size() > 1)
                return std::define_static_string(detail::names({h}) + " has two handlers for " + detail::names({e}));
    std::vector<info> missing;
    for (info e : effects)
        if (std::ranges::none_of(handlers, [&](info h) { return !detail::methods_for(h, e).empty(); }))
            missing.push_back(e);
    if (missing.empty()) return "ok";
    return std::define_static_string("no handler for " + detail::names(missing) + " among " + detail::names(handlers));
}

template <class R, class... Effects>
struct program {
    struct promise_type {
        std::optional<R> result;
        detail::context* ctx = nullptr;
        std::coroutine_handle<> parent = std::noop_coroutine();
        program get_return_object() { return program{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            struct back_to_parent {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    return h.promise().parent;
                }
                void await_resume() noexcept {}
            };
            return back_to_parent{};
        }
        void return_value(R v) { result = std::move(v); }
        void unhandled_exception() { std::terminate(); }

        // performing an effect: leave the request and go back to the runner
        template <class E>
        auto await_transform(E effect) {
            static_assert(detail::unlisted({^^E}, {^^Effects...}).empty(), detail::unlisted({^^E}, {^^Effects...}));
            struct request {
                E effect;
                detail::context& ctx;
                std::optional<detail::stored<typename E::result>> answer;
                bool await_ready() { return false; }
                void await_suspend(std::coroutine_handle<> self) {
                    ctx.effect = &effect;
                    ctx.kind = &detail::kind_of<E>;
                    ctx.answer = &answer;
                    ctx.asker = self;
                }
                typename E::result await_resume() { return static_cast<typename E::result>(std::move(*answer)); }
            };
            return request{std::move(effect), *ctx, {}};
        }

        // a sub-program: its requests reach the same runner
        template <class R2, class... Sub>
        auto await_transform(program<R2, Sub...> child) {
            static_assert(detail::unlisted({^^Sub...}, {^^Effects...}).empty(),
                          detail::unlisted({^^Sub...}, {^^Effects...}));
            struct call {
                program<R2, Sub...> child;
                promise_type& self;
                bool await_ready() { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<> me) {
                    child.handle.promise().ctx = self.ctx;
                    child.handle.promise().parent = me;
                    return child.handle;
                }
                R2 await_resume() { return std::move(*child.handle.promise().result); }
            };
            return call{std::move(child), *this};
        }
    };
    std::coroutine_handle<promise_type> handle;
    explicit program(std::coroutine_handle<promise_type> h) : handle(h) {}
    program(program&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    ~program() {
        if (handle) handle.destroy();
    }
};

namespace detail {
template <class T>
std::string describe(const T& value) {  // for example Ask{prompt: name}
    std::string out = std::string(std::meta::identifier_of(^^T)) + "{";
    bool first = true;
    template for (constexpr info m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
        out += std::format("{}{}: {}", first ? "" : ", ", std::meta::identifier_of(m), value.[:m:]);
        first = false;
    }
    return out + "}";
}

// lets this handler answer the pending request, if it has a method for it
template <class E, class H>
bool answer(H& handler, context& ctx) {
    constexpr auto found = std::define_static_array(methods_for(^^H, ^^E));
    if constexpr (found.empty()) {
        return false;
    } else {
        constexpr info m = found[0];
        const E& effect = *static_cast<const E*>(ctx.effect);
        auto& slot = *static_cast<std::optional<stored<typename E::result>>*>(ctx.answer);
        if constexpr (std::is_void_v<typename E::result>) {
            handler.[:m:](effect);
            slot.emplace();
            if (ctx.log) ctx.log->push_back(describe(effect));
        } else {
            slot.emplace(handler.[:m:](effect));
            if (ctx.log) ctx.log->push_back(std::format("{} -> {}", describe(effect), *slot));
        }
        return true;
    }
}

template <class E, class... Handlers>
void dispatch(context& ctx, Handlers&... handlers) {
    if (ctx.kind == &kind_of<E>) (answer<E>(handlers, ctx) || ...);
}
}  // namespace detail

// runs a program to the end. each request goes to the first handler with a method for it; log may be null
template <class R, class... Effects, class... Handlers>
R trace(std::vector<std::string>* log, program<R, Effects...> prog, Handlers&... handlers) {
    static_assert(coverage({^^Effects...}, {^^Handlers...}) == "ok", coverage({^^Effects...}, {^^Handlers...}));
    detail::context ctx{.asker = prog.handle, .log = log};
    prog.handle.promise().ctx = &ctx;
    for (;;) {
        ctx.asker.resume();
        if (prog.handle.done()) break;
        (detail::dispatch<Effects>(ctx, handlers...), ...);
    }
    return std::move(*prog.handle.promise().result);
}

template <class R, class... Effects, class... Handlers>
R run(program<R, Effects...> prog, Handlers&... handlers) {
    return trace(nullptr, std::move(prog), handlers...);
}
}  // namespace eff
