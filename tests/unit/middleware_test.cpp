#include <flash/flash.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <array>
#include <future>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct LogRecord {
    std::string target;
    flash::status status_code;
    std::string request_id;
};

flash::response_message run(flash::task<flash::response_message> operation) {
    boost::asio::io_context context;
    auto result = boost::asio::co_spawn(
        context, std::move(operation), boost::asio::use_future);
    context.run();
    return result.get();
}

std::string_view response_header(
    const flash::response_message& response, std::string_view name) {
    for (const auto& header : response.headers) {
        if (flash::ascii_iequals(header.name, name)) {
            return header.value;
        }
    }
    return {};
}

} // namespace

int main() {
    std::vector<LogRecord> logs;
    using App = flash::application<
        flash::middleware::request_id,
        flash::middleware::access_log,
        flash::middleware::recover_exceptions>;
    App app{
        flash::middleware::request_id{},
        flash::middleware::access_log{
            [&logs](const flash::middleware::access_log_entry& entry) {
                logs.push_back({
                    std::string{entry.target}, entry.status_code,
                    std::string{entry.request_id}});
            }},
        flash::middleware::recover_exceptions{}};

    constexpr std::array client_headers{
        flash::header_view{"X-Request-ID", "client-7"},
    };
    flash::request_context successful{
        .request = {flash::http_method::get, "/ok", client_headers, {}, false},
        .request_id = {},
    };
    auto success = [&successful]() -> flash::task<flash::response_message> {
        flash::response_message response;
        response.body = successful.request_id;
        co_return response;
    };
    const auto success_response = run(app(successful, success));
    if (success_response.body != "client-7" ||
        response_header(success_response, "X-Request-ID") != "client-7" ||
        logs.size() != 1 || logs[0].target != "/ok" ||
        logs[0].status_code != flash::status::ok ||
        logs[0].request_id != "client-7") {
        return 1;
    }

    constexpr std::array duplicate_headers{
        flash::header_view{"X-Request-ID", "first"},
        flash::header_view{"x-request-id", "second"},
    };
    flash::request_context duplicate{
        .request = {flash::http_method::get, "/duplicate", duplicate_headers, {}, false},
        .request_id = {},
    };
    auto duplicate_final = []() -> flash::task<flash::response_message> {
        co_return flash::response_message{};
    };
    const auto duplicate_response = run(app(duplicate, duplicate_final));
    const auto generated = response_header(duplicate_response, "X-Request-ID");
    if (generated.size() != 32 || generated == "first" || generated == "second" ||
        duplicate.request_id != generated) {
        return 2;
    }

    flash::request_context failed{
        .request = {flash::http_method::post, "/failure", {}, {}, false},
        .request_id = {},
    };
    auto failure = []() -> flash::task<flash::response_message> {
        throw std::runtime_error{"private detail"};
        co_return flash::response_message{};
    };
    const auto failure_response = run(app(failed, failure));
    if (failure_response.status_code != flash::status::internal_server_error ||
        failure_response.content_type != "application/problem+json" ||
        response_header(failure_response, "X-Request-ID") != failed.request_id ||
        failure_response.body.find(failed.request_id) == std::string::npos ||
        failure_response.body.find("private detail") != std::string::npos ||
        logs.size() != 3 ||
        logs.back().status_code != flash::status::internal_server_error) {
        return 3;
    }

    flash::application<flash::middleware::access_log> failing_sink{
        flash::middleware::access_log{
            [](const flash::middleware::access_log_entry&) {
                throw std::runtime_error{"log failure"};
            }}};
    flash::request_context sink_context{
        .request = {flash::http_method::get, "/sink", {}, {}, false},
        .request_id = {},
    };
    auto sink_final = []() -> flash::task<flash::response_message> {
        co_return flash::response_message{};
    };
    return run(failing_sink(sink_context, sink_final)).status_code == flash::status::ok
               ? 0
               : 4;
}
