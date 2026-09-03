#pragma once

#include <flash/response.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace flash {

struct validation_error {
    std::vector<std::string> location;
    std::string code;
    std::string message;
};

struct problem_detail {
    std::string type{"about:blank"};
    std::string title;
    status status_code{status::bad_request};
    std::string detail;
    std::string instance;
    std::vector<validation_error> errors;
    std::string request_id;
};

[[nodiscard]] std::string serialize_problem(const problem_detail& problem);
[[nodiscard]] response_message make_problem_response(problem_detail problem);

} // namespace flash

