// effects: algebraic effects on coroutines. a program asks for things with co_await, the caller decides what
// the requests mean by passing handlers, and a handler method is found by the effect it takes, whatever its name
#include <coroutine>
#include <format>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace eff {
using std::meta::info;

// the public member functions of handler that take this effect as their only parameter
consteval std::vector<info> methods_for(info handler, info effect) {
    std::vector<info> found;
    for (info m : members_of(handler, std::meta::access_context::unprivileged()))
        if (is_function(m) && !is_constructor(m) && parameters_of(m).size() == 1 &&
            is_same_type(remove_cvref(type_of(parameters_of(m)[0])), effect))
            found.push_back(m);
    return found;
}
consteval std::string names(std::vector<info> types) {
    std::string out;
    for (info t : types) out += (out.empty() ? "" : ", ") + std::string(identifier_of(t));
    return out;
}
// "ok", or what is wrong with these handlers for these effects
consteval std::string_view coverage(std::vector<info> effects, std::vector<info> handlers) {
    std::vector<info> missing;
    for (info e : effects) {
        std::size_t count = 0;
        for (info h : handlers) {
            if (methods_for(h, e).size() > 1) return std::define_static_string(names({h}) + " has two methods for " + names({e}));
            count += methods_for(h, e).size();
        }
        if (count == 0) missing.push_back(e);
    }
    if (missing.empty()) return "ok";
    return std::define_static_string("no handler for " + names(missing) + " among " + names(handlers));
}

struct context {  // the request the running program waits on
    const void* kind = nullptr;     // which effect type it is
    void* request = nullptr;        // the awaiter, which holds the effect and gets the answer
    std::coroutine_handle<> asker;  // the program to resume with the answer
    bool trace = false;
};
template <class E>
constexpr char kind_of = 0;  // one address per effect type

template <class E>
struct request : std::suspend_always {  // the awaiter of co_await on an effect: it fills in the context and waits
    using result = E::result;
    E effect;
    context& ctx;
    std::conditional_t<std::is_void_v<result>, std::monostate, result> answer{};
    void await_suspend(std::coroutine_handle<> self) { ctx = {&kind_of<E>, this, self, ctx.trace}; }
    result await_resume() { return static_cast<result>(std::move(answer)); }
};

template <class R, class... Effects>
struct program {
    struct promise_type {
        R result{};
        context* ctx = nullptr;
        std::coroutine_handle<> parent = std::noop_coroutine();  // resumed when this program ends
        template <class T>
        static constexpr bool listed = (std::is_same_v<T, Effects> || ...);

        program get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            struct back : std::suspend_always {
                std::coroutine_handle<> parent;
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept { return parent; }
            };
            return back{{}, parent};
        }
        void return_value(R value) { result = std::move(value); }
        void unhandled_exception() { throw; }

        template <class E>  // co_await on an effect: a request for the handlers
        request<E> await_transform(E effect) {
            static_assert(listed<E>, "this program does not list that effect");
            return {{}, std::move(effect), *ctx};
        }
        template <class R2, class... Sub>  // co_await on a program: run it here, with the same handlers
        auto await_transform(program<R2, Sub...> sub) {
            static_assert((listed<Sub> && ...), "the sub-program asks for effects this program does not list");
            struct call : std::suspend_always {
                program<R2, Sub...> sub;
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> me) {
                    sub.handle.promise().ctx = me.promise().ctx;
                    sub.handle.promise().parent = me;
                    return sub.handle;
                }
                R2 await_resume() { return std::move(sub.handle.promise().result); }
            };
            return call{{}, std::move(sub)};
        }
    };
    std::coroutine_handle<promise_type> handle;
    program(std::coroutine_handle<promise_type> h) : handle(h) {}
    program(program&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    ~program() { if (handle) handle.destroy(); }
};

template <class E>
std::string show(const E& effect) {  // for example Roll{sides: 6}
    std::string out;
    template for (constexpr info m : std::define_static_array(nonstatic_data_members_of(^^E, std::meta::access_context::unprivileged()))) {
        out += std::format("{}{}: {}", out.empty() ? "" : ", ", identifier_of(m), effect.[:m:]);
    }
    return std::format("{}{{{}}}", identifier_of(^^E), out);
}

template <class E, class H>
bool answer(context& ctx, H& handler) {  // false when this handler has no method for E
    constexpr auto found = std::define_static_array(methods_for(^^H, ^^E));
    if constexpr (found.empty()) {
        return false;
    } else {
        auto& req = *static_cast<request<E>*>(ctx.request);
        if constexpr (std::is_void_v<typename E::result>) {
            handler.[:found[0]:](req.effect);
            if (ctx.trace) std::println("  {}", show(req.effect));
        } else {
            req.answer = handler.[:found[0]:](req.effect);
            if (ctx.trace) std::println("  {} -> {}", show(req.effect), req.answer);
        }
        return true;
    }
}
template <class E, class... Handlers>
void dispatch(context& ctx, Handlers&... handlers) {  // the first handler with a method for E answers
    if (ctx.kind == &kind_of<E>) (answer<E>(ctx, handlers) || ...);
}

// runs a program to its end. each time it stops on a request, a handler answers and the program goes on
template <bool Trace = false, class R, class... Effects, class... Handlers>
R run(program<R, Effects...> prog, Handlers&... handlers) {
    static_assert(coverage({^^Effects...}, {^^Handlers...}) == "ok", coverage({^^Effects...}, {^^Handlers...}));
    context ctx{.asker = prog.handle, .trace = Trace};
    prog.handle.promise().ctx = &ctx;
    for (;;) {
        ctx.asker.resume();
        if (prog.handle.done()) return std::move(prog.handle.promise().result);
        (dispatch<Effects>(ctx, handlers...), ...);
    }
}
template <class R, class... Effects, class... Handlers>  // the same, and prints each request with its answer
R trace(program<R, Effects...> prog, Handlers&... handlers) { return run<true>(std::move(prog), handlers...); }
}  // namespace eff

// ---- user code. koka: effect dice { fun roll(sides : int) : int }; ocaml 5: perform (Roll 6)
// effects are plain structs that name the type of their answer
struct Ask { std::string prompt; using result = std::string; };
struct Tell { std::string text; using result = void; };
struct Roll { int sides; using result = int; };

// a program lists the effects it may ask for
eff::program<int, Roll, Tell> roll_twice() {
    int a = co_await Roll{6};
    int b = co_await Roll{6};
    co_await Tell{std::format("rolled {} and {}", a, b)};
    // co_await Ask{"why"};  // does not compile: this program does not list Ask
    co_return a + b;
}
eff::program<std::string, Ask, Tell, Roll> game() {
    std::string name = co_await Ask{"name"};
    int score = co_await roll_twice();  // its requests go to the same handlers
    co_await Tell{std::format("{} scores {}", name, score)};
    co_return std::format("{}:{}", name, score);
}

// handlers are plain structs too. a method is found by the effect it takes, so its name is free
struct Console {
    std::string ask(const Ask& a) { std::println("? {}", a.prompt); return "ada"; }
    void tell(const Tell& t) { std::println("> {}", t.text); }
};
struct Dice {
    int last = 3;
    int roll(const Roll& r) { return last = last % r.sides + 1; }
};
struct Script {  // a test double
    std::string canned(const Ask&) { return "test"; }
    void ignore(Tell) {}
    int always_six(const Roll&) { return 6; }
};
struct Loaded { int low(const Roll&) { return 1; } int high(Roll) { return 6; } };  // two methods for Roll

int main() {
    Console console;
    Dice dice;
    std::println("result: {}", eff::run(game(), console, dice));
    // eff::run(game(), console);  // does not compile: no handler for Roll among Console

    Script script;  // dice comes first, so it answers Roll and the script answers the rest
    std::println("result: {}", eff::trace(game(), dice, script));

    std::println("{}", eff::coverage({^^Ask, ^^Tell, ^^Roll}, {^^Console}));
    std::println("{}", eff::coverage({^^Roll}, {^^Dice, ^^Loaded}));
}

// output:
// ? name
// > rolled 4 and 5
// > ada scores 9
// result: ada:9
//   Ask{prompt: name} -> test
//   Roll{sides: 6} -> 6
//   Roll{sides: 6} -> 1
//   Tell{text: rolled 6 and 1}
//   Tell{text: test scores 7}
// result: test:7
// no handler for Roll among Console
// Loaded has two methods for Roll
