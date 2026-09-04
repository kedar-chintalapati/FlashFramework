#pragma once

#include <flash/annotations.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>

namespace flash {

struct header_view {
    std::string_view name;
    std::string_view value;
};

[[nodiscard]] constexpr char ascii_lower(char value) noexcept {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value + ('a' - 'A'));
    }
    return value;
}

[[nodiscard]] constexpr bool ascii_iequals(std::string_view left,
                                           std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (ascii_lower(left[index]) != ascii_lower(right[index])) {
            return false;
        }
    }
    return true;
}

class request_view {
public:
    constexpr request_view() = default;

    constexpr request_view(http_method method,
                           std::string_view target,
                           std::span<const header_view> headers,
                           std::string_view body_value,
                           bool keep_alive_value,
                           std::stop_token stop_token_value = {}) noexcept
        : method_(method),
          target_(target),
          headers_(headers),
          body_(body_value),
          keep_alive_(keep_alive_value),
          stop_token_(stop_token_value) {
        const auto query_marker = target.find('?');
        path_ = target.substr(0, query_marker);
        if (query_marker != std::string_view::npos) {
            query_ = target.substr(query_marker + 1);
        }
    }

    [[nodiscard]] constexpr http_method method() const noexcept { return method_; }
    [[nodiscard]] constexpr std::string_view target() const noexcept { return target_; }
    [[nodiscard]] constexpr std::string_view path() const noexcept { return path_; }
    [[nodiscard]] constexpr std::string_view query() const noexcept { return query_; }
    [[nodiscard]] constexpr std::span<const header_view> headers() const noexcept { return headers_; }
    [[nodiscard]] constexpr std::string_view body() const noexcept { return body_; }
    [[nodiscard]] constexpr bool keep_alive() const noexcept { return keep_alive_; }
    [[nodiscard]] std::stop_token stop_token() const noexcept { return stop_token_; }

    [[nodiscard]] constexpr std::optional<std::string_view>
    header(std::string_view name) const noexcept {
        for (const auto& field : headers_) {
            if (ascii_iequals(field.name, name)) {
                return field.value;
            }
        }
        return std::nullopt;
    }

private:
    http_method method_{http_method::get};
    std::string_view target_{};
    std::string_view path_{};
    std::string_view query_{};
    std::span<const header_view> headers_{};
    std::string_view body_{};
    bool keep_alive_{};
    std::stop_token stop_token_{};
};

using raw_request_view = request_view;

} // namespace flash
