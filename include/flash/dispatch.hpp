#pragma once

#include <flash/binding/bind.hpp>
#include <flash/error_mapping.hpp>
#include <flash/json/write.hpp>
#include <flash/meta/reflection.hpp>
#include <flash/openapi/docs.hpp>
#include <flash/problem.hpp>
#include <flash/raw.hpp>
#include <flash/response.hpp>
#include <flash/routing/matcher.hpp>
#include <flash/task.hpp>

#include <cstddef>
#include <expected>
#include <meta>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace flash {
namespace detail {

template <std::meta::info Namespace, std::size_t Index>
inline constexpr std::meta::info endpoint_reflection =
    meta::endpoint_at<Namespace, Index>();

template <std::meta::info Function>
inline constexpr std::size_t endpoint_arity = meta::parameter_count<Function>();

template <class>
struct created_traits {
    static constexpr bool value = false;
};

template <class Body>
struct created_traits<created<Body>> {
    static constexpr bool value = true;
};

template <class>
struct response_traits {
    static constexpr bool value = false;
};

template <class Body>
struct response_traits<response<Body>> {
    static constexpr bool value = true;
};

template <class>
struct expected_traits {
    static constexpr bool value = false;
};

template <class Result, class Error>
struct expected_traits<std::expected<Result, Error>> {
    static constexpr bool value = true;
    using result_type = Result;
    using error_type = Error;
};

[[nodiscard]] inline response_message adapt_void_response();

template <class Error>
[[nodiscard]] response_message domain_error_problem(const Error& error) {
    const auto& mapping = mapped_error(error);
    const auto code = mapping.code.view();
    return make_problem_response({
        .type = "https://flash.dev/problems/" + std::string{code},
        .title = std::string{mapping.title.view()},
        .status_code = mapping.status_code,
        .detail = "The endpoint rejected the request.",
        .instance = {},
        .errors = {{{"handler", "result"}, std::string{code},
                    "The endpoint returned a mapped domain error."}},
        .request_id = {},
    });
}

template <class Value>
[[nodiscard]] response_message adapt_response(Value&& value) {
    using value_type = std::remove_cvref_t<Value>;
    if constexpr (expected_traits<value_type>::value) {
        using traits = expected_traits<value_type>;
        static_assert(error_mapping_validated<typename traits::error_type>);
        if (!value) {
            return domain_error_problem(value.error());
        }
        if constexpr (std::is_void_v<typename traits::result_type>) {
            return adapt_void_response();
        } else {
            return adapt_response(*std::forward<Value>(value));
        }
    } else if constexpr (std::same_as<value_type, response_message>) {
        return std::forward<Value>(value);
    } else if constexpr (std::same_as<value_type, text>) {
        response_message response;
        response.content_type = "text/plain; charset=utf-8";
        response.body = std::forward<Value>(value).value;
        return response;
    } else if constexpr (std::same_as<value_type, bytes>) {
        response_message response;
        response.content_type = "application/octet-stream";
        const auto& body = value.value;
        response.body.assign(reinterpret_cast<const char*>(body.data()), body.size());
        return response;
    } else if constexpr (created_traits<value_type>::value) {
        response_message response;
        response.status_code = status::created;
        response.body = json::write(value.body);
        response.set_header("Location", value.location);
        return response;
    } else if constexpr (response_traits<value_type>::value) {
        response_message result;
        result.status_code = value.status_code;
        result.headers = std::forward<Value>(value).headers;
        result.body = json::write(value.body);
        return result;
    } else {
        response_message response;
        response.body = json::write(value);
        return response;
    }
}

[[nodiscard]] inline response_message adapt_void_response() {
    response_message response;
    response.status_code = status::no_content;
    response.content_type.clear();
    return response;
}

[[nodiscard]] inline std::string source_name(source_kind source) {
    switch (source) {
    case source_kind::path: return "path";
    case source_kind::query: return "query";
    case source_kind::header: return "header";
    case source_kind::cookie: return "cookie";
    case source_kind::body: return "body";
    case source_kind::context: return "context";
    case source_kind::state: return "state";
    case source_kind::inferred: return "unknown";
    }
    return "unknown";
}

[[nodiscard]] inline response_message binding_problem(
    const binding::binding_error& error, const request_view& request) {
    const bool media_type_error = error.status_code == status::unsupported_media_type;
    return make_problem_response({
        .type = media_type_error
                    ? "https://flash.dev/problems/unsupported-media-type"
                    : "https://flash.dev/problems/invalid-parameter",
        .title = media_type_error ? "Unsupported media type" : "Invalid request parameter",
        .status_code = error.status_code,
        .detail = media_type_error
                      ? "The request body media type is not supported."
                      : "Request parameter could not be bound to the endpoint signature.",
        .instance = std::string{request.target()},
        .errors = {{{source_name(error.source), error.name}, error.code, error.message}},
        .request_id = {},
    });
}

[[nodiscard]] inline response_message dispatch_problem(status code,
                                                       std::string title,
                                                       std::string message,
                                                       std::string instance) {
    return make_problem_response({
        .type = "https://flash.dev/problems/routing",
        .title = std::move(title),
        .status_code = code,
        .detail = std::move(message),
        .instance = std::move(instance),
        .errors = {},
        .request_id = {},
    });
}

template <std::meta::info Function, std::size_t... Index>
[[nodiscard]] auto bind_arguments(const request_view& request,
                                  const routing::route_match& route,
                                  std::index_sequence<Index...>) {
    return std::tuple{
        binding::bind_parameter<Function, Index>(request, route)...};
}

template <std::size_t Index = 0, class Tuple>
[[nodiscard]] const binding::binding_error* first_binding_error(const Tuple& values) {
    if constexpr (Index == std::tuple_size_v<Tuple>) {
        return nullptr;
    } else {
        const auto& value = std::get<Index>(values);
        if (!value) {
            return &value.error();
        }
        return first_binding_error<Index + 1>(values);
    }
}

template <std::meta::info Function, std::size_t Index, class Value>
decltype(auto) forward_bound(Value& value) {
    using parameter_type = [:std::meta::type_of(meta::parameter_at<Function, Index>()):];
    if constexpr (!std::is_reference_v<parameter_type> ||
                  std::is_rvalue_reference_v<parameter_type>) {
        return std::move(value);
    } else {
        return (value);
    }
}

template <std::meta::info Function, class Tuple, std::size_t... Index>
decltype(auto) call_endpoint(Tuple& values, std::index_sequence<Index...>) {
    return meta::invoke<Function>(
        forward_bound<Function, Index>(*std::get<Index>(values))...);
}

template <std::meta::info Function>
task<response_message> invoke_endpoint(const request_view& request,
                                       const routing::route_match& route) {
    static_assert(binding::endpoint_binding_validated<Function>);
    constexpr auto indices = std::make_index_sequence<endpoint_arity<Function>>{};
    auto arguments = bind_arguments<Function>(request, route, indices);
    if (const auto* error = first_binding_error(arguments)) {
        co_return binding_problem(*error, request);
    }

    using result_type = decltype(call_endpoint<Function>(arguments, indices));
    if constexpr (is_task_v<result_type>) {
        if constexpr (std::is_void_v<task_value_t<result_type>>) {
            co_await call_endpoint<Function>(arguments, indices);
            co_return adapt_void_response();
        } else {
            co_return adapt_response(co_await call_endpoint<Function>(arguments, indices));
        }
    } else if constexpr (std::is_void_v<result_type>) {
        call_endpoint<Function>(arguments, indices);
        co_return adapt_void_response();
    } else {
        co_return adapt_response(call_endpoint<Function>(arguments, indices));
    }
}

template <std::meta::info Namespace, std::size_t Index = 0>
task<response_message> dispatch_endpoint(std::size_t endpoint_index,
                                         const request_view& request,
                                         const routing::route_match& route) {
    if constexpr (Index == meta::endpoint_count_v<Namespace>) {
        co_return dispatch_problem(
            status::internal_server_error, "Internal server error",
            "The generated endpoint index is invalid.", std::string{request.target()});
    } else {
        if (endpoint_index == Index) {
            co_return co_await invoke_endpoint<endpoint_reflection<Namespace, Index>>(
                request, route);
        }
        co_return co_await dispatch_endpoint<Namespace, Index + 1>(
            endpoint_index, request, route);
    }
}

} // namespace detail

template <std::meta::info Namespace>
task<response_message> dispatch(const request_view& request) {
    const auto matched = routing::match_api<Namespace>(request.method(), request.path());
    if (matched.outcome == routing::match_outcome::not_found) {
        co_return detail::dispatch_problem(
            status::not_found, "Not Found", "No route matches the request path.",
            std::string{request.target()});
    }
    if (matched.outcome == routing::match_outcome::method_not_allowed) {
        auto response = detail::dispatch_problem(
            status::method_not_allowed, "Method Not Allowed",
            "The path exists, but not for this HTTP method.",
            std::string{request.target()});
        response.set_header("Allow", routing::allow_header(matched.allow_mask));
        co_return response;
    }
    if (matched.outcome == routing::match_outcome::automatic_options) {
        response_message response;
        response.status_code = status::no_content;
        response.content_type.clear();
        response.set_header("Allow", routing::allow_header(matched.allow_mask));
        co_return response;
    }

    auto response = co_await detail::dispatch_endpoint<Namespace>(
        matched.endpoint_index, request, matched.route);
    if (request.method() == http_method::head) {
        response.body.clear();
    }
    co_return response;
}

template <std::meta::info Namespace,
          openapi::documentation_mode Documentation =
              openapi::documentation_mode::development>
struct reflected_handler {
    task<response_message> operator()(raw_request_view request,
                                      raw_response_writer&) const {
        if (auto response =
                openapi::documentation_response<Namespace, Documentation>(request)) {
            co_return std::move(*response);
        }
        co_return co_await dispatch<Namespace>(request);
    }
};

template <std::meta::info Namespace,
          openapi::documentation_mode Documentation =
              openapi::documentation_mode::development>
int serve(server_config config, std::stop_token stop_token = {}) {
    return serve_raw(
        std::move(config), reflected_handler<Namespace, Documentation>{}, stop_token);
}

} // namespace flash
