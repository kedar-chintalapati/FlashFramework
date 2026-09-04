#include <flash/flash.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <array>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

enum class display_color {
    red,
    green,
    blue,
};

struct CreateOrder {
    std::string sku;
    [[=flash::minimum(1)]] std::uint32_t quantity{};
};

struct OrderReceipt {
    std::string sku;
    std::uint32_t accepted{};
};

namespace typed_api {

[[=flash::get("/add/{a}/{b}")]]
int add(int a, int b) {
    return a + b;
}

[[=flash::get("/search")]]
flash::text search(std::string q, std::optional<int> limit) {
    return {q + ":" + (limit ? std::to_string(*limit) : "none")};
}

[[=flash::get("/request-header")]]
std::string request_header(
    [[=flash::header("X-Request-ID")]] std::string request_id) {
    return request_id;
}

[[=flash::get("/session")]]
std::string session(
    [[=flash::cookie("session")]] std::string value) {
    return value;
}

[[=flash::get("/color")]]
display_color color(display_color value) {
    return value;
}

[[=flash::get("/default")]]
int with_default([[=flash::default_value(10)]] int limit) {
    return limit;
}

[[=flash::delete_("/widgets/{id}")]]
void remove_widget(std::uint64_t id) {
    (void)id;
}

[[=flash::post("/orders")]]
OrderReceipt create_order(CreateOrder order) {
    return {std::move(order.sku), order.quantity};
}

} // namespace typed_api

flash::response_message run(flash::task<flash::response_message> operation) {
    boost::asio::io_context context;
    auto result = boost::asio::co_spawn(
        context, std::move(operation), boost::asio::use_future);
    context.run();
    return result.get();
}

flash::response_message request(
    flash::http_method method,
    std::string_view target,
    std::span<const flash::header_view> headers = {},
    std::string_view body = {}) {
    return run(flash::dispatch<^^typed_api>(
        flash::request_view{method, target, headers, body, false}));
}

int main() {
    const auto added = request(flash::http_method::get, "/add/20/22");
    if (added.status_code != flash::status::ok || added.body != "42") {
        return 1;
    }

    const auto searched = request(
        flash::http_method::get, "/search?q=hello+world&limit=3");
    if (searched.content_type != "text/plain; charset=utf-8" ||
        searched.body != "hello world:3") {
        return 2;
    }

    const auto optional_query = request(flash::http_method::get, "/search?q=test");
    if (optional_query.body != "test:none") {
        return 3;
    }

    constexpr std::array headers{
        flash::header_view{"X-Request-ID", "request-42"},
        flash::header_view{"Cookie", "session=abc123; theme=dark"},
    };
    if (request(flash::http_method::get, "/request-header", headers).body !=
            "\"request-42\"" ||
        request(flash::http_method::get, "/session", headers).body !=
            "\"abc123\"") {
        return 4;
    }

    if (request(flash::http_method::get, "/color?value=green").body != "\"green\"" ||
        request(flash::http_method::get, "/default").body != "10") {
        return 5;
    }

    const auto invalid = request(flash::http_method::get, "/add/nope/2");
    if (invalid.status_code != flash::status::unprocessable_content ||
        invalid.content_type != "application/problem+json" ||
        invalid.body.find("invalid_integer") == std::string::npos ||
        invalid.body.find("\"path\"") == std::string::npos) {
        return 6;
    }

    const auto missing = request(flash::http_method::get, "/search");
    if (missing.status_code != flash::status::unprocessable_content ||
        missing.body.find("missing_parameter") == std::string::npos) {
        return 7;
    }

    const auto deleted = request(flash::http_method::delete_, "/widgets/7");
    if (deleted.status_code != flash::status::no_content || !deleted.body.empty()) {
        return 8;
    }

    const auto wrong_method = request(flash::http_method::post, "/add/1/2");
    if (wrong_method.status_code != flash::status::method_not_allowed ||
        wrong_method.headers.empty() || wrong_method.headers[0].value != "GET, HEAD, OPTIONS") {
        return 9;
    }

    const auto options = request(flash::http_method::options, "/add/1/2");
    if (options.status_code != flash::status::no_content || options.headers.empty()) {
        return 10;
    }

    const auto head = request(flash::http_method::head, "/add/1/2");
    if (head.status_code != flash::status::ok || !head.body.empty()) {
        return 11;
    }

    constexpr std::array json_headers{
        flash::header_view{"Content-Type", "application/json; charset=utf-8"},
    };
    const auto created = request(
        flash::http_method::post, "/orders", json_headers,
        R"({"sku":"part-42","quantity":4})");
    if (created.status_code != flash::status::ok ||
        created.body != R"({"sku":"part-42","accepted":4})") {
        return 12;
    }
    const auto invalid_body = request(
        flash::http_method::post, "/orders", json_headers,
        R"({"sku":"part-42","quantity":0})");
    if (invalid_body.status_code != flash::status::unprocessable_content ||
        invalid_body.body.find("constraint_failed") == std::string::npos ||
        invalid_body.body.find("$.quantity") == std::string::npos) {
        return 13;
    }
    const auto missing_content_type = request(
        flash::http_method::post, "/orders", {},
        R"({"sku":"part-42","quantity":4})");
    if (missing_content_type.status_code != flash::status::unsupported_media_type) {
        return 14;
    }

    return request(flash::http_method::get, "/missing").status_code ==
                   flash::status::not_found
               ? 0
               : 15;
}
