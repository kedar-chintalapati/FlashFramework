#pragma once

#include <flash/detail/fixed_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace flash::routing {

inline constexpr std::size_t max_route_segments = 32;
inline constexpr std::size_t max_path_parameters = 16;

enum class segment_kind {
    literal,
    parameter,
    catch_all,
};

enum class route_error {
    none,
    missing_leading_slash,
    trailing_slash,
    empty_segment,
    unmatched_brace,
    invalid_parameter_name,
    duplicate_parameter_name,
    catch_all_not_final,
    too_many_segments,
    too_many_parameters,
};

struct route_segment {
    segment_kind kind{segment_kind::literal};
    detail::fixed_string text{};
};

struct route_pattern {
    detail::fixed_string source{};
    std::array<route_segment, max_route_segments> segments{};
    std::size_t segment_count{};
    std::size_t parameter_count{};
    route_error error{route_error::none};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return error == route_error::none;
    }
};

[[nodiscard]] consteval detail::fixed_string fixed_substring(
    std::string_view source, std::size_t offset, std::size_t count) {
    detail::fixed_string result;
    if (count >= detail::annotation_text_capacity) {
        count = detail::annotation_text_capacity - 1;
    }
    result.length = count;
    for (std::size_t index = 0; index < count; ++index) {
        result.characters[index] = source[offset + index];
    }
    result.characters[count] = '\0';
    return result;
}

[[nodiscard]] consteval bool valid_parameter_name(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    for (const char character : value) {
        const bool alpha = (character >= 'a' && character <= 'z') ||
                           (character >= 'A' && character <= 'Z');
        const bool digit = character >= '0' && character <= '9';
        if (!alpha && !digit && character != '_' && character != '-') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] consteval route_pattern parse_route(detail::fixed_string path) {
    route_pattern result;
    result.source = path;
    const auto source = path.view();

    if (source.empty() || source.front() != '/') {
        result.error = route_error::missing_leading_slash;
        return result;
    }
    if (source == "/") {
        return result;
    }
    if (source.back() == '/') {
        result.error = route_error::trailing_slash;
        return result;
    }

    std::size_t offset = 1;
    while (offset <= source.size()) {
        if (result.segment_count == max_route_segments) {
            result.error = route_error::too_many_segments;
            return result;
        }

        const auto separator = source.find('/', offset);
        const auto end = separator == std::string_view::npos ? source.size() : separator;
        const auto segment_text = source.substr(offset, end - offset);
        if (segment_text.empty()) {
            result.error = route_error::empty_segment;
            return result;
        }

        route_segment segment;
        if (segment_text.front() == '{' || segment_text.back() == '}') {
            if (segment_text.size() < 3 || segment_text.front() != '{' ||
                segment_text.back() != '}') {
                result.error = route_error::unmatched_brace;
                return result;
            }

            auto name = segment_text.substr(1, segment_text.size() - 2);
            segment.kind = segment_kind::parameter;
            if (!name.empty() && name.front() == '*') {
                segment.kind = segment_kind::catch_all;
                name.remove_prefix(1);
                if (separator != std::string_view::npos) {
                    result.error = route_error::catch_all_not_final;
                    return result;
                }
            }
            if (!valid_parameter_name(name)) {
                result.error = route_error::invalid_parameter_name;
                return result;
            }
            if (result.parameter_count == max_path_parameters) {
                result.error = route_error::too_many_parameters;
                return result;
            }
            for (std::size_t index = 0; index < result.segment_count; ++index) {
                if (result.segments[index].kind != segment_kind::literal &&
                    result.segments[index].text.view() == name) {
                    result.error = route_error::duplicate_parameter_name;
                    return result;
                }
            }
            ++result.parameter_count;
            segment.text = fixed_substring(name, 0, name.size());
        } else {
            if (segment_text.find('{') != std::string_view::npos ||
                segment_text.find('}') != std::string_view::npos) {
                result.error = route_error::unmatched_brace;
                return result;
            }
            segment.kind = segment_kind::literal;
            segment.text = fixed_substring(segment_text, 0, segment_text.size());
        }

        result.segments[result.segment_count++] = segment;
        if (separator == std::string_view::npos) {
            break;
        }
        offset = separator + 1;
    }
    return result;
}

template <std::size_t Size>
[[nodiscard]] consteval route_pattern parse_route(const char (&path)[Size]) {
    return parse_route(detail::fixed_string{path});
}

[[nodiscard]] constexpr bool same_route_shape(const route_pattern& left,
                                              const route_pattern& right) noexcept {
    if (left.segment_count != right.segment_count) {
        return false;
    }
    for (std::size_t index = 0; index < left.segment_count; ++index) {
        const auto& left_segment = left.segments[index];
        const auto& right_segment = right.segments[index];
        if (left_segment.kind != right_segment.kind) {
            return false;
        }
        if (left_segment.kind == segment_kind::literal &&
            left_segment.text != right_segment.text) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr std::uint64_t route_shape_hash(
    const route_pattern& route) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto append = [&hash](unsigned char value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    append(static_cast<unsigned char>(route.segment_count));
    for (std::size_t index = 0; index < route.segment_count; ++index) {
        const auto& segment = route.segments[index];
        append(static_cast<unsigned char>(segment.kind));
        if (segment.kind == segment_kind::literal) {
            for (const char character : segment.text.view()) {
                append(static_cast<unsigned char>(character));
            }
        }
        append(255U);
    }
    return hash;
}

struct path_capture {
    std::string_view name;
    std::string_view value;
};

struct route_match {
    bool matched{};
    std::array<path_capture, max_path_parameters> captures{};
    std::size_t capture_count{};
};

// GCC 16.2 can compile this function incorrectly when it is constexpr.
// ADR 0002 records the compiler behavior.
[[nodiscard]] inline route_match match_route(const route_pattern& pattern,
                                             std::string_view path) noexcept {
    route_match result;
    if (!pattern.valid() || path.empty() || path.front() != '/') {
        return result;
    }
    if (pattern.segment_count == 0) {
        result.matched = path == "/";
        return result;
    }
    if (path.size() > 1 && path.back() == '/') {
        return result;
    }

    std::size_t offset = 1;
    for (std::size_t index = 0; index < pattern.segment_count; ++index) {
        const auto& expected = pattern.segments[index];
        if (expected.kind == segment_kind::catch_all) {
            if (offset >= path.size()) {
                return {};
            }
            result.captures[result.capture_count++] = {
                expected.text.view(), path.substr(offset)};
            result.matched = true;
            return result;
        }

        if (offset > path.size()) {
            return {};
        }
        const auto separator = path.find('/', offset);
        const auto end = separator == std::string_view::npos ? path.size() : separator;
        const auto actual = path.substr(offset, end - offset);
        if (actual.empty()) {
            return {};
        }
        if (expected.kind == segment_kind::literal) {
            if (actual != expected.text.view()) {
                return {};
            }
        } else {
            result.captures[result.capture_count++] = {expected.text.view(), actual};
        }

        if (index + 1 < pattern.segment_count) {
            if (separator == std::string_view::npos) {
                return {};
            }
            offset = separator + 1;
        } else if (separator != std::string_view::npos) {
            return {};
        }
    }
    result.matched = true;
    return result;
}

[[nodiscard]] inline unsigned segment_precedence(segment_kind kind) noexcept {
    switch (kind) {
    case segment_kind::literal: return 3;
    case segment_kind::parameter: return 2;
    case segment_kind::catch_all: return 1;
    }
    return 0;
}

[[nodiscard]] inline bool more_specific(const route_pattern& candidate,
                                        const route_pattern& current) noexcept {
    const auto common = candidate.segment_count < current.segment_count
                            ? candidate.segment_count
                            : current.segment_count;
    for (std::size_t index = 0; index < common; ++index) {
        const auto candidate_rank = segment_precedence(candidate.segments[index].kind);
        const auto current_rank = segment_precedence(current.segments[index].kind);
        if (candidate_rank != current_rank) {
            return candidate_rank > current_rank;
        }
    }
    return candidate.segment_count > current.segment_count;
}

} // namespace flash::routing
