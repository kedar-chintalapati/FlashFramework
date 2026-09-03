#include <flash/routing/matcher.hpp>

namespace ambiguous_route_api {

[[=flash::get("/users/{id}")]] int by_id(int id);
[[=flash::get("/users/{name}")]] int by_name(const char* name);

} // namespace ambiguous_route_api

consteval bool trigger_validation() {
    flash::routing::validate_routes<^^ambiguous_route_api>();
    return true;
}

static_assert(trigger_validation());

