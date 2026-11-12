// utility_types: typescript's Partial, Pick and Omit for plain structs. each one is a real struct with the
// original member names, values move between related structs by name, and get<"name"> checks the name
#include <algorithm>
#include <format>
#include <meta>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ts {
using std::meta::info;

template <std::size_t N>
struct name_list {  // a string literal as a template argument
    char data[N]{};
    consteval name_list(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

consteval std::vector<info> fields(info type) { return nonstatic_data_members_of(type, std::meta::access_context::unprivileged()); }
consteval info find(info type, std::string_view name) {  // the member with that name, or a null reflection
    for (info m : fields(type))
        if (identifier_of(m) == name) return m;
    return {};
}
consteval bool is_optional(info type) { return has_template_arguments(dealias(type)) && template_of(dealias(type)) == ^^std::optional; }

// partial<T>: the same members, each one wrapped in std::optional
template <class T>
struct partial_of {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : fields(^^T)) specs.push_back(data_member_spec(substitute(^^std::optional, {type_of(m)}), {.name = identifier_of(m)}));
        define_aggregate(^^type, specs);
    }
};

// pick<T, "a b"> keeps the listed members, omit<T, "a b"> drops them. both keep the order of T
template <class T, name_list Names, bool Keep>
struct select_of {
    static consteval std::vector<std::string_view> listed() {
        std::vector<std::string_view> out;
        for (auto word : std::views::split(Names.view(), ' ')) out.emplace_back(word);
        return out;
    }
    static_assert(std::ranges::all_of(listed(), [](std::string_view n) { return find(^^T, n) != info{}; }), "a listed name is not a member");
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : fields(^^T))
            if (std::ranges::contains(listed(), identifier_of(m)) == Keep) specs.push_back(data_member_spec(type_of(m), {.name = identifier_of(m)}));
        define_aggregate(^^type, specs);
    }
};

template <class T> using partial = partial_of<T>::type;
template <class T, name_list Names> using pick = select_of<T, Names, true>::type;
template <class T, name_list Names> using omit = select_of<T, Names, false>::type;

// copies each member of src that dst also has, matched by name
template <class Dst, class Src>
constexpr void assign(Dst& dst, const Src& src) {
    template for (constexpr info d : std::define_static_array(fields(^^Dst))) {
        constexpr info s = find(^^Src, identifier_of(d));
        if constexpr (s == info{}) continue;  // src has no member of that name
        else if constexpr (!is_optional(type_of(s))) dst.[:d:] = src.[:s:];
        else if (src.[:s:]) dst.[:d:] = *src.[:s:];  // an empty optional leaves dst alone
    }
}

template <class Dst, class Src>
constexpr Dst as(const Src& src) {  // the members dst has, the rest value-initialized
    Dst dst{};
    assign(dst, src);
    return dst;
}

template <class T>  // T comes from base only, so changes can be a braced list
constexpr T with(T base, const std::type_identity_t<partial<T>>& changes) {  // { ...base, ...changes }
    assign(base, changes);
    return base;
}

template <name_list Name, class T>
    requires(find(^^T, Name.view()) != info{})
constexpr auto& get(T& object) {
    return object.[:find(^^T, Name.view()):];
}

template <class T>
std::string show(const T& object) {
    std::string out = "{";
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        out += std::format("{}{}: ", out.size() > 1 ? ", " : "", identifier_of(m));
        if constexpr (is_optional(type_of(m))) out += object.[:m:] ? std::format("{}", *object.[:m:]) : "none";
        else out += std::format("{}", object.[:m:]);
    }
    return out + "}";
}
}  // namespace ts

// ---- user code. typescript: type Patch = Partial<User>; type PublicUser = Omit<User, "password_hash">
struct User {
    int id;
    std::string name;
    std::string email;
    std::string password_hash;
};
using Patch = ts::partial<User>;                      // Partial<User>
using PublicUser = ts::omit<User, "password_hash">;   // Omit<User, "password_hash">
using Login = ts::pick<User, "email password_hash">;  // Pick<User, "email" | "password_hash">

static_assert(std::is_same_v<decltype(Patch::id), std::optional<int>>);
static_assert(std::is_same_v<decltype(PublicUser::email), std::string> && sizeof(Login) == 2 * sizeof(std::string));
static_assert(std::is_same_v<Login, ts::pick<User, "email password_hash">>);  // the same arguments give the same type

template <class T, ts::name_list Name>
constexpr bool can_get = requires(T& t) { ts::get<Name>(t); };
static_assert(can_get<User, "name">);
static_assert(!can_get<User, "nme">);    // a misspelled name does not compile
static_assert(!can_get<Login, "name">);  // and Login has no name at all
// using Bad = ts::pick<User, "emial">;  // error: a listed name is not a member

int main() {
    User sam{1, "sam", "sam@example.org", "x9f"};
    User max = ts::with(sam, {.name = "max", .email = "max@example.org"});  // { ...sam, name: "max", email: ... }
    std::println("{}", ts::show(max));
    std::println("{}", ts::show(ts::as<PublicUser>(max)));           // no password hash
    std::println("{}", ts::show(ts::as<Login>(sam)));
    std::println("{}", ts::show(ts::as<User>(ts::as<Login>(sam))));  // what Login lacks starts empty

    Patch patch{.id = 7};
    std::println("{}", ts::show(patch));
    ts::assign(sam, patch);  // Object.assign(sam, patch)
    ts::get<"name">(sam) += "uel";
    std::println("{}", ts::show(sam));
}

// output:
// {id: 1, name: max, email: max@example.org, password_hash: x9f}
// {id: 1, name: max, email: max@example.org}
// {email: sam@example.org, password_hash: x9f}
// {id: 0, name: , email: sam@example.org, password_hash: x9f}
// {id: 7, name: none, email: none, password_hash: none}
// {id: 7, name: samuel, email: sam@example.org, password_hash: x9f}
