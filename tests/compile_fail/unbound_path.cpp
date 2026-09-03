#include <flash/binding/meta.hpp>

namespace unbound_path_api {

[[=flash::get("/users/{id}")]] int user(int different_name);

} // namespace unbound_path_api

constexpr auto function = flash::meta::endpoint_at<^^unbound_path_api, 0>();

consteval bool trigger_validation() {
    flash::binding::validate_endpoint_binding<function>();
    return true;
}

static_assert(trigger_validation());

