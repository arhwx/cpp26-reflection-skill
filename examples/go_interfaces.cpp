// go_interfaces: go-style interfaces. a type satisfies an interface by having the methods, no inheritance,
// and dyn<I> erases the type while keeping real method-call syntax: s->area()
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace iface {
using std::meta::info;

// a member function bound to an erased object; the generated table has one of these per method
template <class R, class... A>
struct method {
    void* self;
    R (*fn)(void*, A...);
    R operator()(A... a) const { return fn(self, std::forward<A>(a)...); }
};

consteval std::vector<info> methods(info type) {  // named member functions: no constructors, destructors, operators
    std::vector<info> out;
    for (info m : members_of(type, std::meta::access_context::unprivileged()))
        if (is_function(m) && has_identifier(m) && !is_static_member(m)) out.push_back(m);
    return out;
}
consteval std::vector<info> signature(info m) {  // return type followed by the parameter types, without alias sugar
    std::vector<info> out{dealias(return_type_of(m))};
    for (info p : parameters_of(m)) out.push_back(dealias(type_of(p)));
    return out;
}
// the member function of type that implements interface method m, or a null reflection
consteval info find_impl(info type, info m) {
    for (info c : methods(type))
        if (identifier_of(c) == identifier_of(m) && signature(c) == signature(m) && (is_const(c) || !is_const(m))) return c;
    return {};
}

consteval std::string_view why_not(info type, info i) {  // empty when type satisfies i
    std::string missing;
    for (info m : methods(i))
        if (find_impl(type, m) == info{}) missing += (missing.empty() ? "" : ", ") + std::string(identifier_of(m));
    if (missing.empty()) return "";
    return std::define_static_string(std::string(identifier_of(type)) + " does not satisfy " + std::string(identifier_of(i)) +
                                     ": missing " + missing);
}
template <class T, class I>
concept implements = why_not(^^T, ^^I).empty();

template <class I>
struct table {  // struct type { method<double> area; method<void, double> scale; ... };
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : methods(^^I)) specs.push_back(data_member_spec(substitute(^^method, signature(m)), {.name = identifier_of(m)}));
        define_aggregate(^^type, specs);
    }
};

template <class T, info Impl, class R, class... A>
R thunk(void* self, A... a) {
    return static_cast<T*>(self)->[:Impl:](std::forward<A>(a)...);
}
consteval info thunk_for(info type, info m) {
    std::vector<info> args{type, std::meta::reflect_constant(find_impl(type, m))};  // a reflection of a reflection
    args.append_range(signature(m));
    return substitute(^^thunk, args);
}

template <class I>
class dyn {
    using table_t = table<I>::type;
    table_t bound;

    template <class T, std::size_t... K>
    static table_t bind(T* obj, std::index_sequence<K...>) {
        static constexpr auto ms = std::define_static_array(methods(^^I));
        return table_t{{obj, [:thunk_for(^^T, ms[K]):]}...};
    }
    template <class T>
    static table_t make(T& obj) {
        static_assert(implements<T, I>, why_not(^^T, ^^I));
        constexpr std::size_t n = methods(^^I).size();
        return bind(&obj, std::make_index_sequence<n>{});
    }

public:
    template <class T>
    dyn(T& obj) : bound(make(obj)) {}
    const table_t* operator->() const { return &bound; }
};
}  // namespace iface

// ---- user code
struct Shape {  // the interface is a plain struct of declarations
    double area() const;
    void scale(double factor);
    std::string name() const;
};

struct Rect {
    double w, h;
    double area() const { return w * h; }
    void scale(double factor) { w *= factor, h *= factor; }
    std::string name() const { return "rect"; }
};
struct Square {
    double side;
    double area() const { return side * side; }
    void scale(double factor) { side *= factor; }
    std::string name() const { return "square"; }
    void rotate() {}
};
struct Blob {
    double area() const { return 1; }
    void scale(int) {}  // wrong parameter type
};

static_assert(iface::implements<Rect, Shape> && iface::implements<Square, Shape>);
static_assert(!iface::implements<Blob, Shape>);

double total_area(const std::vector<iface::dyn<Shape>>& shapes) {
    double sum = 0;
    for (const auto& s : shapes) sum += s->area();
    return sum;
}

template <iface::implements<Shape> T>  // the same interface as a compile-time constraint
void describe(const T& s) { std::println("{} with area {}", s.name(), s.area()); }

int main() {
    Rect r{2, 3};
    Square q{4};
    std::vector<iface::dyn<Shape>> shapes{r, q};
    for (const auto& s : shapes) std::println("{}: {}", s->name(), s->area());
    for (const auto& s : shapes) s->scale(0.5);
    std::println("total after scale: {}", total_area(shapes));
    describe(q);
    std::println("{}", iface::why_not(^^Blob, ^^Shape));
    std::println("sizeof(dyn<Shape>) = {} pointers", sizeof(iface::dyn<Shape>) / sizeof(void*));
}

// output:
// rect: 6
// square: 16
// total after scale: 5.5
// square with area 4
// Blob does not satisfy Shape: missing scale, name
// sizeof(dyn<Shape>) = 6 pointers
