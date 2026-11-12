#pragma once

#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace check {

struct range {
    int min;
    int max;
};

}  // namespace check

namespace detail {

// null reflection when the member has no check::range annotation
consteval std::meta::info range_annotation(std::meta::info member) {
    for (std::meta::info a : std::meta::annotations_of(member))
        // annotation types are const-qualified
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^check::range)
            return a;
    return {};
}

}  // namespace detail

template <class T>
std::vector<std::string> validate(const T& obj) {
    std::vector<std::string> errors;
    constexpr auto ctx = std::meta::access_context::unprivileged();
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        constexpr std::meta::info a = detail::range_annotation(m);
        if constexpr (a != std::meta::info{}) {
            constexpr check::range r = std::meta::extract<check::range>(a);
            if (obj.[:m:] < r.min || obj.[:m:] > r.max)
                errors.push_back(std::string(std::meta::identifier_of(m)) + " out of range");
        }
    }
    return errors;
}
