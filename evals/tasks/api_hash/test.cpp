#include "solution.hpp"

#include <cstddef>
#include <print>

namespace shop {
using id_t = unsigned long;
inline constexpr int version = 3;

enum class Currency : unsigned char { eur = 1, usd = 2, gbp = 4 };

struct Money {
    long long cents;
    Currency currency;

    bool operator==(const Money& other) const;
    Money scaled(double factor) const noexcept;
    static Money zero(Currency c = Currency::eur);
};

class Cart {
public:
    enum class State { open, paid = 10 };

    Cart();
    explicit Cart(id_t owner);
    Cart(const Cart&) = delete;
    ~Cart();

    void add(id_t item, int count = 1);
    Money total() const;
    State state() const noexcept;
    const char* label() const;
    static int max_items;

private:
    int secret_ = 0;
    void recount();
};

Money parse(const char* text, std::size_t length);
Money parse(const char* text);
void apply(Cart& cart, Money&& discount) noexcept;
char* const* argv_like(int*& slot);
template <class T>
T convert(const Money& m);

namespace tax {
double rate(Currency c);
inline int rounding = 2;
}  // namespace tax

namespace detail {
int cache_size();
}  // namespace detail
}  // namespace shop

namespace geo_a {
struct Point {
    double x, y;
};
double dist(const Point& a, const Point& b);
inline constexpr int dims = 2;
}  // namespace geo_a

namespace geo_b {  // the same api in another order, with other private and detail parts
inline constexpr int dims = 2;
struct Point {
    double x, y;

private:
    int cache_ = 0;
};
namespace detail {
int helper();
}
double dist(Point const& from, Point const& to) { return from.x - to.x; }
}  // namespace geo_b

namespace geo_c {  // aliases from detail are looked through
namespace detail {
using real = double;
}
struct Point {
    detail::real x, y;
};
namespace detail {
using cref = const Point&;
}
detail::real dist(detail::cref a, detail::cref b);
inline constexpr int dims = 2;
}  // namespace geo_c

namespace geo_d {  // parameters by value
struct Point {
    double x, y;
};
double dist(Point a, Point b);
inline constexpr int dims = 2;
}  // namespace geo_d

namespace geo_e {  // a new default argument
struct Point {
    double x, y;
};
double dist(const Point& a, const Point& b, int norm = 2);
inline constexpr int dims = 2;
}  // namespace geo_e

namespace geo_f {  // a variable changes type
struct Point {
    double x, y;
};
double dist(const Point& a, const Point& b);
inline constexpr long dims = 2;
}  // namespace geo_f

// the api guard: fails to compile when the public api of geo_a changes
static_assert(surface::hash<^^geo_a> == 0x11552bc56ae2a317);

template <std::meta::info A, std::meta::info B>
void compare(const char* what) {
    std::println("{}: {}", what, surface::hash<A> == surface::hash<B> ? "same api" : "api changed");
}

int main() {
    std::print("{}", surface::snapshot<^^shop>);
    std::println("hash of shop: {:#018x}", surface::hash<^^shop>);
    std::print("{}", surface::snapshot<^^geo_a>);
    std::println("hash of geo_a: {:#018x}", surface::hash<^^geo_a>);
    compare<^^geo_a, ^^geo_b>("geo_b reordered");
    compare<^^geo_a, ^^geo_c>("geo_c aliases");
    compare<^^geo_a, ^^geo_d>("geo_d by value");
    compare<^^geo_a, ^^geo_e>("geo_e default argument");
    compare<^^geo_a, ^^geo_f>("geo_f long dims");
}
