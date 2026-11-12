#include "solution.hpp"

#include <cstddef>
#include <cstdint>
#include <print>

struct Packet {
    std::uint8_t kind;
    std::uint32_t length;
    std::uint16_t flags;
    double stamp;
    char tag[6];
    std::uint64_t checksum;
};

struct Pair {
    short a;
    short b;
};

constexpr auto packet = member_offsets<Packet>();
static_assert(packet.size() == 6);
static_assert(packet[0].name == "kind" && packet[0].offset == offsetof(Packet, kind));
static_assert(packet[1].offset == offsetof(Packet, length) && packet[1].size == sizeof(std::uint32_t));
static_assert(packet[2].offset == offsetof(Packet, flags));
static_assert(packet[3].offset == offsetof(Packet, stamp) && packet[3].size == sizeof(double));
static_assert(packet[4].name == "tag" && packet[4].size == sizeof(Packet::tag));
static_assert(packet[5].offset == offsetof(Packet, checksum));
static_assert(member_offsets<Pair>()[1].offset == offsetof(Pair, b));

int main() {
    for (const field_info& f : packet)
        std::println("{:<9}{:>4}{:>4}", f.name, f.offset, f.size);
    for (const field_info& f : member_offsets<Pair>())
        std::println("{:<9}{:>4}{:>4}", f.name, f.offset, f.size);
}
