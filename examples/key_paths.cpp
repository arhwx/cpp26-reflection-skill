// key_paths: swift-style key paths. a path such as "favorites[1].name" is checked against the struct when the
// program is compiled and becomes plain member accesses and indexing, with get, set and a functional update
#include <array>
#include <meta>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace kp {
using std::meta::info;

template <std::size_t N>
struct path {  // a string literal as a template argument
    char s[N]{};
    consteval path(const char (&a)[N]) { for (std::size_t i = 0; i < N; ++i) s[i] = a[i]; }
    constexpr std::string_view view() const { return {s, N - 1}; }
};

struct step {
    info member;  // the member to read, or null for [N]
    std::size_t index = 0;
};
struct resolved {
    std::vector<step> steps;
    std::string error = "ok";
};

consteval bool letter(char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
consteval bool digit(char c) { return c >= '0' && c <= '9'; }
consteval bool is_spec(info type, info tmpl) { return has_template_arguments(type) && template_of(type) == tmpl; }

// reads the path one step at a time and follows it through the types. stops at the first problem
consteval resolved resolve(info root, std::string_view p) {
    resolved malformed{{}, "malformed path"}, out;
    info type = root;
    std::size_t i = 0;  // where the current step starts
    auto fail = [&](std::string what) {  // such as "Person.favorites[1] has no member 'price'"
        std::string where(identifier_of(root));
        if (i > 0) where += "." + std::string(p.substr(0, i));
        return resolved{{}, where + " " + what};
    };
    while (i == 0 || i < p.size()) {
        type = dealias(type);
        if (i > 0 && p[i] == '[') {  // [N] on a C array, a std::array or a std::vector
            std::size_t end = i + 1, n = 0;
            for (; end < p.size() && digit(p[end]); ++end) n = n * 10 + (p[end] - '0');
            if (end == i + 1 || end == p.size() || p[end] != ']') return malformed;
            std::string no_element = "has no element " + std::string(p.substr(i + 1, end - i - 1));
            if (is_array_type(type)) {
                if (n >= extent(type)) return fail(no_element);
                type = remove_extent(type);
            } else if (is_spec(type, ^^std::array)) {
                if (n >= extract<std::size_t>(template_arguments_of(type)[1])) return fail(no_element);
                type = template_arguments_of(type)[0];
            } else if (is_spec(type, ^^std::vector)) {
                type = template_arguments_of(type)[0];  // the size of a vector is checked at run time
            } else {
                return fail("cannot be indexed");
            }
            out.steps.push_back({{}, n});
            i = end + 1;
        } else {  // a name, then .name
            if (i > 0 && p[i] != '.') return malformed;
            std::size_t start = i > 0 ? i + 1 : 0, end = start;
            while (end < p.size() && (letter(p[end]) || (end > start && digit(p[end])))) ++end;
            if (end == start) return malformed;
            std::string_view name = p.substr(start, end - start);
            info found{};
            if (is_class_type(type))
                for (info m : nonstatic_data_members_of(type, std::meta::access_context::unprivileged()))
                    if (has_identifier(m) && identifier_of(m) == name) found = m;
            if (found == info{}) return fail("has no member '" + std::string(name) + "'");
            out.steps.push_back({found});
            type = type_of(found);
            i = end;
        }
    }
    return out;
}

// "ok", or what is wrong with the path
template <class Root, path P>
constexpr std::string_view check = std::define_static_string(resolve(^^Root, P.view()).error);
template <class Root, path P>
constexpr auto steps = std::define_static_array(resolve(^^Root, P.view()).steps);

template <class Root, path P, std::size_t I = 0>
constexpr auto& walk(auto& x) {
    if constexpr (I == steps<Root, P>.size()) {
        return x;
    } else {
        constexpr info member = steps<Root, P>[I].member;
        constexpr std::size_t n = steps<Root, P>[I].index;
        if constexpr (member != info{}) return walk<Root, P, I + 1>(x.[:member:]);
        else if constexpr (requires { x.at(n); }) return walk<Root, P, I + 1>(x.at(n));  // std::array, std::vector
        else return walk<Root, P, I + 1>(x[n]);  // C array
    }
}

// a reference to the value at the path, const when obj is const
template <path P, class T>
constexpr auto& get(T& obj) {
    static_assert(check<std::remove_const_t<T>, P> == "ok", check<std::remove_const_t<T>, P>);
    return walk<std::remove_const_t<T>, P>(obj);
}
template <path P, class T, class V>
constexpr void set(T& obj, V&& value) {
    get<P>(obj) = std::forward<V>(value);
}
// a copy of obj with the value at the path replaced by fn(value)
template <path P, class T, class F>
constexpr T over(const T& obj, F fn) {
    T copy = obj;
    auto& target = get<P>(copy);
    target = fn(std::as_const(target));
    return copy;
}
}  // namespace kp

// ---- user code. swift: person[keyPath: \Person.favorites[1].name]
struct Address {
    std::string street, city;
    int zip;
};
struct Item {
    std::string name;
    int qty;
};
struct Person {
    std::string name;
    int age;
    Address address;
    std::array<Item, 2> favorites;
    std::vector<Address> past;
    int scores[3];
};

int main() {
    Person p{"kim", 34, {"2 elm st", "oslo", 150}, {{{"tea", 1}, {"cake", 2}}}, {{"9 old rd", "bergen", 5003}}, {7, 8, 9}};
    std::println("{} lives at {}, {}", kp::get<"name">(p), kp::get<"address.street">(p), kp::get<"address.city">(p));
    std::println("second favorite: {} x{}", kp::get<"favorites[1].name">(p), kp::get<"favorites[1].qty">(p));
    std::println("last score {}, used to live in {}", kp::get<"scores[2]">(p), kp::get<"past[0].city">(p));

    // the type at the end of a path is known when the program is compiled
    static_assert(std::is_same_v<decltype(kp::get<"favorites[1]">(p)), Item&>);
    static_assert(std::is_same_v<decltype(kp::get<"past[9].zip">(std::as_const(p))), const int&>);

    kp::set<"address.zip">(p, 151);
    kp::set<"past[0].city">(p, "rome");
    kp::get<"favorites[1].qty">(p) += 5;
    std::println("after set: zip {}, used to live in {}, cake x{}", p.address.zip, p.past[0].city, p.favorites[1].qty);

    Person older = kp::over<"age">(p, [](int age) { return age + 1; });
    std::println("over: {} in the copy, {} in the original", older.age, p.age);

    try {
        kp::set<"past[3].city">(p, "nowhere");
    } catch (const std::out_of_range&) {
        std::println("past[3]: a vector index is checked at run time");
    }

    // a bad path does not compile, and the error says where it went wrong
    // kp::get<"favorites[1].price">(p);  error: static assertion failed: Person.favorites[1] has no member 'price'
    // kp::check holds the same message as a string
    for (std::string_view message : {kp::check<Person, "favorites[1].name">, kp::check<Person, "nmae">,
                                     kp::check<Person, "address.stret">, kp::check<Person, "favorites[1].price">,
                                     kp::check<Person, "age.value">, kp::check<Person, "name[0]">,
                                     kp::check<Person, "scores[3]">, kp::check<Person, "address..city">})
        std::println("{}", message);
}

// output:
// kim lives at 2 elm st, oslo
// second favorite: cake x2
// last score 9, used to live in bergen
// after set: zip 151, used to live in rome, cake x7
// over: 35 in the copy, 34 in the original
// past[3]: a vector index is checked at run time
// ok
// Person has no member 'nmae'
// Person.address has no member 'stret'
// Person.favorites[1] has no member 'price'
// Person.age has no member 'value'
// Person.name cannot be indexed
// Person.scores has no element 3
// malformed path
