#pragma once

#include <algorithm>
#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace metapipe {
using std::meta::info;

template <std::size_t N>
struct text {
    char data[N]{};
    consteval text(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace detail {

// the functions of std::meta that take one reflection and return a reflection
consteval std::vector<info> type_functions() {
    std::vector<info> out;
    for (info m : members_of(^^std::meta, std::meta::access_context::unprivileged())) {
        if (!is_function(m) || !has_identifier(m)) continue;
        std::vector<info> ps = parameters_of(m);
        if (ps.size() == 1 && is_same_type(return_type_of(m), ^^std::meta::info) &&
            is_same_type(type_of(ps[0]), ^^std::meta::info))
            out.push_back(m);
    }
    return out;
}
inline constexpr auto functions = std::define_static_array(type_functions());

consteval bool known(std::string_view word) {
    for (info f : functions)
        if (identifier_of(f) == word) return true;
    return false;
}

// the function named word applied to x; empty when it rejects x
consteval std::optional<info> apply(std::string_view word, [[maybe_unused]] info x) {
    template for (constexpr info f : functions) {
        if (identifier_of(f) == word) {
            try {
                return [:f:](x);
            } catch (const std::meta::exception&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

consteval std::vector<std::string_view> split(std::string_view s) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == ' ') {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < s.size() && s[j] != ' ') ++j;
        out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

consteval std::string number(std::size_t n) {
    std::string out;
    do out.insert(out.begin(), char('0' + n % 10));
    while (n /= 10);
    return out;
}

// runs the pipeline; the message is empty on success
consteval std::string run(std::string_view pipeline, info& type) {
    std::vector<std::string_view> words = split(pipeline);
    for (std::size_t k = 0; k < words.size(); ++k) {
        std::string w(words[k]);
        if (!known(w)) return "unknown word '" + w + "' at step " + number(k + 1);
        std::optional<info> next = apply(w, type);
        if (!next) return "'" + w + "' failed at step " + number(k + 1);
        type = *next;
    }
    return "";
}

}  // namespace detail

consteval std::string_view check(std::string_view pipeline, info type) {
    return std::define_static_string(detail::run(pipeline, type));
}

consteval info run(std::string_view pipeline, info type) {
    std::string error = detail::run(pipeline, type);
    if (!error.empty()) throw std::meta::exception(error, ^^run);
    return type;
}

template <text P, class T>
using piped = [:run(P.view(), ^^T):];

consteval std::string_view search(std::string_view vocabulary, info from, info to, std::size_t max_steps) {
    std::vector<std::string_view> words = detail::split(vocabulary);
    for (std::string_view w : words)
        if (!detail::known(w)) return std::define_static_string("unknown word '" + std::string(w) + "'");
    if (is_same_type(from, to)) return "";
    // pipelines by length, then in the order of the word positions in the vocabulary
    for (std::size_t len = 1; len <= max_steps && !words.empty(); ++len) {
        std::vector<std::size_t> pick(len, 0);
        while (true) {
            info x = from;
            bool ok = true;
            for (std::size_t k = 0; k < len && ok; ++k) {
                std::optional<info> next = detail::apply(words[pick[k]], x);
                if (next) x = *next;
                else ok = false;
            }
            if (ok && is_same_type(x, to)) {
                std::string out;
                for (std::size_t k = 0; k < len; ++k) out += (k ? " " : "") + std::string(words[pick[k]]);
                return std::define_static_string(out);
            }
            std::size_t k = len;
            while (k > 0 && pick[k - 1] + 1 == words.size()) pick[--k] = 0;
            if (k == 0) break;
            ++pick[k - 1];
        }
    }
    return "no pipeline";
}

}  // namespace metapipe
