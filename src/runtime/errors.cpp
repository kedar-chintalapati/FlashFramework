#include <flash/problem.hpp>

#include <charconv>
#include <string>
#include <string_view>

namespace flash {
namespace {

void append_json_string(std::string& output, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        switch (character) {
        case '"': output.append("\\\""); break;
        case '\\': output.append("\\\\"); break;
        case '\b': output.append("\\b"); break;
        case '\f': output.append("\\f"); break;
        case '\n': output.append("\\n"); break;
        case '\r': output.append("\\r"); break;
        case '\t': output.append("\\t"); break;
        default:
            if (character < 0x20U) {
                output.append("\\u00");
                output.push_back(hex[character >> 4U]);
                output.push_back(hex[character & 0x0fU]);
            } else {
                output.push_back(static_cast<char>(character));
            }
        }
    }
    output.push_back('"');
}

void append_member(std::string& output,
                   std::string_view name,
                   std::string_view value,
                   bool& first) {
    if (!first) {
        output.push_back(',');
    }
    first = false;
    append_json_string(output, name);
    output.push_back(':');
    append_json_string(output, value);
}

} // namespace

std::string serialize_problem(const problem_detail& problem) {
    std::string output;
    output.reserve(256);
    output.push_back('{');
    bool first = true;
    append_member(output, "type", problem.type, first);
    append_member(output, "title", problem.title, first);

    output.append(",\"status\":");
    char number[8]{};
    const auto numeric_status = static_cast<unsigned>(problem.status_code);
    const auto result = std::to_chars(number, number + sizeof(number), numeric_status);
    output.append(number, result.ptr);

    if (!problem.detail.empty()) {
        append_member(output, "detail", problem.detail, first);
    }
    if (!problem.instance.empty()) {
        append_member(output, "instance", problem.instance, first);
    }
    if (!problem.errors.empty()) {
        output.append(",\"errors\":[");
        bool first_error = true;
        for (const auto& error : problem.errors) {
            if (!first_error) {
                output.push_back(',');
            }
            first_error = false;
            output.append("{\"location\":[");
            bool first_location = true;
            for (const auto& item : error.location) {
                if (!first_location) {
                    output.push_back(',');
                }
                first_location = false;
                append_json_string(output, item);
            }
            output.append("],\"code\":");
            append_json_string(output, error.code);
            output.append(",\"message\":");
            append_json_string(output, error.message);
            output.push_back('}');
        }
        output.push_back(']');
    }
    if (!problem.request_id.empty()) {
        append_member(output, "request_id", problem.request_id, first);
    }
    output.push_back('}');
    return output;
}

response_message make_problem_response(problem_detail problem) {
    response_message response;
    response.status_code = problem.status_code;
    response.content_type = "application/problem+json";
    response.body = serialize_problem(problem);
    return response;
}

} // namespace flash
