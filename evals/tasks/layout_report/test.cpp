#include "solution.hpp"

#include <cstdint>
#include <print>

struct Header {
    std::uint8_t version;
    std::uint32_t length;
    std::uint16_t kind;
};

struct Tagged {};

struct Base {
    std::uint16_t id;
};

class Message : public Tagged, public Base {
public:
    Header head;
    std::uint8_t prio : 3;
    std::uint8_t urgent : 1;
    std::uint32_t seq : 20;
    char name[5];
    double score;

private:
    bool sealed_ = false;
};

struct Bits {
    std::uint16_t low : 3;
    std::uint16_t mid : 4;
};

union Number {
    std::int64_t i;
    double d;
};

struct Cell {
    Bits flags;
    Number value;
    Header history[2];
};

namespace v1 {
struct Record { std::uint32_t id; std::uint16_t flags; std::uint16_t size; };
}
namespace v2 {  // other member types of the same sizes
struct Record { float id; std::int16_t flags; char size[2]; };
}
namespace v3 {  // members reordered
struct Record { std::uint16_t flags; std::uint32_t id; std::uint16_t size; };
}
namespace v4 {  // a member renamed
struct Record { std::uint32_t id; std::uint16_t flags; std::uint16_t bytes; };
}

// the abi guard: both must hold at compile time
static_assert(layout::signature<v1::Record> == "id@0:32;flags@32:16;size@48:16;=8/4");
static_assert(layout::hash<v1::Record> == 0xfd1f39e88010ac25);

template <class A, class B>
void compare(const char* what) {
    std::println("{}: {}", what, layout::hash<A> == layout::hash<B> ? "same layout" : "layout changed");
}

int main() {
    std::print("{}", layout::report<Header>());
    std::print("{}", layout::report<Message>());
    std::print("{}", layout::report<Bits>());
    std::print("{}", layout::report<Cell>());
    std::print("{}", layout::report<v3::Record>());

    std::println("signature of Message: {}", layout::signature<Message>);
    std::println("signature of Cell: {}", layout::signature<Cell>);
    std::println("hash of Message: {:#018x}", layout::hash<Message>);
    std::println("hash of Bits: {:#018x}", layout::hash<Bits>);

    compare<v1::Record, v2::Record>("v2 other types");
    compare<v1::Record, v3::Record>("v3 reordered");
    compare<v1::Record, v4::Record>("v4 renamed");
    compare<Header, Header>("Header with itself");
}
