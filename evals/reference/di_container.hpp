#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <meta>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace detail {

// public constructors of type, other than copy and move constructors
consteval std::vector<std::meta::info> injection_constructors(std::meta::info type) {
    std::vector<std::meta::info> found;
    for (std::meta::info m : std::meta::members_of(type, std::meta::access_context::unprivileged()))
        if (std::meta::is_constructor(m) && !std::meta::is_copy_constructor(m) && !std::meta::is_move_constructor(m))
            found.push_back(m);
    return found;
}

template <class T>
inline constexpr auto ctor_params =
    std::define_static_array(std::meta::parameters_of(injection_constructors(^^T).front()));

// a parameter of type U& or const U& asks for the shared U
template <class T, std::size_t I>
using dependency = std::remove_cvref_t<typename [:std::meta::type_of(ctor_params<T>[I]):]>;

}  // namespace detail

class injector {
public:
    injector() = default;
    injector(const injector&) = delete;
    injector& operator=(const injector&) = delete;

    // the last object constructed is destroyed first
    ~injector() {
        while (!owned_.empty())
            owned_.pop_back();
    }

    template <class T>
    T& get() {
        constexpr std::size_t ctors = detail::injection_constructors(^^T).size();
        static_assert(ctors == 1, "T needs exactly one public constructor besides copy and move");

        auto it = objects_.find(typeid(T));
        if (it != objects_.end())
            return *static_cast<T*>(it->second);
        T* object = create<T>(std::make_index_sequence<detail::ctor_params<T>.size()>{});
        owned_.emplace_back(object, [](void* p) { delete static_cast<T*>(p); });
        objects_.emplace(typeid(T), object);
        return *object;
    }

private:
    template <class T, std::size_t... I>
    T* create(std::index_sequence<I...>) {
        // a braced list is evaluated left to right, so dependencies are created in parameter order
        std::tuple<detail::dependency<T, I>&...> deps{get<detail::dependency<T, I>>()...};
        return new T(std::get<I>(deps)...);
    }

    std::vector<std::unique_ptr<void, void (*)(void*)>> owned_;
    std::map<std::type_index, void*> objects_;
};
