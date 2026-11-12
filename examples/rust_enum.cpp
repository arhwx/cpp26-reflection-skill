// rust_enum: rust-style enums with data. the alternatives are named, construction and match use the names,
// and a match that forgets an alternative does not compile
#include <functional>
#include <memory>
#include <meta>
#include <print>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace adt {
using std::meta::info;

consteval std::vector<info> alts(info spec) { return nonstatic_data_members_of(spec, std::meta::access_context::unprivileged()); }
consteval info storage_for(info spec) {  // std::variant of the alternative types, in order
    std::vector<info> types;
    for (info a : alts(spec)) types.push_back(type_of(a));
    return substitute(^^std::variant, types);
}

template <class Spec>
class sum;

// an empty callable named after alternative i; calling it builds the sum
template <class Spec, std::size_t I>
struct ctor {
    using payload = [:type_of(alts(^^Spec)[I]):];
    constexpr sum<Spec> operator()(payload value = {}) const { return sum<Spec>(std::in_place_index<I>, std::move(value)); }
};

// generates struct type { <member type from wrap> <name of alternative>; ... };
template <class Spec, info Wrap, class... Extra>
struct per_alternative {
    struct type;
    consteval {
        std::vector<info> specs;
        for (std::size_t i = 0; info a : alts(^^Spec)) {
            info arg = Wrap == ^^ctor ? std::meta::reflect_constant(i++) : type_of(a);
            std::vector<info> args{^^Extra..., arg};
            specs.push_back(data_member_spec(substitute(Wrap, args), {.name = identifier_of(a)}));
        }
        define_aggregate(^^type, specs);
    }
};
template <class R, class A>
using handler = std::function_ref<R(const A&)>;  // no default constructor, so every arm is required
template <class Spec>
using make = per_alternative<Spec, ^^ctor, Spec>::type;
template <class Spec, class R>
using arms = per_alternative<Spec, ^^handler, R>::type;

template <class Spec>
class sum {
    typename [:storage_for(^^Spec):] value;
    static constexpr auto names = std::define_static_array(alts(^^Spec));

public:
    template <std::size_t I, class A>
    constexpr sum(std::in_place_index_t<I> tag, A&& a) : value(tag, std::forward<A>(a)) {}

    template <class R>
    R match(arms<Spec, R> on) const {
        static constexpr auto hs = std::define_static_array(alts(^^arms<Spec, R>));
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, names.size()))) {
            if (value.index() == i) return on.[:hs[i]:](*std::get_if<i>(&value));
        }
        std::unreachable();
    }
    std::string_view name() const {  // name of the active alternative
        std::string_view out;
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, names.size()))) {
            if (value.index() == i) out = identifier_of(names[i]);
        }
        return out;
    }
};
}  // namespace adt

// ---- user code. rust: enum Shape { Circle { r: f64 }, Rect { w: f64, h: f64 }, Dot }
struct ShapeSpec {
    struct { double r; } circle;
    struct { double w, h; } rect;
    struct {} dot;
};
using Shape = adt::sum<ShapeSpec>;
constexpr adt::make<ShapeSpec> shape{};

double area(const Shape& s) {
    return s.match<double>({
        .circle = [](const auto& c) { return 3.0 * c.r * c.r; },
        .rect = [](const auto& r) { return r.w * r.h; },
        .dot = [](const auto&) { return 0.0; },
    });
}

// rust: enum Expr { Num(f64), Add(Box<Expr>, Box<Expr>), Mul(Box<Expr>, Box<Expr>), Neg(Box<Expr>) }
struct ExprSpec;
using Expr = adt::sum<ExprSpec>;
using Box = std::unique_ptr<Expr>;
struct ExprSpec {
    double num;
    struct { Box lhs, rhs; } add;
    struct { Box lhs, rhs; } mul;
    struct { Box arg; } neg;
};
constexpr adt::make<ExprSpec> expr{};
Box box(Expr e) { return std::make_unique<Expr>(std::move(e)); }

double eval(const Expr& e) {
    return e.match<double>({
        .num = [](const double& n) { return n; },
        .add = [](const auto& a) { return eval(*a.lhs) + eval(*a.rhs); },
        .mul = [](const auto& m) { return eval(*m.lhs) * eval(*m.rhs); },
        .neg = [](const auto& n) { return -eval(*n.arg); },
    });
}
std::string show(const Expr& e) {
    return e.match<std::string>({
        .num = [](const double& n) { return std::format("{}", n); },
        .add = [](const auto& a) { return "(" + show(*a.lhs) + " + " + show(*a.rhs) + ")"; },
        .mul = [](const auto& m) { return "(" + show(*m.lhs) + " * " + show(*m.rhs) + ")"; },
        .neg = [](const auto& n) { return "-" + show(*n.arg); },
    });
}

int main() {
    std::vector<Shape> shapes;
    shapes.push_back(shape.circle({1.0}));
    shapes.push_back(shape.rect({2.0, 3.5}));
    shapes.push_back(shape.dot());
    for (const Shape& s : shapes) std::println("{}: area {}", s.name(), area(s));

    Expr e = expr.add({box(expr.num(1)), box(expr.mul({box(expr.num(2)), box(expr.neg({box(expr.num(3))}))}))});
    std::println("{} = {}", show(e), eval(e));
    std::println("active alternative: {}", e.name());
}

// output:
// circle: area 3
// rect: area 7
// dot: area 0
// (1 + (2 * -3)) = -5
// active alternative: add
