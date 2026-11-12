#pragma once

#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lock {

// annotation naming the mutex member that guards a member
struct guarded_by {
    std::meta::info mutex;
};

namespace detail {

// guarded members may be private, so every query sees all members
constexpr auto ctx = std::meta::access_context::unchecked();

template <class U>
constexpr bool lockable_v = requires(U& u) {
    u.lock();
    u.unlock();
};

// the guard reflections written on a member, in annotation order
consteval std::vector<std::meta::info> guards_of(std::meta::info m) {
    std::vector<std::meta::info> out;
    for (std::meta::info a : std::meta::annotations_of(m))
        // class-type annotations have type const guarded_by
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^guarded_by)
            out.push_back(std::meta::extract<guarded_by>(a).mutex);
    return out;
}

consteval bool is_member_of(std::meta::info type, std::meta::info r) {
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (m == r)
            return true;
    return false;
}

// the first problem with the guard annotations of type, or an empty string
consteval std::string problem(std::meta::info type) {
    std::string name(std::meta::identifier_of(type));
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx)) {
        std::vector<std::meta::info> guards = guards_of(m);
        if (guards.empty())
            continue;
        std::string where = name + "::" + std::string(std::meta::identifier_of(m)) + ": ";
        if (guards.size() > 1)
            return where + "guarded more than once";
        if (!is_member_of(type, guards[0]))
            return where + "guard is not a data member of " + name;
        std::meta::info guard_type = std::meta::type_of(guards[0]);
        if (!std::meta::extract<bool>(std::meta::substitute(^^lockable_v, {guard_type})))
            return where + "guard cannot be locked";
    }
    return "";
}

// does member m belong in a view that selects mutex (null selects every guard)
consteval bool selected(std::meta::info m, std::meta::info mutex) {
    std::vector<std::meta::info> guards = guards_of(m);
    return !guards.empty() && (mutex == std::meta::info{} || guards[0] == mutex);
}

consteval std::vector<std::meta::info> exposed(std::meta::info type, std::meta::info mutex) {
    std::vector<std::meta::info> out;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (selected(m, mutex))
            out.push_back(m);
    return out;
}

// the mutexes to lock, in their declaration order, each once
consteval std::vector<std::meta::info> mutexes(std::meta::info type, std::meta::info mutex) {
    std::vector<std::meta::info> used;
    for (std::meta::info m : exposed(type, mutex))
        used.push_back(guards_of(m)[0]);
    std::vector<std::meta::info> out;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        for (std::meta::info u : used)
            if (u == m) {
                out.push_back(m);
                break;
            }
    return out;
}

consteval bool guards_something(std::meta::info type, std::meta::info mutex) {
    return is_member_of(type, mutex) && !exposed(type, mutex).empty();
}

// one reference member per exposed member, with the same name
template <class T, std::meta::info Mutex>
struct access_builder {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::meta::info m : exposed(^^T, Mutex))
            specs.push_back(std::meta::data_member_spec(std::meta::add_lvalue_reference(std::meta::type_of(m)),
                                                        {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

}  // namespace detail

template <class T, std::meta::info Mutex>
class view {
public:
    using access = typename detail::access_builder<T, Mutex>::type;

    explicit view(T& obj) : obj_(obj), access_(make(obj, std::make_index_sequence<members.size()>{})) {
        template for (constexpr std::meta::info m : locks)
            obj_.[:m:].lock();
    }

    ~view() {
        // unlock in the reverse of the locking order
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((obj_.[:locks[locks.size() - 1 - I]:].unlock()), ...);
        }(std::make_index_sequence<locks.size()>{});
    }

    view(const view&) = delete;
    view& operator=(const view&) = delete;

    access* operator->() { return &access_; }
    access& operator*() { return access_; }

private:
    static constexpr auto members = std::define_static_array(detail::exposed(^^T, Mutex));
    static constexpr auto locks = std::define_static_array(detail::mutexes(^^T, Mutex));

    template <std::size_t... I>
    static access make(T& obj, std::index_sequence<I...>) {
        return access{obj.[:members[I]:]...};
    }

    T& obj_;
    access access_;
};

// "ok" when every guard annotation of T is valid, else the first problem
template <class T>
consteval std::string_view check() {
    std::string p = detail::problem(^^T);
    return std::define_static_string(p.empty() ? std::string("ok") : p);
}

template <class T>
    requires(detail::problem(^^T).empty())
view<T, std::meta::info{}> locked(T& obj) {
    return view<T, std::meta::info{}>(obj);
}

template <std::meta::info Mutex, class T>
    requires(detail::problem(^^T).empty() && detail::guards_something(^^T, Mutex))
view<T, Mutex> locked(T& obj) {
    return view<T, Mutex>(obj);
}

}  // namespace lock
