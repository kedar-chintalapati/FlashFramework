#pragma once

#include <flash/binding/meta.hpp>
#include <flash/binding/request.hpp>
#include <flash/binding/scalar.hpp>
#include <flash/context.hpp>
#include <flash/json/read.hpp>
#include <flash/request.hpp>
#include <flash/response.hpp>
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
    status status_code{status::unprocessable_content};
};

[[nodiscard]] constexpr bool is_json_media_type(std::string_view value) noexcept {
    value = trim_optional_whitespace(value);
    const auto parameter = value.find(';');
    const auto media_type = trim_optional_whitespace(value.substr(0, parameter));
    if (ascii_iequals(media_type, "application/json")) {
        return true;
    }
    constexpr std::string_view suffix = "+json";
    return media_type.size() > suffix.size() &&
           ascii_iequals(media_type.substr(media_type.size() - suffix.size()), suffix) &&
           media_type.find('/') != std::string_view::npos;
}

template <std::meta::info Function,
          std::size_t Index,
          class Value,
          std::size_t ConstraintIndex = 0>
[[nodiscard]] std::expected<void, scalar_error>
validate_parameter_constraints(const Value& value) {
    if constexpr (ConstraintIndex == parameter_constraint_count<Function, Index>) {
        return {};
    } else {
        auto checked = json::detail::validate_constraint(
            value, parameter_constraint<Function, Index, ConstraintIndex>(), {}, 0);
        if (!checked) {
            return std::unexpected{scalar_error{
                checked.error().code, checked.error().message}};
        }
        return validate_parameter_constraints<
            Function, Index, Value, ConstraintIndex + 1>(value);
    }
}

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

template <std::meta::info Function, std::size_t Index, class StateRegistry>
[[nodiscard]] std::expected<parameter_storage_t<Function, Index>, binding_error>
bind_parameter(const request_view& request,
               const routing::route_match& match,
               StateRegistry& states) {
    using storage_type = parameter_storage_t<Function, Index>;
    constexpr auto source = parameter_source<Function, Index>();
    constexpr auto name = wire_name<Function, Index>();

    if constexpr (source == source_kind::context) {
        if constexpr (std::same_as<storage_type, request_context>) {
            return request_context{.request = request, .request_id = {}};
        } else if constexpr (std::same_as<storage_type, raw_request_view>) {
            return request;
        }
    } else if constexpr (source == source_kind::body) {
        auto content_type = find_header_value(request, "Content-Type");
        if (!content_type) {
            return std::unexpected{binding_error{
                source, std::string{name}, content_type.error().code,
                content_type.error().message, status::bad_request}};
        }
        if (!*content_type || !is_json_media_type(**content_type)) {
            return std::unexpected{binding_error{
                source, std::string{name}, "unsupported_media_type",
                "A JSON request body requires an application/json media type.",
                status::unsupported_media_type}};
        }
        auto parsed = json::read<storage_type>(request.body());
        if (!parsed) {
            return std::unexpected{binding_error{
                source, parsed.error().path, parsed.error().code,
                parsed.error().message, status::unprocessable_content}};
        }
        return std::move(*parsed);
    } else if constexpr (source == source_kind::state) {
        using state_type = typename storage_type::value_type;
        return storage_type{states.template get<state_type>()};
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
            storage_type result{std::move(*parsed)};
            auto checked = validate_parameter_constraints<Function, Index>(result);
            if (!checked) {
                return std::unexpected{binding_error{
                    source, std::string{name}, checked.error().code,
                    checked.error().message}};
            }
            return result;
        } else {
            auto parsed = parse_decoded_scalar<storage_type>(**raw, plus_as_space);
            if (!parsed) {
                return std::unexpected{binding_error{
                    source, std::string{name}, parsed.error().code, parsed.error().message}};
            }
            auto checked = validate_parameter_constraints<Function, Index>(*parsed);
            if (!checked) {
                return std::unexpected{binding_error{
                    source, std::string{name}, checked.error().code,
                    checked.error().message}};
            }
            return std::move(*parsed);
        }
    }
}

template <std::meta::info Function, std::size_t Index>
[[nodiscard]] std::expected<parameter_storage_t<Function, Index>, binding_error>
bind_parameter(const request_view& request, const routing::route_match& match) {
    flash::detail::state_registry<> states;
    return bind_parameter<Function, Index>(request, match, states);
}

} // namespace flash::binding
