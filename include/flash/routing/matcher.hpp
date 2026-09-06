#pragma once

#include <flash/annotations.hpp>
#include <flash/meta/reflection.hpp>
#include <flash/routing/route.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace flash::routing {

template <std::meta::info Function>
[[nodiscard]] consteval route_pattern route_pattern_of() {
    constexpr auto parsed = parse_route(meta::route_of<Function>().path);
    static_assert(parsed.error == route_error::none,
                  "FLASH-E200: endpoint route does not satisfy the Flash route grammar");
    return parsed;
}

template <std::meta::info Function>
inline constexpr route_pattern compiled_route = route_pattern_of<Function>();

template <std::meta::info Namespace, std::size_t... Index>
[[nodiscard]] consteval auto compiled_api_routes_impl(
    std::index_sequence<Index...>) {
    return std::array<route_pattern, sizeof...(Index)>{
        compiled_route<meta::endpoint_at<Namespace, Index>()>...};
}

template <std::meta::info Namespace>
inline constexpr auto compiled_api_routes = compiled_api_routes_impl<Namespace>(
    std::make_index_sequence<meta::endpoint_count_v<Namespace>>{});

template <std::meta::info Namespace, std::size_t... Index>
[[nodiscard]] consteval auto compiled_api_methods_impl(
    std::index_sequence<Index...>) {
    return std::array<http_method, sizeof...(Index)>{
        meta::route_of<meta::endpoint_at<Namespace, Index>()>().method...};
}

template <std::meta::info Namespace>
inline constexpr auto compiled_api_methods = compiled_api_methods_impl<Namespace>(
    std::make_index_sequence<meta::endpoint_count_v<Namespace>>{});

template <std::meta::info Namespace, std::size_t... Index>
[[nodiscard]] consteval auto compiled_api_shape_hashes_impl(
    std::index_sequence<Index...>) {
    return std::array<std::uint64_t, sizeof...(Index)>{
        route_shape_hash(compiled_route<meta::endpoint_at<Namespace, Index>()>)...};
}

template <std::meta::info Namespace>
inline constexpr auto compiled_api_shape_hashes =
    compiled_api_shape_hashes_impl<Namespace>(
        std::make_index_sequence<meta::endpoint_count_v<Namespace>>{});

template <std::meta::info Namespace>
[[nodiscard]] consteval bool validate_route_pairs() {
    constexpr const auto& routes = compiled_api_routes<Namespace>;
    constexpr const auto& methods = compiled_api_methods<Namespace>;
    constexpr const auto& hashes = compiled_api_shape_hashes<Namespace>;
    for (std::size_t left = 0; left < routes.size(); ++left) {
        for (std::size_t right = left + 1; right < routes.size(); ++right) {
            if (methods[left] == methods[right] && hashes[left] == hashes[right] &&
                same_route_shape(routes[left], routes[right])) {
                return false;
            }
        }
    }
    return true;
}

template <std::meta::info Namespace>
consteval void validate_routes() {
    static_assert(validate_route_pairs<Namespace>(),
                  "FLASH-E201: two endpoints have the same method and route shape");
}

enum class match_outcome {
    found,
    not_found,
    method_not_allowed,
    automatic_options,
};

[[nodiscard]] constexpr std::uint16_t method_mask(http_method method) noexcept {
    return static_cast<std::uint16_t>(1U << static_cast<unsigned>(method));
}

struct api_match {
    match_outcome outcome{match_outcome::not_found};
    std::size_t endpoint_index{};
    route_match route{};
    std::uint16_t allow_mask{};
    bool head_fallback{};
};

namespace detail {

struct api_match_state {
    api_match result{};
    const route_pattern* best_pattern{};
    bool has_path_match{};
    bool has_method_match{};
    bool best_is_exact_method{};
    bool explicit_options{};
};

template <std::meta::info Namespace, std::size_t Index = 0>
void consider_routes(http_method requested,
                     std::string_view path,
                     api_match_state& state) {
    if constexpr (Index < meta::endpoint_count<Namespace>()) {
        constexpr auto function = meta::endpoint_at<Namespace, Index>();
        constexpr auto annotation = meta::route_of<function>();
        constexpr const auto& pattern = compiled_route<function>;
        const auto matched = match_route(pattern, path);
        if (matched.matched) {
            state.has_path_match = true;
            state.result.allow_mask |= method_mask(annotation.method);
            if (annotation.method == http_method::get) {
                state.result.allow_mask |= method_mask(http_method::head);
            }
            state.result.allow_mask |= method_mask(http_method::options);

            const bool exact = annotation.method == requested;
            const bool head_fallback = requested == http_method::head &&
                                       annotation.method == http_method::get;
            if (requested == http_method::options && exact) {
                state.explicit_options = true;
            }
            if (exact || head_fallback) {
                const bool replace = !state.has_method_match ||
                                     (exact && !state.best_is_exact_method) ||
                                     (exact == state.best_is_exact_method &&
                                      more_specific(pattern, *state.best_pattern));
                if (replace) {
                    state.has_method_match = true;
                    state.best_is_exact_method = exact;
                    state.best_pattern = &pattern;
                    state.result.endpoint_index = Index;
                    state.result.route = matched;
                    state.result.head_fallback = head_fallback;
                }
            }
        }
        consider_routes<Namespace, Index + 1>(requested, path, state);
    }
}

} // namespace detail

template <std::meta::info Namespace>
[[nodiscard]] api_match match_api(http_method method, std::string_view path) {
    validate_routes<Namespace>();
    detail::api_match_state state;
    detail::consider_routes<Namespace>(method, path, state);

    if (method == http_method::options && state.has_path_match && !state.explicit_options) {
        state.result.outcome = match_outcome::automatic_options;
    } else if (state.has_method_match) {
        state.result.outcome = match_outcome::found;
    } else if (state.has_path_match) {
        state.result.outcome = match_outcome::method_not_allowed;
    } else {
        state.result.outcome = match_outcome::not_found;
    }
    return state.result;
}

[[nodiscard]] inline std::string allow_header(std::uint16_t mask) {
    std::string result;
    const auto append = [&result, mask](http_method method, std::string_view name) {
        if ((mask & method_mask(method)) == 0) {
            return;
        }
        if (!result.empty()) {
            result.append(", ");
        }
        result.append(name);
    };
    append(http_method::get, "GET");
    append(http_method::head, "HEAD");
    append(http_method::post, "POST");
    append(http_method::put, "PUT");
    append(http_method::patch, "PATCH");
    append(http_method::delete_, "DELETE");
    append(http_method::options, "OPTIONS");
    return result;
}

} // namespace flash::routing
