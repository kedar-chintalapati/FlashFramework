#pragma once

#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <expected>
#include <limits>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace flash::binding {

struct scalar_error {
    std::string code;
    std::string message;
};

struct decoded_value {
    std::string owned{};
    std::string_view borrowed{};
    bool owns_value{};

    [[nodiscard]] std::string_view view() const noexcept {
        return owns_value ? std::string_view{owned} : borrowed;
    }
};

[[nodiscard]] constexpr int hex_value(char character) noexcept {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] inline std::expected<decoded_value, scalar_error>
percent_decode(std::string_view input, bool plus_as_space = false) {
    const bool requires_decoding = input.find('%') != std::string_view::npos ||
                                   (plus_as_space && input.find('+') != std::string_view::npos);
    if (!requires_decoding) {
        return decoded_value{.borrowed = input};
    }

    decoded_value result;
    result.owns_value = true;
    result.owned.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char character = input[index];
        if (character == '+' && plus_as_space) {
            result.owned.push_back(' ');
            continue;
        }
        if (character != '%') {
            result.owned.push_back(character);
            continue;
        }
        if (index + 2 >= input.size()) {
            return std::unexpected{scalar_error{
                "invalid_percent_encoding", "Percent escape must contain two hexadecimal digits."}};
        }
        const int high = hex_value(input[index + 1]);
        const int low = hex_value(input[index + 2]);
        if (high < 0 || low < 0) {
            return std::unexpected{scalar_error{
                "invalid_percent_encoding", "Percent escape contains a non-hexadecimal digit."}};
        }
        result.owned.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return result;
}

namespace detail {

template <class>
struct optional_traits {
    static constexpr bool is_optional = false;
};

template <class Value>
struct optional_traits<std::optional<Value>> {
    static constexpr bool is_optional = true;
    using value_type = Value;
};

template <class Value>
inline constexpr bool is_optional_v = optional_traits<std::remove_cv_t<Value>>::is_optional;

template <class Value>
using optional_value_t = typename optional_traits<std::remove_cv_t<Value>>::value_type;

template <class Enum>
[[nodiscard]] consteval std::size_t enumerator_count() {
    return std::meta::enumerators_of(^^Enum).size();
}

template <class Enum, std::size_t Index>
[[nodiscard]] consteval std::meta::info enumerator_at() {
    return std::meta::enumerators_of(^^Enum)[Index];
}

template <class Enum, std::size_t Index = 0>
[[nodiscard]] std::optional<Enum> parse_enum(std::string_view input) {
    if constexpr (Index == enumerator_count<Enum>()) {
        return std::nullopt;
    } else {
        constexpr auto enumerator = enumerator_at<Enum, Index>();
        if (std::meta::identifier_of(enumerator) == input) {
            return std::meta::extract<Enum>(std::meta::constant_of(enumerator));
        }
        return parse_enum<Enum, Index + 1>(input);
    }
}

} // namespace detail

template <class Value>
[[nodiscard]] std::expected<Value, scalar_error> parse_scalar(std::string_view input) {
    using value_type = std::remove_cv_t<Value>;

    if constexpr (std::same_as<value_type, std::string>) {
        return std::string{input};
    } else if constexpr (std::same_as<value_type, std::string_view>) {
        return input;
    } else if constexpr (std::same_as<value_type, bool>) {
        if (input == "true" || input == "1") {
            return true;
        }
        if (input == "false" || input == "0") {
            return false;
        }
        return std::unexpected{scalar_error{
            "invalid_boolean", "Expected true, false, 1, or 0."}};
    } else if constexpr (std::integral<value_type>) {
        value_type result{};
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), result, 10);
        if (parsed.ec == std::errc::result_out_of_range) {
            return std::unexpected{scalar_error{
                "integer_out_of_range", "Integer is outside the target type's range."}};
        }
        if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() || input.empty()) {
            return std::unexpected{scalar_error{
                "invalid_integer", "Expected a base-10 integer."}};
        }
        return result;
    } else if constexpr (std::floating_point<value_type>) {
        value_type result{};
        const auto parsed = std::from_chars(
            input.data(), input.data() + input.size(), result, std::chars_format::general);
        if (parsed.ec == std::errc::result_out_of_range) {
            return std::unexpected{scalar_error{
                "number_out_of_range", "Number is outside the target type's range."}};
        }
        if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() ||
            input.empty() || !std::isfinite(result)) {
            return std::unexpected{scalar_error{
                "invalid_number", "Expected a finite floating-point number."}};
        }
        return result;
    } else if constexpr (std::is_enum_v<value_type>) {
        if (auto result = detail::parse_enum<value_type>(input)) {
            return *result;
        }
        return std::unexpected{scalar_error{
            "invalid_enum", "Value is not one of the enum's reflected names."}};
    } else {
        static_assert(std::is_void_v<value_type>,
                      "FLASH-E300: no scalar codec exists for this parameter type");
    }
}

template <class Value>
inline constexpr bool scalar_bindable =
    std::same_as<std::remove_cv_t<Value>, std::string> ||
    std::same_as<std::remove_cv_t<Value>, std::string_view> ||
    std::same_as<std::remove_cv_t<Value>, bool> ||
    std::integral<std::remove_cv_t<Value>> ||
    std::floating_point<std::remove_cv_t<Value>> ||
    std::is_enum_v<std::remove_cv_t<Value>>;

} // namespace flash::binding

