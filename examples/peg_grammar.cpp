// peg_grammar: a PEG parser, its grammar text and a printer, all derived from plain AST structs, so they cannot drift apart
#include <algorithm>
#include <charconv>
#include <functional>
#include <map>
#include <memory>
#include <meta>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace peg {
using std::meta::info;

// lit makes an empty struct a token, prefix and suffix add tokens around a member
struct lit { const char* text; consteval lit(std::string_view t) : text(std::define_static_string(t)) {} };
struct prefix { const char* text; consteval prefix(std::string_view t) : text(std::define_static_string(t)) {} };
struct suffix { const char* text; consteval suffix(std::string_view t) : text(std::define_static_string(t)) {} };

template <class A>  // the texts of the annotations of type A on r, in the order written
consteval std::span<const char* const> texts(info r) {
    std::vector<const char*> out;
    for (info a : annotations_of(r)) if (remove_const(type_of(a)) == ^^A) out.push_back(extract<A>(a).text);
    return std::define_static_array(out);
}
consteval const char* token_of(info t) { return texts<lit>(t).empty() ? nullptr : texts<lit>(t)[0]; }
consteval std::vector<info> fields(info t) { return nonstatic_data_members_of(t, std::meta::access_context::unprivileged()); }
consteval bool is(info t, info what) { return t == dealias(what) || (has_template_arguments(t) && template_of(t) == what); }

// the grammar text. ebnf is one item of a rule; rules it names for the first time join the list
consteval std::string quoted(std::string_view s) { return "\"" + std::string(s) + "\""; }
consteval std::string ebnf(info t, std::vector<info>& rules) {
    t = dealias(t);
    if (is(t, ^^int)) return "integer";
    if (is(t, ^^std::string)) return "identifier";
    if (is(t, ^^std::unique_ptr)) return ebnf(template_arguments_of(t)[0], rules);
    if (is(t, ^^std::vector)) return "{ " + ebnf(template_arguments_of(t)[0], rules) + " }";
    if (is(t, ^^std::optional)) return "[ " + ebnf(template_arguments_of(t)[0], rules) + " ]";
    if (is(t, ^^std::variant)) {
        std::string out;
        for (info a : template_arguments_of(t)) out += (out.empty() ? "( " : " | ") + ebnf(a, rules);
        return out + " )";
    }
    if (token_of(t)) return quoted(token_of(t));
    if (!std::ranges::contains(rules, t)) rules.push_back(t);
    return std::string(identifier_of(t));
}
template <class T>
consteval std::string_view grammar() {
    std::vector<info> rules{^^T};
    std::string out;
    for (std::size_t i = 0; i < rules.size(); ++i) {  // the list grows while we go
        out += std::string(identifier_of(rules[i])) + " =";
        for (info m : fields(rules[i])) {
            for (const char* p : texts<prefix>(m)) out += " " + quoted(p);
            out += " " + ebnf(type_of(m), rules);
            for (const char* s : texts<suffix>(m)) out += " " + quoted(s);
        }
        out += " ;\n";
    }
    return std::define_static_string(out);
}

// the parser. read takes a T from the front of in; a failed read gives back what it took, except spaces
constexpr std::string_view& skip(std::string_view& in) {  // spaces, tabs and newlines
    return in = in.substr(std::min(in.find_first_not_of(" \t\n"), in.size()));
}
constexpr bool eat(std::string_view& in, std::string_view token) {
    if (!skip(in).starts_with(token)) return false;
    in.remove_prefix(token.size());
    return true;
}
constexpr bool digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
template <class T>
constexpr bool read(std::string_view& in, T& out) {
    if constexpr (is(^^T, ^^int) || is(^^T, ^^std::string)) {
        constexpr bool name = is(^^T, ^^std::string);
        std::size_t n = 0;  // the length of the number or name at the front
        for (skip(in); n < in.size() && (digit(in[n]) || (name && letter(in[n]))); ++n) {}
        if (n == 0 || (name && digit(in[0]))) return false;
        if constexpr (name) out = in.substr(0, n);
        else std::from_chars(in.data(), in.data() + n, out);
        in.remove_prefix(n);
        return true;
    } else if constexpr (is(^^T, ^^std::unique_ptr)) {
        out = std::make_unique<typename T::element_type>();
        return read(in, *out);
    } else if constexpr (is(^^T, ^^std::vector)) {  // as often as it matches
        while (read(in, out.emplace_back())) {}
        out.pop_back();
        return true;
    } else if constexpr (is(^^T, ^^std::optional)) {
        if (!read(in, out.emplace())) out.reset();
        return true;
    } else if constexpr (is(^^T, ^^std::variant)) {  // the first alternative that matches
        template for (constexpr info a : std::define_static_array(template_arguments_of(^^T))) {
            if (read(in, out.template emplace<typename [:a:]>())) return true;
        }
        return false;
    } else if constexpr (token_of(^^T)) {
        return eat(in, token_of(^^T));
    } else {  // a rule: the members in order, each between its prefixes and suffixes
        std::string_view start = in;
        bool ok = true;
        template for (constexpr info m : std::define_static_array(fields(^^T))) {
            for (const char* p : texts<prefix>(m)) ok = ok && eat(in, p);
            ok = ok && read(in, out.[:m:]);
            for (const char* s : texts<suffix>(m)) ok = ok && eat(in, s);
        }
        if (!ok) in = start;
        return ok;
    }
}
template <class T>
constexpr std::optional<T> parse(std::string_view in) {  // the whole text as a T
    if (T out{}; read(in, out) && skip(in).empty()) return out;
    return std::nullopt;
}

// the printer: the tokens of a value in grammar order, joined by spaces
template <class T>
constexpr std::string to_source(const T& value) {
    std::string out;
    auto add = [&](std::string_view token) { out += out.empty() || token.empty() ? "" : " "; out += token; };
    if constexpr (is(^^T, ^^int)) {
        char buf[12];
        add({buf, std::to_chars(buf, buf + 12, value).ptr});
    } else if constexpr (is(^^T, ^^std::string)) add(value);
    else if constexpr (is(^^T, ^^std::unique_ptr)) add(to_source(*value));
    else if constexpr (is(^^T, ^^std::vector)) for (const auto& item : value) add(to_source(item));
    else if constexpr (is(^^T, ^^std::optional)) { if (value) add(to_source(*value)); }
    else if constexpr (is(^^T, ^^std::variant)) add(std::visit([](const auto& alt) { return to_source(alt); }, value));
    else if constexpr (token_of(^^T)) add(token_of(^^T));
    else template for (constexpr info m : std::define_static_array(fields(^^T))) {
        for (const char* p : texts<prefix>(m)) add(p);
        add(to_source(value.[:m:]));
        for (const char* s : texts<suffix>(m)) add(s);
    }
    return out;
}
}  // namespace peg

// ---- user code. pest: expr = { term ~ (("+" | "-") ~ term)* } in a .pest file. here the AST types are the grammar
struct [[=peg::lit("+")]] Plus : std::plus<> {};  // a token, and also the operation
struct [[=peg::lit("-")]] Minus : std::minus<> {};
struct [[=peg::lit("*")]] Times : std::multiplies<> {};
struct [[=peg::lit("/")]] Over : std::divides<> {};
struct Expr;
struct Group { [[=peg::prefix("(")]] [[=peg::suffix(")")]] std::unique_ptr<Expr> inner; };
struct Factor { std::optional<Minus> neg; std::variant<int, std::string, Group> value; };
struct MulOp { std::variant<Times, Over> op; Factor rhs; };
struct Term { Factor first; std::vector<MulOp> rest; };
struct AddOp { std::variant<Plus, Minus> op; Term rhs; };
struct Expr { Term first; std::vector<AddOp> rest; };
struct Let { [[=peg::prefix("let")]] std::string name; [[=peg::prefix("=")]] [[=peg::suffix(";")]] Expr value; };
struct Program { std::vector<Let> lets; Expr result; };

// the parser and the printer also run in the compiler
static_assert(peg::to_source(*peg::parse<Program>("let a=1;a+a")) == "let a = 1 ; a + a");
static_assert(!peg::parse<Program>("let = 3; 1"));

// an interpreter over the same types
using Env = std::map<std::string, int>;
int eval(const Factor& f, Env& env);
int eval(const auto& chain, Env& env) {  // Expr and Term: an operand, then operator and operand pairs
    int v = eval(chain.first, env);
    for (const auto& [op, rhs] : chain.rest) v = std::visit([&](auto f) { return f(v, eval(rhs, env)); }, op);
    return v;
}
int eval(int n, Env&) { return n; }
int eval(const std::string& name, Env& env) { return env[name]; }
int eval(const Group& g, Env& env) { return eval(*g.inner, env); }
int eval(const Factor& f, Env& env) { return (f.neg ? -1 : 1) * std::visit([&](const auto& x) { return eval(x, env); }, f.value); }
int eval(const Program& p, Env env = {}) {
    for (const Let& l : p.lets) env[l.name] = eval(l.value, env);
    return eval(p.result, env);
}

int main() {
    std::print("{}", peg::grammar<Program>());
    for (std::string_view src : {"let x = 2 * (3 + 4);\nlet y = x * x - -1;\ny / 3 + x", "1 +"}) {
        if (auto p = peg::parse<Program>(src)) std::println("{} => {}", peg::to_source(*p), eval(*p));
        else std::println("no parse: {:?}", src);
    }
}

// output:
// Program = { Let } Expr ;
// Let = "let" identifier "=" Expr ";" ;
// Expr = Term { AddOp } ;
// Term = Factor { MulOp } ;
// AddOp = ( "+" | "-" ) Term ;
// Factor = [ "-" ] ( integer | identifier | Group ) ;
// MulOp = ( "*" | "/" ) Factor ;
// Group = "(" Expr ")" ;
// let x = 2 * ( 3 + 4 ) ; let y = x * x - - 1 ; y / 3 + x => 79
// no parse: "1 +"
