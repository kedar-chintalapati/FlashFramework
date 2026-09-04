#pragma once

#include <flash/request.hpp>

#include <concepts>
#include <cstddef>
#include <stop_token>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flash {

struct request_context {
    request_view request;
    std::string request_id;
    std::stop_token stop_token;
};

template <class Value>
class state {
public:
    using value_type = Value;

    explicit state(Value& value) noexcept : value_(&value) {}

    [[nodiscard]] Value* operator->() const noexcept { return value_; }
    [[nodiscard]] Value& operator*() const noexcept { return *value_; }

private:
    Value* value_;
};

namespace detail {

template <class... Registered>
class state_registry {
    template <class Value>
    static constexpr std::size_t exact_count =
        (std::size_t{std::same_as<Value, Registered>} + ... + std::size_t{0});

    template <class Value>
    static constexpr std::size_t unqualified_count =
        (std::size_t{std::same_as<std::remove_cv_t<Value>,
                                  std::remove_cv_t<Registered>>} +
         ... + std::size_t{0});

public:
    static_assert(
        ((unqualified_count<Registered> == 1) && ...),
        "FLASH-E308: application state types must be unique after removing cv qualifiers");

    explicit state_registry(Registered&... values) noexcept : values_{&values...} {}

    template <class Value>
    [[nodiscard]] Value& get() const noexcept {
        static_assert(exact_count<Value> == 1,
                      "FLASH-E307: endpoint state type is not registered");
        if constexpr (exact_count<Value> == 1) {
            return *std::get<Value*>(values_);
        }
    }

private:
    std::tuple<Registered*...> values_;
};

} // namespace detail

} // namespace flash
