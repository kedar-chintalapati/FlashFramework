#include <flash/routing/matcher.hpp>

namespace invalid_route_api {

[[=flash::get("/users/")]] int users();

} // namespace invalid_route_api

constexpr auto must_fail = flash::routing::route_pattern_of<
    flash::meta::endpoint_at<^^invalid_route_api, 0>()>();

