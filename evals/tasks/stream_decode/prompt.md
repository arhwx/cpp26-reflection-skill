I want a binary encoder and a resumable decoder for any struct. The encoder writes a value in one go. The decoder gets bytes in pieces as they arrive from the network, can stop after any byte, and carries on when more bytes come, without me writing a state machine for each message type. The walk over the members must come from the struct definition itself.

Everything goes in `namespace wire`:
- `template <class T> void wire::write(std::string& out, const T& value)`: appends the encoding of `value` to `out`.
- `class wire::source` (or `struct`): default constructible. It has `void feed(std::string_view chunk)`, which adds bytes, and two public data members of type `std::size_t`: `fed`, the total number of bytes passed to `feed` so far, and `pauses` (see below). Both start at 0.
- `template <class T, class F> auto wire::receive(wire::source& in, F on_message)`: starts a decoder that reads one `T` after another from `in`, forever, and calls `on_message` with each `T` (as an rvalue) as soon as its last byte is decoded. It returns an object that keeps the decoder alive. The test keeps that object in a local `auto` variable, never copies or moves it, and destroys it before `in`. Destroying it stops the decoder.

Encoding:
- An integral type (including `bool`) or an enum takes `sizeof(T)` bytes. The value, converted to a 64-bit unsigned integer, is written least significant byte first. So `bool` is one byte, 0 or 1, and a negative `std::int16_t` is its two's complement.
- `std::string`: one byte holding the length, then the characters.
- `std::optional<X>`: a `bool` that tells whether a value is present, then the `X` if it is.
- `std::vector<X>`: one byte holding the number of items, then each item.
- Any other type is an aggregate struct with public members: its members in declaration order, with nothing before, between or after them.
- Strings and vectors are always shorter than 256.

Decoding:
- The decoder reads exactly what `write` wrote, and asks the source for bytes in these pieces: a number is one piece of `sizeof(T)` bytes, a string is a 1-byte piece for the length followed by one piece of `length` bytes, an optional is its `bool` piece followed by the value's pieces if present, a vector is its 1-byte count piece followed by the pieces of each item, and a struct is the pieces of its members in order.
- If the source holds all bytes of the piece, the decoder takes them at once and goes on. A piece of 0 bytes never waits. Otherwise the decoder pauses: `pauses` goes up by 1, and the decoder waits until the source holds the whole piece. It then resumes during the `feed` call that completed the piece, and goes on as far as the bytes it has allow.
- `receive` starts decoding before it returns, so bytes fed before `receive` was called are decoded during the `receive` call.
- After `on_message` returns, the decoder starts the next `T` at once.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
attachment (21 bytes): 01 0a 65 6e 67 69 6e 65 2e 70 6e 67 70 11 01 00 01 80 02 e0 01
message 9 (9 bytes): 09 00 00 4a 01 01 00 00 01
one byte at a time:
  message 7 from 'ada' is complete after 50 bytes, with 2 attachments
  message 8 from 'charles' is complete after 65 bytes, with 0 attachments
  message 9 from '' is complete after 74 bytes, with 0 attachments
  fed 74 bytes, pauses: 36, round trip ok
seven bytes at a time:
  message 7 from 'ada' is complete after 56 bytes, with 2 attachments
  message 8 from 'charles' is complete after 70 bytes, with 0 attachments
  message 9 from '' is complete after 74 bytes, with 0 attachments
  fed 74 bytes, pauses: 12, round trip ok
all at once:
  message 7 from 'ada' is complete after 74 bytes, with 2 attachments
  message 8 from 'charles' is complete after 74 bytes, with 0 attachments
  message 9 from '' is complete after 74 bytes, with 0 attachments
  fed 74 bytes, pauses: 1, round trip ok
```

Write the complete header to `solution.hpp` in the current directory.
