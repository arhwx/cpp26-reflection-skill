#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace net {

struct replicated_t {};
inline constexpr replicated_t replicated{};

namespace detail {

// replicated members may be private, so the source type is read with unchecked access
constexpr auto all = std::meta::access_context::unchecked();
constexpr auto pub = std::meta::access_context::unprivileged();

consteval bool is_replicated(std::meta::info m) {
    for (std::meta::info a : std::meta::annotations_of(m))
        // class-type annotations are const-qualified
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^replicated_t)
            return true;
    return false;
}

consteval std::vector<std::meta::info> replicated_members(std::meta::info type) {
    std::vector<std::meta::info> out;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, all))
        if (is_replicated(m))
            out.push_back(m);
    return out;
}

template <class T>
struct snapshot_builder {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::meta::info m : replicated_members(^^T))
            specs.push_back(std::meta::data_member_spec(std::meta::type_of(m), {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

consteval std::meta::info member_named(std::meta::info type, std::string_view name) {
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, pub))
        if (std::meta::identifier_of(m) == name)
            return m;
    return {};
}

template <class U>
void put_uint(std::vector<std::uint8_t>& out, U value) {
    for (std::size_t i = 0; i < sizeof(U); ++i)
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

template <class M>
void put(std::vector<std::uint8_t>& out, const M& value) {
    if constexpr (std::is_same_v<M, bool>) {
        out.push_back(value ? 1 : 0);
    } else if constexpr (std::is_enum_v<M>) {
        put(out, static_cast<std::underlying_type_t<M>>(value));
    } else if constexpr (std::is_integral_v<M>) {
        put_uint(out, static_cast<std::make_unsigned_t<M>>(value));
    } else if constexpr (std::is_same_v<M, float>) {
        put_uint(out, std::bit_cast<std::uint32_t>(value));
    } else if constexpr (std::is_same_v<M, double>) {
        put_uint(out, std::bit_cast<std::uint64_t>(value));
    } else {
        static_assert(std::is_same_v<M, std::string>, "unsupported replicated member type");
        std::size_t n = value.size();
        do {
            std::uint8_t byte = n & 0x7f;
            n >>= 7;
            out.push_back(n ? byte | 0x80 : byte);
        } while (n);
        out.insert(out.end(), value.begin(), value.end());
    }
}

class reader {
public:
    explicit reader(std::span<const std::uint8_t> data) : data_(data) {}

    bool done() const { return pos_ == data_.size(); }

    bool byte(std::uint8_t& b) {
        if (pos_ == data_.size())
            return false;
        b = data_[pos_++];
        return true;
    }

    template <class U>
    bool uint(U& value) {
        value = 0;
        for (std::size_t i = 0; i < sizeof(U); ++i) {
            std::uint8_t b;
            if (!byte(b))
                return false;
            value |= static_cast<U>(static_cast<U>(b) << (8 * i));
        }
        return true;
    }

    template <class M>
    bool get(M& value) {
        if constexpr (std::is_same_v<M, bool>) {
            std::uint8_t b;
            if (!byte(b) || b > 1)
                return false;
            value = b == 1;
            return true;
        } else if constexpr (std::is_enum_v<M>) {
            std::underlying_type_t<M> raw;
            if (!get(raw))
                return false;
            value = static_cast<M>(raw);
            return true;
        } else if constexpr (std::is_integral_v<M>) {
            std::make_unsigned_t<M> raw;
            if (!uint(raw))
                return false;
            value = static_cast<M>(raw);
            return true;
        } else if constexpr (std::is_same_v<M, float> || std::is_same_v<M, double>) {
            std::conditional_t<sizeof(M) == 4, std::uint32_t, std::uint64_t> raw;
            if (!uint(raw))
                return false;
            value = std::bit_cast<M>(raw);
            return true;
        } else {
            std::size_t n = 0;
            for (int shift = 0;; shift += 7) {
                std::uint8_t b;
                if (shift > 56 || !byte(b))
                    return false;
                n |= static_cast<std::size_t>(b & 0x7f) << shift;
                if (!(b & 0x80))
                    break;
            }
            if (data_.size() - pos_ < n)
                return false;
            value.assign(data_.begin() + pos_, data_.begin() + pos_ + n);
            pos_ += n;
            return true;
        }
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

template <class S>
constexpr std::size_t field_count = std::meta::nonstatic_data_members_of(^^S, pub).size();

template <class S>
std::vector<std::uint8_t> encode_fields(const S* prev, const S& cur) {
    constexpr std::size_t n = field_count<S>;
    std::vector<std::uint8_t> out((n + 7) / 8, 0);
    bool any = false;
    std::size_t i = 0;
    template for (constexpr std::meta::info m : std::define_static_array(std::meta::nonstatic_data_members_of(^^S, pub))) {
        std::vector<std::uint8_t> now;
        put(now, cur.[:m:]);
        bool changed = true;
        if (prev) {
            std::vector<std::uint8_t> before;
            put(before, prev->[:m:]);
            changed = before != now;
        }
        if (changed) {
            any = true;
            out[i / 8] |= static_cast<std::uint8_t>(1u << (i % 8));
            out.insert(out.end(), now.begin(), now.end());
        }
        ++i;
    }
    if (!any)
        out.clear();
    return out;
}

}  // namespace detail

// one generated type per T, however often snapshot_t<T> is named
template <class T>
using snapshot_t = typename detail::snapshot_builder<T>::type;

template <class T>
snapshot_t<T> snapshot(const T& obj) {
    snapshot_t<T> out{};
    template for (constexpr std::meta::info m : std::define_static_array(detail::replicated_members(^^T))) {
        constexpr std::meta::info target = detail::member_named(^^snapshot_t<T>, std::meta::identifier_of(m));
        out.[:target:] = obj.[:m:];
    }
    return out;
}

template <class T>
void restore(T& obj, const snapshot_t<T>& snap) {
    template for (constexpr std::meta::info m : std::define_static_array(detail::replicated_members(^^T))) {
        constexpr std::meta::info source = detail::member_named(^^snapshot_t<T>, std::meta::identifier_of(m));
        obj.[:m:] = snap.[:source:];
    }
}

template <class S>
std::vector<std::uint8_t> encode(const S& cur) {
    return detail::encode_fields<S>(nullptr, cur);
}

template <class S>
std::vector<std::uint8_t> encode(const S& prev, const S& cur) {
    return detail::encode_fields<S>(&prev, cur);
}

template <class S>
bool apply(S& base, std::span<const std::uint8_t> delta) {
    if (delta.empty())
        return true;
    constexpr std::size_t n = detail::field_count<S>;
    constexpr std::size_t mask_size = (n + 7) / 8;
    if (delta.size() < mask_size)
        return false;
    for (std::size_t bit = n; bit < 8 * mask_size; ++bit)
        if (delta[bit / 8] & (1u << (bit % 8)))
            return false;
    S next = base;
    detail::reader in(delta.subspan(mask_size));
    std::size_t i = 0;
    bool ok = true;
    template for (constexpr std::meta::info m : std::define_static_array(std::meta::nonstatic_data_members_of(^^S, detail::pub))) {
        if (ok && (delta[i / 8] & (1u << (i % 8))))
            ok = in.get(next.[:m:]);
        ++i;
    }
    if (!ok || !in.done())
        return false;
    base = std::move(next);
    return true;
}

}  // namespace net
