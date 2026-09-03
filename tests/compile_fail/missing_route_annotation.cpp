#include <flash/meta/reflection.hpp>

constexpr int ordinary_function() {
    return 42;
}

constexpr auto must_fail = flash::meta::route_of<^^ordinary_function>();
