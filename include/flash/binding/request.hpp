#pragma once

#include <flash/binding/scalar.hpp>
#include <flash/request.hpp>

#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>

namespace flash::binding {

[[nodiscard]] inline std::expected<std::optional<std::string_view>, scalar_error>
find_query_value(const request_view& request, std::string_view expected_name) {
    std::optional<std::string_view> result;
    std::size_t offset = 0;
    const auto query = request.query();
    while (offset <= query.size() && !query.empty()) {
        const auto separator = query.find('&', offset);
        const auto end = separator == std::string_view::npos ? query.size() : separator;
        const auto pair = query.substr(offset, end - offset);
        const auto equals = pair.find('=');
        const auto raw_name = pair.substr(0, equals);
        const auto raw_value = equals == std::string_view::npos
                                   ? std::string_view{}
                                   : pair.substr(equals + 1);
        auto decoded_name = percent_decode(raw_name, true);
        if (!decoded_name) {
            return std::unexpected{decoded_name.error()};
        }
        if (decoded_name->view() == expected_name) {
            if (result) {
                return std::unexpected{scalar_error{
                    "duplicate_query_parameter", "A scalar query parameter occurred more than once."}};
            }
            result = raw_value;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        offset = separator + 1;
    }
    return result;
}

[[nodiscard]] inline std::expected<std::optional<std::string_view>, scalar_error>
find_header_value(const request_view& request, std::string_view expected_name) {
    std::optional<std::string_view> result;
    for (const auto& header : request.headers()) {
        if (!ascii_iequals(header.name, expected_name)) {
            continue;
        }
        if (result) {
            return std::unexpected{scalar_error{
                "duplicate_header", "A scalar header occurred more than once."}};
        }
        result = header.value;
    }
    return result;
}

[[nodiscard]] constexpr std::string_view trim_optional_whitespace(
    std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] inline std::expected<std::optional<std::string_view>, scalar_error>
find_cookie_value(const request_view& request, std::string_view expected_name) {
    const auto cookie_header = find_header_value(request, "Cookie");
    if (!cookie_header) {
        return std::unexpected{cookie_header.error()};
    }
    if (!*cookie_header) {
        return std::optional<std::string_view>{};
    }

    std::optional<std::string_view> result;
    std::size_t offset = 0;
    const auto cookies = **cookie_header;
    while (offset <= cookies.size() && !cookies.empty()) {
        const auto separator = cookies.find(';', offset);
        const auto end = separator == std::string_view::npos ? cookies.size() : separator;
        const auto pair = trim_optional_whitespace(cookies.substr(offset, end - offset));
        const auto equals = pair.find('=');
        if (equals != std::string_view::npos) {
            const auto name = trim_optional_whitespace(pair.substr(0, equals));
            const auto value = trim_optional_whitespace(pair.substr(equals + 1));
            if (name == expected_name) {
                if (result) {
                    return std::unexpected{scalar_error{
                        "duplicate_cookie", "A scalar cookie occurred more than once."}};
                }
                result = value;
            }
        }
        if (separator == std::string_view::npos) {
            break;
        }
        offset = separator + 1;
    }
    return result;
}

} // namespace flash::binding

