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
