// mocks: test doubles made from a plain interface struct, with no MOCK_METHOD macros. mock<I> records every
// call and answers as told, and logged<I>(obj) forwards each call to a real object and prints it
#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <meta>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace mocks {
using std::meta::info;
constexpr auto ctx = std::meta::access_context::unprivileged();

consteval std::vector<info> methods(info type) {  // named member functions: no constructors, destructors, operators
    std::vector<info> out;
    for (info m : members_of(type, ctx))
        if (is_function(m) && has_identifier(m) && !is_static_member(m)) out.push_back(m);
    return out;
}
consteval std::vector<info> signature(info m) {  // return type followed by the parameter types
    std::vector<info> out{dealias(return_type_of(m))};
    for (info p : parameters_of(m)) out.push_back(dealias(type_of(p)));
    return out;
}
consteval info find_impl(info type, info m) {  // the member function of type with the name and signature of m
    for (info c : methods(type))
        if (identifier_of(c) == identifier_of(m) && signature(c) == signature(m)) return c;
    return {};
}
template <class T, class I>  // T has every method of I
concept implements = std::ranges::all_of(methods(^^I), [](info m) { return find_impl(^^T, m) != info{}; });

// one member of a mock: records each call, then answers with what returns or invokes set
template <info /* the method, not needed here */, class R, class... A>
class mock_method {
    mutable std::vector<std::tuple<std::remove_cvref_t<A>...>> log;
    std::function<R(A...)> answer;

public:
    R operator()(A... args) const {
        log.emplace_back(args...);
        return answer ? answer(args...) : R();  // R() is 0, "" or void until told otherwise
    }
    void returns(auto value) { answer = [r = R(value)](A...) { return r; }; }
    void invokes(auto fn) { answer = fn; }
    std::size_t count() const { return log.size(); }
    const auto& calls() const { return log; }
};

template <class V>
std::string show(const V& v) {  // strings in quotes
    if constexpr (std::is_convertible_v<V, std::string_view>) return std::format("\"{}\"", std::string_view(v));
    else return std::format("{}", v);
}

// one member of a logging proxy: calls the object's method with the same name and signature, then prints the call
template <class T, info M, class R, class... A>
struct logged_method {
    T* self = nullptr;
    R operator()(A... args) const {
        std::string params;  // key="hits", value=1
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, sizeof...(A)))) {
            constexpr std::string_view name = identifier_of(parameters_of(M)[i]);  // named as in the interface
            params += std::format("{}{}={}", i ? ", " : "", name, show(args...[i]));
        }
        if constexpr (std::is_void_v<R>) {
            self->[:find_impl(^^T, M):](args...);
            std::println("[log] {}({})", identifier_of(M), params);
        } else {
            R result = self->[:find_impl(^^T, M):](args...);
            std::println("[log] {}({}) -> {}", identifier_of(M), params, show(result));
            return result;
        }
    }
};

// struct type { Wrap<Extra..., m, return type, parameter types...> <name of m>; ... } for each method m of I
template <class I, info Wrap, class... Extra>
struct per_method {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : methods(^^I)) {
            std::vector<info> args{^^Extra..., std::meta::reflect_constant(m)};
            args.append_range(signature(m));
            specs.push_back(data_member_spec(substitute(Wrap, args), {.name = identifier_of(m)}));
        }
        define_aggregate(^^type, specs);
    }
};

template <class I>
using mock = per_method<I, ^^mock_method>::type;

template <class I, implements<I> T>
auto logged(T& object) {
    using proxy_t = per_method<I, ^^logged_method, T>::type;
    proxy_t proxy;
    template for (constexpr info m : std::define_static_array(nonstatic_data_members_of(^^proxy_t, ctx)))
        proxy.[:m:].self = &object;
    return proxy;
}
}  // namespace mocks

// ---- user code. gmock: struct MockStore : Store { MOCK_METHOD(int, get, (std::string_view key), (const, override)); ... };
// the interface is a plain struct of declarations: nothing virtual, nothing defined
struct Store {
    int get(std::string_view key) const;
    void put(std::string_view key, int value);
    bool erase(const std::string& key);
};

// code under test is a template, so it takes the real store, a mock or a proxy
template <class S>
int bump(S& store, std::string_view key) {
    int next = store.get(key) + 1;
    store.put(key, next);
    return next;
}

// the real store knows nothing about Store and names its parameters differently
class MemoryStore {
    std::map<std::string, int, std::less<>> data;

public:
    int get(std::string_view k) const { auto it = data.find(k); return it == data.end() ? 0 : it->second; }
    void put(std::string_view k, int v) { data[std::string(k)] = v; }
    void put(std::string_view k, double v) { data[std::string(k)] = int(v * 100); }  // not in Store, the proxy skips it
    bool erase(const std::string& k) { return data.erase(k) > 0; }
};

static_assert(mocks::implements<MemoryStore, Store>);
static_assert(!mocks::implements<std::map<std::string, int>, Store>);  // so logged<Store>(a_map) does not compile

// the mock takes exactly the parameter types of the interface
using StoreMock = mocks::mock<Store>;
static_assert(std::is_invocable_v<decltype(StoreMock::put), std::string_view, int>);
static_assert(!std::is_invocable_v<decltype(StoreMock::put), std::string_view, std::string_view>);

int main() {
    StoreMock store;        // mockito: Store store = mock(Store.class)
    store.get.returns(41);  // mockito: when(store.get(any())).thenReturn(41)
    std::println("bump: {}", bump(store, "hits"));
    std::println("get calls: {}, put calls: {}", store.get.calls(), store.put.calls());
    auto [key, value] = store.put.calls()[0];  // mockito: verify(store).put("hits", 42)
    std::println("put got {} = {}", key, value);

    store.erase.invokes([](const std::string& k) { return k.starts_with("tmp"); });
    bool a = store.erase("tmp1"), b = store.erase("keep");
    std::println("erase: {} {} after {} calls", a, b, store.erase.count());

    std::println("--- logged");
    MemoryStore real;
    auto logged = mocks::logged<Store>(real);
    bump(logged, "hits");
    bump(logged, "hits");
    std::println("real store: hits = {}", real.get("hits"));
    logged.erase("hits");
    logged.erase("hits");
}

// output:
// bump: 42
// get calls: [("hits")], put calls: [("hits", 42)]
// put got hits = 42
// erase: true false after 2 calls
// --- logged
// [log] get(key="hits") -> 0
// [log] put(key="hits", value=1)
// [log] get(key="hits") -> 1
// [log] put(key="hits", value=2)
// real store: hits = 2
// [log] erase(key="hits") -> true
// [log] erase(key="hits") -> false
