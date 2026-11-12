#include "solution.hpp"

#include <meta>
#include <print>
#include <string_view>

struct Global {
    int id;
};

int answer() { return 42; }

namespace net {
enum class Proto { tcp, udp };
enum Kind { kind_a, kind_b };
inline constexpr int default_port = 80;

namespace http {
struct Request {
    int id;
};
enum class Method { get, post };
int parse(std::string_view text) { return static_cast<int>(text.size()); }
}  // namespace http
}  // namespace net

namespace a::b::c {
struct Deep {};
int counter = 0;
}  // namespace a::b::c

namespace nh = net::http;

static_assert(qualified_name<^^net::Proto::tcp>() == "net::Proto::tcp");
static_assert(qualified_name<^^Global>() == "Global");
static_assert(qualified_name<^^nh::Request>().size() == 18);

template <std::meta::info R>
void show() {
    constexpr std::string_view name = qualified_name<R>();
    std::string_view at_run_time = qualified_name<R>();
    std::println("{} ({}, {})", at_run_time, at_run_time.size(), name == at_run_time);
}

int main() {
    show<^^Global>();
    show<^^answer>();
    show<^^net::Proto>();
    show<^^net::Proto::udp>();
    show<^^net::Kind>();
    show<^^net::kind_b>();
    show<^^net::Kind::kind_a>();
    show<^^net::default_port>();
    show<^^net::http::Request>();
    show<^^nh::Method::post>();
    show<^^net::http::parse>();
    show<^^a::b::c::Deep>();
    show<^^a::b::c::counter>();
}
