#include "solution.hpp"

#include <cstdint>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

struct Rgb {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct alignas(16) Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct Record {
    char kind;
    double score;
    Rgb color;
    short port;
    std::string name;
    bool active;
    Vec4 pos;
    int count;
    std::int64_t id;
};

struct Sample {
    char unit;
    double temp;
    int id;
    short raw0;
    double pressure;
    char flag;
    int count;
    short raw1;
    double humidity;
    bool valid;
    int seq;
    short raw2;
    double voltage;
    char mode;
    int errors;
    short raw3;
    float gain;
    char zone;
};

struct Empty {};

static_assert(std::is_aggregate_v<compact_t<Record>>);
static_assert(!std::is_same_v<compact_t<Record>, Record>);
static_assert(std::is_same_v<decltype(compact_t<Record>::name), std::string>);
static_assert(std::is_same_v<decltype(compact_t<Record>::color), Rgb>);
static_assert(std::is_same_v<decltype(compact_t<Sample>::gain), float>);
static_assert(compact_names<Record>().size() == 9 && compact_names<Record>()[0] == "pos");
static_assert(compact_names<Empty>().empty());

template <class T>
void print_names(std::string_view label) {
    std::print("{}:", label);
    for (std::string_view name : compact_names<T>())
        std::print(" {}", name);
    std::println();
}

int main() {
    print_names<Record>("Record");
    print_names<Sample>("Sample");
    print_names<Empty>("Empty");
    std::println("not larger: {}", sizeof(compact_t<Record>) <= sizeof(Record));

    Record r{'k', 2.5, {1, 2, 3}, 8080, "node", true, {1, 2, 3, 4}, 42, 7'000'000'000};
    compact_t<Record> c = to_compact(r);
    std::println("{} {} {} {} {} {} {} {} {}", c.pos.w, c.score, c.name, c.id, c.count, c.port, c.kind, c.color.b,
                 c.active);

    // designated initializers must follow the member order of compact_t<Record>
    compact_t<Record> d{.pos = {0.5f, 0, 0, 0}, .score = -1.5, .name = "edge", .id = -9, .count = 3, .port = 22,
                       .kind = 'e', .color = {9, 8, 7}, .active = false};
    Record back = from_compact<Record>(d);
    std::println("{} {} {} {} {} {} {} {} {}", back.kind, back.score, back.color.r, back.port, back.name, back.active,
                 back.pos.x, back.count, back.id);

    Sample s{'C', 21.5, 1, 10, 1013.25, 'f', 5, 11, 40.5, true, 77, 12, 3.25, 'm', 0, 13, 0.75f, 'z'};
    Sample t = from_compact<Sample>(to_compact(s));
    std::println("{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}", t.unit, t.temp, t.id, t.raw0, t.pressure,
                 t.flag, t.count, t.raw1, t.humidity, t.valid, t.seq, t.raw2, t.voltage, t.mode, t.errors, t.raw3,
                 t.gain, t.zone);
}
