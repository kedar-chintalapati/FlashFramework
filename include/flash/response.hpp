#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace flash {

enum class status : std::uint16_t {
    ok = 200,
    created = 201,
    no_content = 204,
    bad_request = 400,
    unauthorized = 401,
    forbidden = 403,
    not_found = 404,
    method_not_allowed = 405,
    request_timeout = 408,
    payload_too_large = 413,
    unsupported_media_type = 415,
    unprocessable_content = 422,
    internal_server_error = 500,
    service_unavailable = 503,
};

struct header_value {
    std::string name;
    std::string value;
};

[[nodiscard]] constexpr bool valid_header_name(std::string_view name) noexcept {
    if (name.empty()) {
        return false;
    }
    constexpr std::string_view separators{"()<>@,;:\\\"/[]?={} \t"};
    for (const char raw_character : name) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character <= 31U || character >= 127U ||
            separators.find(static_cast<char>(character)) != std::string_view::npos) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool valid_header_value(std::string_view value) noexcept {
    if (value.size() > 8U * 1024U) {
        return false;
    }
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character == '\r' || character == '\n' || character == 0U ||
            (character < 32U && character != '\t')) {
            return false;
        }
    }
    return true;
}

struct response_message {
    status status_code{status::ok};
    std::vector<header_value> headers{};
    std::string body{};
    std::string content_type{"application/json"};
    bool keep_alive{true};

    void set_header(std::string name, std::string value) {
        if (!valid_header_name(name) || !valid_header_value(value)) {
            throw std::invalid_argument{"invalid HTTP response header"};
        }
        for (auto& header : headers) {
            if (header.name == name) {
                header.value = std::move(value);
                return;
            }
        }
        headers.push_back({std::move(name), std::move(value)});
    }
};

struct text {
    std::string value;
};

struct bytes {
    std::vector<std::byte> value;
};

template <class Body>
struct created {
    Body body;
    std::string location;
};

template <class Body>
struct response {
    status status_code{status::ok};
    std::vector<header_value> headers{};
    Body body;
};

} // namespace flash
