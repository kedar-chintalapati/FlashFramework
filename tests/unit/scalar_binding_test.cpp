#include <flash/binding/request.hpp>
#include <flash/binding/scalar.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string>

enum class color {
    red,
    green,
    blue,
};

int main() {
    using flash::binding::parse_scalar;

    if (parse_scalar<int>("-42") != -42 ||
        parse_scalar<std::uint64_t>("18446744073709551615") !=
            std::numeric_limits<std::uint64_t>::max() ||
        parse_scalar<bool>("true") != true || parse_scalar<bool>("0") != false ||
        parse_scalar<double>("1.25") != 1.25 ||
        parse_scalar<color>("green") != color::green ||
        parse_scalar<std::string>("hello") != "hello") {
        return 1;
    }
    if (parse_scalar<unsigned>("-1") || parse_scalar<int>("12x") ||
        parse_scalar<bool>("yes") || parse_scalar<double>("nan") ||
        parse_scalar<color>("purple")) {
        return 2;
    }

    const auto decoded = flash::binding::percent_decode("hello%20world", true);
    if (!decoded || decoded->view() != "hello world" ||
        flash::binding::percent_decode("%xz")) {
        return 3;
    }

    const std::array headers{
        flash::header_view{"X-Request-ID", "abc"},
        flash::header_view{"Cookie", "session=token; theme=dark"},
    };
    const flash::request_view request{
        flash::http_method::get,
        "/search?q=hello+world&limit=20",
        headers,
        {},
        true,
    };
    const auto query = flash::binding::find_query_value(request, "q");
    const auto header = flash::binding::find_header_value(request, "x-request-id");
    const auto cookie = flash::binding::find_cookie_value(request, "session");
    if (!query || !*query || **query != "hello+world" ||
        !header || !*header || **header != "abc" ||
        !cookie || !*cookie || **cookie != "token") {
        return 4;
    }

    const flash::request_view duplicate_query{
        flash::http_method::get,
        "/search?q=one&q=two",
        {},
        {},
        false,
    };
    if (flash::binding::find_query_value(duplicate_query, "q")) {
        return 5;
    }

    constexpr std::array duplicate_headers{
        flash::header_view{"X-Value", "one"},
        flash::header_view{"x-value", "two"},
    };
    const flash::request_view duplicate_header{
        flash::http_method::get, "/", duplicate_headers, {}, false};
    const auto duplicate_header_value =
        flash::binding::find_header_value(duplicate_header, "X-Value");
    if (duplicate_header_value ||
        duplicate_header_value.error().code != "duplicate_header") {
        return 6;
    }

    constexpr std::array duplicate_cookie_headers{
        flash::header_view{"Cookie", "session=one; session=two"},
    };
    const flash::request_view duplicate_cookie{
        flash::http_method::get, "/", duplicate_cookie_headers, {}, false};
    const auto duplicate_cookie_value =
        flash::binding::find_cookie_value(duplicate_cookie, "session");
    if (duplicate_cookie_value ||
        duplicate_cookie_value.error().code != "duplicate_cookie") {
        return 7;
    }
    return 0;
}
