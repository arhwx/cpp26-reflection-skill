#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace layout {

namespace detail {

// one listed field after flattening bases and class-type members
struct entry {
    std::string path;
    std::size_t start;  // in bits from the start of the outermost object
    std::size_t bits;
    bool bit_field;
};

consteval bool expands(std::meta::info type) {
    type = std::meta::remove_cv(std::meta::dealias(type));
    return std::meta::is_class_type(type) && !std::meta::is_union_type(type);
}

// layout is about every member, so private ones count too
consteval void flatten(std::meta::info type, std::size_t base_bits, const std::string& prefix,
                       std::vector<entry>& out) {
    constexpr auto ctx = std::meta::access_context::unchecked();
    for (std::meta::info b : std::meta::bases_of(type, ctx)) {
        std::meta::info bt = std::meta::type_of(b);
        flatten(bt, base_bits + std::meta::offset_of(b).bytes * 8,
                prefix + std::string(std::meta::identifier_of(bt)) + "::", out);
    }
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx)) {
        auto off = std::meta::offset_of(m);
        std::size_t start = base_bits + off.bytes * 8 + off.bits;
        std::string path = prefix + std::string(std::meta::identifier_of(m));
        if (std::meta::is_bit_field(m))
            out.push_back({path, start, std::meta::bit_size_of(m), true});
        else if (expands(std::meta::type_of(m)))
            flatten(std::meta::remove_cv(std::meta::dealias(std::meta::type_of(m))), start, path + ".", out);
        else
            out.push_back({path, start, std::meta::size_of(m) * 8, false});
    }
}

consteval std::vector<entry> entries(std::meta::info type) {
    std::vector<entry> out;
    flatten(type, 0, "", out);
    return out;
}

constexpr std::string decimal(std::size_t n) {
    std::string s(1, char('0' + n % 10));
    while (n /= 10) s.insert(s.begin(), char('0' + n % 10));
    return s;
}

consteval std::string make_signature(std::meta::info type) {
    std::string s;
    for (const entry& e : entries(type))
        s += e.path + "@" + decimal(e.start) + ":" + decimal(e.bits) + ";";
    s += "=" + decimal(std::meta::size_of(type)) + "/" + decimal(std::meta::alignment_of(type));
    return s;
}

constexpr std::uint64_t fnv1a(std::string_view text) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (char c : text) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ull;
    }
    return h;
}

inline std::string position(std::size_t bit) {
    return bit % 8 == 0 ? std::format("{}", bit / 8) : std::format("{}.{}", bit / 8, bit % 8);
}

inline std::string extent(std::size_t start, std::size_t bits) {
    return start % 8 == 0 && bits % 8 == 0 ? std::format("size={}", bits / 8) : std::format("bits={}", bits);
}

// plain copy of an entry for run time use
struct row {
    std::string_view path;
    std::size_t start, bits;
    bool bit_field;
};

template <class T>
inline constexpr auto rows = [] {
    constexpr std::size_t n = entries(^^T).size();
    std::array<row, n> out{};
    auto es = entries(^^T);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = {std::define_static_string(es[i].path), es[i].start, es[i].bits, es[i].bit_field};
    return out;
}();

}  // namespace detail

template <class T>
inline constexpr std::string_view signature = std::define_static_string(detail::make_signature(^^T));

template <class T>
inline constexpr std::uint64_t hash = detail::fnv1a(signature<T>);

template <class T>
std::string report() {
    std::string out = std::format("{} size={} align={}\n", std::meta::identifier_of(^^T), sizeof(T), alignof(T));
    std::size_t end = 0, holes = 0, unused = 0;
    for (const detail::row& r : detail::rows<T>) {
        if (r.start > end) {
            out += std::format("  {} hole {}\n", detail::position(end), detail::extent(end, r.start - end));
            ++holes;
            unused += r.start - end;
        }
        out += std::format("  {} {} {}\n", detail::position(r.start), r.path,
                           r.bit_field ? std::format("bits={}", r.bits) : std::format("size={}", r.bits / 8));
        end = r.start + r.bits;
    }
    if (sizeof(T) * 8 > end) {
        out += std::format("  {} padding {}\n", detail::position(end), detail::extent(end, sizeof(T) * 8 - end));
        unused += sizeof(T) * 8 - end;
    }
    out += std::format("  fields={} holes={} unused_bits={}\n", detail::rows<T>.size(), holes, unused);
    return out;
}

}  // namespace layout
