#include <flash/binding/meta.hpp>

namespace multiple_sources_api {

[[=flash::get("/search")]]
int search([[=flash::query("q"), =flash::header("X-Query")]] int query);

} // namespace multiple_sources_api

constexpr auto function = flash::meta::endpoint_at<^^multiple_sources_api, 0>();

consteval bool trigger_validation() {
    flash::binding::validate_endpoint_binding<function>();
    return true;
}

static_assert(trigger_validation());

