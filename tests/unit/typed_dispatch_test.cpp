#include <flash/flash.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_future.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <future>
#include <optional>
#include <string>

enum class display_color {
    red,
    green,
    blue,
};

enum class order_error {
    missing,
    conflict,
};

template <>
inline constexpr auto flash::error_map<order_error> = flash::errors(
    flash::map<order_error::missing>(
        flash::status::not_found, "order_missing", "Order not found"),
    flash::map<order_error::conflict>(
        flash::status::conflict, "order_conflict", "Order conflict"));

struct CreateOrder {
    std::string sku;
    [[=flash::minimum(1)]] std::uint32_t quantity{};
};

struct OrderReceipt {
    std::string sku;
    std::uint32_t accepted{};
};

struct Services {
    int offset{40};
    int uses{};
};

namespace typed_api {

[[=flash::get("/add/{a}/{b}")]]
int add(int a, int b) {
    return a + b;
}

[[=flash::get("/search")]]
flash::text search([[=flash::min_length(2)]] std::string q,
                   [[=flash::minimum(1)]] std::optional<int> limit) {
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

[[=flash::post("/optional-order")]]
int optional_order([[=flash::body]] std::optional<CreateOrder> order) {
    return order ? static_cast<int>(order->quantity) : -1;
}

[[=flash::put("/orders/{id}")]]
OrderReceipt replace_order(std::uint32_t id, CreateOrder order) {
    return {std::move(order.sku), order.quantity + id};
}

[[=flash::patch("/orders/{id}")]]
OrderReceipt patch_order(std::uint32_t id, CreateOrder order) {
    return {std::move(order.sku), order.quantity + id + 1U};
}

[[=flash::options("/manual-options")]]
void manual_options() {}

[[=flash::get("/orders/{id}")]]
std::expected<OrderReceipt, order_error> get_order(std::uint32_t id) {
    if (id == 0) {
        return std::unexpected{order_error::missing};
    }
    return OrderReceipt{"part-42", id};
}

} // namespace typed_api

namespace state_api {

[[=flash::get("/state/{value}")]]
int state_value(int value, flash::state<Services>& services) {
    ++services->uses;
    return services->offset + value;
}

[[=flash::get("/async-state/{value}")]]
flash::task<std::string> async_state_value(
    int value,
    flash::state<Services>& services,
    flash::request_context& context) {
    const auto target = context.request.target();
    const auto executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::post(executor, boost::asio::use_awaitable);
    ++services->uses;
    co_return std::to_string(services->offset + value) + ":" + std::string{target};
}

} // namespace state_api

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

flash::response_message state_request(
    flash::http_method method,
    std::string_view target,
    Services& services) {
    return run(flash::dispatch<^^state_api>(
        flash::request_view{method, target, {}, {}, false}, services));
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
    const auto constrained = request(flash::http_method::get, "/search?q=x");
    if (constrained.status_code != flash::status::unprocessable_content ||
        constrained.body.find("constraint_failed") == std::string::npos) {
        return 8;
    }
    const auto constrained_optional =
        request(flash::http_method::get, "/search?q=test&limit=0");
    if (constrained_optional.status_code != flash::status::unprocessable_content ||
        constrained_optional.body.find("constraint_failed") == std::string::npos) {
        return 24;
    }

    const auto deleted = request(flash::http_method::delete_, "/widgets/7");
    if (deleted.status_code != flash::status::no_content || !deleted.body.empty()) {
        return 9;
    }

    const auto wrong_method = request(flash::http_method::post, "/add/1/2");
    if (wrong_method.status_code != flash::status::method_not_allowed ||
        wrong_method.headers.empty() || wrong_method.headers[0].value != "GET, HEAD, OPTIONS") {
        return 10;
    }

    const auto options = request(flash::http_method::options, "/add/1/2");
    if (options.status_code != flash::status::no_content || options.headers.empty()) {
        return 11;
    }

    const auto explicit_options =
        request(flash::http_method::options, "/manual-options");
    if (explicit_options.status_code != flash::status::no_content) {
        return 21;
    }

    const auto head = request(flash::http_method::head, "/add/1/2");
    if (head.status_code != flash::status::ok || !head.body.empty()) {
        return 12;
    }

    constexpr std::array json_headers{
        flash::header_view{"Content-Type", "application/json; charset=utf-8"},
    };
    const auto created = request(
        flash::http_method::post, "/orders", json_headers,
        R"({"sku":"part-42","quantity":4})");
    if (created.status_code != flash::status::ok ||
        created.body != R"({"sku":"part-42","accepted":4})") {
        return 13;
    }
    const auto null_optional_body = request(
        flash::http_method::post, "/optional-order", json_headers, "null");
    if (null_optional_body.status_code != flash::status::ok ||
        null_optional_body.body != "-1") {
        return 25;
    }
    const auto empty_optional_body = request(
        flash::http_method::post, "/optional-order", json_headers);
    if (empty_optional_body.status_code != flash::status::unprocessable_content ||
        empty_optional_body.content_type != "application/problem+json") {
        return 26;
    }
    const auto replaced = request(
        flash::http_method::put, "/orders/5", json_headers,
        R"({"sku":"replacement","quantity":4})");
    if (replaced.status_code != flash::status::ok ||
        replaced.body != R"({"sku":"replacement","accepted":9})") {
        return 22;
    }
    const auto patched = request(
        flash::http_method::patch, "/orders/5", json_headers,
        R"({"sku":"patch","quantity":4})");
    if (patched.status_code != flash::status::ok ||
        patched.body != R"({"sku":"patch","accepted":10})") {
        return 23;
    }
    const auto invalid_body = request(
        flash::http_method::post, "/orders", json_headers,
        R"({"sku":"part-42","quantity":0})");
    if (invalid_body.status_code != flash::status::unprocessable_content ||
        invalid_body.body.find("constraint_failed") == std::string::npos ||
        invalid_body.body.find("$.quantity") == std::string::npos) {
        return 14;
    }
    const auto missing_content_type = request(
        flash::http_method::post, "/orders", {},
        R"({"sku":"part-42","quantity":4})");
    if (missing_content_type.status_code != flash::status::unsupported_media_type) {
        return 15;
    }
    const auto expected_success = request(
        flash::http_method::get, "/orders/3");
    if (expected_success.status_code != flash::status::ok ||
        expected_success.body != R"({"sku":"part-42","accepted":3})") {
        return 16;
    }
    const auto expected_error = request(
        flash::http_method::get, "/orders/0");
    if (expected_error.status_code != flash::status::not_found ||
        expected_error.content_type != "application/problem+json" ||
        expected_error.body.find("order_missing") == std::string::npos) {
        return 17;
    }

    Services services;
    const auto state_result = state_request(
        flash::http_method::get, "/state/2", services);
    if (state_result.status_code != flash::status::ok ||
        state_result.body != "42" || services.uses != 1) {
        return 18;
    }
    const auto async_result = state_request(
        flash::http_method::get, "/async-state/3", services);
    if (async_result.status_code != flash::status::ok ||
        async_result.body != R"("43:/async-state/3")" || services.uses != 2) {
        return 19;
    }

    return request(flash::http_method::get, "/missing").status_code ==
                   flash::status::not_found
               ? 0
               : 20;
}
