#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace pb {
using std::meta::info;

// annotations
struct field {
    int number;
};
struct zigzag_t {};
inline constexpr zigzag_t zigzag{};

namespace detail {

// own aliases: the standard names may be using-declarations, which cannot be reflected
using i32 = std::int32_t;
using i64 = std::int64_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

template <class T>
struct is_vector : std::false_type {};
template <class T>
struct is_vector<std::vector<T>> : std::true_type {};
template <class T>
struct is_optional : std::false_type {};
template <class T>
struct is_optional<std::optional<T>> : std::true_type {};

template <class T>
constexpr bool is_varint = std::is_same_v<T, bool> || std::is_integral_v<T> || std::is_enum_v<T>;
template <class T>
constexpr bool is_packable = is_varint<T> || std::is_floating_point_v<T>;

template <class T>
constexpr int wire_of = is_varint<T> ? 0 : std::is_same_v<T, double> ? 1 : std::is_same_v<T, float> ? 5 : 2;

consteval bool has_zigzag(info member) {
    for (info a : std::meta::annotations_of(member))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^zigzag_t) return true;
    return false;
}

consteval std::optional<int> number_of(info member) {
    for (info a : std::meta::annotations_of(member))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^field) return std::meta::extract<field>(a).number;
    return std::nullopt;
}

// the members that have a field number, in declaration order
consteval std::vector<info> fields(info type) {
    std::vector<info> out;
    for (info m : std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged()))
        if (number_of(m)) out.push_back(m);
    return out;
}

consteval bool is_template(info t, info tmpl) {
    t = std::meta::dealias(t);
    return std::meta::has_template_arguments(t) && std::meta::template_of(t) == tmpl;
}

consteval info element_of(info t) { return std::meta::dealias(std::meta::template_arguments_of(std::meta::dealias(t))[0]); }

// proto scalar name, or empty when the type is not a scalar
consteval std::string_view scalar_name(info t, bool zz) {
    t = std::meta::dealias(t);
    if (t == ^^bool) return "bool";
    if (std::meta::is_same_type(t, ^^i32)) return zz ? "sint32" : "int32";
    if (std::meta::is_same_type(t, ^^i64)) return zz ? "sint64" : "int64";
    if (std::meta::is_same_type(t, ^^u32)) return "uint32";
    if (std::meta::is_same_type(t, ^^u64)) return "uint64";
    if (t == ^^float) return "float";
    if (t == ^^double) return "double";
    if (std::meta::is_same_type(t, ^^std::string)) return "string";
    return "";
}

consteval bool is_message(info t) {
    t = std::meta::dealias(t);
    return std::meta::is_class_type(t) && scalar_name(t, false).empty() && !is_template(t, ^^std::vector) &&
           !is_template(t, ^^std::optional);
}

// a type that may appear directly, or as the element of a vector or optional
consteval bool is_single(info t) {
    return !scalar_name(t, false).empty() || std::meta::is_enum_type(std::meta::dealias(t)) || is_message(t);
}

consteval std::string decimal(long long n) {
    if (n < 0) return "-" + decimal(-n);
    return (n > 9 ? decimal(n / 10) : "") + char('0' + n % 10);
}

}  // namespace detail

consteval std::string_view check(info type) {
    std::vector<info> seen;
    for (info m : detail::fields(type)) {
        std::string at = std::string(std::meta::identifier_of(type)) + "." + std::string(std::meta::identifier_of(m)) + ": ";
        int n = *detail::number_of(m);
        if (n < 1 || n > 536870911)
            return std::define_static_string(at + "field number " + detail::decimal(n) + " is out of range");
        for (info s : seen)
            if (*detail::number_of(s) == n)
                return std::define_static_string(at + "field number " + detail::decimal(n) + " is already used by " +
                                                  std::string(std::meta::identifier_of(s)));
        seen.push_back(m);
        info t = std::meta::type_of(m);
        if (detail::is_template(t, ^^std::vector) || detail::is_template(t, ^^std::optional)) t = detail::element_of(t);
        if (!detail::is_single(t)) return std::define_static_string(at + "unsupported type");
        if (detail::has_zigzag(m) && !std::meta::is_same_type(t, ^^detail::i32) && !std::meta::is_same_type(t, ^^detail::i64))
            return std::define_static_string(at + "zigzag needs a signed integer");
    }
    return "ok";
}

namespace detail {

inline void put_varint(std::string& out, std::uint64_t v) {
    while (v >= 0x80) {
        out += static_cast<char>((v & 0x7f) | 0x80);
        v >>= 7;
    }
    out += static_cast<char>(v);
}

inline void put_fixed(std::string& out, std::uint64_t v, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out += static_cast<char>((v >> (8 * i)) & 0xff);
}

inline void put_tag(std::string& out, int number, int wire) {
    put_varint(out, (static_cast<std::uint64_t>(number) << 3) | static_cast<std::uint64_t>(wire));
}

template <class T>
void encode_to(std::string& out, const T& msg);

// the value without its tag
template <class T, bool ZZ>
void put_payload(std::string& out, const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        put_varint(out, v ? 1 : 0);
    } else if constexpr (std::is_enum_v<T>) {
        put_varint(out, static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T> && ZZ) {
        using U = std::make_unsigned_t<T>;
        put_varint(out, static_cast<U>(static_cast<U>(v) << 1) ^ static_cast<U>(v >> (sizeof(T) * 8 - 1)));
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
        put_varint(out, static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
    } else if constexpr (std::is_integral_v<T>) {
        put_varint(out, v);
    } else if constexpr (std::is_same_v<T, double>) {
        put_fixed(out, std::bit_cast<std::uint64_t>(v), 8);
    } else if constexpr (std::is_same_v<T, float>) {
        put_fixed(out, std::bit_cast<std::uint32_t>(v), 4);
    } else if constexpr (std::is_same_v<T, std::string>) {
        put_varint(out, v.size());
        out += v;
    } else {
        std::string inner;
        encode_to(inner, v);
        put_varint(out, inner.size());
        out += inner;
    }
}

template <class T>
bool is_zero(const T& v) {
    if constexpr (std::is_same_v<T, std::string>)
        return v.empty();
    else if constexpr (std::is_enum_v<T>)
        return static_cast<std::int64_t>(v) == 0;
    else
        return v == T{};
}

template <class T>
void encode_to(std::string& out, const T& msg) {
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        constexpr int n = *number_of(m);
        constexpr bool zz = has_zigzag(m);
        using M = typename [:std::meta::type_of(m):];
        const M& v = msg.[:m:];
        if constexpr (is_vector<M>::value) {
            using E = typename M::value_type;
            if constexpr (is_packable<E>) {
                if (!v.empty()) {
                    std::string packed;
                    for (const E& e : v) put_payload<E, zz>(packed, e);
                    put_tag(out, n, 2);
                    put_varint(out, packed.size());
                    out += packed;
                }
            } else {
                for (const E& e : v) {
                    put_tag(out, n, 2);
                    put_payload<E, zz>(out, e);
                }
            }
        } else if constexpr (is_optional<M>::value) {
            using E = typename M::value_type;
            if (v) {
                put_tag(out, n, wire_of<E>);
                put_payload<E, zz>(out, *v);
            }
        } else if constexpr (std::is_class_v<M> && !std::is_same_v<M, std::string>) {
            put_tag(out, n, 2);
            put_payload<M, zz>(out, v);
        } else if (!is_zero(v)) {
            put_tag(out, n, wire_of<M>);
            put_payload<M, zz>(out, v);
        }
    }
}

struct reader {
    std::string_view s;
    std::size_t pos = 0;

    bool done() const { return pos == s.size(); }
    bool varint(std::uint64_t& v) {
        v = 0;
        for (int i = 0; i < 10; ++i) {
            if (pos == s.size()) return false;
            auto byte = static_cast<unsigned char>(s[pos++]);
            v |= static_cast<std::uint64_t>(byte & 0x7f) << (7 * i);
            if (!(byte & 0x80)) return true;
        }
        return false;
    }
    bool fixed(std::uint64_t& v, int bytes) {
        if (s.size() - pos < static_cast<std::size_t>(bytes)) return false;
        v = 0;
        for (int i = 0; i < bytes; ++i)
            v |= static_cast<std::uint64_t>(static_cast<unsigned char>(s[pos++])) << (8 * i);
        return true;
    }
    bool chunk(std::string_view& out) {
        std::uint64_t len;
        if (!varint(len) || len > s.size() - pos) return false;
        out = s.substr(pos, len);
        pos += len;
        return true;
    }
    bool skip(int wire) {
        std::uint64_t v;
        std::string_view c;
        switch (wire) {
        case 0: return varint(v);
        case 1: return fixed(v, 8);
        case 2: return chunk(c);
        case 5: return fixed(v, 4);
        default: return false;
        }
    }
};

template <class T>
bool decode_into(T& msg, std::string_view bytes);

// reads one value whose wire type is already known to match
template <class T, bool ZZ>
bool read_payload(T& out, reader& r) {
    if constexpr (is_varint<T>) {
        std::uint64_t v;
        if (!r.varint(v)) return false;
        if constexpr (std::is_same_v<T, bool>) {
            out = v != 0;
        } else if constexpr (std::is_enum_v<T>) {
            out = static_cast<T>(static_cast<std::int32_t>(static_cast<std::uint32_t>(v)));
        } else if constexpr (std::is_signed_v<T> && ZZ) {
            using U = std::make_unsigned_t<T>;
            U u = static_cast<U>(v);
            out = static_cast<T>((u >> 1) ^ (U{0} - (u & 1)));
        } else {
            out = static_cast<T>(static_cast<std::make_unsigned_t<T>>(v));
        }
        return true;
    } else if constexpr (std::is_same_v<T, double>) {
        std::uint64_t v;
        if (!r.fixed(v, 8)) return false;
        out = std::bit_cast<double>(v);
        return true;
    } else if constexpr (std::is_same_v<T, float>) {
        std::uint64_t v;
        if (!r.fixed(v, 4)) return false;
        out = std::bit_cast<float>(static_cast<std::uint32_t>(v));
        return true;
    } else {
        std::string_view c;
        if (!r.chunk(c)) return false;
        if constexpr (std::is_same_v<T, std::string>) {
            out = std::string(c);
            return true;
        } else {
            return decode_into(out, c);
        }
    }
}

template <class M, bool ZZ>
bool read_field(M& member, int wire, reader& r) {
    if constexpr (is_vector<M>::value) {
        using E = typename M::value_type;
        if constexpr (is_packable<E>) {
            if (wire == 2) {
                std::string_view c;
                if (!r.chunk(c)) return false;
                reader inner{c};
                while (!inner.done()) {
                    E e{};
                    if (!read_payload<E, ZZ>(e, inner)) return false;
                    member.push_back(e);
                }
                return true;
            }
        }
        if (wire != wire_of<E>) return false;
        E e{};
        if (!read_payload<E, ZZ>(e, r)) return false;
        member.push_back(std::move(e));
        return true;
    } else if constexpr (is_optional<M>::value) {
        using E = typename M::value_type;
        if (wire != wire_of<E>) return false;
        E e = member ? *member : E{};
        if (!read_payload<E, ZZ>(e, r)) return false;
        member = std::move(e);
        return true;
    } else {
        if (wire != wire_of<M>) return false;
        return read_payload<M, ZZ>(member, r);
    }
}

template <class T>
bool decode_into(T& msg, std::string_view bytes) {
    reader r{bytes};
    while (!r.done()) {
        std::uint64_t tag;
        if (!r.varint(tag)) return false;
        std::uint64_t number = tag >> 3;
        int wire = static_cast<int>(tag & 7);
        if (number == 0 || wire == 3 || wire == 4 || wire > 5) return false;
        bool known = false, good = true;
        template for (constexpr info m : std::define_static_array(fields(^^T))) {
            if (number == static_cast<std::uint64_t>(*number_of(m))) {
                known = true;
                good = read_field<typename [:std::meta::type_of(m):], has_zigzag(m)>(msg.[:m:], wire, r);
            }
        }
        if (!good) return false;
        if (!known && !r.skip(wire)) return false;
    }
    return true;
}

// definitions of the types used by T that are not written yet, then T
template <class T>
consteval void define(std::string& out, std::vector<info>& done) {
    for (info d : done)
        if (d == ^^T) return;
    done.push_back(^^T);
    if constexpr (std::is_enum_v<T>) {
        out += "\nenum " + std::string(std::meta::identifier_of(^^T)) + " {\n";
        template for (constexpr info e : std::define_static_array(std::meta::enumerators_of(^^T)))
            out += "  " + std::string(std::meta::identifier_of(e)) + " = " + decimal(static_cast<long long>([:e:])) + ";\n";
        out += "}\n";
    } else {
        std::string body;
        template for (constexpr info m : std::define_static_array(fields(^^T))) {
            constexpr info type = std::meta::dealias(std::meta::type_of(m));
            constexpr bool repeated = is_template(type, ^^std::vector), opt = is_template(type, ^^std::optional);
            constexpr info single = repeated || opt ? element_of(type) : type;
            std::string name(scalar_name(single, has_zigzag(m)));
            if constexpr (std::meta::is_enum_type(single) || is_message(single)) {
                define<typename [:single:]>(out, done);
                name = std::meta::identifier_of(single);
            }
            body += std::string("  ") + (repeated ? "repeated " : opt ? "optional " : "") + name + " " +
                    std::string(std::meta::identifier_of(m)) + " = " + decimal(*number_of(m)) + ";\n";
        }
        out += "\nmessage " + std::string(std::meta::identifier_of(^^T)) + " {\n" + body + "}\n";
    }
}

template <class T>
consteval std::string_view schema_text() {
    std::string out = "syntax = \"proto3\";\n";
    std::vector<info> done;
    define<T>(out, done);
    return std::define_static_string(out);
}

}  // namespace detail

template <class T>
std::string encode(const T& msg) {
    static_assert(check(^^T) == "ok", "T is not a valid message, see pb::check");
    std::string out;
    detail::encode_to(out, msg);
    return out;
}

template <class T>
std::optional<T> decode(std::string_view bytes) {
    static_assert(check(^^T) == "ok", "T is not a valid message, see pb::check");
    T msg{};
    if (!detail::decode_into(msg, bytes)) return std::nullopt;
    return msg;
}

template <class T>
std::string schema() {
    static_assert(check(^^T) == "ok", "T is not a valid message, see pb::check");
    constexpr std::string_view text = detail::schema_text<T>();
    return std::string(text);
}

}  // namespace pb
