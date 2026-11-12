#include "solution.hpp"

#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct Product {
    std::string name;
    int price = 0;
    int stock = 0;
    [[=track::untracked]] int views = 0;
    std::vector<std::string> tags;
};

struct Counter {
    [[=track::untracked]] long hits = 0;
};

std::string join(const std::vector<std::string_view>& names) {
    std::string out = "[";
    for (std::string_view n : names) out += (out.size() > 1 ? " " : "") + std::string(n);
    return out + "]";
}

template <class T>
concept set_pricee = requires(T& t) { t.template set<"pricee">(1); };
template <class T>
concept set_price_text = requires(T& t) { t.template set<"price">("cheap"); };
template <class T>
concept set_price_double = requires(T& t) { t.template set<"price">(2.5); };
template <class T>
concept set_name_int = requires(T& t) { t.template set<"name">(42); };
template <class T>
concept dirty_views = requires(const T& t) { t.template is_dirty<"views">(); };
template <class T>
concept dirty_stock = requires(const T& t) { t.template is_dirty<"stock">(); };

int main() {
    track::tracked<Product> p(Product{"lamp", 20, 5, 0, {"home"}});
    int notified = 0;
    p.on_change([](std::string_view m) { std::println("  changed: {}", m); });
    p.on_change([&](std::string_view) { ++notified; });

    std::println("start: {} / {}", join(p.changed()), p.summary());
    std::println("set price 25: {}", p.set<"price">(25));
    std::println("set price 25 again: {}", p.set<"price">(25));
    std::println("set name: {}", p.set<"name">("desk lamp"));
    std::println("set views: {}", p.set<"views">(100));
    std::println("set stock 5: {}", p.set<"stock">(5));
    std::println("notified: {}", notified);
    std::println("changed: {}", join(p.changed()));

    auto [name, price, stock, tags] = p.dirty();
    std::println("flags: name={} price={} stock={} tags={}", name, price, stock, tags);
    std::println("dirty().price = {}, is_dirty<stock> = {}", p.dirty().price, p.is_dirty<"stock">());
    std::println("summary: {}", p.summary());
    std::println("price {} views {} name {}", p->price, p.get<"views">(), p.value().name);

    p.revert();
    std::println("after revert: {} {} {} views {}", p->name, p->price, join(p.changed()), p->views);
    std::println("summary: {}", p.summary());

    p.set<"tags">(std::vector<std::string>{"home", "sale"});
    p.set<"price">(18);
    std::println("summary: {}", p.summary());
    p.commit();
    std::println("after commit: {} / price {}", p.summary(), p->price);
    p.set<"price">(20);
    p.set<"price">(18);
    std::println("set back: {}", p.summary());
    p.revert();
    std::println("after revert: price {} tags {}", p->price, p->tags);
    std::println("notified: {}", notified);

    track::tracked<Counter> c(Counter{});
    c.on_change([](std::string_view m) { std::println("  counter changed: {}", m); });
    std::println("set hits: {}", c.set<"hits">(3));
    std::println("counter: hits {} changed {} summary {}", c->hits, join(c.changed()), c.summary());

    using F = track::flags<Product>;
    std::println("flags aggregate: {}, empty for Counter: {}", std::is_aggregate_v<F>,
                 std::is_empty_v<track::flags<Counter>>);
    std::println("flag types: {} {}", std::is_same_v<decltype(F{}.tags), bool>, sizeof(F) == 4 * sizeof(bool));
    std::println("same flags type: {}", std::is_same_v<std::remove_cvref_t<decltype(p.dirty())>, F>);

    using T = track::tracked<Product>;
    std::println("set<pricee>(1): {}", set_pricee<T>);
    std::println("set<price>(\"cheap\"): {}", set_price_text<T>);
    std::println("set<price>(2.5): {}", set_price_double<T>);
    std::println("set<name>(42): {}", set_name_int<T>);
    std::println("is_dirty<views>: {}", dirty_views<T>);
    std::println("is_dirty<stock>: {}", dirty_stock<T>);
}
