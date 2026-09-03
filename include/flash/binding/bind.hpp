#pragma once

#include <flash/binding/meta.hpp>
#include <flash/binding/request.hpp>
#include <flash/binding/scalar.hpp>
#include <flash/context.hpp>
#include <flash/request.hpp>
#include <flash/routing/route.hpp>

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace flash::binding {

struct binding_error {
    source_kind source{source_kind::inferred};
    std::string name;
    std::string code;
    std::string message;
};

[[nodiscard]] inline std::optional<std::string_view>
find_path_value(const routing::route_match& match, std::string_view name) noexcept {
    for (std::size_t index = 0; index < match.capture_count; ++index) {
        if (match.captures[index].name == name) {
            return match.captures[index].value;
        }
    }
    return std::nullopt;
}

template <class Value>
[[nodiscard]] std::expected<Value, scalar_error>
parse_decoded_scalar(std::string_view raw, bool plus_as_space) {
    auto decoded = percent_decode(raw, plus_as_space);
    if (!decoded) {
        return std::unexpected{decoded.error()};
    }
    if constexpr (std::same_as<std::remove_cv_t<Value>, std::string_view>) {
        if (decoded->owns_value) {
            return std::unexpected{scalar_error{
                "escaped_borrowed_string",
                "Escaped input cannot bind to string_view; use std::string for ownership."}};
        }
    }
    return parse_scalar<Value>(decoded->view());
}

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] std::expected<parameter_storage_t<Function, Index>, binding_error>
bind_parameter(const request_view& request, const routing::route_match& match) {
    using storage_type = parameter_storage_t<Function, Index>;
    constexpr auto source = parameter_source<Function, Index>();
    constexpr auto name = wire_name<Function, Index>();

    if constexpr (source == source_kind::context) {
        if constexpr (std::same_as<storage_type, request_context>) {
            return request_context{.request = request, .request_id = {}};
        } else if constexpr (std::same_as<storage_type, raw_request_view>) {
            return request;
        }
    } else if constexpr (source == source_kind::body || source == source_kind::state) {
        static_assert(std::is_void_v<storage_type>,
                      "FLASH-E304: this parameter source is not enabled by the current dispatcher");
    } else {
        std::expected<std::optional<std::string_view>, scalar_error> raw =
            std::optional<std::string_view>{};
        bool plus_as_space = false;
        if constexpr (source == source_kind::path) {
            raw = find_path_value(match, name);
        } else if constexpr (source == source_kind::query) {
            raw = find_query_value(request, name);
            plus_as_space = true;
        } else if constexpr (source == source_kind::header) {
            raw = find_header_value(request, name);
        } else if constexpr (source == source_kind::cookie) {
            raw = find_cookie_value(request, name);
        }

        if (!raw) {
            return std::unexpected{binding_error{
                source, std::string{name}, raw.error().code, raw.error().message}};
        }
        if (!*raw) {
            if constexpr (detail::is_optional_v<storage_type>) {
                return std::nullopt;
            } else if constexpr (default_annotation_count<Function, Index> == 1) {
                return std::meta::extract<default_value_annotation<storage_type>>(
                           std::meta::annotations_of_with_type(
                               meta::parameter_at<Function, Index>(),
                               ^^default_value_annotation<storage_type>)[0])
                    .value;
            } else {
                return std::unexpected{binding_error{
                    source, std::string{name}, "missing_parameter", "Required parameter is missing."}};
            }
        }

        if constexpr (detail::is_optional_v<storage_type>) {
            using value_type = detail::optional_value_t<storage_type>;
            auto parsed = parse_decoded_scalar<value_type>(**raw, plus_as_space);
            if (!parsed) {
                return std::unexpected{binding_error{
                    source, std::string{name}, parsed.error().code, parsed.error().message}};
            }
            return storage_type{std::move(*parsed)};
        } else {
            auto parsed = parse_decoded_scalar<storage_type>(**raw, plus_as_space);
            if (!parsed) {
                return std::unexpected{binding_error{
                    source, std::string{name}, parsed.error().code, parsed.error().message}};
            }
            return std::move(*parsed);
        }
    }
}

} // namespace flash::binding

