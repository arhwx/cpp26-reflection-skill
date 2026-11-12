#pragma once

#include <algorithm>
#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace fsm {
using std::meta::info;

struct initial_t {};
inline constexpr initial_t initial{};
struct terminal_t {};
inline constexpr terminal_t terminal{};

struct on {
    char event[32]{};
    char target[32]{};
    consteval on(std::string_view e, std::string_view t) {
        std::ranges::copy(e, event);
        std::ranges::copy(t, target);
    }
    constexpr std::string_view event_name() const { return event; }
    constexpr std::string_view target_name() const { return target; }
};

namespace detail {

template <class A>
consteval bool has(info r) {
    for (info a : annotations_of(r))
        if (remove_const(type_of(a)) == ^^A) return true;
    return false;
}

consteval std::vector<on> transitions(info state) {
    std::vector<on> out;
    for (info a : annotations_of(state))
        if (remove_const(type_of(a)) == ^^on) out.push_back(std::meta::extract<on>(a));
    return out;
}

// index of the enumerator named name, or -1
consteval int find(const std::vector<info>& states, std::string_view name) {
    for (std::size_t i = 0; i < states.size(); ++i)
        if (identifier_of(states[i]) == name) return int(i);
    return -1;
}

consteval std::string check(info e) {
    std::vector<info> states = enumerators_of(e);
    std::vector<std::string> lines;
    std::vector<int> starts;
    for (std::size_t i = 0; i < states.size(); ++i)
        if (has<initial_t>(states[i])) starts.push_back(int(i));
    if (starts.empty()) lines.push_back("no initial state");
    if (starts.size() > 1) {
        std::string names;
        for (int s : starts) names += (names.empty() ? "" : ", ") + std::string(identifier_of(states[s]));
        lines.push_back("several initial states: " + names);
    }

    // states reachable from any initial state
    std::vector<bool> reached(states.size(), false);
    std::vector<int> todo = starts;
    for (int s : starts) reached[s] = true;
    while (!todo.empty()) {
        int s = todo.back();
        todo.pop_back();
        for (const on& t : transitions(states[s]))
            if (int n = find(states, t.target_name()); n >= 0 && !reached[n]) reached[n] = true, todo.push_back(n);
    }

    for (std::size_t i = 0; i < states.size(); ++i) {
        std::string name(identifier_of(states[i]));
        std::vector<on> ts = transitions(states[i]);
        for (std::size_t k = 0; k < ts.size(); ++k) {
            std::string ev(ts[k].event_name());
            if (find(states, ts[k].target_name()) < 0)
                lines.push_back(name + ": event '" + ev + "' goes to unknown state '" + std::string(ts[k].target_name()) + "'");
            for (std::size_t j = 0; j < k; ++j)
                if (ts[j].event_name() == ts[k].event_name()) {
                    lines.push_back(name + ": event '" + ev + "' has more than one transition");
                    break;
                }
        }
        if (!reached[i]) lines.push_back(name + ": unreachable");
        bool term = has<terminal_t>(states[i]);
        if (!term && ts.empty()) lines.push_back(name + ": dead end");
        if (term && !ts.empty()) lines.push_back(name + ": terminal state has transitions");
    }
    std::string out;
    for (const std::string& l : lines) out += (out.empty() ? "" : "\n") + l;
    return out;
}

}  // namespace detail

template <class E>
consteval std::string_view check() {
    return std::define_static_string(detail::check(^^E));
}

template <class E>
concept valid = std::is_enum_v<E> && check<E>().empty();

template <valid E>
consteval std::string_view dot() {
    std::vector<info> states = enumerators_of(^^E);
    std::string out = "digraph " + std::string(identifier_of(^^E)) + " {\n  start [shape=point];\n";
    for (info s : states)
        if (detail::has<initial_t>(s)) out += "  start -> " + std::string(identifier_of(s)) + ";\n";
    for (info s : states) {
        std::string name(identifier_of(s));
        for (const on& t : detail::transitions(s))
            out += "  " + name + " -> " + std::string(t.target_name()) + " [label=\"" + std::string(t.event_name()) + "\"];\n";
        if (detail::has<terminal_t>(s)) out += "  " + name + " [shape=doublecircle];\n";
    }
    out += "}";
    return std::define_static_string(out);
}

template <valid E>
class machine {
    static consteval E start() {
        for (info s : enumerators_of(^^E))
            if (detail::has<initial_t>(s)) return std::meta::extract<E>(constant_of(s));
        return E{};
    }

    E current = start();

public:
    machine() = default;

    E state() const { return current; }

    std::string_view state_name() const {
        template for (constexpr info s : std::define_static_array(enumerators_of(^^E)))
            if (current == [:s:]) return identifier_of(s);
        return {};
    }

    bool fire(std::string_view event) {
        template for (constexpr info s : std::define_static_array(enumerators_of(^^E))) {
            if (current == [:s:]) {
                template for (constexpr on t : std::define_static_array(detail::transitions(s))) {
                    if (event == t.event_name()) {
                        constexpr info target = enumerators_of(^^E)[detail::find(enumerators_of(^^E), t.target_name())];
                        current = [:target:];
                        return true;
                    }
                }
                return false;
            }
        }
        return false;
    }

    bool is_terminal() const {
        template for (constexpr info s : std::define_static_array(enumerators_of(^^E)))
            if (current == [:s:]) return detail::has<terminal_t>(s);
        return false;
    }
};

}  // namespace fsm
