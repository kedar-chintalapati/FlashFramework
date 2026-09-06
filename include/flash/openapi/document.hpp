#pragma once

#include <flash/openapi/schema.hpp>
#include <flash/problem.hpp>
#include <flash/routing/matcher.hpp>

#include <charconv>
#include <cstddef>
#include <expected>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

namespace flash::openapi {
namespace detail {

[[nodiscard]] constexpr std::string_view method_name(http_method method) noexcept {
    switch (method) {
    case http_method::get: return "get";
    case http_method::post: return "post";
    case http_method::put: return "put";
    case http_method::patch: return "patch";
    case http_method::delete_: return "delete";
    case http_method::head: return "head";
    case http_method::options: return "options";
    }
    return {};
}

inline void append_status(std::string& output, status value) {
    char buffer[8]{};
    const auto result = std::to_chars(
        buffer, buffer + sizeof(buffer), static_cast<unsigned>(value));
    output.append(buffer, result.ptr);
}

inline void append_openapi_path(std::string& output, std::string_view route) {
    std::string normalized;
    normalized.reserve(route.size());
    for (std::size_t index = 0; index < route.size(); ++index) {
        if (route[index] == '{' && index + 1 < route.size() && route[index + 1] == '*') {
            normalized.push_back('{');
            ++index;
        } else {
            normalized.push_back(route[index]);
        }
    }
    json::detail::append_escaped_string(output, normalized);
}

template <std::meta::info Declaration>
inline constexpr std::size_t description_count =
    std::meta::annotations_of_with_type(Declaration, ^^description_annotation).size();

template <std::meta::info Declaration>
[[nodiscard]] consteval description_annotation description_value() {
    static_assert(description_count<Declaration> <= 1,
                  "FLASH-E604: an OpenAPI declaration may have at most one description");
    if constexpr (description_count<Declaration> == 1) {
        return std::meta::extract<description_annotation>(
            std::meta::annotations_of_with_type(
                Declaration, ^^description_annotation)[0]);
    }
    return {};
}

template <std::meta::info Declaration>
inline constexpr description_annotation description_storage =
    description_value<Declaration>();

template <std::meta::info Function, std::size_t Left, std::size_t Right>
[[nodiscard]] consteval bool operation_ids_unique() {
    constexpr auto count = meta::endpoint_count_v<Function>;
    if constexpr (Left >= count) {
        return true;
    } else if constexpr (Right >= count) {
        return operation_ids_unique<Function, Left + 1, Left + 2>();
    } else if constexpr (
        std::meta::identifier_of(meta::endpoint_at<Function, Left>()) ==
        std::meta::identifier_of(meta::endpoint_at<Function, Right>())) {
        return false;
    } else {
        return operation_ids_unique<Function, Left, Right + 1>();
    }
}

template <std::meta::info Namespace>
consteval void validate_document() {
    routing::validate_routes<Namespace>();
    validate_component_names<api_schema_types<Namespace>>();
    static_assert(operation_ids_unique<Namespace, 0, 1>(),
                  "FLASH-E605: generated OpenAPI operation identifiers must be unique");
}

template <std::meta::info Function,
          std::size_t ParameterIndex,
          std::size_t ConstraintIndex = 0>
void append_parameter_constraint_values(std::string& output) {
    if constexpr (ConstraintIndex <
                  binding::parameter_constraint_count<Function, ParameterIndex>) {
        constexpr auto rule =
            binding::parameter_constraint<Function, ParameterIndex, ConstraintIndex>();
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
        append_parameter_constraint_values<
            Function, ParameterIndex, ConstraintIndex + 1>(output);
    }
}

template <std::meta::info Function, std::size_t Index = 0>
void append_parameters(std::string& output, bool& first) {
    if constexpr (Index < meta::parameter_count<Function>()) {
        constexpr auto source = binding::parameter_source<Function, Index>();
        if constexpr (source == source_kind::path || source == source_kind::query ||
                      source == source_kind::header || source == source_kind::cookie) {
            if (!first) {
                output.push_back(',');
            }
            first = false;
            output.append("{\"name\":");
            json::detail::append_escaped_string(
                output, binding::wire_name<Function, Index>());
            output.append(",\"in\":");
            if constexpr (source == source_kind::path) {
                json::detail::append_escaped_string(output, "path");
            } else if constexpr (source == source_kind::query) {
                json::detail::append_escaped_string(output, "query");
            } else if constexpr (source == source_kind::header) {
                json::detail::append_escaped_string(output, "header");
            } else {
                json::detail::append_escaped_string(output, "cookie");
            }
            constexpr auto parameter = meta::parameter_at<Function, Index>();
            if constexpr (description_count<parameter> == 1) {
                output.append(",\"description\":");
                json::detail::append_escaped_string(
                    output, description_storage<parameter>.value.view());
            }
            if constexpr (std::meta::annotations_of_with_type(
                              parameter, ^^deprecated_annotation)
                              .size() != 0) {
                output.append(",\"deprecated\":true");
            }
            using parameter_type = binding::parameter_storage_t<Function, Index>;
            constexpr bool required = source == source_kind::path ||
                                      (!binding::detail::is_optional_v<parameter_type> &&
                                       binding::default_annotation_count<Function, Index> == 0);
            output.append(required ? ",\"required\":true,\"schema\":{"
                                   : ",\"required\":false,\"schema\":{");
            append_schema<parameter_type>(output);
            append_parameter_constraint_values<Function, Index>(output);
            if constexpr (binding::default_annotation_count<Function, Index> == 1) {
                output.append(",\"default\":");
                json::append(
                    output,
                    std::meta::extract<default_value_annotation<parameter_type>>(
                        std::meta::annotations_of_with_type(
                            parameter,
                            ^^default_value_annotation<parameter_type>)[0])
                        .value);
            }
            output.append("}}");
        }
        append_parameters<Function, Index + 1>(output, first);
    }
}

template <std::meta::info Function, std::size_t Index = 0>
void append_request_body(std::string& output) {
    if constexpr (Index < meta::parameter_count<Function>()) {
        if constexpr (binding::parameter_source<Function, Index>() == source_kind::body) {
            using body_type = binding::parameter_storage_t<Function, Index>;
            output.append(",\"requestBody\":{\"required\":true,\"content\":{"
                          "\"application/json\":{\"schema\":{");
            append_schema<body_type>(output);
            output.append("}}}}");
        } else {
            append_request_body<Function, Index + 1>(output);
        }
    }
}

template <class>
struct expected_response_traits {
    static constexpr bool value = false;
};

template <class Result, class Error>
struct expected_response_traits<std::expected<Result, Error>> {
    static constexpr bool value = true;
    using result_type = Result;
    using error_type = Error;
};

template <class>
struct created_response_traits {
    static constexpr bool value = false;
};

template <class Body>
struct created_response_traits<created<Body>> {
    static constexpr bool value = true;
    using body_type = Body;
};

template <class>
struct explicit_response_traits {
    static constexpr bool value = false;
};

template <class Body>
struct explicit_response_traits<response<Body>> {
    static constexpr bool value = true;
    using body_type = Body;
};

template <class>
struct awaitable_response_traits {
    static constexpr bool value = false;
};

template <class Value, class Executor>
struct awaitable_response_traits<boost::asio::awaitable<Value, Executor>> {
    static constexpr bool value = true;
    using value_type = Value;
};

template <class Value>
[[nodiscard]] consteval status success_status() {
    using value_type = std::remove_cvref_t<Value>;
    if constexpr (awaitable_response_traits<value_type>::value) {
        return success_status<typename awaitable_response_traits<value_type>::value_type>();
    } else if constexpr (expected_response_traits<value_type>::value) {
        return success_status<typename expected_response_traits<value_type>::result_type>();
    } else if constexpr (created_response_traits<value_type>::value) {
        return status::created;
    } else if constexpr (std::is_void_v<value_type>) {
        return status::no_content;
    } else {
        return status::ok;
    }
}

template <class Value>
void append_success_response(std::string& output) {
    using value_type = std::remove_cvref_t<Value>;
    if constexpr (awaitable_response_traits<value_type>::value) {
        append_success_response<typename awaitable_response_traits<value_type>::value_type>(output);
    } else if constexpr (expected_response_traits<value_type>::value) {
        append_success_response<typename expected_response_traits<value_type>::result_type>(output);
    } else {
        output.push_back('"');
        append_status(output, success_status<value_type>());
        output.append("\":{\"description\":\"Successful response\"");
        if constexpr (std::is_void_v<value_type>) {
            output.push_back('}');
        } else if constexpr (std::same_as<value_type, text>) {
            output.append(",\"content\":{\"text/plain\":{\"schema\":{\"type\":\"string\"}}}}");
        } else if constexpr (std::same_as<value_type, bytes>) {
            output.append(",\"content\":{\"application/octet-stream\":{\"schema\":{"
                          "\"type\":\"string\",\"format\":\"binary\"}}}}");
        } else if constexpr (std::same_as<value_type, response_message>) {
            output.push_back('}');
        } else {
            output.append(",\"content\":{\"application/json\":{\"schema\":{");
            if constexpr (created_response_traits<value_type>::value) {
                append_schema<typename created_response_traits<value_type>::body_type>(output);
            } else if constexpr (explicit_response_traits<value_type>::value) {
                append_schema<typename explicit_response_traits<value_type>::body_type>(output);
            } else {
                append_schema<value_type>(output);
            }
            output.append("}}}}");
        }
    }
}

template <class Error, std::size_t Index = 0>
void append_domain_responses(std::string& output, status success) {
    if constexpr (Index < error_map<Error>.entries.size()) {
        constexpr auto mapping = error_map<Error>.entries[Index];
        constexpr bool duplicate = [] consteval {
            for (std::size_t previous = 0; previous < Index; ++previous) {
                if (error_map<Error>.entries[previous].status_code == mapping.status_code) {
                    return true;
                }
            }
            return false;
        }();
        if (!duplicate && mapping.status_code != success) {
            output.append(",\"");
            append_status(output, mapping.status_code);
            output.append("\":{\"$ref\":\"#/components/responses/Problem\"}");
        }
        append_domain_responses<Error, Index + 1>(output, success);
    }
}

template <class Error, status Status, std::size_t Index = 0>
[[nodiscard]] consteval bool has_mapped_status() {
    if constexpr (Index == error_map<Error>.entries.size()) {
        return false;
    } else if constexpr (error_map<Error>.entries[Index].status_code == Status) {
        return true;
    } else {
        return has_mapped_status<Error, Status, Index + 1>();
    }
}

template <class Value>
void append_error_responses(std::string& output) {
    using value_type = std::remove_cvref_t<Value>;
    if constexpr (awaitable_response_traits<value_type>::value) {
        append_error_responses<typename awaitable_response_traits<value_type>::value_type>(output);
    } else if constexpr (expected_response_traits<value_type>::value) {
        using error_type = typename expected_response_traits<value_type>::error_type;
        static_assert(error_mapping_validated<error_type>);
        append_domain_responses<error_type>(output, success_status<value_type>());
        if constexpr (!has_mapped_status<error_type, status::unprocessable_content>() &&
                      success_status<value_type>() != status::unprocessable_content) {
            output.append(",\"422\":{\"$ref\":\"#/components/responses/Problem\"}");
        }
    } else if constexpr (success_status<value_type>() != status::unprocessable_content) {
        output.append(",\"422\":{\"$ref\":\"#/components/responses/Problem\"}");
    }
}

template <std::meta::info Function>
void append_operation(std::string& output) {
    static_assert(binding::endpoint_binding_validated<Function>);
    output.append("{\"operationId\":");
    json::detail::append_escaped_string(output, std::meta::identifier_of(Function));
    if constexpr (description_count<Function> == 1) {
        output.append(",\"description\":");
        json::detail::append_escaped_string(
            output, description_storage<Function>.value.view());
    }
    if constexpr (std::meta::annotations_of_with_type(
                      Function, ^^deprecated_annotation).size() != 0) {
        output.append(",\"deprecated\":true");
    }
    output.append(",\"parameters\":[");
    bool first = true;
    append_parameters<Function>(output, first);
    output.push_back(']');
    append_request_body<Function>(output);
    output.append(",\"responses\":{");
    using return_type = function_return_t<Function>;
    append_success_response<return_type>(output);
    append_error_responses<return_type>(output);
    output.append("}}");
}

template <std::meta::info Namespace, std::size_t PathIndex, std::size_t Index = 0>
void append_path_operations(std::string& output, bool& first) {
    if constexpr (Index < meta::endpoint_count_v<Namespace>) {
        constexpr auto path_function = meta::endpoint_at<Namespace, PathIndex>();
        constexpr auto function = meta::endpoint_at<Namespace, Index>();
        if constexpr (meta::route_of<path_function>().path ==
                      meta::route_of<function>().path) {
            if (!first) {
                output.push_back(',');
            }
            first = false;
            json::detail::append_escaped_string(
                output, method_name(meta::route_of<function>().method));
            output.push_back(':');
            append_operation<function>(output);
        }
        append_path_operations<Namespace, PathIndex, Index + 1>(output, first);
    }
}

template <std::meta::info Namespace, std::size_t Index, std::size_t Previous = 0>
[[nodiscard]] consteval bool first_path_occurrence() {
    if constexpr (Previous == Index) {
        return true;
    } else {
        constexpr auto current = meta::endpoint_at<Namespace, Index>();
        constexpr auto earlier = meta::endpoint_at<Namespace, Previous>();
        if constexpr (meta::route_of<current>().path == meta::route_of<earlier>().path) {
            return false;
        }
        return first_path_occurrence<Namespace, Index, Previous + 1>();
    }
}

template <std::meta::info Namespace, std::size_t Index = 0>
void append_paths(std::string& output, bool& first) {
    if constexpr (Index < meta::endpoint_count_v<Namespace>) {
        if constexpr (first_path_occurrence<Namespace, Index>()) {
            if (!first) {
                output.push_back(',');
            }
            first = false;
            constexpr auto function = meta::endpoint_at<Namespace, Index>();
            append_openapi_path(output, meta::route_of<function>().path.view());
            output.append(":{");
            bool first_operation = true;
            append_path_operations<Namespace, Index>(output, first_operation);
            output.push_back('}');
        }
        append_paths<Namespace, Index + 1>(output, first);
    }
}

inline void append_problem_components(std::string& output) {
    output.append(
        "\"Problem\":{\"type\":\"object\",\"required\":[\"type\",\"title\",\"status\"],"
        "\"properties\":{\"type\":{\"type\":\"string\",\"format\":\"uri-reference\"},"
        "\"title\":{\"type\":\"string\"},\"status\":{\"type\":\"integer\"},"
        "\"detail\":{\"type\":\"string\"},\"instance\":{\"type\":\"string\"},"
        "\"errors\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}}}}},"
        "\"responses\":{\"Problem\":{\"description\":\"Request failed\","
        "\"content\":{\"application/problem+json\":{\"schema\":{"
        "\"$ref\":\"#/components/schemas/Problem\"}}}}}");
}

} // namespace detail

template <std::meta::info Namespace>
[[nodiscard]] std::string generate_document(std::string_view title = "Flash API",
                                            std::string_view version = "0.1.0") {
    detail::validate_document<Namespace>();
    std::string output;
    output.reserve(4096);
    output.append("{\"openapi\":\"3.1.1\",\"info\":{\"title\":");
    json::detail::append_escaped_string(output, title);
    output.append(",\"version\":");
    json::detail::append_escaped_string(output, version);
    output.append("},\"paths\":{");
    bool first = true;
    detail::append_paths<Namespace>(output, first);
    output.append("},\"components\":{\"schemas\":{");
    detail::append_components(output, api_schema_types<Namespace>{});
    if constexpr (type_list_size_v<api_schema_types<Namespace>> != 0) {
        output.push_back(',');
    }
    detail::append_problem_components(output);
    output.append("}}");
    return output;
}

template <std::meta::info Namespace>
[[nodiscard]] const std::string& document() {
    static const std::string value = generate_document<Namespace>();
    return value;
}

} // namespace flash::openapi
