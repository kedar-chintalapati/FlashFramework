#include <flash/routing/matcher.hpp>

#include <string>

namespace route_api {

[[=flash::get("/users/me")]] int current_user();
[[=flash::get("/users/{id}")]] int user(int id);
[[=flash::post("/users/{id}")]] int replace_user(int id);
[[=flash::get("/assets/{*path}")]] int asset(std::string path);
[[=flash::head("/health")]] void health_head();
[[=flash::get("/health")]] void health();
[[=flash::put("/users/{id}")]] int put_user(int id);
[[=flash::patch("/users/{id}")]] int patch_user(int id);
[[=flash::options("/health")]] void health_options();

} // namespace route_api

static_assert(flash::routing::parse_route("/").valid());
static_assert(flash::routing::parse_route("/users/{id}").segment_count == 2);
static_assert(flash::routing::parse_route("/assets/{*path}").segments[1].kind ==
              flash::routing::segment_kind::catch_all);
static_assert(flash::routing::route_shape_hash(
                  flash::routing::parse_route("/users/{id}")) ==
              flash::routing::route_shape_hash(
                  flash::routing::parse_route("/users/{name}")));
static_assert(flash::routing::route_shape_hash(
                  flash::routing::parse_route("/users/me")) !=
              flash::routing::route_shape_hash(
                  flash::routing::parse_route("/users/you")));
static_assert(flash::routing::parse_route("users").error ==
              flash::routing::route_error::missing_leading_slash);
static_assert(flash::routing::parse_route("/users/").error ==
              flash::routing::route_error::trailing_slash);
static_assert(flash::routing::parse_route("/users/{id}/{id}").error ==
              flash::routing::route_error::duplicate_parameter_name);
static_assert(flash::routing::parse_route("/assets/{*path}/file").error ==
              flash::routing::route_error::catch_all_not_final);
static_assert(flash::routing::validate_route_pairs<^^route_api>());

int main() {
    using enum flash::http_method;
    using enum flash::routing::match_outcome;

    const auto static_match = flash::routing::match_api<^^route_api>(get, "/users/me");
    if (static_match.outcome != found || static_match.endpoint_index != 0 ||
        static_match.route.capture_count != 0) {
        return 1;
    }

    const auto parameter_match = flash::routing::match_api<^^route_api>(get, "/users/42");
    if (parameter_match.outcome != found || parameter_match.endpoint_index != 1 ||
        parameter_match.route.capture_count != 1 ||
        parameter_match.route.captures[0].name != "id" ||
        parameter_match.route.captures[0].value != "42") {
        return 2;
    }

    const auto catch_all_match = flash::routing::match_api<^^route_api>(get, "/assets/css/app.css");
    if (catch_all_match.outcome != found || catch_all_match.endpoint_index != 3 ||
        catch_all_match.route.captures[0].value != "css/app.css") {
        return 3;
    }

    const auto wrong_method = flash::routing::match_api<^^route_api>(delete_, "/users/42");
    if (wrong_method.outcome != method_not_allowed ||
        flash::routing::allow_header(wrong_method.allow_mask) !=
            "GET, HEAD, POST, PUT, PATCH, OPTIONS") {
        return 4;
    }

    const auto options_match = flash::routing::match_api<^^route_api>(options, "/users/42");
    if (options_match.outcome != flash::routing::match_outcome::automatic_options) {
        return 5;
    }

    const auto implicit_head = flash::routing::match_api<^^route_api>(head, "/users/42");
    if (implicit_head.outcome != found || !implicit_head.head_fallback ||
        implicit_head.endpoint_index != 1) {
        return 6;
    }

    const auto explicit_head = flash::routing::match_api<^^route_api>(head, "/health");
    if (explicit_head.outcome != found || explicit_head.head_fallback ||
        explicit_head.endpoint_index != 4) {
        return 7;
    }

    if (flash::routing::match_api<^^route_api>(get, "/unknown").outcome != not_found) {
        return 8;
    }

    const auto put_match = flash::routing::match_api<^^route_api>(put, "/users/42");
    if (put_match.outcome != found || put_match.endpoint_index != 6) {
        return 9;
    }

    const auto patch_match = flash::routing::match_api<^^route_api>(patch, "/users/42");
    if (patch_match.outcome != found || patch_match.endpoint_index != 7) {
        return 10;
    }

    const auto explicit_options =
        flash::routing::match_api<^^route_api>(options, "/health");
    if (explicit_options.outcome != found || explicit_options.endpoint_index != 8) {
        return 11;
    }
    return 0;
}
