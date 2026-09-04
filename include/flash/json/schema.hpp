#pragma once

#include <flash/annotations.hpp>
#include <flash/detail/fixed_string.hpp>
#include <flash/meta/reflection.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <meta>
#include <string_view>
#include <type_traits>
#include <vector>

namespace flash::json {

namespace detail {

template <class>
struct vector_traits {
    static constexpr bool value = false;
};

template <class Value, class Allocator>
struct vector_traits<std::vector<Value, Allocator>> {
    static constexpr bool value = true;
    using element_type = Value;
};

template <class>
struct array_traits {
    static constexpr bool value = false;
};

template <class Value, std::size_t Size>
struct array_traits<std::array<Value, Size>> {
    static constexpr bool value = true;
    using element_type = Value;
    static constexpr std::size_t size = Size;
};

template <class Value>
inline constexpr bool is_vector_v = vector_traits<std::remove_cv_t<Value>>::value;

template <class Value>
inline constexpr bool is_array_v = array_traits<std::remove_cv_t<Value>>::value;

} // namespace detail

template <class Value>
using object_type_t = std::remove_cvref_t<Value>;

template <class Value>
concept reflectable_object =
    std::is_class_v<object_type_t<Value>> && std::is_aggregate_v<object_type_t<Value>>;

template <class Object>
inline constexpr std::size_t field_count = meta::data_member_count<object_type_t<Object>>();

template <class Object, std::size_t Index>
inline constexpr std::meta::info field_reflection =
    meta::data_member_at<object_type_t<Object>, Index>();

template <class Object, std::size_t Index>
[[nodiscard]] consteval std::meta::info field_type_reflection() {
    return std::meta::remove_cvref(std::meta::type_of(field_reflection<Object, Index>));
}

template <class Object, std::size_t Index>
using field_type_t = [:field_type_reflection<Object, Index>():];

template <class Object, std::size_t Index>
inline constexpr std::size_t field_alias_count =
    std::meta::annotations_of_with_type(field_reflection<Object, Index>, ^^name_annotation)
        .size();

template <class Object, std::size_t Index>
[[nodiscard]] consteval flash::detail::fixed_string field_wire_name_value() {
    static_assert(field_alias_count<Object, Index> <= 1,
                  "FLASH-E401: a JSON field may have at most one name annotation");

    if constexpr (field_alias_count<Object, Index> == 1) {
        return std::meta::extract<name_annotation>(
                   std::meta::annotations_of_with_type(
                       field_reflection<Object, Index>, ^^name_annotation)[0])
            .value;
    } else {
        constexpr auto identifier = std::meta::identifier_of(field_reflection<Object, Index>);
        static_assert(identifier.size() < flash::detail::annotation_text_capacity,
                      "FLASH-E406: a reflected JSON field name is too long");
        flash::detail::fixed_string result{};
        result.length = identifier.size();
        for (std::size_t index = 0; index < identifier.size(); ++index) {
            result.characters[index] = identifier[index];
        }
        return result;
    }
}

template <class Object, std::size_t Index>
inline constexpr flash::detail::fixed_string field_wire_name_storage =
    field_wire_name_value<Object, Index>();

template <class Object, std::size_t Index>
[[nodiscard]] constexpr std::string_view field_wire_name() noexcept {
    return field_wire_name_storage<Object, Index>.view();
}

template <class Object, std::size_t Index>
inline constexpr std::size_t field_default_count =
    std::meta::annotations_of_with_type(
        field_reflection<Object, Index>,
        ^^default_value_annotation<field_type_t<Object, Index>>)
        .size();

template <class Object, std::size_t Index>
[[nodiscard]] consteval field_type_t<Object, Index> field_default_value() {
    static_assert(field_default_count<Object, Index> == 1,
                  "FLASH-E408: a JSON field default must be unique and match its field type");
    return std::meta::extract<default_value_annotation<field_type_t<Object, Index>>>(
               std::meta::annotations_of_with_type(
                   field_reflection<Object, Index>,
                   ^^default_value_annotation<field_type_t<Object, Index>>)[0])
        .value;
}

template <class Object, std::size_t Index = 0>
[[nodiscard]] consteval bool all_fields_public() {
    if constexpr (Index == field_count<Object>) {
        return true;
    } else if constexpr (!std::meta::is_public(field_reflection<Object, Index>)) {
        return false;
    } else {
        return all_fields_public<Object, Index + 1>();
    }
}

template <class Object, std::size_t Index = 0>
consteval void validate_field_metadata() {
    if constexpr (Index < field_count<Object>) {
        static_assert(field_alias_count<Object, Index> <= 1,
                      "FLASH-E401: a JSON field may have at most one name annotation");
        static_assert(field_default_count<Object, Index> <= 1,
                      "FLASH-E408: a JSON field may have at most one matching default annotation");
        validate_field_metadata<Object, Index + 1>();
    }
}

template <class Object, std::size_t Left = 0, std::size_t Right = Left + 1>
[[nodiscard]] consteval bool field_names_unique() {
    if constexpr (Left >= field_count<Object>) {
        return true;
    } else if constexpr (Right >= field_count<Object>) {
        return field_names_unique<Object, Left + 1, Left + 2>();
    } else if constexpr (field_wire_name_storage<Object, Left> ==
                         field_wire_name_storage<Object, Right>) {
        return false;
    } else {
        return field_names_unique<Object, Left, Right + 1>();
    }
}

template <class Object>
consteval void validate_output_schema() {
    using object_type = object_type_t<Object>;
    static_assert(reflectable_object<object_type>,
                  "FLASH-E402: a JSON object must be a class aggregate");
    if constexpr (reflectable_object<object_type>) {
        validate_field_metadata<object_type>();
        static_assert(all_fields_public<object_type>(),
                      "FLASH-E403: reflected JSON fields must be public");
        static_assert(field_names_unique<object_type>(),
                      "FLASH-E404: reflected JSON field names must be unique");
    }
}

template <class Object>
consteval void validate_input_schema() {
    using object_type = object_type_t<Object>;
    validate_output_schema<object_type>();
    static_assert(std::default_initializable<object_type>,
                  "FLASH-E405: a JSON request aggregate must be default constructible");
}

template <class Object>
inline constexpr bool output_schema_validated = [] consteval {
    validate_output_schema<Object>();
    return true;
}();

template <class Object>
inline constexpr bool input_schema_validated = [] consteval {
    validate_input_schema<Object>();
    return true;
}();

template <class Object, std::size_t Index>
inline constexpr std::size_t field_constraint_count =
    std::meta::annotations_of_with_type(
        field_reflection<Object, Index>, ^^constraint_annotation)
        .size();

template <class Object, std::size_t Index, std::size_t ConstraintIndex>
[[nodiscard]] consteval constraint_annotation field_constraint() {
    static_assert(ConstraintIndex < field_constraint_count<Object, Index>,
                  "FLASH-E407: JSON field constraint index is out of range");
    return std::meta::extract<constraint_annotation>(
        std::meta::annotations_of_with_type(
            field_reflection<Object, Index>, ^^constraint_annotation)[ConstraintIndex]);
}

} // namespace flash::json
