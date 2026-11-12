#pragma once

#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <meta>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace wire {
namespace detail {
using std::meta::info;

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}
consteval bool is_a(info type, info tmpl) {
    type = std::meta::dealias(type);
    return std::meta::has_template_arguments(type) && std::meta::template_of(type) == tmpl;
}
template <class T>
constexpr bool is_number = std::is_integral_v<T> || std::is_enum_v<T>;
template <class T>
constexpr bool is_text = std::is_same_v<T, std::string>;
template <class T>
constexpr bool is_optional = is_a(^^T, ^^std::optional);
template <class T>
constexpr bool is_list = !is_text<T> && is_a(^^T, ^^std::vector);
template <class T>
constexpr auto indices = std::define_static_array(std::views::iota(0uz, fields(^^T).size()));

template <class T>
struct task {  // a lazy coroutine that hands its result to whoever awaits it
    struct promise_type {
        std::optional<T> result;
        std::coroutine_handle<> parent = std::noop_coroutine();
        task get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            struct back_to_parent {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    return h.promise().parent;
                }
                void await_resume() noexcept {}
            };
            return back_to_parent{};
        }
        void return_value(T v) { result = std::move(v); }
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
    explicit task(std::coroutine_handle<promise_type> h) : handle(h) {}
    task(task&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    ~task() {
        if (handle) handle.destroy();
    }
    bool await_ready() { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) {
        handle.promise().parent = parent;
        return handle;
    }
    T await_resume() { return std::move(*handle.promise().result); }
};
}  // namespace detail

// numbers are little endian, text and lists carry a one byte length, structs are their members
template <class T>
void write(std::string& out, const T& v) {
    if constexpr (detail::is_number<T>) {
        for (std::size_t i = 0; i < sizeof(T); ++i) out += static_cast<char>(static_cast<std::uint64_t>(v) >> (8 * i));
    } else if constexpr (detail::is_text<T>) {
        out += static_cast<char>(v.size());
        out += v;
    } else if constexpr (detail::is_optional<T>) {
        write(out, v.has_value());
        if (v) write(out, *v);
    } else if constexpr (detail::is_list<T>) {
        out += static_cast<char>(v.size());
        for (const auto& item : v) write(out, item);
    } else {
        template for (constexpr std::size_t i : detail::indices<T>) write(out, v.[:detail::fields(^^T)[i]:]);
    }
}

class source {  // bytes arrive in pieces; the reader waits here until it has a whole piece
public:
    std::size_t fed = 0;
    std::size_t pauses = 0;

    void feed(std::string_view chunk) {
        buffer_ += chunk;
        fed += chunk.size();
        if (reader_ && buffer_.size() >= wanted_) std::exchange(reader_, {}).resume();
    }

    auto bytes(std::size_t n) {
        struct awaiter {
            source& in;
            std::size_t n;
            bool await_ready() { return in.buffer_.size() >= n; }
            void await_suspend(std::coroutine_handle<> h) {
                in.reader_ = h;
                in.wanted_ = n;
                ++in.pauses;
            }
            std::string await_resume() {
                std::string piece = in.buffer_.substr(0, n);
                in.buffer_.erase(0, n);
                return piece;
            }
        };
        return awaiter{*this, n};
    }

private:
    std::string buffer_;
    std::size_t wanted_ = 0;
    std::coroutine_handle<> reader_;
};

// the decoder mirrors write. every co_await is a point where it may stop for lack of bytes
template <class T>
detail::task<T> read(source& in) {
    if constexpr (detail::is_number<T>) {
        std::string raw = co_await in.bytes(sizeof(T));
        std::uint64_t bits = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bits |= static_cast<std::uint64_t>(static_cast<unsigned char>(raw[i])) << (8 * i);
        co_return static_cast<T>(bits);
    } else if constexpr (detail::is_text<T>) {
        std::size_t size = co_await read<std::uint8_t>(in);
        co_return co_await in.bytes(size);
    } else if constexpr (detail::is_optional<T>) {
        if (co_await read<bool>(in)) co_return co_await read<typename T::value_type>(in);
        co_return std::nullopt;
    } else if constexpr (detail::is_list<T>) {
        T out;
        for (std::size_t left = co_await read<std::uint8_t>(in); left > 0; --left)
            out.push_back(co_await read<typename T::value_type>(in));
        co_return out;
    } else {
        T out{};
        template for (constexpr std::size_t i : detail::indices<T>) {
            static constexpr std::meta::info m = detail::fields(^^T)[i];  // static: a reflection cannot live in a coroutine frame
            out.[:m:] = co_await read<typename [:std::meta::type_of(m):]>(in);
        }
        co_return out;
    }
}

namespace detail {
template <class T, class F>
task<bool> receive_all(source& in, F on_message) {  // decodes one T after another, forever
    for (;;) on_message(co_await read<T>(in));
}
}  // namespace detail

// keeps the decoder alive; destroying it stops decoding
template <class T, class F>
class session {
public:
    session(source& in, F on_message) : job_(detail::receive_all<T>(in, std::move(on_message))) { job_.handle.resume(); }

private:
    detail::task<bool> job_;
};

template <class T, class F>
session<T, F> receive(source& in, F on_message) {
    return session<T, F>(in, std::move(on_message));
}
}  // namespace wire
