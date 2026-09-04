#pragma once

#include <flash/binding/scalar.hpp>
#include <flash/json/schema.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace flash::json {

struct read_limits {
    std::size_t maximum_input_bytes{1024U * 1024U};
    std::size_t maximum_depth{64};
    std::size_t maximum_string_bytes{1024U * 1024U};
    std::size_t maximum_array_elements{100'000};
    std::size_t maximum_object_members{100'000};
    bool reject_unknown_fields{true};
};

struct read_error {
    std::string code;
    std::string message;
    std::string path{"$"};
    std::size_t offset{};
};

namespace detail {

template <class Value>
inline constexpr bool string_like =
    std::same_as<std::remove_cv_t<Value>, std::string> ||
    std::same_as<std::remove_cv_t<Value>, std::string_view>;

struct parsed_string {
    std::string owned{};
    std::string_view borrowed{};
    bool owns_value{};

    [[nodiscard]] std::string_view view() const noexcept {
        return owns_value ? std::string_view{owned} : borrowed;
    }
};

class cursor {
public:
    cursor(std::string_view input, const read_limits& limits)
        : input_(input), limits_(limits) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] bool at_end() const noexcept { return position_ == input_.size(); }
    [[nodiscard]] char peek() const noexcept {
        return at_end() ? '\0' : input_[position_];
    }

    void skip_whitespace() noexcept {
        while (!at_end()) {
            const char character = input_[position_];
            if (character != ' ' && character != '\t' &&
                character != '\r' && character != '\n') {
                return;
            }
            ++position_;
        }
    }

    [[nodiscard]] read_error error(std::string code,
                                   std::string message,
                                   std::string path) const {
        return {std::move(code), std::move(message), std::move(path), position_};
    }

    [[nodiscard]] bool consume(char character) noexcept {
        if (peek() != character) {
            return false;
        }
        ++position_;
        return true;
    }

    [[nodiscard]] bool starts_with(std::string_view literal) const noexcept {
        return input_.substr(position_).starts_with(literal);
    }

    [[nodiscard]] std::expected<void, read_error>
    consume_literal(std::string_view literal, std::string_view path) {
        if (!starts_with(literal)) {
            return std::unexpected{error(
                "invalid_literal", "JSON literal is invalid.", std::string{path})};
        }
        position_ += literal.size();
        return {};
    }

    [[nodiscard]] std::expected<parsed_string, read_error>
    parse_string(std::string_view path) {
        if (!consume('"')) {
            return std::unexpected{error(
                "expected_string", "Expected a JSON string.", std::string{path})};
        }

        parsed_string result;
        std::size_t segment_start = position_;
        while (!at_end()) {
            const std::size_t character_offset = position_;
            const auto character = static_cast<unsigned char>(input_[position_++]);
            if (character == '"') {
                if (result.owns_value) {
                    result.owned.append(input_.substr(
                        segment_start, character_offset - segment_start));
                    if (result.owned.size() > limits_.maximum_string_bytes) {
                        return std::unexpected{error(
                            "string_too_large", "JSON string exceeds the configured limit.",
                            std::string{path})};
                    }
                } else {
                    const auto length = character_offset - segment_start;
                    if (length > limits_.maximum_string_bytes) {
                        return std::unexpected{error(
                            "string_too_large", "JSON string exceeds the configured limit.",
                            std::string{path})};
                    }
                    result.borrowed = input_.substr(segment_start, length);
                }
                if (!valid_utf8(result.view())) {
                    return std::unexpected{error(
                        "invalid_utf8", "JSON string contains invalid UTF-8.",
                        std::string{path})};
                }
                return result;
            }
            if (character < 0x20U) {
                return std::unexpected{error(
                    "invalid_string", "JSON strings cannot contain unescaped control bytes.",
                    std::string{path})};
            }
            if (character != '\\') {
                continue;
            }

            if (!result.owns_value) {
                result.owns_value = true;
                result.owned.reserve(character_offset - segment_start + 16);
            }
            result.owned.append(input_.substr(
                segment_start, character_offset - segment_start));
            if (at_end()) {
                return std::unexpected{error(
                    "invalid_escape", "JSON string ends inside an escape sequence.",
                    std::string{path})};
            }

            const char escape = input_[position_++];
            switch (escape) {
            case '"': result.owned.push_back('"'); break;
            case '\\': result.owned.push_back('\\'); break;
            case '/': result.owned.push_back('/'); break;
            case 'b': result.owned.push_back('\b'); break;
            case 'f': result.owned.push_back('\f'); break;
            case 'n': result.owned.push_back('\n'); break;
            case 'r': result.owned.push_back('\r'); break;
            case 't': result.owned.push_back('\t'); break;
            case 'u': {
                auto code_point = parse_hex_quad(path);
                if (!code_point) {
                    return std::unexpected{std::move(code_point.error())};
                }
                std::uint32_t scalar = *code_point;
                if (scalar >= 0xD800U && scalar <= 0xDBFFU) {
                    if (!starts_with("\\u")) {
                        return std::unexpected{error(
                            "invalid_unicode", "High surrogate must be followed by a low surrogate.",
                            std::string{path})};
                    }
                    position_ += 2;
                    auto low = parse_hex_quad(path);
                    if (!low) {
                        return std::unexpected{std::move(low.error())};
                    }
                    if (*low < 0xDC00U || *low > 0xDFFFU) {
                        return std::unexpected{error(
                            "invalid_unicode", "High surrogate is followed by an invalid low surrogate.",
                            std::string{path})};
                    }
                    scalar = 0x10000U + ((scalar - 0xD800U) << 10U) + (*low - 0xDC00U);
                } else if (scalar >= 0xDC00U && scalar <= 0xDFFFU) {
                    return std::unexpected{error(
                        "invalid_unicode", "Low surrogate has no preceding high surrogate.",
                        std::string{path})};
                }
                append_utf8(result.owned, scalar);
                break;
            }
            default:
                return std::unexpected{error(
                    "invalid_escape", "JSON string contains an unsupported escape sequence.",
                    std::string{path})};
            }
            segment_start = position_;
            if (result.owned.size() > limits_.maximum_string_bytes) {
                return std::unexpected{error(
                    "string_too_large", "JSON string exceeds the configured limit.",
                    std::string{path})};
            }
        }
        return std::unexpected{error(
            "unterminated_string", "JSON string is missing its closing quote.",
            std::string{path})};
    }

    [[nodiscard]] std::expected<std::string_view, read_error>
    parse_number(std::string_view path) {
        const std::size_t start = position_;
        (void)consume('-');
        if (consume('0')) {
            if (peek() >= '0' && peek() <= '9') {
                return std::unexpected{error(
                    "invalid_number", "JSON numbers cannot contain leading zeroes.",
                    std::string{path})};
            }
        } else {
            if (peek() < '1' || peek() > '9') {
                return std::unexpected{error(
                    "invalid_number", "Expected a JSON number.", std::string{path})};
            }
            while (peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }
        if (consume('.')) {
            if (peek() < '0' || peek() > '9') {
                return std::unexpected{error(
                    "invalid_number", "Fractional part requires at least one digit.",
                    std::string{path})};
            }
            while (peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            ++position_;
            if (peek() == '+' || peek() == '-') {
                ++position_;
            }
            if (peek() < '0' || peek() > '9') {
                return std::unexpected{error(
                    "invalid_number", "Exponent requires at least one digit.",
                    std::string{path})};
            }
            while (peek() >= '0' && peek() <= '9') {
                ++position_;
            }
        }
        return input_.substr(start, position_ - start);
    }

    [[nodiscard]] std::expected<void, read_error>
    skip_value(std::size_t depth, std::string_view path) {
        skip_whitespace();
        if (peek() == '"') {
            auto value = parse_string(path);
            if (!value) {
                return std::unexpected{std::move(value.error())};
            }
            return {};
        }
        if (peek() == '{') {
            if (depth >= limits_.maximum_depth) {
                return std::unexpected{error(
                    "maximum_depth", "JSON nesting exceeds the configured limit.",
                    std::string{path})};
            }
            ++position_;
            skip_whitespace();
            std::size_t members = 0;
            if (consume('}')) {
                return {};
            }
            while (true) {
                if (++members > limits_.maximum_object_members) {
                    return std::unexpected{error(
                        "object_too_large", "JSON object exceeds the configured member limit.",
                        std::string{path})};
                }
                auto key = parse_string(path);
                if (!key) {
                    return std::unexpected{std::move(key.error())};
                }
                skip_whitespace();
                if (!consume(':')) {
                    return std::unexpected{error(
                        "expected_colon", "Expected ':' after JSON object key.",
                        std::string{path})};
                }
                auto skipped = skip_value(depth + 1, path);
                if (!skipped) {
                    return skipped;
                }
                skip_whitespace();
                if (consume('}')) {
                    return {};
                }
                if (!consume(',')) {
                    return std::unexpected{error(
                        "expected_comma", "Expected ',' or '}' in JSON object.",
                        std::string{path})};
                }
                skip_whitespace();
            }
        }
        if (peek() == '[') {
            if (depth >= limits_.maximum_depth) {
                return std::unexpected{error(
                    "maximum_depth", "JSON nesting exceeds the configured limit.",
                    std::string{path})};
            }
            ++position_;
            skip_whitespace();
            std::size_t elements = 0;
            if (consume(']')) {
                return {};
            }
            while (true) {
                if (++elements > limits_.maximum_array_elements) {
                    return std::unexpected{error(
                        "array_too_large", "JSON array exceeds the configured element limit.",
                        std::string{path})};
                }
                auto skipped = skip_value(depth + 1, path);
                if (!skipped) {
                    return skipped;
                }
                skip_whitespace();
                if (consume(']')) {
                    return {};
                }
                if (!consume(',')) {
                    return std::unexpected{error(
                        "expected_comma", "Expected ',' or ']' in JSON array.",
                        std::string{path})};
                }
                skip_whitespace();
            }
        }
        if (starts_with("true")) return consume_literal("true", path);
        if (starts_with("false")) return consume_literal("false", path);
        if (starts_with("null")) return consume_literal("null", path);
        auto number = parse_number(path);
        if (!number) {
            return std::unexpected{std::move(number.error())};
        }
        return {};
    }

    [[nodiscard]] const read_limits& limits() const noexcept { return limits_; }

private:
    [[nodiscard]] static bool valid_utf8(std::string_view value) noexcept {
        std::size_t index = 0;
        while (index < value.size()) {
            const auto lead = static_cast<unsigned char>(value[index]);
            if (lead <= 0x7FU) {
                ++index;
                continue;
            }
            std::size_t continuation_count = 0;
            std::uint32_t scalar = 0;
            std::uint32_t minimum = 0;
            if ((lead & 0xE0U) == 0xC0U) {
                continuation_count = 1;
                scalar = lead & 0x1FU;
                minimum = 0x80U;
            } else if ((lead & 0xF0U) == 0xE0U) {
                continuation_count = 2;
                scalar = lead & 0x0FU;
                minimum = 0x800U;
            } else if ((lead & 0xF8U) == 0xF0U) {
                continuation_count = 3;
                scalar = lead & 0x07U;
                minimum = 0x10000U;
            } else {
                return false;
            }
            if (value.size() - index - 1 < continuation_count) {
                return false;
            }
            for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
                const auto continuation = static_cast<unsigned char>(value[index + offset]);
                if ((continuation & 0xC0U) != 0x80U) {
                    return false;
                }
                scalar = (scalar << 6U) | (continuation & 0x3FU);
            }
            if (scalar < minimum || scalar > 0x10FFFFU ||
                (scalar >= 0xD800U && scalar <= 0xDFFFU)) {
                return false;
            }
            index += continuation_count + 1;
        }
        return true;
    }

    [[nodiscard]] std::expected<std::uint32_t, read_error>
    parse_hex_quad(std::string_view path) {
        if (input_.size() - position_ < 4) {
            return std::unexpected{error(
                "invalid_unicode", "Unicode escape requires four hexadecimal digits.",
                std::string{path})};
        }
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            const int digit = binding::hex_value(input_[position_++]);
            if (digit < 0) {
                return std::unexpected{error(
                    "invalid_unicode", "Unicode escape contains a non-hexadecimal digit.",
                    std::string{path})};
            }
            value = (value << 4U) | static_cast<std::uint32_t>(digit);
        }
        return value;
    }

    static void append_utf8(std::string& output, std::uint32_t scalar) {
        if (scalar <= 0x7FU) {
            output.push_back(static_cast<char>(scalar));
        } else if (scalar <= 0x7FFU) {
            output.push_back(static_cast<char>(0xC0U | (scalar >> 6U)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        } else if (scalar <= 0xFFFFU) {
            output.push_back(static_cast<char>(0xE0U | (scalar >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        } else {
            output.push_back(static_cast<char>(0xF0U | (scalar >> 18U)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 12U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
        }
    }

    std::string_view input_;
    const read_limits& limits_;
    std::size_t position_{};
};

[[nodiscard]] inline std::string child_path(std::string_view path,
                                            std::string_view name) {
    std::string result{path};
    result.push_back('.');
    result.append(name);
    return result;
}

[[nodiscard]] inline std::string element_path(std::string_view path,
                                              std::size_t index) {
    std::string result{path};
    result.push_back('[');
    result.append(std::to_string(index));
    result.push_back(']');
    return result;
}

template <class Value>
[[nodiscard]] std::expected<Value, read_error>
read_value(cursor& input, std::size_t depth, std::string path);

template <class Value>
[[nodiscard]] std::expected<void, read_error>
validate_constraint(const Value& value,
                    constraint_annotation constraint,
                    std::string_view path,
                    std::size_t offset) {
    if constexpr (binding::detail::is_optional_v<Value>) {
        if (!value) {
            return {};
        }
        return validate_constraint(*value, constraint, path, offset);
    } else {
        bool valid = true;
        switch (constraint.kind) {
        case constraint_kind::minimum:
        case constraint_kind::exclusive_minimum:
        case constraint_kind::maximum:
        case constraint_kind::exclusive_maximum:
            if constexpr (std::is_arithmetic_v<Value> && !std::same_as<Value, bool>) {
                const double number = static_cast<double>(value);
                if (constraint.kind == constraint_kind::minimum) valid = number >= constraint.numeric_value;
                if (constraint.kind == constraint_kind::exclusive_minimum) valid = number > constraint.numeric_value;
                if (constraint.kind == constraint_kind::maximum) valid = number <= constraint.numeric_value;
                if (constraint.kind == constraint_kind::exclusive_maximum) valid = number < constraint.numeric_value;
            } else {
                valid = false;
            }
            break;
        case constraint_kind::min_length:
        case constraint_kind::max_length:
            if constexpr (string_like<Value>) {
                valid = constraint.kind == constraint_kind::min_length
                            ? value.size() >= constraint.size_value
                            : value.size() <= constraint.size_value;
            } else {
                valid = false;
            }
            break;
        case constraint_kind::min_items:
        case constraint_kind::max_items:
            if constexpr (is_vector_v<Value> || is_array_v<Value>) {
                valid = constraint.kind == constraint_kind::min_items
                            ? value.size() >= constraint.size_value
                            : value.size() <= constraint.size_value;
            } else {
                valid = false;
            }
            break;
        }
        if (!valid) {
            return std::unexpected{read_error{
                "constraint_failed", "JSON value violates a reflected field constraint.",
                std::string{path}, offset}};
        }
        return {};
    }
}

template <class Object, std::size_t Field, std::size_t Constraint = 0>
[[nodiscard]] std::expected<void, read_error>
validate_field(const field_type_t<Object, Field>& value,
               std::string_view path,
               std::size_t offset) {
    if constexpr (Constraint == field_constraint_count<Object, Field>) {
        return {};
    } else {
        auto checked = validate_constraint(
            value, field_constraint<Object, Field, Constraint>(), path, offset);
        if (!checked) {
            return checked;
        }
        return validate_field<Object, Field, Constraint + 1>(value, path, offset);
    }
}

template <class Object, std::size_t Index = 0>
[[nodiscard]] std::expected<bool, read_error>
read_named_field(cursor& input,
                 Object& object,
                 std::array<bool, field_count<Object>>& seen,
                 std::string_view name,
                 std::string_view path,
                 std::size_t depth) {
    if constexpr (Index == field_count<Object>) {
        return false;
    } else {
        if (name == field_wire_name<Object, Index>()) {
            const auto value_path = child_path(path, name);
            if (seen[Index]) {
                return std::unexpected{input.error(
                    "duplicate_field", "JSON object contains a duplicate field.", value_path)};
            }
            seen[Index] = true;
            const auto value_offset = input.position();
            auto value = read_value<field_type_t<Object, Index>>(
                input, depth, value_path);
            if (!value) {
                return std::unexpected{std::move(value.error())};
            }
            auto checked = validate_field<Object, Index>(*value, value_path, value_offset);
            if (!checked) {
                return std::unexpected{std::move(checked.error())};
            }
            object.[:field_reflection<Object, Index>:] = std::move(*value);
            return true;
        }
        return read_named_field<Object, Index + 1>(
            input, object, seen, name, path, depth);
    }
}

template <class Object, std::size_t Index = 0>
[[nodiscard]] std::expected<void, read_error>
finish_missing_fields(Object& object,
                      const std::array<bool, field_count<Object>>& seen,
                      std::string_view path,
                      std::size_t offset) {
    if constexpr (Index == field_count<Object>) {
        return {};
    } else {
        using field_type = field_type_t<Object, Index>;
        if (!seen[Index]) {
            if constexpr (binding::detail::is_optional_v<field_type>) {
                object.[:field_reflection<Object, Index>:] = std::nullopt;
            } else if constexpr (field_default_count<Object, Index> == 1) {
                object.[:field_reflection<Object, Index>:] =
                    field_default_value<Object, Index>();
            } else {
                return std::unexpected{read_error{
                    "missing_field", "Required JSON object field is missing.",
                    child_path(path, field_wire_name<Object, Index>()), offset}};
            }
        }
        return finish_missing_fields<Object, Index + 1>(object, seen, path, offset);
    }
}

template <class Object>
[[nodiscard]] std::expected<Object, read_error>
read_object(cursor& input, std::size_t depth, std::string path) {
    static_assert(input_schema_validated<Object>);
    if (depth >= input.limits().maximum_depth) {
        return std::unexpected{input.error(
            "maximum_depth", "JSON nesting exceeds the configured limit.", path)};
    }
    if (!input.consume('{')) {
        return std::unexpected{input.error(
            "expected_object", "Expected a JSON object.", path)};
    }

    Object object{};
    std::array<bool, field_count<Object>> seen{};
    std::size_t member_count = 0;
    input.skip_whitespace();
    if (!input.consume('}')) {
        while (true) {
            if (++member_count > input.limits().maximum_object_members) {
                return std::unexpected{input.error(
                    "object_too_large", "JSON object exceeds the configured member limit.", path)};
            }
            auto key = input.parse_string(path);
            if (!key) {
                return std::unexpected{std::move(key.error())};
            }
            input.skip_whitespace();
            if (!input.consume(':')) {
                return std::unexpected{input.error(
                    "expected_colon", "Expected ':' after JSON object key.", path)};
            }
            input.skip_whitespace();
            auto known = read_named_field(
                input, object, seen, key->view(), path, depth + 1);
            if (!known) {
                return std::unexpected{std::move(known.error())};
            }
            if (!*known) {
                const auto unknown_path = child_path(path, key->view());
                if (input.limits().reject_unknown_fields) {
                    return std::unexpected{input.error(
                        "unknown_field", "JSON object contains an unknown field.",
                        unknown_path)};
                }
                auto skipped = input.skip_value(depth + 1, unknown_path);
                if (!skipped) {
                    return std::unexpected{std::move(skipped.error())};
                }
            }
            input.skip_whitespace();
            if (input.consume('}')) {
                break;
            }
            if (!input.consume(',')) {
                return std::unexpected{input.error(
                    "expected_comma", "Expected ',' or '}' in JSON object.", path)};
            }
            input.skip_whitespace();
        }
    }
    auto missing = finish_missing_fields(object, seen, path, input.position());
    if (!missing) {
        return std::unexpected{std::move(missing.error())};
    }
    return object;
}

template <class Vector>
[[nodiscard]] std::expected<Vector, read_error>
read_vector(cursor& input, std::size_t depth, std::string path) {
    if (depth >= input.limits().maximum_depth) {
        return std::unexpected{input.error(
            "maximum_depth", "JSON nesting exceeds the configured limit.", path)};
    }
    if (!input.consume('[')) {
        return std::unexpected{input.error(
            "expected_array", "Expected a JSON array.", path)};
    }
    Vector result;
    input.skip_whitespace();
    if (input.consume(']')) {
        return result;
    }
    while (true) {
        if (result.size() >= input.limits().maximum_array_elements) {
            return std::unexpected{input.error(
                "array_too_large", "JSON array exceeds the configured element limit.", path)};
        }
        const auto item_path = element_path(path, result.size());
        using element_type = typename vector_traits<Vector>::element_type;
        auto element = read_value<element_type>(input, depth + 1, item_path);
        if (!element) {
            return std::unexpected{std::move(element.error())};
        }
        result.push_back(std::move(*element));
        input.skip_whitespace();
        if (input.consume(']')) {
            return result;
        }
        if (!input.consume(',')) {
            return std::unexpected{input.error(
                "expected_comma", "Expected ',' or ']' in JSON array.", path)};
        }
        input.skip_whitespace();
    }
}

template <class Array>
[[nodiscard]] std::expected<Array, read_error>
read_array(cursor& input, std::size_t depth, std::string path) {
    if (depth >= input.limits().maximum_depth) {
        return std::unexpected{input.error(
            "maximum_depth", "JSON nesting exceeds the configured limit.", path)};
    }
    if (!input.consume('[')) {
        return std::unexpected{input.error(
            "expected_array", "Expected a JSON array.", path)};
    }
    Array result{};
    constexpr auto expected_size = array_traits<Array>::size;
    if (expected_size > input.limits().maximum_array_elements) {
        return std::unexpected{input.error(
            "array_too_large", "Fixed array exceeds the configured element limit.", path)};
    }
    input.skip_whitespace();
    if constexpr (expected_size == 0) {
        if (!input.consume(']')) {
            return std::unexpected{input.error(
                "array_size", "JSON array does not match the fixed array size.", path)};
        }
        return result;
    }
    if (input.consume(']')) {
        return std::unexpected{input.error(
            "array_size", "JSON array has fewer elements than the fixed array size.", path)};
    }
    for (std::size_t index = 0; index < expected_size; ++index) {
        using element_type = typename array_traits<Array>::element_type;
        auto element = read_value<element_type>(
            input, depth + 1, element_path(path, index));
        if (!element) {
            return std::unexpected{std::move(element.error())};
        }
        result[index] = std::move(*element);
        input.skip_whitespace();
        if (index + 1 < expected_size) {
            if (!input.consume(',')) {
                return std::unexpected{input.error(
                    "array_size", "JSON array has fewer elements than the fixed array size.", path)};
            }
            input.skip_whitespace();
        }
    }
    if (!input.consume(']')) {
        return std::unexpected{input.error(
            "array_size", "JSON array has more elements than the fixed array size.", path)};
    }
    return result;
}

template <class Value>
[[nodiscard]] std::expected<Value, read_error>
read_value(cursor& input, std::size_t depth, std::string path) {
    using value_type = std::remove_cv_t<Value>;
    input.skip_whitespace();
    if constexpr (binding::detail::is_optional_v<value_type>) {
        if (input.starts_with("null")) {
            auto consumed = input.consume_literal("null", path);
            if (!consumed) {
                return std::unexpected{std::move(consumed.error())};
            }
            return std::nullopt;
        }
        using contained_type = binding::detail::optional_value_t<value_type>;
        auto contained = read_value<contained_type>(input, depth, path);
        if (!contained) {
            return std::unexpected{std::move(contained.error())};
        }
        return value_type{std::move(*contained)};
    } else {
        if (input.starts_with("null")) {
            return std::unexpected{input.error(
                "null_not_allowed", "JSON null is not allowed for this value.", path)};
        }
        if constexpr (string_like<value_type>) {
            auto parsed = input.parse_string(path);
            if (!parsed) {
                return std::unexpected{std::move(parsed.error())};
            }
            if constexpr (std::same_as<value_type, std::string_view>) {
                if (parsed->owns_value) {
                    return std::unexpected{input.error(
                        "escaped_borrowed_string",
                        "Escaped JSON cannot bind to string_view; use std::string for ownership.",
                        path)};
                }
                return parsed->borrowed;
            } else {
                return std::string{parsed->view()};
            }
        } else if constexpr (std::same_as<value_type, bool>) {
            if (input.starts_with("true")) {
                auto consumed = input.consume_literal("true", path);
                if (!consumed) return std::unexpected{std::move(consumed.error())};
                return true;
            }
            if (input.starts_with("false")) {
                auto consumed = input.consume_literal("false", path);
                if (!consumed) return std::unexpected{std::move(consumed.error())};
                return false;
            }
            return std::unexpected{input.error(
                "invalid_boolean", "Expected JSON true or false.", path)};
        } else if constexpr (std::integral<value_type> || std::floating_point<value_type>) {
            auto token = input.parse_number(path);
            if (!token) {
                return std::unexpected{std::move(token.error())};
            }
            value_type result{};
            const auto format = std::floating_point<value_type>
                                    ? std::chars_format::general
                                    : std::chars_format::general;
            std::from_chars_result parsed{};
            if constexpr (std::integral<value_type>) {
                parsed = std::from_chars(token->data(), token->data() + token->size(), result, 10);
            } else {
                parsed = std::from_chars(
                    token->data(), token->data() + token->size(), result, format);
            }
            if (parsed.ec == std::errc::result_out_of_range) {
                return std::unexpected{input.error(
                    "number_out_of_range", "JSON number is outside the target type's range.", path)};
            }
            if (parsed.ec != std::errc{} || parsed.ptr != token->data() + token->size() ||
                (std::floating_point<value_type> && !std::isfinite(result))) {
                return std::unexpected{input.error(
                    "invalid_number", "JSON number cannot be represented by the target type.", path)};
            }
            return result;
        } else if constexpr (std::is_enum_v<value_type>) {
            auto parsed = input.parse_string(path);
            if (!parsed) {
                return std::unexpected{std::move(parsed.error())};
            }
            auto value = binding::detail::parse_enum<value_type>(parsed->view());
            if (!value) {
                return std::unexpected{input.error(
                    "invalid_enum", "JSON string is not a reflected enumerator name.", path)};
            }
            return *value;
        } else if constexpr (is_vector_v<value_type>) {
            return read_vector<value_type>(input, depth, std::move(path));
        } else if constexpr (is_array_v<value_type>) {
            return read_array<value_type>(input, depth, std::move(path));
        } else if constexpr (reflectable_object<value_type>) {
            return read_object<value_type>(input, depth, std::move(path));
        } else {
            static_assert(std::is_void_v<value_type>,
                          "FLASH-E409: no JSON reader exists for this request type");
        }
    }
}

} // namespace detail

template <class Value>
[[nodiscard]] std::expected<Value, read_error>
read(std::string_view input, const read_limits& limits = {}) {
    if (input.size() > limits.maximum_input_bytes) {
        return std::unexpected{read_error{
            "input_too_large", "JSON input exceeds the configured limit.", "$", 0}};
    }
    detail::cursor cursor{input, limits};
    auto result = detail::read_value<Value>(cursor, 0, "$" );
    if (!result) {
        return std::unexpected{std::move(result.error())};
    }
    cursor.skip_whitespace();
    if (!cursor.at_end()) {
        return std::unexpected{cursor.error(
            "trailing_characters", "JSON input contains trailing characters.", "$")};
    }
    return result;
}

} // namespace flash::json
