#pragma once

#include <flash/annotations.hpp>
#include <flash/context.hpp>
#include <flash/meta/reflection.hpp>
#include <flash/routing/matcher.hpp>

#include <cstddef>
#include <meta>
#include <optional>
#include <string_view>
#include <type_traits>

namespace flash::binding {

template <class>
struct is_state_wrapper : std::false_type {};

template <class Value>
struct is_state_wrapper<state<Value>> : std::true_type {};

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval std::meta::info parameter_storage_info() {
    return std::meta::remove_cvref(
        std::meta::type_of(meta::parameter_at<Function, Index>()));
}

template <std::meta::info Function, std::size_t Index>
using parameter_storage_t = [:parameter_storage_info<Function, Index>():];

template <std::meta::info Function, std::size_t Index>
inline constexpr std::size_t source_annotation_count =
    std::meta::annotations_of_with_type(
        meta::parameter_at<Function, Index>(), ^^source_annotation)
        .size();

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval source_annotation explicit_source() {
    static_assert(source_annotation_count<Function, Index> <= 1,
                  "FLASH-E301: a parameter may have at most one source annotation");
    if constexpr (source_annotation_count<Function, Index> == 1) {
        return std::meta::extract<source_annotation>(
            std::meta::annotations_of_with_type(
                meta::parameter_at<Function, Index>(), ^^source_annotation)[0]);
    }
    return {};
}

template <std::meta::info Function, std::size_t Index>
inline constexpr source_annotation explicit_source_metadata = explicit_source<Function, Index>();

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval std::string_view wire_name() {
    constexpr const auto& source = explicit_source_metadata<Function, Index>;
    if constexpr (!source.wire_name.empty()) {
        return source.wire_name.view();
    }
    return meta::parameter_name<Function, Index>();
}

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval bool name_appears_in_route() {
    constexpr const auto& route = routing::compiled_route<Function>;
    constexpr auto name = wire_name<Function, Index>();
    for (std::size_t segment_index = 0;
         segment_index < route.segment_count;
         ++segment_index) {
        if (route.segments[segment_index].kind != routing::segment_kind::literal &&
            route.segments[segment_index].text.view() == name) {
            return true;
        }
    }
    return false;
}

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval source_kind parameter_source() {
    constexpr const auto& explicit_value = explicit_source_metadata<Function, Index>;
    if constexpr (explicit_value.source != source_kind::inferred) {
        return explicit_value.source;
    } else if constexpr (name_appears_in_route<Function, Index>()) {
        return source_kind::path;
    } else {
        using storage_type = parameter_storage_t<Function, Index>;
        if constexpr (std::same_as<storage_type, request_context> ||
                      std::same_as<storage_type, raw_request_view>) {
            return source_kind::context;
        } else if constexpr (is_state_wrapper<storage_type>::value) {
            return source_kind::state;
        } else if constexpr (std::is_aggregate_v<storage_type> &&
                             !std::same_as<storage_type, std::string>) {
            constexpr auto method = meta::route_of<Function>().method;
            if constexpr (method == http_method::post || method == http_method::put ||
                          method == http_method::patch) {
                return source_kind::body;
            }
        }
        return source_kind::query;
    }
}

template <std::meta::info Function, std::size_t Index>
inline constexpr std::size_t default_annotation_count =
    std::meta::annotations_of_with_type(
        meta::parameter_at<Function, Index>(),
        ^^default_value_annotation<parameter_storage_t<Function, Index>>)
        .size();

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] consteval bool route_has_wire_name() {
    constexpr const auto& route = routing::compiled_route<Function>;
    constexpr auto name = wire_name<Function, Index>();
    for (std::size_t route_index = 0; route_index < route.segment_count; ++route_index) {
        if (route.segments[route_index].kind != routing::segment_kind::literal &&
            route.segments[route_index].text.view() == name) {
            return true;
        }
    }
    return false;
}

template <std::meta::info Function>
[[nodiscard]] consteval bool every_placeholder_is_bound_once() {
    constexpr const auto& route = routing::compiled_route<Function>;
    for (std::size_t route_index = 0; route_index < route.segment_count; ++route_index) {
        const auto& segment = route.segments[route_index];
        if (segment.kind == routing::segment_kind::literal) {
            continue;
        }
        std::size_t bindings = 0;
        for (auto parameter : std::meta::parameters_of(Function)) {
            const auto annotations =
                std::meta::annotations_of_with_type(parameter, ^^source_annotation);
            if (annotations.size() > 1) {
                return false;
            }
            if (annotations.size() == 1) {
                const auto source = std::meta::extract<source_annotation>(annotations[0]);
                const auto name = source.wire_name.empty()
                                      ? std::meta::identifier_of(parameter)
                                      : source.wire_name.view();
                if (source.source == source_kind::path && name == segment.text.view()) {
                    ++bindings;
                }
            } else if (std::meta::identifier_of(parameter) == segment.text.view()) {
                ++bindings;
            }
        }
        if (bindings != 1) {
            return false;
        }
    }
    return true;
}

template <std::meta::info Function, std::size_t Index = 0>
[[nodiscard]] consteval bool every_explicit_path_exists() {
    if constexpr (Index == meta::parameter_count<Function>()) {
        return true;
    } else if constexpr (parameter_source<Function, Index>() == source_kind::path &&
                         !route_has_wire_name<Function, Index>()) {
        return false;
    } else {
        return every_explicit_path_exists<Function, Index + 1>();
    }
}

template <std::meta::info Function, std::size_t Index = 0>
[[nodiscard]] consteval bool parameter_annotations_valid() {
    if constexpr (Index == meta::parameter_count<Function>()) {
        return true;
    } else if constexpr (source_annotation_count<Function, Index> > 1 ||
                         default_annotation_count<Function, Index> > 1) {
        return false;
    } else {
        return parameter_annotations_valid<Function, Index + 1>();
    }
}

template <std::meta::info Function, std::size_t Index = 0>
[[nodiscard]] consteval std::size_t body_parameter_count() {
    if constexpr (Index == meta::parameter_count<Function>()) {
        return 0;
    } else {
        return (parameter_source<Function, Index>() == source_kind::body ? 1U : 0U) +
               body_parameter_count<Function, Index + 1>();
    }
}

template <std::meta::info Function>
consteval void validate_endpoint_binding() {
    static_assert(parameter_annotations_valid<Function>(),
                  "FLASH-E301: parameter source/default annotations are ambiguous");
    static_assert(every_placeholder_is_bound_once<Function>(),
                  "FLASH-E302: every route placeholder must bind exactly one parameter");
    static_assert(every_explicit_path_exists<Function>(),
                  "FLASH-E303: a path parameter must name a route placeholder");
    static_assert(body_parameter_count<Function>() <= 1,
                  "FLASH-E305: an endpoint may have at most one body parameter");
}

template <std::meta::info Function>
inline constexpr bool endpoint_binding_validated = [] consteval {
    validate_endpoint_binding<Function>();
    return true;
}();

} // namespace flash::binding
