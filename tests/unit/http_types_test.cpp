#include <flash/problem.hpp>
#include <flash/request.hpp>
#include <flash/response.hpp>

#include <array>
#include <stdexcept>
#include <string>

int main() {
    constexpr std::array headers{
        flash::header_view{"Content-Type", "application/json"},
        flash::header_view{"X-Request-ID", "abc-123"},
    };
    const flash::request_view request{
        flash::http_method::post,
        "/widgets?verbose=true",
        headers,
        "{}",
        true,
    };

    if (request.path() != "/widgets" || request.query() != "verbose=true" ||
        request.header("content-type") != "application/json" ||
        request.header("missing") != std::nullopt) {
        return 1;
    }

    flash::response_message response;
    response.set_header("Cache-Control", "private");
    response.set_header("Cache-Control", "no-store");
    if (response.headers.size() != 1 || response.headers[0].value != "no-store") {
        return 2;
    }

    bool rejected = false;
    try {
        response.set_header("X-Test", "valid\r\nInjected: yes");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected) {
        return 3;
    }

    const auto problem = flash::make_problem_response({
        .type = "https://flash.dev/problems/invalid-parameter",
        .title = "Invalid request parameter",
        .status_code = flash::status::unprocessable_content,
        .detail = "Path parameter \"id\" is invalid.",
        .instance = "/widgets/nope",
        .errors = {{{"path", "id"}, "invalid_integer", "Expected an integer."}},
        .request_id = "request-1",
    });

    if (problem.status_code != flash::status::unprocessable_content ||
        problem.content_type != "application/problem+json" ||
        problem.body.find("\\\"id\\\"") == std::string::npos ||
        problem.body.find("invalid_integer") == std::string::npos) {
        return 4;
    }
    return 0;
}
