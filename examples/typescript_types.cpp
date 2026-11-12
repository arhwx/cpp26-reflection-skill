// typescript_types: typescript declarations generated from plain c++ types. every enum and class a type uses
// is declared once, after the types it needs, and types that refer to themselves or to each other work
#include <algorithm>
#include <cstdint>
#include <map>
#include <meta>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace ts {
using std::meta::info;

consteval bool is(info t, info tmpl) { return has_template_arguments(t) && template_of(t) == tmpl; }
consteval info arg(info t, std::size_t i) { return template_arguments_of(t)[i]; }

struct writer {
    std::vector<info> seen;  // enums and classes already declared, or being declared right now
    std::string out;         // the declarations so far, each one after the types it uses
    bool ok = true;          // false once a type has no typescript equivalent

    // the typescript type for t. on the way it declares every enum and class that t names
    consteval std::string type(info t) {
        t = dealias(remove_cv(t));
        if (t == ^^bool) return "boolean";
        if (is_integral_type(t) && size_of(t) == 8) return "bigint";  // too big for a js number
        if (is_arithmetic_type(t)) return "number";
        if (t == dealias(^^std::string) || t == dealias(^^std::string_view)) return "string";
        if (t == ^^std::monostate) return "null";
        if (is(t, ^^std::optional)) return type(arg(t, 0)) + " | null";
        if (is(t, ^^std::vector)) {
            std::string e = type(arg(t, 0));
            return e.contains(" | ") ? "(" + e + ")[]" : e + "[]";
        }
        if (is(t, ^^std::map)) return "Record<string, " + type(arg(t, 1)) + ">";
        if (is(t, ^^std::variant)) return join(t, " | ");
        if (is(t, ^^std::pair) || is(t, ^^std::tuple)) return "[" + join(t, ", ") + "]";
        if (is_enum_type(t) || is_class_type(t)) {
            declare(t);
            return std::string(identifier_of(t));
        }
        ok = false;
        return "?";
    }
    consteval std::string join(info t, std::string sep) {  // the type arguments of t, joined with sep
        std::string s;
        for (info a : template_arguments_of(t)) s += sep + type(a);
        return s.substr(sep.size());
    }

    // an enum becomes a union of strings and a class an interface. the types it uses are declared first
    consteval void declare(info t) {
        if (std::ranges::contains(seen, t)) return;  // this also ends a cycle: the name is all we need
        seen.push_back(t);
        std::string text, name(identifier_of(t));
        if (is_enum_type(t)) {
            for (info e : enumerators_of(t)) text += " | \"" + std::string(identifier_of(e)) + "\"";
            text = "export type " + name + " = " + text.substr(3) + ";\n";
        } else {
            constexpr auto ctx = std::meta::access_context::unprivileged();  // public bases and members only
            std::string bases;
            for (info b : bases_of(t, ctx)) bases += ", " + type(type_of(b));
            for (info m : nonstatic_data_members_of(t, ctx)) {
                info mt = dealias(remove_cv(type_of(m)));
                bool opt = is(mt, ^^std::optional);  // an optional member becomes an optional field
                text += "  " + std::string(identifier_of(m)) + (opt ? "?: " : ": ") + type(opt ? arg(mt, 0) : mt) + ";\n";
            }
            text = "export interface " + name + (bases.empty() ? "" : " extends " + bases.substr(2)) + " {\n" + text + "}\n";
        }
        out += (out.empty() ? "" : "\n") + text;
    }
};
consteval writer write(info t) {
    writer w;
    w.type(t);
    return w;
}

template <class T>
concept exportable = write(^^T).ok;
template <exportable T>  // the declarations of every enum and class that T uses
constexpr std::string_view declarations = std::define_static_string(write(^^T).out);
template <exportable T>  // the typescript type that stands for T
constexpr std::string_view type = std::define_static_string(writer{}.type(^^T));
}  // namespace ts

// ---- user code. rust with ts-rs: #[derive(TS)] struct Order { ... }
namespace shop {
enum class Currency { usd, eur, jpy };
struct Money {
    std::int64_t cents;
    Currency currency;
};
struct Entity { std::uint32_t id; };
struct Customer : Entity {
    std::string name;
    std::optional<std::string> email;
    std::map<std::string, std::string> tags;
};
struct Card { std::string last4; };
struct Transfer { std::string iban; };

struct Order : Entity {
    enum Status { open, paid, shipped };
    Customer customer;
    std::vector<std::pair<std::string, Money>> lines;  // product and price
    std::variant<std::monostate, Card, Transfer> payment;
    Status status;
    std::optional<Money> refund;

private:
    int cost = 0;  // not public, so not exported
};
}  // namespace shop

// a type that refers to itself, and two types that refer to each other
struct Person;
struct Team {
    std::string title;
    std::vector<Person> members;
    std::vector<Team> subteams;
};
struct Person {
    std::string name;
    std::optional<Team> team;
};

// type expressions are compile-time strings too
static_assert(ts::type<std::map<std::string, std::vector<std::optional<int>>>> == "Record<string, (number | null)[]>");
static_assert(ts::type<std::tuple<bool, std::int64_t, shop::Order::Status>> == "[boolean, bigint, Status]");

struct Job {
    std::string name;
    void* handle;  // a pointer has no typescript type
};
static_assert(!ts::exportable<Job>);  // so ts::declarations<Job> is a compile error, not a broken .ts file

int main() {
    std::print("{}", ts::declarations<shop::Order>);
    std::println("---");
    std::print("{}", ts::declarations<Team>);
}

// output:
// export interface Entity {
//   id: number;
// }
//
// export interface Customer extends Entity {
//   name: string;
//   email?: string;
//   tags: Record<string, string>;
// }
//
// export type Currency = "usd" | "eur" | "jpy";
//
// export interface Money {
//   cents: bigint;
//   currency: Currency;
// }
//
// export interface Card {
//   last4: string;
// }
//
// export interface Transfer {
//   iban: string;
// }
//
// export type Status = "open" | "paid" | "shipped";
//
// export interface Order extends Entity {
//   customer: Customer;
//   lines: [string, Money][];
//   payment: null | Card | Transfer;
//   status: Status;
//   refund?: Money;
// }
// ---
// export interface Person {
//   name: string;
//   team?: Team;
// }
//
// export interface Team {
//   title: string;
//   members: Person[];
//   subteams: Team[];
// }
