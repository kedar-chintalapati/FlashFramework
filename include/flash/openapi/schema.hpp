#pragma once

#include <flash/binding/meta.hpp>
#include <flash/error_mapping.hpp>
#include <flash/json/schema.hpp>
#include <flash/json/write.hpp>
#include <flash/meta/reflection.hpp>
#include <flash/response.hpp>
#include <flash/task.hpp>

#include <boost/asio/awaitable.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <vector>

namespace flash::openapi {

template <class... Types>
struct type_list {};

template <class List>
struct type_list_size;

template <class... Types>
struct type_list_size<type_list<Types...>>
    : std::integral_constant<std::size_t, sizeof...(Types)> {};

template <class List>
inline constexpr std::size_t type_list_size_v = type_list_size<List>::value;

namespace detail {

template <class Value, class List>
struct list_contains;

template <class Value, class... Types>
struct list_contains<Value, type_list<Types...>>
    : std::bool_constant<(std::same_as<Value, Types> || ...)> {};

template <class List, class Value>
struct list_append;

template <class... Types, class Value>
struct list_append<type_list<Types...>, Value> {
    using type = type_list<Types..., Value>;
};

template <class Value, class List>
struct collect_type;

template <class Object, class List, std::size_t Index>
struct collect_fields {
    using field_type = std::remove_cvref_t<json::field_type_t<Object, Index>>;
    using with_field = typename collect_type<field_type, List>::type;
    using type = typename collect_fields<Object, with_field, Index + 1>::type;
};

template <class Object, class List>
struct collect_fields<Object, List, json::field_count<Object>> {
    using type = List;
};

template <class Value, class List, bool IsObject, bool AlreadyPresent>
struct collect_plain_type {
    using type = List;
};

template <class Value, class List>
struct collect_plain_type<Value, List, true, false> {
    using with_object = typename list_append<List, Value>::type;
    using type = typename collect_fields<Value, with_object, 0>::type;
};

template <class Value, class List>
struct collect_type {
    using clean_type = std::remove_cvref_t<Value>;
    using type = typename collect_plain_type<
        clean_type, List, json::reflectable_object<clean_type>,
        list_contains<clean_type, List>::value>::type;
};

template <class Value, class List>
struct collect_type<std::optional<Value>, List> : collect_type<Value, List> {};

template <class Value, class Allocator, class List>
struct collect_type<std::vector<Value, Allocator>, List> : collect_type<Value, List> {};

template <class Value, std::size_t Size, class List>
struct collect_type<std::array<Value, Size>, List> : collect_type<Value, List> {};

template <class Value, class Error, class List>
struct collect_type<std::expected<Value, Error>, List> : collect_type<Value, List> {};

template <class Error, class List>
struct collect_type<std::expected<void, Error>, List> {
    using type = List;
};

template <class Value, class List>
struct collect_type<created<Value>, List> : collect_type<Value, List> {};

template <class Value, class List>
struct collect_type<response<Value>, List> : collect_type<Value, List> {};

template <class Value, class Executor, class List>
struct collect_type<boost::asio::awaitable<Value, Executor>, List>
    : collect_type<Value, List> {};

template <std::meta::info Function>
[[nodiscard]] consteval std::meta::info function_return_reflection() {
    return std::meta::remove_cvref(std::meta::return_type_of(Function));
}

template <std::meta::info Function>
using function_return_t = [:function_return_reflection<Function>():];

template <std::meta::info Function, class List, std::size_t Index = 0>
struct collect_body_types {
    using parameter_type = binding::parameter_storage_t<Function, Index>;
    using with_parameter = std::conditional_t<
        binding::parameter_source<Function, Index>() == source_kind::body,
        typename collect_type<parameter_type, List>::type,
        List>;
    using type = typename collect_body_types<Function, with_parameter, Index + 1>::type;
};

template <std::meta::info Function, class List>
struct collect_body_types<Function, List, meta::parameter_count<Function>()> {
    using type = List;
};

template <std::meta::info Function, class List>
struct collect_endpoint_types {
    using with_body = typename collect_body_types<Function, List>::type;
    using type = typename collect_type<function_return_t<Function>, with_body>::type;
};

template <std::meta::info Namespace, class List, std::size_t Index = 0>
struct collect_api_types {
    static constexpr auto function = meta::endpoint_at<Namespace, Index>();
    using with_endpoint = typename collect_endpoint_types<function, List>::type;
    using type = typename collect_api_types<Namespace, with_endpoint, Index + 1>::type;
};

template <std::meta::info Namespace, class List>
struct collect_api_types<Namespace, List, meta::endpoint_count_v<Namespace>> {
    using type = List;
};

template <class Object>
inline constexpr std::size_t schema_name_count =
    std::meta::annotations_of_with_type(^^Object, ^^schema_name_annotation).size();

template <class Object>
[[nodiscard]] consteval flash::detail::fixed_string component_name_value() {
    static_assert(schema_name_count<Object> <= 1,
                  "FLASH-E600: a JSON schema type may have at most one schema name");
    if constexpr (schema_name_count<Object> == 1) {
        return std::meta::extract<schema_name_annotation>(
                   std::meta::annotations_of_with_type(
                       ^^Object, ^^schema_name_annotation)[0])
            .value;
    } else {
        constexpr auto display = std::meta::display_string_of(^^Object);
        static_assert(display.size() < flash::detail::annotation_text_capacity,
                      "FLASH-E601: a generated schema name is too long");
        flash::detail::fixed_string result{};
        for (const char character : display) {
            const bool alpha_numeric =
                (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') || character == '_';
            result.characters[result.length++] = alpha_numeric ? character : '_';
        }
        return result;
    }
}

template <class Object>
inline constexpr flash::detail::fixed_string component_name_storage =
    component_name_value<Object>();

template <class Object>
[[nodiscard]] constexpr std::string_view component_name() noexcept {
    return component_name_storage<Object>.view();
}

template <class List, std::size_t Left, std::size_t Right>
struct component_names_unique;

template <class... Types, std::size_t Left, std::size_t Right>
struct component_names_unique<type_list<Types...>, Left, Right> {
    using list_type = type_list<Types...>;
    static constexpr std::size_t size = sizeof...(Types);

    template <std::size_t Index>
    using at = std::tuple_element_t<Index, std::tuple<Types...>>;

    static consteval bool check() {
        if constexpr (Left >= size) {
            return true;
        } else if constexpr (Right >= size) {
            return component_names_unique<list_type, Left + 1, Left + 2>::check();
        } else if constexpr (component_name_storage<at<Left>> ==
                             component_name_storage<at<Right>>) {
            return false;
        } else {
            return component_names_unique<list_type, Left, Right + 1>::check();
        }
    }
};

template <class List>
consteval void validate_component_names() {
    static_assert(component_names_unique<List, 0, 1>::check(),
                  "FLASH-E602: generated OpenAPI component names must be unique");
}

template <class Value>
void append_schema(std::string& output);

template <class Enum, std::size_t Index = 0>
void append_enum_values(std::string& output) {
    constexpr auto count = std::meta::enumerators_of(^^Enum).size();
    if constexpr (Index < count) {
        if constexpr (Index != 0) {
            output.push_back(',');
        }
        json::detail::append_escaped_string(
            output,
            std::meta::identifier_of(std::meta::enumerators_of(^^Enum)[Index]));
        append_enum_values<Enum, Index + 1>(output);
    }
}

template <class Object, std::size_t Field, std::size_t Constraint = 0>
void append_constraints(std::string& output) {
    if constexpr (Constraint < json::field_constraint_count<Object, Field>) {
        constexpr auto rule = json::field_constraint<Object, Field, Constraint>();
        switch (rule.kind) {
        case constraint_kind::minimum:
            output.append(",\"minimum\":");
            json::append(output, rule.numeric_value);
            break;
        case constraint_kind::exclusive_minimum:
            output.append(",\"exclusiveMinimum\":");
            json::append(output, rule.numeric_value);
            break;
        case constraint_kind::maximum:
            output.append(",\"maximum\":");
            json::append(output, rule.numeric_value);
            break;
        case constraint_kind::exclusive_maximum:
            output.append(",\"exclusiveMaximum\":");
            json::append(output, rule.numeric_value);
            break;
        case constraint_kind::min_length:
            output.append(",\"minLength\":");
            json::append(output, rule.size_value);
            break;
        case constraint_kind::max_length:
            output.append(",\"maxLength\":");
            json::append(output, rule.size_value);
            break;
        case constraint_kind::min_items:
            output.append(",\"minItems\":");
            json::append(output, rule.size_value);
            break;
        case constraint_kind::max_items:
            output.append(",\"maxItems\":");
            json::append(output, rule.size_value);
            break;
        }
        append_constraints<Object, Field, Constraint + 1>(output);
    }
}

template <class Object, std::size_t Field>
inline constexpr std::size_t field_description_count =
    std::meta::annotations_of_with_type(
        json::field_reflection<Object, Field>, ^^description_annotation)
        .size();

template <class Object, std::size_t Field>
[[nodiscard]] consteval description_annotation field_description_value() {
    static_assert(field_description_count<Object, Field> <= 1,
                  "FLASH-E604: an OpenAPI declaration may have at most one description");
    if constexpr (field_description_count<Object, Field> == 1) {
        return std::meta::extract<description_annotation>(
            std::meta::annotations_of_with_type(
                json::field_reflection<Object, Field>, ^^description_annotation)[0]);
    }
    return {};
}

template <class Object, std::size_t Field>
inline constexpr description_annotation field_description_storage =
    field_description_value<Object, Field>();

template <class Object, std::size_t Index = 0>
void append_properties(std::string& output) {
    if constexpr (Index < json::field_count<Object>) {
        if constexpr (Index != 0) {
            output.push_back(',');
        }
        json::detail::append_escaped_string(
            output, json::field_wire_name<Object, Index>());
        output.append(":{");
        using field_type = json::field_type_t<Object, Index>;
        append_schema<field_type>(output);
        append_constraints<Object, Index>(output);
        if constexpr (json::field_default_count<Object, Index> == 1) {
            output.append(",\"default\":");
            json::append(output, json::field_default_value<Object, Index>());
        }
        if constexpr (field_description_count<Object, Index> == 1) {
            output.append(",\"description\":");
            json::detail::append_escaped_string(
                output, field_description_storage<Object, Index>.value.view());
        }
        if constexpr (std::meta::annotations_of_with_type(
                          json::field_reflection<Object, Index>,
                          ^^deprecated_annotation)
                          .size() != 0) {
            output.append(",\"deprecated\":true");
        }
        output.push_back('}');
        append_properties<Object, Index + 1>(output);
    }
}

template <class Object, std::size_t Index = 0>
void append_required(std::string& output, bool& first) {
    if constexpr (Index < json::field_count<Object>) {
        using field_type = json::field_type_t<Object, Index>;
        if constexpr (!binding::detail::is_optional_v<field_type> &&
                      json::field_default_count<Object, Index> == 0) {
            if (!first) {
                output.push_back(',');
            }
            first = false;
            json::detail::append_escaped_string(
                output, json::field_wire_name<Object, Index>());
        }
        append_required<Object, Index + 1>(output, first);
    }
}

template <class Value>
void append_schema(std::string& output) {
    using value_type = std::remove_cv_t<Value>;
    if constexpr (binding::detail::is_optional_v<value_type>) {
        output.append("\"anyOf\":[{");
        append_schema<binding::detail::optional_value_t<value_type>>(output);
        output.append("},{\"type\":\"null\"}]");
    } else if constexpr (std::same_as<value_type, std::string> ||
                         std::same_as<value_type, std::string_view>) {
        output.append("\"type\":\"string\"");
    } else if constexpr (std::same_as<value_type, bool>) {
        output.append("\"type\":\"boolean\"");
    } else if constexpr (std::integral<value_type>) {
        output.append("\"type\":\"integer\"");
        if constexpr (sizeof(value_type) <= 4) {
            output.append(",\"format\":\"int32\"");
        } else {
            output.append(",\"format\":\"int64\"");
        }
    } else if constexpr (std::floating_point<value_type>) {
        output.append("\"type\":\"number\"");
        output.append(sizeof(value_type) <= 4 ? ",\"format\":\"float\""
                                              : ",\"format\":\"double\"");
    } else if constexpr (std::is_enum_v<value_type>) {
        output.append("\"type\":\"string\",\"enum\":[");
        append_enum_values<value_type>(output);
        output.push_back(']');
    } else if constexpr (json::detail::is_vector_v<value_type>) {
        output.append("\"type\":\"array\",\"items\":{");
        append_schema<typename json::detail::vector_traits<value_type>::element_type>(output);
        output.push_back('}');
    } else if constexpr (json::detail::is_array_v<value_type>) {
        constexpr auto size = json::detail::array_traits<value_type>::size;
        output.append("\"type\":\"array\",\"items\":{");
        append_schema<typename json::detail::array_traits<value_type>::element_type>(output);
        output.append("},\"minItems\":");
        json::append(output, size);
        output.append(",\"maxItems\":");
        json::append(output, size);
    } else if constexpr (json::reflectable_object<value_type>) {
        output.append("\"$ref\":\"#/components/schemas/");
        output.append(component_name<value_type>());
        output.push_back('"');
    } else {
        static_assert(std::is_void_v<value_type>,
                      "FLASH-E603: no OpenAPI schema exists for this C++ type");
    }
}

template <class Object>
void append_component(std::string& output) {
    static_assert(json::output_schema_validated<Object>);
    output.append("{\"type\":\"object\",\"additionalProperties\":false,\"properties\":{");
    append_properties<Object>(output);
    output.append("},\"required\":[");
    bool first = true;
    append_required<Object>(output, first);
    output.append("]}");
}

template <class... Types>
void append_components(std::string& output, type_list<Types...>) {
    if constexpr (sizeof...(Types) > 0) {
        bool first = true;
        const auto append_one = [&]<class Object>() {
            if (!first) {
                output.push_back(',');
            }
            first = false;
            json::detail::append_escaped_string(output, component_name<Object>());
            output.push_back(':');
            append_component<Object>(output);
        };
        (append_one.template operator()<Types>(), ...);
    } else {
        static_cast<void>(output);
    }
}

} // namespace detail

template <std::meta::info Namespace>
using api_schema_types =
    typename detail::collect_api_types<Namespace, type_list<>>::type;

template <std::meta::info Namespace>
inline constexpr bool component_names_validated = [] consteval {
    detail::validate_component_names<api_schema_types<Namespace>>();
    return true;
}();

template <class Object>
[[nodiscard]] std::string component_schema() {
    std::string output;
    detail::append_component<Object>(output);
    return output;
}

} // namespace flash::openapi
