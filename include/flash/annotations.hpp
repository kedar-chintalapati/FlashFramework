#pragma once

#include <flash/detail/fixed_string.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace flash {

enum class http_method {
    get,
    post,
    put,
    patch,
    delete_,
    head,
    options,
};

enum class source_kind {
    inferred,
    path,
    query,
    header,
    cookie,
    body,
    context,
    state,
};

enum class constraint_kind {
    minimum,
    exclusive_minimum,
    maximum,
    exclusive_maximum,
    min_length,
    max_length,
    min_items,
    max_items,
};

struct route_annotation {
    http_method method{};
    detail::fixed_string path{};
};

struct source_annotation {
    source_kind source{source_kind::inferred};
    detail::fixed_string wire_name{};
};

struct constraint_annotation {
    constraint_kind kind{};
    double numeric_value{};
    std::size_t size_value{};
};

struct name_annotation {
    detail::fixed_string value{};
};

struct description_annotation {
    detail::fixed_string value{};
};

struct pattern_annotation {
    detail::fixed_string value{};
};

struct deprecated_annotation {};

template <class Value>
struct default_value_annotation {
    Value value;
};

template <std::size_t Size>
consteval route_annotation get(const char (&value)[Size]) {
    return {http_method::get, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval route_annotation post(const char (&value)[Size]) {
    return {http_method::post, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval route_annotation put(const char (&value)[Size]) {
    return {http_method::put, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval route_annotation patch(const char (&value)[Size]) {
    return {http_method::patch, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval route_annotation delete_(const char (&value)[Size]) {
    return {http_method::delete_, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval route_annotation head(const char (&value)[Size]) {
    return {http_method::head, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval route_annotation options(const char (&value)[Size]) {
    return {http_method::options, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval source_annotation path(const char (&value)[Size]) {
    return {source_kind::path, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval source_annotation query(const char (&value)[Size]) {
    return {source_kind::query, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval source_annotation header(const char (&value)[Size]) {
    return {source_kind::header, detail::fixed_string{value}};
}

template <std::size_t Size>
consteval source_annotation cookie(const char (&value)[Size]) {
    return {source_kind::cookie, detail::fixed_string{value}};
}

inline constexpr source_annotation body{source_kind::body, {}};
inline constexpr deprecated_annotation deprecated{};

template <class Value>
    requires std::is_arithmetic_v<std::remove_cvref_t<Value>>
consteval constraint_annotation minimum(Value value) {
    return {constraint_kind::minimum, static_cast<double>(value), 0};
}

template <class Value>
    requires std::is_arithmetic_v<std::remove_cvref_t<Value>>
consteval constraint_annotation exclusive_minimum(Value value) {
    return {constraint_kind::exclusive_minimum, static_cast<double>(value), 0};
}

template <class Value>
    requires std::is_arithmetic_v<std::remove_cvref_t<Value>>
consteval constraint_annotation maximum(Value value) {
    return {constraint_kind::maximum, static_cast<double>(value), 0};
}

template <class Value>
    requires std::is_arithmetic_v<std::remove_cvref_t<Value>>
consteval constraint_annotation exclusive_maximum(Value value) {
    return {constraint_kind::exclusive_maximum, static_cast<double>(value), 0};
}

consteval constraint_annotation min_length(std::size_t value) {
    return {constraint_kind::min_length, 0.0, value};
}

consteval constraint_annotation max_length(std::size_t value) {
    return {constraint_kind::max_length, 0.0, value};
}

consteval constraint_annotation min_items(std::size_t value) {
    return {constraint_kind::min_items, 0.0, value};
}

consteval constraint_annotation max_items(std::size_t value) {
    return {constraint_kind::max_items, 0.0, value};
}

template <std::size_t Size>
consteval name_annotation name(const char (&value)[Size]) {
    return {detail::fixed_string{value}};
}

template <std::size_t Size>
consteval description_annotation description(const char (&value)[Size]) {
    return {detail::fixed_string{value}};
}

template <std::size_t Size>
consteval pattern_annotation pattern(const char (&value)[Size]) {
    return {detail::fixed_string{value}};
}

template <class Value>
    requires std::copy_constructible<std::remove_cvref_t<Value>>
consteval auto default_value(Value&& value) {
    using stored_type = std::remove_cvref_t<Value>;
    return default_value_annotation<stored_type>{std::forward<Value>(value)};
}

} // namespace flash

