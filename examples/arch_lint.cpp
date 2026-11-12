// arch_lint: architecture and style rules checked by the compiler. one call walks a whole namespace tree and reports
// bad names, a half-written rule of five, too much padding and lower layers that reach up into higher ones
#include <algorithm>
#include <cstdint>
#include <map>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace lint {
using std::meta::info;

struct max_padding { int bytes; };  // the annotation: struct [[=lint::max_padding{0}]] Order { ... };

constexpr auto all = std::meta::access_context::unchecked();  // private members are checked too

consteval std::string name(info r) {  // shop::core::Order
    std::string out(identifier_of(r));
    for (info p = parent_of(r); p != ^^::; p = parent_of(p)) out = std::string(identifier_of(p)) + "::" + out;
    return out;
}
consteval std::string number(std::size_t n) { return (n >= 10 ? number(n / 10) : "") + char('0' + n % 10); }

consteval void classes(info ns, std::vector<info>& out) {  // the classes in ns and in every namespace inside it
    for (info m : members_of(ns, all)) {
        if (is_namespace(m) && !is_namespace_alias(m)) classes(m, out);
        else if (is_type(m) && !is_type_alias(m) && is_class_type(m) && !is_union_type(m) && is_complete_type(m))
            out.push_back(m);
    }
}

constexpr bool lower(char c) { return c >= 'a' && c <= 'z'; }
constexpr bool upper(char c) { return c >= 'A' && c <= 'Z'; }
constexpr bool digit(char c) { return c >= '0' && c <= '9'; }
consteval bool upper_camel(std::string_view s) {
    return !s.empty() && upper(s[0]) && std::ranges::all_of(s, [](char c) { return lower(c) || upper(c) || digit(c); });
}
consteval bool snake_case(std::string_view s) {
    return !s.empty() && lower(s[0]) && std::ranges::all_of(s, [](char c) { return lower(c) || digit(c) || c == '_'; });
}

consteval int layer_of(info cls, const std::vector<info>& layers) {  // index of the layer around cls, or -1
    for (info p = parent_of(cls); p != ^^::; p = parent_of(p))
        if (auto it = std::ranges::find(layers, p); it != layers.end()) return int(it - layers.begin());
    return -1;
}

// the first class that type t uses from a layer above `mine`. looks through aliases, pointers, references,
// arrays, const and the arguments of templates such as std::vector or std::map
consteval info higher(info t, int mine, const std::vector<info>& layers) {
    t = remove_cvref(dealias(t));
    while (is_pointer_type(t) || is_array_type(t)) t = remove_cv(is_array_type(t) ? remove_extent(t) : remove_pointer(t));
    if (has_template_arguments(t)) {
        for (info a : template_arguments_of(t))
            if (info h = is_type(a) ? higher(a, mine, layers) : info{}; h != info{}) return h;
        return {};
    }
    return is_class_type(t) && layer_of(t, layers) > mine ? t : info{};
}

consteval std::vector<std::string> problems(info cls, const std::vector<info>& layers) {
    std::vector<std::string> out;
    if (!upper_camel(identifier_of(cls))) out.push_back("type name is not UpperCamelCase");
    for (info m : nonstatic_data_members_of(cls, all))
        if (!snake_case(identifier_of(m))) out.push_back("member " + std::string(identifier_of(m)) + " is not snake_case");

    const char* special[] = {"destructor", "copy constructor", "copy assignment", "move constructor", "move assignment"};
    bool has[5] = {};
    for (info f : members_of(cls, all)) {
        if (!is_function(f) || !is_user_declared(f)) continue;  // "= default" and "= delete" count, implicit ones do not
        has[0] |= is_destructor(f), has[1] |= is_copy_constructor(f), has[2] |= is_copy_assignment(f);
        has[3] |= is_move_constructor(f), has[4] |= is_move_assignment(f);
    }
    std::string yes, no;
    for (int i = 0; i < 5; ++i) {
        std::string& list = has[i] ? yes : no;
        list += (list.empty() ? "" : ", ") + std::string(special[i]);
    }
    if (!yes.empty() && !no.empty()) out.push_back("rule of five: declares " + yes + " but not " + no);

    for (info a : annotations_of(cls)) {
        if (remove_const(type_of(a)) != ^^max_padding) continue;
        std::size_t used = 0, limit = extract<max_padding>(a).bytes;
        for (info m : nonstatic_data_members_of(cls, all)) used += size_of(m);
        if (size_of(cls) - used > limit) out.push_back("padding " + number(size_of(cls) - used) + " bytes, limit " + number(limit));
    }

    if (int mine = layer_of(cls, layers); mine >= 0) {
        for (info b : bases_of(cls, all))
            if (info h = higher(type_of(b), mine, layers); h != info{}) out.push_back("base uses " + name(h) + " from a higher layer");
        for (info m : nonstatic_data_members_of(cls, all))
            if (info h = higher(type_of(m), mine, layers); h != info{})
                out.push_back("member " + std::string(identifier_of(m)) + " uses " + name(h) + " from a higher layer");
    }
    return out;
}

// one line per problem, sorted by class name. layers are namespaces, the lowest one first
consteval std::string_view report(info ns, std::vector<info> layers = {}) {
    std::vector<info> found;
    classes(ns, found);
    std::ranges::sort(found, {}, [](info c) { return name(c); });
    std::string out;
    for (info c : found)
        for (const std::string& p : problems(c, layers)) out += name(c) + ": " + p + "\n";
    return std::define_static_string(out);
}
}  // namespace lint

// ---- user code. archunit: noClasses().that().resideInAPackage("..core..").should().dependOnClassesThat().resideInAPackage("..ui..")
namespace shop {
namespace ui { struct Widget; }

namespace db {
struct Row { std::int64_t id; double value; };
struct row_cache {
    std::vector<Row> rows;
    int hitCount;
};
}  // namespace db

namespace core {
struct [[=lint::max_padding{0}]] Order {
    char kind;
    double price;
    std::int32_t qty;
};
struct Session {
    ~Session() {}
    Session(const Session&) = default;
    std::string user;
};
struct Cart {
    std::vector<Order> orders;
    std::map<int, std::vector<const ui::Widget*>> views;
};
}  // namespace core

namespace ui {
struct Widget {
    int width;
    core::Order* order;  // fine: ui sits above core
};
struct Panel : Widget { db::Row rows[4]; };
}  // namespace ui

namespace core { struct Screen : ui::Panel {}; }
}  // namespace shop

namespace fixed {  // the same classes after the fixes
struct [[=lint::max_padding{3}]] Order { double price; std::int32_t qty; char kind; };
struct Session { std::string user; };  // rule of zero
}  // namespace fixed

// a lint that fails the build. the same line with ^^shop stops the compile and shows the report as the error
static_assert(lint::report(^^fixed).empty(), lint::report(^^fixed));

int main() {
    // the whole check runs in the compiler, the program only prints the result
    constexpr std::string_view problems = lint::report(^^shop, {^^shop::db, ^^shop::core, ^^shop::ui});
    std::print("{}", problems);
    std::println("fixed: {}", lint::report(^^fixed).empty() ? "no problems" : "problems");
}

// output:
// shop::core::Cart: member views uses shop::ui::Widget from a higher layer
// shop::core::Order: padding 11 bytes, limit 0
// shop::core::Screen: base uses shop::ui::Panel from a higher layer
// shop::core::Session: rule of five: declares destructor, copy constructor but not copy assignment, move constructor, move assignment
// shop::db::row_cache: type name is not UpperCamelCase
// shop::db::row_cache: member hitCount is not snake_case
// fixed: no problems
