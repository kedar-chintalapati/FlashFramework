#include <flash/flash.hpp>

[[=flash::get("/duplicate"), =flash::post("/duplicate")]]
constexpr int duplicate_route() {
    return 42;
}

constexpr auto must_fail = flash::meta::route_of<^^duplicate_route>();

