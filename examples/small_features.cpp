// small_features: small features of other languages, each in a few lines of library code. swift CaseIterable, java
// valueOf, python Flag and pascal sets, ada ranges and bean validation, go struct tags, python repr, rust Hash
#include <array>
#include <cstdint>
#include <format>
#include <functional>
#include <meta>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace lang {
using std::meta::info;

consteval std::vector<info> fields(info type) { return nonstatic_data_members_of(type, std::meta::access_context::unprivileged()); }
template <class A>
consteval std::optional<A> annotation(info r) {
    for (info a : annotations_of(r))
        if (remove_const(type_of(a)) == ^^A) return extract<A>(a);
    return std::nullopt;
}

// swift CaseIterable, java values(): every enumerator in declaration order
template <class E>
constexpr auto cases = [] {
    std::array<E, enumerators_of(^^E).size()> out{};
    std::size_t i = 0;
    template for (constexpr info e : std::define_static_array(enumerators_of(^^E))) out[i++] = [:e:];
    return out;
}();
// java name(), rust Debug for enums
template <class E>
constexpr std::string_view name(E value) {
    template for (constexpr info e : std::define_static_array(enumerators_of(^^E)))
        if (value == [:e:]) return std::meta::identifier_of(e);
    return "?";
}
// java valueOf, rust FromStr
template <class E>
constexpr std::optional<E> parse(std::string_view text) {
    template for (constexpr info e : std::define_static_array(enumerators_of(^^E)))
        if (text == std::meta::identifier_of(e)) return [:e:];
    return std::nullopt;
}
// pascal set of, python enum.Flag: a set of enumerators that knows their names. the values need not be powers of two
template <class E>
struct set {
    std::uint64_t bits = 0;
    constexpr set(std::initializer_list<E> items) {
        for (E e : items) bits |= bit(e);
    }
    static constexpr std::uint64_t bit(E e) { return std::uint64_t{1} << (std::ranges::find(cases<E>, e) - cases<E>.begin()); }
    constexpr bool contains(E e) const { return bits & bit(e); }
    std::string text() const {
        std::string out = "{";
        for (E e : cases<E>)
            if (contains(e)) out += std::format("{}{}", out.size() > 1 ? ", " : "", name(e));
        return out + "}";
    }
};

// ada range types, java bean validation: limits written on the members and checked by one function
struct between { long low, high; };
struct not_empty_t {};
inline constexpr not_empty_t not_empty;
template <class T>
std::vector<std::string> violations(const T& object) {
    std::vector<std::string> out;
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        if constexpr (constexpr auto limits = annotation<between>(m); limits) {
            if (object.[:m:] < limits->low || object.[:m:] > limits->high)
                out.push_back(std::format("{} = {} is outside {}..{}", std::meta::identifier_of(m), object.[:m:], limits->low, limits->high));
        }
        if constexpr (annotation<not_empty_t>(m)) {
            if (std::ranges::empty(object.[:m:])) out.push_back(std::format("{} is empty", std::meta::identifier_of(m)));
        }
    }
    return out;
}

// go struct tags: free key and value pairs on a member, looked up by key
struct tag {
    const char* key;
    const char* value;
    consteval tag(std::string_view k, std::string_view v) : key(std::define_static_string(k)), value(std::define_static_string(v)) {}
};
consteval std::string_view tag_of(info member, std::string_view key) {
    for (info a : annotations_of(member))
        if (remove_const(type_of(a)) == (^^tag) && extract<tag>(a).key == key) return extract<tag>(a).value;  // ^^tag && x would parse as ^^(tag&&) x
    return "";
}

// python dataclass repr
template <class T>
std::string repr(const T& object) {
    std::string out = std::string(std::meta::identifier_of(^^T)) + "(";
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        out += std::format("{}{}=", out.back() == '(' ? "" : ", ", std::meta::identifier_of(m));
        if constexpr (std::is_convertible_v<typename [:type_of(m):], std::string_view>) out += std::format("'{}'", object.[:m:]);
        else out += std::format("{}", object.[:m:]);
    }
    return out + ")";
}
// rust derive(Hash), java hashCode
template <class T>
std::size_t hash(const T& object) {
    std::size_t h = 0;
    template for (constexpr info m : std::define_static_array(fields(^^T))) h = h * 31 + std::hash<typename [:type_of(m):]>{}(object.[:m:]);
    return h;
}
}  // namespace lang

// ---- user code
enum class Weekday { mon, tue, wed, thu, fri, sat, sun };
enum class Permission { read = 4, write = 2, execute = 1 };
struct Signup {
    [[=lang::not_empty, =lang::tag("form", "user-name")]] std::string user;
    [[=lang::between{13, 120}]] int age;
    [[=lang::between{1, 12}, =lang::tag("form", "birth-month")]] int month;
};
static_assert(lang::parse<Weekday>("thu") == Weekday::thu && !lang::parse<Weekday>("someday"));
static_assert(lang::set<Permission>{Permission::read}.contains(Permission::read));

int main() {
    for (Weekday day : lang::cases<Weekday>) std::print("{} ", lang::name(day));
    std::println("are {} days", lang::cases<Weekday>.size());
    lang::set<Permission> allowed{Permission::write, Permission::read};
    std::println("{} contains execute: {}", allowed.text(), allowed.contains(Permission::execute));
    for (const std::string& problem : lang::violations(Signup{"", 9, 14})) std::println("{}", problem);
    std::println("{} is sent as {}", std::meta::identifier_of(^^Signup::month), lang::tag_of(^^Signup::month, "form"));
    Signup ada{"ada", 36, 12};
    std::println("{}", lang::repr(ada));
    std::println("equal values, equal hashes: {}", lang::hash(ada) == lang::hash(Signup{"ada", 36, 12}));
}

// output:
// mon tue wed thu fri sat sun are 7 days
// {read, write} contains execute: false
// user is empty
// age = 9 is outside 13..120
// month = 14 is outside 1..12
// month is sent as birth-month
// Signup(user='ada', age=36, month=12)
// equal values, equal hashes: true
