#include "solution.hpp"

#include <cstdint>
#include <print>
#include <string>
#include <type_traits>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

constexpr pack::bits nibble(4);

struct Header {
    [[=pack::bits(3)]] unsigned char version;
    [[=pack::bits(1)]] bool urgent;
    [[=audit::note{2}]] [[=pack::bits(12)]] std::uint16_t length;
    [[=nibble]] unsigned channel;
    [[=pack::bits(40)]] std::uint64_t stamp;
    unsigned legacy : 5;
    std::uint32_t crc;
    [[=audit::note{1}]] std::string tag;
    double weight;
};

// a bit-field has no address
template <class P>
concept version_addressable = requires(P p) { &p.version; };
template <class P>
concept legacy_addressable = requires(P p) { &p.legacy; };
template <class P>
concept crc_addressable = requires(P p) { &p.crc; };

static_assert(version_addressable<Header> && !version_addressable<pack::packed_t<Header>>);
static_assert(!legacy_addressable<pack::packed_t<Header>>);
static_assert(crc_addressable<pack::packed_t<Header>>);
static_assert(std::is_same_v<decltype(pack::packed_t<Header>::version), unsigned char>);
static_assert(std::is_same_v<decltype(pack::packed_t<Header>::stamp), std::uint64_t>);
static_assert(std::is_same_v<decltype(pack::packed_t<Header>::tag), std::string>);

static_assert(pack::bit_width<Header>("version") == 3);
static_assert(pack::bit_width<Header>("urgent") == 1);
static_assert(pack::bit_width<Header>("length") == 12);
static_assert(pack::bit_width<Header>("channel") == 4);
static_assert(pack::bit_width<Header>("stamp") == 40);
static_assert(pack::bit_width<Header>("legacy") == 5);
static_assert(pack::bit_width<Header>("crc") == 0);
static_assert(pack::bit_width<Header>("tag") == 0);
static_assert(pack::bit_width<Header>("size") == 0);

void print(const Header& h) {
    std::println("version={} urgent={} length={} channel={} stamp={} legacy={} crc={} tag={} weight={}", h.version,
                 h.urgent, h.length, h.channel, h.stamp, +h.legacy, h.crc, h.tag, h.weight);
}

int main() {
    Header h{5, true, 1500, 9, 1'000'000'000'000, 17, 48879, "ok", 0.5};
    pack::packed_t<Header> p = pack::pack(h);
    std::uint64_t stamp = p.stamp;
    std::println("packed version={} length={} stamp={} crc={}", +p.version, +p.length, stamp, p.crc);
    print(pack::unpack<Header>(p));

    // values that do not fit are masked to the width
    Header big{9, false, 5000, 31, 1'200'000'000'000, 31, 70000, "big", -2.0};
    print(pack::unpack<Header>(pack::pack(big)));

    pack::packed_t<Header> q{.version = 7, .urgent = true, .length = 4095, .channel = 15, .stamp = 1'099'511'627'775,
                             .legacy = 31, .crc = 1, .tag = "max", .weight = 1.5};
    print(pack::unpack<Header>(q));
}
