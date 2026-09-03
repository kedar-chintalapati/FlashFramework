#pragma once

#include <flash/binding/scalar.hpp>

#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <meta>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace flash::json {
namespace detail {

inline void append_escaped_string(std::string& output, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        switch (character) {
        case '"': output.append("\\\""); break;
        case '\\': output.append("\\\\"); break;
        case '\b': output.append("\\b"); break;
        case '\f': output.append("\\f"); break;
        case '\n': output.append("\\n"); break;
        case '\r': output.append("\\r"); break;
        case '\t': output.append("\\t"); break;
        default:
            if (character < 0x20U) {
                output.append("\\u00");
                output.push_back(hex[character >> 4U]);
                output.push_back(hex[character & 0x0fU]);
            } else {
                output.push_back(static_cast<char>(character));
            }
        }
    }
    output.push_back('"');
}

template <class Enum, std::size_t Index = 0>
[[nodiscard]] std::string_view enum_name(Enum value) {
    if constexpr (Index == binding::detail::enumerator_count<Enum>()) {
        return {};
    } else {
        constexpr auto enumerator = binding::detail::enumerator_at<Enum, Index>();
        constexpr auto candidate =
            std::meta::extract<Enum>(std::meta::constant_of(enumerator));
        if (candidate == value) {
            return std::meta::identifier_of(enumerator);
        }
        return enum_name<Enum, Index + 1>(value);
    }
}

} // namespace detail

template <class Value>
void append(std::string& output, const Value& value) {
    using value_type = std::remove_cv_t<Value>;
    if constexpr (std::same_as<value_type, std::string> ||
                  std::same_as<value_type, std::string_view>) {
        detail::append_escaped_string(output, value);
    } else if constexpr (std::same_as<value_type, bool>) {
        output.append(value ? "true" : "false");
    } else if constexpr (std::integral<value_type>) {
        char buffer[std::numeric_limits<value_type>::digits10 + 4]{};
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (result.ec != std::errc{}) {
            throw std::runtime_error{"integer JSON serialization failed"};
        }
        output.append(buffer, result.ptr);
    } else if constexpr (std::floating_point<value_type>) {
        if (!std::isfinite(value)) {
            throw std::domain_error{"non-finite JSON number"};
        }
        char buffer[128]{};
        const auto result = std::to_chars(
            buffer, buffer + sizeof(buffer), value, std::chars_format::general);
        if (result.ec != std::errc{}) {
            throw std::runtime_error{"floating JSON serialization failed"};
        }
        output.append(buffer, result.ptr);
    } else if constexpr (std::is_enum_v<value_type>) {
        const auto name = detail::enum_name(value);
        if (name.empty()) {
            throw std::domain_error{"enum value has no reflected enumerator"};
        }
        detail::append_escaped_string(output, name);
    } else if constexpr (binding::detail::is_optional_v<value_type>) {
        if (value) {
            append(output, *value);
        } else {
            output.append("null");
        }
    } else {
        static_assert(std::is_void_v<value_type>,
                      "FLASH-E400: no JSON writer exists for this response type");
    }
}

template <class Value>
[[nodiscard]] std::string write(const Value& value) {
    std::string output;
    output.reserve(64);
    append(output, value);
    return output;
}

} // namespace flash::json

