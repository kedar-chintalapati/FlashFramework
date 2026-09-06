#pragma once

#include <string_view>

namespace flash {

inline constexpr std::string_view version = "0.1.0";

[[nodiscard]] std::string_view runtime_version() noexcept;

} // namespace flash
