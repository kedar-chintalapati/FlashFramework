#pragma once

#include <flash/detail/fixed_string.hpp>
#include <flash/response.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <meta>
#include <string_view>
#include <type_traits>
#include <utility>

namespace flash {

template <class Error>
struct error_mapping {
    Error value{};
    status status_code{status::internal_server_error};
    detail::fixed_string code{};
    detail::fixed_string title{};
};

template <class Error, std::size_t Size>
struct error_mapping_set {
    using error_type = Error;
    static constexpr bool configured = true;
    std::array<error_mapping<Error>, Size> entries{};
};

template <class Error>
struct missing_error_mapping {
    using error_type = Error;
    static constexpr bool configured = false;
};

template <class Error>
inline constexpr auto error_map = missing_error_mapping<Error>{};

template <auto Error, std::size_t CodeSize, std::size_t TitleSize>
    requires std::is_enum_v<decltype(Error)>
[[nodiscard]] consteval auto map(status status_code,
                                 const char (&code)[CodeSize],
                                 const char (&title)[TitleSize]) {
    return error_mapping<decltype(Error)>{
        Error, status_code, detail::fixed_string{code}, detail::fixed_string{title}};
}

template <class First, class... Rest>
    requires ((std::same_as<First, Rest>) && ...)
[[nodiscard]] consteval auto errors(First first, Rest... rest) {
    using error_type = decltype(first.value);
    return error_mapping_set<error_type, 1 + sizeof...(Rest)>{{first, rest...}};
}

template <class Error>
[[nodiscard]] consteval std::size_t reflected_error_count() {
    static_assert(std::is_enum_v<Error>,
                  "FLASH-E500: a domain error mapping requires an enum type");
    return std::meta::enumerators_of(^^Error).size();
}

template <class Error, std::size_t Index>
[[nodiscard]] consteval Error reflected_error_value() {
    return std::meta::extract<Error>(
        std::meta::constant_of(std::meta::enumerators_of(^^Error)[Index]));
}

template <class Error, std::size_t EnumIndex, std::size_t MappingIndex = 0>
[[nodiscard]] consteval std::size_t mapping_count_for_error() {
    if constexpr (MappingIndex == error_map<Error>.entries.size()) {
        return 0;
    } else {
        return (error_map<Error>.entries[MappingIndex].value ==
                        reflected_error_value<Error, EnumIndex>()
                    ? 1U
                    : 0U) +
               mapping_count_for_error<Error, EnumIndex, MappingIndex + 1>();
    }
}

template <class Error, std::size_t EnumIndex = 0>
[[nodiscard]] consteval bool every_error_mapped_once() {
    if constexpr (EnumIndex == reflected_error_count<Error>()) {
        return true;
    } else if constexpr (mapping_count_for_error<Error, EnumIndex>() != 1) {
        return false;
    } else {
        return every_error_mapped_once<Error, EnumIndex + 1>();
    }
}

template <class Error>
consteval void validate_error_mapping() {
    using mapping_type = std::remove_cvref_t<decltype(error_map<Error>)>;
    static_assert(mapping_type::configured,
                  "FLASH-E501: std::expected error type has no Flash error mapping");
    if constexpr (mapping_type::configured) {
        static_assert(every_error_mapped_once<Error>(),
                      "FLASH-E502: every domain error enumerator must be mapped exactly once");
    }
}

template <class Error>
inline constexpr bool error_mapping_validated = [] consteval {
    validate_error_mapping<Error>();
    return true;
}();

template <class Error>
[[nodiscard]] constexpr const error_mapping<Error>& mapped_error(Error error) noexcept {
    static_assert(error_mapping_validated<Error>);
    for (const auto& entry : error_map<Error>.entries) {
        if (entry.value == error) {
            return entry;
        }
    }
    std::unreachable();
}

} // namespace flash
