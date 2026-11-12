// protobuf: plain structs with field numbers in annotations are protobuf messages. they encode to and decode
// from the protobuf wire format, and the .proto file is written from the same structs, with no protoc step
#include <algorithm>
#include <cstdint>
#include <format>
#include <meta>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace pb {
using std::meta::info;
struct field { int number; };  // the annotation: [[=pb::field(1)]]

consteval std::optional<int> number_of(info member) {
    for (info a : annotations_of(member))
        if (remove_const(type_of(a)) == ^^field) return extract<field>(a).number;
    return std::nullopt;
}
consteval std::vector<info> fields(info type) {  // the members with a field number, in declaration order
    std::vector<info> out;
    for (info m : nonstatic_data_members_of(type, std::meta::access_context::unprivileged()))
        if (number_of(m)) out.push_back(m);
    return out;
}
consteval std::string decimal(int n) { return n < 0 ? "-" + decimal(-n) : (n > 9 ? decimal(n / 10) : "") + char('0' + n % 10); }
consteval std::string_view check(info type) {  // "ok", or the first problem with the field numbers
    for (std::vector<info> seen; info m : fields(type)) {
        int n = *number_of(m);
        std::string at = std::string(identifier_of(type)) + "." + std::string(identifier_of(m)) + ": field number " + decimal(n);
        if (n < 1 || n > 536870911) return std::define_static_string(at + " is out of range");
        for (info s : seen)
            if (*number_of(s) == n) return std::define_static_string(at + " is already used by " + std::string(identifier_of(s)));
        seen.push_back(m);
    }
    return "ok";
}
template <class T>
consteval auto fields_of() {  // a message with a problem stops here, with the problem as the error
    static_assert(check(^^T) == "ok", check(^^T));
    return std::define_static_array(fields(^^T));
}

// encoding. a record is a varint tag, (number << 3) | wire type, then the value
template <class T>  // wire type 0 is a varint, 2 is a length and that many bytes
constexpr int wire = std::is_integral_v<T> ? 0 : 2;

constexpr void put_varint(std::string& out, std::uint64_t v) {  // 7 bits per byte, low bits first
    for (; v >= 0x80; v >>= 7) out += static_cast<char>(v | 0x80);
    out += static_cast<char>(v);
}
constexpr void put_tag(std::string& out, std::uint64_t number, int wire) { put_varint(out, (number << 3) | wire); }
template <class T>
constexpr std::string encode(const T& msg);
template <class T>
constexpr void put(std::string& out, const T& v) {  // a value without its tag
    if constexpr (wire<T> == 0) put_varint(out, v);  // a negative number takes ten bytes
    else if constexpr (std::is_same_v<T, std::string>) put_varint(out, v.size()), out += v;
    else put(out, encode(v));  // a message is a string of its own bytes
}
template <class T>
constexpr void write(std::string& out, int number, const T& v) {
    if constexpr (!std::is_aggregate_v<T>)
        if (v == T{}) return;  // 0, false and "" are left out, a message never is
    put_tag(out, number, wire<T>);
    put(out, v);
}
template <class E>
constexpr void write(std::string& out, int number, const std::vector<E>& v) {
    std::string packed;  // numbers are packed into one record, strings and messages get a record each
    for (const E& e : v)
        if constexpr (wire<E> == 0) put(packed, e);
        else put_tag(out, number, 2), put(out, e);
    if (!packed.empty()) put_tag(out, number, 2), put(out, packed);
}
template <class T>
constexpr std::string encode(const T& msg) {
    std::string out;
    template for (constexpr info m : fields_of<T>()) {
        write(out, *number_of(m), msg.[:m:]);
    }
    return out;
}

// decoding. a read returns false when the bytes run out or are malformed
template <class T>
constexpr bool varint(std::string_view& s, T& out) {  // ten bytes at most; a narrower T keeps the low bits
    std::uint64_t v = 0, b = 0x80;
    for (int shift = 0; b >= 0x80; shift += 7) {  // the high bit of a byte says that another byte follows
        if (shift == 70 || s.empty()) return false;
        b = static_cast<unsigned char>(s[0]);
        s.remove_prefix(1);
        v |= (b & 0x7f) << shift;
    }
    out = static_cast<T>(v);
    return true;
}
template <class S>
constexpr bool chunk(std::string_view& s, S& out, std::uint64_t n = 0) {  // n bytes; with no n, a length and that many bytes
    if ((n == 0 && !varint(s, n)) || n > s.size()) return false;
    out = s.substr(0, n);
    s.remove_prefix(n);
    return true;
}
constexpr bool skip(std::string_view& s, int wt) {  // the value of a field that the message does not have
    std::uint64_t v = 0;
    std::string_view b;
    return wt == 0 ? varint(s, v) : wt == 2 ? chunk(s, b) : (wt == 1 || wt == 5) && chunk(s, b, wt == 1 ? 8 : 4);
}
template <class T>
constexpr bool decode_into(T& msg, std::string_view s);
template <class T>
constexpr bool read(std::string_view& s, int wt, T& out) {  // one value of wire type wt; a message is merged into out
    std::string_view b;
    if (wt != wire<T>) return false;
    if constexpr (wire<T> == 0) return varint(s, out);
    else if constexpr (std::is_same_v<T, std::string>) return chunk(s, out);
    else return chunk(s, b) && decode_into(out, b);
}
template <class E>
constexpr bool read(std::string_view& s, int wt, std::vector<E>& out) {  // one more element, or packed numbers
    std::string_view packed;
    if (wt != 2 || wire<E> == 2) return read(s, wt, out.emplace_back());
    bool ok = chunk(s, packed);
    while (ok && !packed.empty()) ok = read(packed, 0, out.emplace_back());
    return ok;
}
template <class T>
constexpr bool decode_into(T& msg, std::string_view s) {  // the last value wins, vectors append
    while (!s.empty()) {
        std::uint64_t tag = 0;
        if (!varint(s, tag) || tag >> 3 == 0) return false;
        bool known = false, ok = true;
        template for (constexpr info m : fields_of<T>()) {
            if (tag >> 3 == std::uint64_t(*number_of(m))) known = true, ok = read(s, tag & 7, msg.[:m:]);
        }
        if (!ok || (!known && !skip(s, tag & 7))) return false;
    }
    return true;
}
template <class T>
constexpr std::optional<T> decode(std::string_view bytes) {  // nullopt when the bytes are malformed
    if (T msg{}; decode_into(msg, bytes)) return msg;
    return std::nullopt;
}

// the .proto text
consteval bool is_vector(info t) { return has_template_arguments(dealias(t)) && template_of(dealias(t)) == ^^std::vector; }
consteval info element(info t) { return is_vector(t) ? dealias(template_arguments_of(dealias(t))[0]) : dealias(t); }
consteval std::string_view proto_name(info t) {  // int32, uint64, string, ... or the name of a message
    if (is_same_type(t, ^^bool)) return "bool";
    if (is_same_type(t, ^^std::string)) return "string";
    if (!is_integral_type(t)) return identifier_of(t);
    return std::define_static_string(std::string(is_signed_type(t) ? "int" : "uint") + (size_of(t) == 8 ? "64" : "32"));
}
template <class T>
std::string define(std::vector<std::string_view>& done) {  // first the messages that T uses and are not done yet, then T
    if (std::ranges::contains(done, identifier_of(^^T))) return "";
    done.push_back(identifier_of(^^T));
    std::string before, body;
    template for (constexpr info m : fields_of<T>()) {
        constexpr info e = element(type_of(m));
        if constexpr (is_aggregate_type(e)) before += define<typename [:e:]>(done);
        body += std::format("  {}{} {} = {};\n", is_vector(type_of(m)) ? "repeated " : "", proto_name(e), identifier_of(m), *number_of(m));
    }
    return before + std::format("\nmessage {} {{\n{}}}\n", identifier_of(^^T), body);
}
template <class T>
std::string schema() {
    std::vector<std::string_view> done;
    return "syntax = \"proto3\";\n" + define<T>(done);
}
}  // namespace pb

// ---- user code. protobuf: message User { string name = 1; ... }; rust prost: #[prost(string, tag = "1")] name: String
struct Address {
    [[=pb::field(1)]] std::string city;
    [[=pb::field(2)]] std::uint32_t zip;
};

struct User {
    [[=pb::field(1)]] std::string name;
    [[=pb::field(2)]] std::int64_t id;
    [[=pb::field(3)]] bool admin;
    [[=pb::field(4)]] std::vector<std::int32_t> scores;
    [[=pb::field(5)]] std::vector<std::string> tags;
    [[=pb::field(6)]] Address home;
    int cache = 0;  // no field number, so not part of the message
};

// an older version of User: it reads the new bytes and skips the fields it does not know
struct OldUser { [[=pb::field(1)]] std::string name; [[=pb::field(6)]] Address home; };

// the first example of the protobuf encoding guide, encoded and decoded by the compiler
struct Test1 { [[=pb::field(1)]] std::int32_t a; };
static_assert(pb::encode(Test1{150}) == "\x08\x96\x01" && pb::decode<Test1>("\x08\x96\x01")->a == 150);

// a field number used twice: pb::encode(Clash{}) does not compile, and the error is what pb::check says
struct Clash { [[=pb::field(1)]] int a; [[=pb::field(1)]] int b; };

int main() {
    std::print("{}", pb::schema<User>());
    User alice{.name = "alice", .id = 42, .admin = true, .scores = {1, 300}, .tags = {"x", "y"}, .home = {"Oslo", 150}, .cache = 9};
    std::string wire = pb::encode(alice), hex;
    for (char c : wire) hex += std::format(" {:02x}", static_cast<unsigned char>(c));
    std::println("{} bytes:{}", wire.size(), hex);  // the same bytes that protoc writes for this message

    User back = pb::decode<User>(wire).value();
    std::println("decoded: {} {} {} {} {} {}, cache {}", back.name, back.id, back.admin, back.scores, back.tags, back.home.city, back.cache);
    OldUser old = pb::decode<OldUser>(wire).value();
    std::println("old reader: {} from {}", old.name, old.home.city);
    std::println("cut short: {}", pb::decode<User>(wire.substr(0, 5)) ? "ok" : "malformed");
    std::println("{}", pb::check(^^Clash));
}

// output:
// syntax = "proto3";
//
// message Address {
//   string city = 1;
//   uint32 zip = 2;
// }
//
// message User {
//   string name = 1;
//   int64 id = 2;
//   bool admin = 3;
//   repeated int32 scores = 4;
//   repeated string tags = 5;
//   Address home = 6;
// }
// 33 bytes: 0a 05 61 6c 69 63 65 10 2a 18 01 22 03 01 ac 02 2a 01 78 2a 01 79 32 09 0a 04 4f 73 6c 6f 10 96 01
// decoded: alice 42 true [1, 300] ["x", "y"] Oslo, cache 0
// old reader: alice from Oslo
// cut short: malformed
// Clash.b: field number 1 is already used by a
