I want to declare a state machine on the enum of its states: each enumerator carries its outgoing transitions as annotations, and one is marked as the initial state. The compiler should check the machine (unknown target states, duplicate events, unreachable states, dead ends), give me a Graphviz DOT text of it, and run it at run time by event name.

Everything goes in `namespace fsm`:
- `fsm::initial` and `fsm::terminal`: constants used as annotations on an enumerator, `[[=fsm::initial]]` and `[[=fsm::terminal]]`.
- `fsm::on(event, target)`: an annotation on an enumerator, `[[=fsm::on("open", "Opened")]]`: on `event`, the machine goes from this state to the state whose enumerator is named `target`. Both arguments are string literals of at most 31 characters. An enumerator can carry several of them; their order is the order in which they are written.
- `template <class E> consteval std::string_view fsm::check()` (`constexpr` is fine too): an empty string when the machine of `E` has no problems, otherwise the problems as described below.
- `template <class E> concept fsm::valid`: true exactly when `E` is an enum type and `fsm::check<E>()` is empty.
- `template <class E> consteval std::string_view fsm::dot()` (`constexpr` is fine too), for a valid `E`: the DOT text described below.
- `template <class E> class fsm::machine`: a running machine of `E`. Using it with an `E` that is not valid must not compile, and `requires(fsm::machine<E> m) { m.fire("x"); }` must be false for such an `E`.
  - Default constructible; it starts in the initial state.
  - `E state() const`: the current state.
  - `std::string_view state_name() const`: the name of the enumerator of the current state.
  - `bool fire(std::string_view event)`: if the current state has a transition for `event`, moves to its target and returns `true`. Otherwise it stays and returns `false`.
  - `bool is_terminal() const`: whether the current state is annotated `fsm::terminal`.

The states of `E` are its enumerators in declaration order, which may differ from the order of their values. Enumerators in the test have distinct values. The name of a state is the enumerator name, and the name of the enum is its name without namespaces.

What `check<E>()` returns: each problem is a line, lines are joined with `\n`, with no newline at the end. First:
- `no initial state` when no enumerator is annotated `fsm::initial`, or
- `several initial states: <names>` when more than one is, with their names in declaration order joined with `, `.

Then, for each state in declaration order:
1. For each of its transitions in order: `<state>: event '<event>' goes to unknown state '<target>'` when `E` has no enumerator named `<target>`, and then `<state>: event '<event>' has more than one transition` when an earlier transition of the same state has the same event.
2. `<state>: unreachable` when the state cannot be reached from an initial state. Initial states are reachable, and so is the target of every transition of a reachable state, when the target exists.
3. `<state>: dead end` when the state is not annotated `fsm::terminal` and has no transitions.
4. `<state>: terminal state has transitions` when it is annotated `fsm::terminal` and has transitions.

Format of `dot<E>()`, where every line except the last ends with a newline and there is no newline at the end:

```
digraph <enum name> {
  start [shape=point];
  start -> <initial state>;
  <state lines>
}
```

The state lines come for each state in declaration order: first one line `  <state> -> <target> [label="<event>"];` for each of its transitions in order, then, if it is annotated `fsm::terminal`, the line `  <state> [shape=doublecircle];`. Every line inside the braces is indented by two spaces.

The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <initializer_list>
#include <print>
#include <string_view>

enum class Door {
    Closed [[=fsm::initial]] [[=fsm::on("open", "Opened")]] [[=fsm::on("lock", "Locked")]],
    Opened [[=fsm::on("close", "Closed")]],
    Locked [[=fsm::on("unlock", "Closed")]] [[=fsm::on("smash", "Broken")]],
    Broken [[=fsm::terminal]],
};

namespace net {
enum Conn : unsigned char {
    Done [[=fsm::terminal]] = 7,
    Idle [[=fsm::initial]] [[=fsm::on("dial", "Dialing")]] = 1,
    Dialing [[=fsm::on("ok", "Online")]] [[=fsm::on("fail", "Idle")]] [[=fsm::on("hangup", "Done")]] = 2,
    Online [[=fsm::on("hangup", "Done")]] [[=fsm::on("drop", "Dialing")]] = 4,
};
}  // namespace net

// one state that is both initial and terminal
enum class Single { Only [[=fsm::initial]] [[=fsm::terminal]] };

enum class NoStart {
    A [[=fsm::on("go", "B")]],
    B [[=fsm::on("go", "A")]],
};

enum class Twice {
    Left [[=fsm::initial]] [[=fsm::on("go", "Right")]],
    Right [[=fsm::initial]] [[=fsm::terminal]],
    Down [[=fsm::initial]] [[=fsm::on("go", "Left")]],
};

enum class Messy {
    Start [[=fsm::initial]] [[=fsm::on("next", "Middle")]] [[=fsm::on("skip", "End")]] [[=fsm::on("next", "End")]],
    Middle [[=fsm::on("next", "Ende")]] [[=fsm::on("back", "Start")]],
    End [[=fsm::terminal]] [[=fsm::on("restart", "Start")]],
    Island [[=fsm::on("next", "Lost")]] [[=fsm::on("next", "Gone")]],
    Lost,
    Stuck,
};

template <class E>
concept has_machine = requires(fsm::machine<E> m) { m.fire("x"); };

template <class E>
void run(std::initializer_list<std::string_view> events) {
    fsm::machine<E> m;
    std::println("start in {}", m.state_name());
    for (std::string_view ev : events) {
        bool moved = m.fire(ev);
        std::println("  {:<7} -> {}{}{}", ev, m.state_name(), moved ? "" : " (ignored)", m.is_terminal() ? " [terminal]" : "");
    }
}

template <class E>
void report(std::string_view name) {
    constexpr std::string_view problems = fsm::check<E>();
    std::println("{}: valid={}", name, fsm::valid<E>);
    if (!problems.empty()) std::println("{}", problems);
}

int main() {
    report<Door>("Door");
    report<net::Conn>("Conn");
    report<Single>("Single");
    report<NoStart>("NoStart");
    report<Twice>("Twice");
    report<Messy>("Messy");

    std::println("{}", fsm::dot<Door>());
    std::println("{}", fsm::dot<net::Conn>());
    std::println("{}", fsm::dot<Single>());

    run<Door>({"open", "lock", "close", "lock", "open", "unlock", "lock", "smash", "unlock"});
    run<net::Conn>({"dial", "fail", "ok", "dial", "ok", "drop", "ok", "hangup", "dial"});
    run<Single>({"go"});

    fsm::machine<net::Conn> c;
    c.fire("dial");
    std::println("state value {}", +static_cast<unsigned char>(c.state()));
    std::println("machines: {} {} {} {}", has_machine<Door>, has_machine<Single>, has_machine<NoStart>, has_machine<Messy>);
}
```

It must print exactly:

```
Door: valid=true
Conn: valid=true
Single: valid=true
NoStart: valid=false
no initial state
A: unreachable
B: unreachable
Twice: valid=false
several initial states: Left, Right, Down
Messy: valid=false
Start: event 'next' has more than one transition
Middle: event 'next' goes to unknown state 'Ende'
End: terminal state has transitions
Island: event 'next' goes to unknown state 'Gone'
Island: event 'next' has more than one transition
Island: unreachable
Lost: unreachable
Lost: dead end
Stuck: unreachable
Stuck: dead end
digraph Door {
  start [shape=point];
  start -> Closed;
  Closed -> Opened [label="open"];
  Closed -> Locked [label="lock"];
  Opened -> Closed [label="close"];
  Locked -> Closed [label="unlock"];
  Locked -> Broken [label="smash"];
  Broken [shape=doublecircle];
}
digraph Conn {
  start [shape=point];
  start -> Idle;
  Done [shape=doublecircle];
  Idle -> Dialing [label="dial"];
  Dialing -> Online [label="ok"];
  Dialing -> Idle [label="fail"];
  Dialing -> Done [label="hangup"];
  Online -> Done [label="hangup"];
  Online -> Dialing [label="drop"];
}
digraph Single {
  start [shape=point];
  start -> Only;
  Only [shape=doublecircle];
}
start in Closed
  open    -> Opened
  lock    -> Opened (ignored)
  close   -> Closed
  lock    -> Locked
  open    -> Locked (ignored)
  unlock  -> Closed
  lock    -> Locked
  smash   -> Broken [terminal]
  unlock  -> Broken (ignored) [terminal]
start in Idle
  dial    -> Dialing
  fail    -> Idle
  ok      -> Idle (ignored)
  dial    -> Dialing
  ok      -> Online
  drop    -> Dialing
  ok      -> Online
  hangup  -> Done [terminal]
  dial    -> Done (ignored) [terminal]
start in Only
  go      -> Only (ignored) [terminal]
state value 2
machines: true true false false
```

Write the complete header to `solution.hpp` in the current directory.
