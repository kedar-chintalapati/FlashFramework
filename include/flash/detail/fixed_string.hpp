#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace flash::detail {

inline constexpr std::size_t annotation_text_capacity = 256;

struct fixed_string {
    std::array<char, annotation_text_capacity> characters{};
    std::size_t length{};

    consteval fixed_string() = default;

    template <std::size_t Size>
    consteval fixed_string(const char (&value)[Size]) : length(Size - 1) {
        static_assert(Size <= annotation_text_capacity,
                      "FLASH-E001: annotation text exceeds the supported capacity");
        for (std::size_t index = 0; index < Size; ++index) {
            characters[index] = value[index];
        }
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept {
        return {characters.data(), length};
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return length == 0;
    }

    [[nodiscard]] constexpr bool operator==(
        const fixed_string& other) const noexcept {
        return view() == other.view();
    }

    [[nodiscard]] constexpr auto operator<=>(
        const fixed_string& other) const noexcept {
        return view() <=> other.view();
    }
};

} // namespace flash::detail
