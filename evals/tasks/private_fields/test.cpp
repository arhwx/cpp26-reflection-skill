#include "solution.hpp"

#include <print>
#include <string>
#include <string_view>

class Widget {
public:
    Widget() = default;
    int area() const { return w_ * h_; }

    int id = 0;
    std::string label;

protected:
    double scale_ = 1.0;

private:
    int w_ = 0;
    int h_ = 0;
    static inline int count_ = 0;

public:
    bool visible = true;
};

struct Plain {
    int a;
    int b;
};

class Hidden {
    int secret_ = 0;
};

static_assert(field_count<Widget>() == 6);
static_assert(field_count<Plain>() == 2);
static_assert(field_count<Hidden>() == 1);

int main() {
    std::println("{}", field_count<Widget>());
    for (std::string_view n : public_field_names<Widget>())
        std::println("{}", n);
    std::println("{} {}", public_field_names<Plain>().size(), public_field_names<Hidden>().size());
}
