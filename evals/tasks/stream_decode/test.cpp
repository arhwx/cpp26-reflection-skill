#include "solution.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class Kind : std::uint8_t { text, image };
struct Size {
    std::uint16_t width;
    std::uint16_t height;
    bool operator==(const Size&) const = default;
};
struct Attachment {
    Kind kind;
    std::string name;
    std::uint32_t bytes;
    std::optional<Size> preview;
    bool operator==(const Attachment&) const = default;
};
struct Message {
    std::uint16_t id;
    std::string from;
    std::int16_t zone;
    std::optional<std::string> subject;
    std::vector<Attachment> attachments;
    bool urgent;
    bool operator==(const Message&) const = default;
};

void dump(std::string_view label, const std::string& bytes) {
    std::string hex;
    for (char c : bytes) hex += std::format("{}{:02x}", hex.empty() ? "" : " ", static_cast<unsigned char>(c));
    std::println("{} ({} bytes): {}", label, bytes.size(), hex);
}

// feeds bytes in chunks of the given size; chunk 0 means all bytes before the decoder starts
void session(std::string_view name, const std::string& bytes, std::size_t chunk, const std::vector<Message>& sent) {
    std::println("{}:", name);
    wire::source in;
    if (chunk == 0) in.feed(bytes);
    std::vector<Message> got;
    auto decoder = wire::receive<Message>(in, [&](Message m) {
        std::println("  message {} from '{}' is complete after {} bytes, with {} attachments", m.id, m.from, in.fed,
                     m.attachments.size());
        got.push_back(std::move(m));
    });
    if (chunk != 0)
        for (std::size_t at = 0; at < bytes.size(); at += chunk) in.feed(std::string_view(bytes).substr(at, chunk));
    std::println("  fed {} bytes, pauses: {}, round trip {}", in.fed, in.pauses, got == sent ? "ok" : "broken");
}

int main() {
    std::vector<Message> sent{
        {7, "ada", -60, "notes", {{Kind::text, "g.txt", 1200, std::nullopt}, {Kind::image, "engine.png", 70000, Size{640, 480}}}, true},
        {8, "charles", 0, std::nullopt, {}, false},
        {9, "", 330, "", {}, true},
    };

    std::string one;
    wire::write(one, sent[0].attachments[1]);
    dump("attachment", one);
    std::string last;
    wire::write(last, sent[2]);
    dump("message 9", last);

    std::string bytes;
    for (const Message& m : sent) wire::write(bytes, m);
    session("one byte at a time", bytes, 1, sent);
    session("seven bytes at a time", bytes, 7, sent);
    session("all at once", bytes, 0, sent);
}
