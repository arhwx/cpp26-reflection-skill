#include "solution.hpp"

#include <map>
#include <optional>
#include <print>
#include <string>

int add(int a, int b) { return a + b; }

int divide(int num, int den) { return num / den; }

std::string greet(const std::string& name, int times = 1, bool shout = false) {
    std::string text = name;
    for (int i = 1; i < times; ++i)
        text += " " + name;
    return shout ? text + "!" : text;
}

namespace geo {
double scale(double value, double factor = 2.0) { return value * factor; }
}

int counter(int start = 10, int step = 1) { return start * 100 + step; }

int answer() { return 42; }

// the second parameter is unused, kept for old callers
int legacy(int code, int = 0) { return code * 2; }

template <class R>
void show(const char* label, const std::optional<R>& result) {
    if (result)
        std::println("{}: [{}]", label, *result);
    else
        std::println("{}: nullopt", label);
}

int main() {
    show("add", call_named<^^add>({{"a", "2"}, {"b", "3"}}));
    show("divide", call_named<^^divide>({{"num", "20"}, {"den", "4"}}));
    show("divide negative", call_named<^^divide>({{"den", "-3"}, {"num", "9"}}));
    show("add missing", call_named<^^add>({{"a", "2"}}));
    show("add unknown", call_named<^^add>({{"a", "2"}, {"b", "3"}, {"c", "4"}}));
    show("add bad int", call_named<^^add>({{"a", "2"}, {"b", "3x"}}));
    show("add empty int", call_named<^^add>({{"a", ""}, {"b", "3"}}));

    show("greet", call_named<^^greet>({{"name", "ann"}}));
    show("greet times", call_named<^^greet>({{"name", "ann"}, {"times", "3"}}));
    show("greet all", call_named<^^greet>({{"name", "ann"}, {"times", "2"}, {"shout", "true"}}));
    show("greet false", call_named<^^greet>({{"name", "bo"}, {"times", "1"}, {"shout", "false"}}));
    show("greet gap", call_named<^^greet>({{"name", "ann"}, {"shout", "true"}}));
    show("greet bad bool", call_named<^^greet>({{"name", "ann"}, {"times", "2"}, {"shout", "1"}}));
    show("greet no name", call_named<^^greet>({{"times", "2"}}));
    show("greet empty name", call_named<^^greet>({{"name", ""}}));

    show("scale", call_named<^^geo::scale>({{"value", "2.5"}}));
    show("scale factor", call_named<^^geo::scale>({{"factor", "3"}, {"value", "1.5"}}));
    show("scale bad", call_named<^^geo::scale>({{"value", "abc"}}));

    show("counter", call_named<^^counter>({}));
    show("counter start", call_named<^^counter>({{"start", "7"}}));
    show("counter step", call_named<^^counter>({{"step", "5"}}));
    show("counter both", call_named<^^counter>({{"step", "5"}, {"start", "7"}}));

    show("answer", call_named<^^answer>({}));
    show("answer extra", call_named<^^answer>({{"x", "1"}}));

    show("legacy", call_named<^^legacy>({{"code", "21"}}));
    show("legacy missing", call_named<^^legacy>({}));
}
