#pragma once

#include <flash/annotations.hpp>

#include <cstddef>
#include <meta>
#include <string_view>
#include <utility>

namespace flash::meta {

inline constexpr auto reflection_access = std::meta::access_context::unchecked();

[[nodiscard]] consteval bool is_endpoint(std::meta::info declaration) {
    if (!std::meta::is_function(declaration)) {
        return false;
    }
    return std::meta::annotations_of_with_type(declaration, ^^route_annotation).size() == 1;
}

template <std::meta::info Function>
inline constexpr std::size_t route_annotation_count =
    std::meta::annotations_of_with_type(Function, ^^route_annotation).size();

template <std::meta::info Function>
[[nodiscard]] consteval route_annotation route_of() {
    static_assert(std::meta::is_function(Function),
                  "FLASH-E002: route metadata requires a function reflection");
    static_assert(route_annotation_count<Function> == 1,
                  "FLASH-E003: an endpoint must have exactly one Flash route annotation");
    return std::meta::extract<route_annotation>(
        std::meta::annotations_of_with_type(Function, ^^route_annotation)[0]);
}

template <std::meta::info Namespace>
[[nodiscard]] consteval std::size_t endpoint_count() {
    static_assert(std::meta::is_namespace(Namespace),
                  "FLASH-E004: reflected API must name a namespace");
    std::size_t count = 0;
    for (auto member : std::meta::members_of(Namespace, reflection_access)) {
        count += is_endpoint(member) ? 1U : 0U;
    }
    return count;
}

template <std::meta::info Namespace>
inline constexpr std::size_t endpoint_count_v = endpoint_count<Namespace>();

template <std::meta::info Namespace, std::size_t Index>
[[nodiscard]] consteval std::meta::info endpoint_at() {
    static_assert(Index < endpoint_count<Namespace>(),
                  "FLASH-E005: endpoint index is outside the reflected API");
    std::size_t endpoint_index = 0;
    for (auto member : std::meta::members_of(Namespace, reflection_access)) {
        if (!is_endpoint(member)) {
            continue;
        }
        if (endpoint_index == Index) {
            return member;
        }
        ++endpoint_index;
    }
    return {};
}

template <std::meta::info Function>
[[nodiscard]] consteval std::size_t parameter_count() {
    static_assert(std::meta::is_function(Function),
                  "FLASH-E006: parameter inspection requires a function reflection");
    return std::meta::parameters_of(Function).size();
}

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval std::meta::info parameter_at() {
    static_assert(Index < parameter_count<Function>(),
                  "FLASH-E007: parameter index is outside the endpoint signature");
    return std::meta::parameters_of(Function)[Index];
}

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval std::string_view parameter_name() {
    constexpr auto parameter = parameter_at<Function, Index>();
    static_assert(std::meta::has_identifier(parameter),
                  "FLASH-E008: every endpoint parameter must have an identifier");
    return std::meta::identifier_of(parameter);
}

template <class Aggregate>
[[nodiscard]] consteval std::size_t data_member_count() {
    return std::meta::nonstatic_data_members_of(^^Aggregate, reflection_access).size();
}

template <class Aggregate, std::size_t Index>
[[nodiscard]] consteval std::meta::info data_member_at() {
    static_assert(Index < data_member_count<Aggregate>(),
                  "FLASH-E009: data member index is outside the reflected aggregate");
    return std::meta::nonstatic_data_members_of(^^Aggregate, reflection_access)[Index];
}

template <std::meta::info Function, class... Arguments>
constexpr decltype(auto) invoke(Arguments&&... arguments) {
    static_assert(std::meta::is_function(Function),
                  "FLASH-E010: invocation requires a function reflection");
    return [:Function:](std::forward<Arguments>(arguments)...);
}

} // namespace flash::meta
