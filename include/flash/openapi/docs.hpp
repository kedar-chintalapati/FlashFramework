#pragma once

#include <flash/openapi/document.hpp>
#include <flash/problem.hpp>
#include <flash/request.hpp>
#include <flash/response.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace flash::openapi {

enum class documentation_mode {
    disabled,
    development,
};

inline constexpr std::string_view documentation_html =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>API documentation</title></head><body>"
    "<h1>API documentation</h1><p><a href=\"/openapi.json\">OpenAPI document</a></p>"
    "<pre id=\"document\">Loading.</pre><script>"
    "fetch('/openapi.json').then(function(response){return response.json();})"
    ".then(function(value){document.getElementById('document').textContent="
    "JSON.stringify(value,null,2);})"
    ".catch(function(error){document.getElementById('document').textContent="
    "String(error);});</script></body></html>";

template <std::meta::info Namespace, std::size_t Index = 0>
[[nodiscard]] consteval bool documentation_paths_available() {
    if constexpr (Index == meta::endpoint_count_v<Namespace>) {
        return true;
    } else {
        constexpr auto function = meta::endpoint_at<Namespace, Index>();
        constexpr auto path = routing::compiled_route<function>.source.view();
        if constexpr (path == "/openapi.json" || path == "/docs") {
            return false;
        }
        return documentation_paths_available<Namespace, Index + 1>();
    }
}

template <std::meta::info Namespace,
          documentation_mode Mode = documentation_mode::development>
[[nodiscard]] std::optional<response_message>
documentation_response(const request_view& request) {
    if constexpr (Mode == documentation_mode::disabled) {
        return std::nullopt;
    } else {
        static_assert(documentation_paths_available<Namespace>(),
                      "FLASH-E606: application routes conflict with development documentation paths");
        const bool openapi_path = request.path() == "/openapi.json";
        const bool docs_path = request.path() == "/docs";
        if (!openapi_path && !docs_path) {
            return std::nullopt;
        }

        response_message response;
        response.set_header("Allow", "GET, HEAD, OPTIONS");
        response.set_header("Cache-Control", "no-store");
        if (request.method() == http_method::options) {
            response.status_code = status::no_content;
            response.content_type.clear();
            return response;
        }
        if (request.method() != http_method::get && request.method() != http_method::head) {
            response = make_problem_response({
                .type = "https://flash.dev/problems/routing",
                .title = "Method Not Allowed",
                .status_code = status::method_not_allowed,
                .detail = "The documentation route only accepts GET, HEAD, and OPTIONS.",
                .instance = std::string{request.target()},
                .errors = {},
                .request_id = {},
            });
            response.set_header("Allow", "GET, HEAD, OPTIONS");
            return response;
        }

        if (openapi_path) {
            response.content_type = "application/json";
            response.body = document<Namespace>();
        } else {
            response.content_type = "text/html; charset=utf-8";
            response.body = documentation_html;
            response.set_header(
                "Content-Security-Policy",
                "default-src 'self'; script-src 'unsafe-inline'; object-src 'none'");
        }
        if (request.method() == http_method::head) {
            response.body.clear();
        }
        return response;
    }
}

} // namespace flash::openapi
