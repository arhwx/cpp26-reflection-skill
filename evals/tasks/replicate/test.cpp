#include "solution.hpp"

#include <cmath>
#include <cstdint>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

enum class Team : std::uint8_t { red, blue };
enum Stance : std::int16_t { standing = 0, crouching = 3, prone = -2 };

struct Player {
    [[=net::replicated]] std::uint32_t id = 0;
    [[=net::replicated]] std::string name;
    int local_frame = 0;
    [[=net::replicated]] Team team = Team::red;
    [[=net::replicated]] std::int16_t health = 100;
    [[=net::replicated]] float x = 0;
    [[=net::replicated]] float y = 0;
    [[=audit::note{2}]] std::string debug;
    [[=net::replicated]] double heading = 0;
    [[=net::replicated]] bool alive = true;
    [[=audit::note{1}]] [[=net::replicated]] std::int64_t score = 0;
    [[=net::replicated]] Stance stance = standing;
};

class Door {
public:
    void open(int angle) { open_ = true; angle_ = angle; }
    void print() const { std::println("door open={} angle={} cache={}", open_, angle_, cache_); }
    int cache_ = 0;

private:
    [[=net::replicated]] bool open_ = false;
    [[=net::replicated]] std::int8_t angle_ = 0;
};

using Snap = net::snapshot_t<Player>;
static_assert(std::is_same_v<Snap, net::snapshot_t<Player>>);
static_assert(std::is_aggregate_v<Snap>);
static_assert(std::is_same_v<decltype(Snap::health), std::int16_t>);
static_assert(std::is_same_v<decltype(Snap::name), std::string>);
static_assert(std::is_same_v<decltype(net::snapshot_t<Door>::angle_), std::int8_t>);

template <class S>
concept has_local_frame = requires(S s) { s.local_frame; };
template <class S>
concept has_debug = requires(S s) { s.debug; };

void dump(const char* what, const std::vector<std::uint8_t>& bytes) {
    std::print("{} ({} bytes):", what, bytes.size());
    for (std::size_t i = 0; i < bytes.size() && i < 24; ++i) std::print(" {:02x}", bytes[i]);
    std::println("{}", bytes.size() > 24 ? " ..." : "");
}

void show(const Player& p) {
    std::println("player id={} name={} team={} health={} x={} y={} heading={} alive={} score={} stance={} frame={}",
                 p.id, p.name, static_cast<int>(p.team), p.health, p.x, p.y, p.heading, p.alive, p.score,
                 static_cast<int>(p.stance), p.local_frame);
}

int main() {
    std::println("snapshot has local_frame {}, debug {}", has_local_frame<Snap>, has_debug<Snap>);

    Player p{.id = 258, .name = "ann", .team = Team::blue, .x = 1.5f, .y = -2, .heading = 0.0, .score = 7};
    Snap s0 = net::snapshot(p);
    std::vector<std::uint8_t> full = net::encode(s0);
    dump("full", full);

    Snap client{};
    std::println("apply full: {}", net::apply(client, full));

    p.health = -5;
    p.x = 2.0f;
    p.local_frame = 99;
    Snap s1 = net::snapshot(p);
    std::vector<std::uint8_t> d1 = net::encode(s0, s1);
    dump("health and x", d1);

    p.score = -1;
    p.stance = prone;
    Snap s2 = net::snapshot(p);
    std::vector<std::uint8_t> d2 = net::encode(s1, s2);
    dump("score and stance", d2);

    p.debug = "only local";
    std::vector<std::uint8_t> d3 = net::encode(s2, net::snapshot(p));
    dump("local change", d3);

    p.heading = -0.0;
    p.y = NAN;
    Snap s3 = net::snapshot(p);
    std::vector<std::uint8_t> d4 = net::encode(s2, s3);
    dump("negative zero and nan", d4);
    std::println("nan again: {} bytes", net::encode(s3, net::snapshot(p)).size());

    std::println("apply d1: {}", net::apply(client, d1));
    std::println("apply d2: {}", net::apply(client, d2));
    std::println("apply d3: {}", net::apply(client, d3));

    std::vector<std::uint8_t> cut(d4.begin(), d4.end() - 1);
    std::println("apply truncated: {}", net::apply(client, cut));
    std::vector<std::uint8_t> extra = d2;
    extra.push_back(0);
    std::println("apply with extra byte: {}", net::apply(client, extra));
    std::vector<std::uint8_t> unknown = d2;
    unknown[1] |= 0x04;
    std::println("apply with unknown field: {}", net::apply(client, unknown));
    std::println("apply bad bool: {}", net::apply(client, std::vector<std::uint8_t>{0x80, 0x00, 0x02}));

    Player copy;
    copy.local_frame = 5;
    net::restore(copy, client);
    show(copy);
    std::println("apply d4: {}", net::apply(client, d4));
    net::restore(copy, client);
    std::println("heading signbit {}, y is nan {}", std::signbit(copy.heading), std::isnan(copy.y));

    p.name = std::string(200, 'z');
    std::vector<std::uint8_t> d5 = net::encode(s3, net::snapshot(p));
    dump("long name", d5);
    bool applied = net::apply(client, d5);
    std::println("apply long name: {}, size {}", applied, client.name.size());

    Door door;
    door.open(-90);
    door.cache_ = 3;
    auto ds = net::snapshot(door);
    dump("door", net::encode(ds));
    Door other;
    net::restore(other, ds);
    other.print();
}
