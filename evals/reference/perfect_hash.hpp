#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <optional>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ph {

namespace detail {

// the keys: enumerator names, or public non-static data member names
consteval std::vector<std::meta::info> keys_of(std::meta::info type) {
    if (std::meta::is_enum_type(type)) return std::meta::enumerators_of(type);
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

template <class K>
inline constexpr auto keys = std::define_static_array(keys_of(^^K));

template <class K>
inline constexpr std::size_t count = keys<K>.size();

template <class K>
inline constexpr auto names = [] {
    std::array<std::string_view, count<K>> out{};
    for (std::size_t i = 0; i < count<K>; ++i) out[i] = std::meta::identifier_of(keys<K>[i]);
    return out;
}();

// fnv-1a with a seed folded into the start value, then a final mix
constexpr std::uint64_t hash(std::uint64_t seed, std::string_view s) {
    std::uint64_t h = 0xcbf29ce484222325ull ^ (seed * 0x9e3779b97f4a7c15ull);
    for (char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ull;
    }
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    return h ^ (h >> 33);
}

// hash and displace: keys go to buckets by one hash, and each bucket gets a seed that sends
// its keys to free slots of a table twice the number of keys
template <std::size_t N>
struct table {
    std::array<std::uint64_t, N> seed{};
    std::array<std::size_t, 2 * N> key{};  // key index per slot, or N when empty
};

template <std::size_t N>
consteval table<N> build(const std::array<std::string_view, N>& names) {
    table<N> t;
    t.key.fill(N);
    std::vector<std::vector<std::size_t>> buckets(N);
    for (std::size_t i = 0; i < N; ++i) buckets[hash(0, names[i]) % N].push_back(i);
    std::vector<std::size_t> order(N);
    for (std::size_t b = 0; b < N; ++b) order[b] = b;
    std::ranges::stable_sort(order, [&](std::size_t a, std::size_t b) { return buckets[a].size() > buckets[b].size(); });
    for (std::size_t b : order) {
        if (buckets[b].empty()) break;
        for (std::uint64_t seed = 1;; ++seed) {
            std::vector<std::size_t> slots;
            bool ok = true;
            for (std::size_t k : buckets[b]) {
                std::size_t s = hash(seed, names[k]) % (2 * N);
                if (t.key[s] != N || std::ranges::find(slots, s) != slots.end()) {
                    ok = false;
                    break;
                }
                slots.push_back(s);
            }
            if (!ok) continue;
            t.seed[b] = seed;
            for (std::size_t j = 0; j < slots.size(); ++j) t.key[slots[j]] = buckets[b][j];
            break;
        }
    }
    return t;
}

template <class K>
inline constexpr table<count<K>> lookup_table = build(names<K>);

// index of the key named s, or count<K> when there is none; one hash and one string comparison
template <class K>
constexpr std::size_t find(std::string_view s) {
    constexpr std::size_t n = count<K>;
    if constexpr (n == 0) {
        return 0;
    } else {
        std::size_t k = lookup_table<K>.key[hash(lookup_table<K>.seed[hash(0, s) % n], s) % (2 * n)];
        return k != n && names<K>[k] == s ? k : n;
    }
}

template <class E>
inline constexpr auto enum_values = [] {
    std::array<E, count<E>> out{};
    for (std::size_t i = 0; i < count<E>; ++i) out[i] = std::meta::extract<E>(keys<E>[i]);
    return out;
}();

}  // namespace detail

template <class K>
inline constexpr std::size_t table_size = 2 * detail::count<K>;

template <class K>
constexpr std::size_t slot_of(std::string_view s) {
    constexpr std::size_t n = detail::count<K>;
    return detail::hash(detail::lookup_table<K>.seed[detail::hash(0, s) % n], s) % (2 * n);
}

template <class E>
    requires std::is_enum_v<E>
constexpr std::optional<E> enum_from_string(std::string_view s) {
    std::size_t k = detail::find<E>(s);
    if (k == detail::count<E>) return std::nullopt;
    return detail::enum_values<E>[k];
}

template <class T, class F>
    requires std::is_class_v<T>
bool visit_field(T& obj, std::string_view name, F&& f) {
    using K = std::remove_cv_t<T>;
    std::size_t k = detail::find<K>(name);
    bool found = false;
    template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, detail::count<K>)))
        if (k == i) {
            std::forward<F>(f)(obj.[:detail::keys<K>[i]:]);
            found = true;
        }
    return found;
}

}  // namespace ph
