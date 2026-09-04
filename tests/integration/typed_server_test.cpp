#include <flash/flash.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <cstdint>
#include <string>

struct CreateItem {
    std::string name;
    [[=flash::minimum(1)]] std::uint32_t quantity{};
};

struct StoredItem {
    std::string name;
    std::uint32_t quantity{};
};

struct Services {
    int base{40};
    int uses{};
};

namespace typed_server_api {

[[=flash::get("/health")]]
flash::text health() {
    return {"ok"};
}

[[=flash::get("/add/{a}/{b}")]]
int add(int a, int b) {
    return a + b;
}

[[=flash::delete_("/items/{id}")]]
void erase(std::uint64_t id) {
    (void)id;
}

[[=flash::post("/items")]]
StoredItem create(CreateItem item) {
    return {std::move(item.name), item.quantity};
}

[[=flash::get("/async-state/{value}")]]
flash::task<int> async_state_value(int value, flash::state<Services>& services) {
    const auto executor = co_await boost::asio::this_coro::executor;
    co_await boost::asio::post(executor, boost::asio::use_awaitable);
    ++services->uses;
    co_return services->base + value;
}

} // namespace typed_server_api

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

int main() {
    flash::server_config config;
    config.port = 0;
    config.workers = 1;
    config.header_timeout = std::chrono::seconds{2};
    config.body_timeout = std::chrono::seconds{2};
    config.write_timeout = std::chrono::seconds{2};

    Services services;
    flash::raw_server server{
        config,
        flash::reflected_handler<
            ^^typed_server_api,
            flash::openapi::documentation_mode::development,
            Services>{services}};
    server.start();

    asio::io_context client_context;
    tcp::resolver resolver{client_context};
    beast::tcp_stream stream{client_context};
    stream.connect(resolver.resolve("127.0.0.1", std::to_string(server.port())));
    beast::flat_buffer buffer;

    const auto exchange = [&stream, &buffer](http::verb method,
                                             std::string target,
                                             bool keep_alive,
                                             std::string body = {},
                                             std::string content_type = {}) {
        http::request<http::string_body> request{method, std::move(target), 11};
        request.set(http::field::host, "127.0.0.1");
        request.body() = std::move(body);
        if (!content_type.empty()) {
            request.set(http::field::content_type, content_type);
        }
        request.prepare_payload();
        request.keep_alive(keep_alive);
        http::write(stream, request);
        http::response<http::string_body> response;
        http::read(stream, buffer, response);
        return response;
    };

    const auto health = exchange(http::verb::get, "/health", true);
    const auto added = exchange(http::verb::get, "/add/19/23", true);
    const auto invalid = exchange(http::verb::get, "/add/nope/1", true);
    const auto created = exchange(
        http::verb::post, "/items", true,
        R"({"name":"widget","quantity":2})", "application/json");
    const auto invalid_body = exchange(
        http::verb::post, "/items", true,
        R"({"name":"widget","quantity":0})", "application/json");
    const auto openapi = exchange(http::verb::get, "/openapi.json", true);
    const auto docs = exchange(http::verb::get, "/docs", true);
    const auto state = exchange(http::verb::get, "/async-state/2", true);
    const auto erased = exchange(http::verb::delete_, "/items/7", false);

    server.stop();
    server.wait();

    if (health.result() != http::status::ok || health.body() != "ok" ||
        health[http::field::content_type] != "text/plain; charset=utf-8") {
        return 1;
    }
    if (added.result() != http::status::ok || added.body() != "42" ||
        added[http::field::content_type] != "application/json") {
        return 2;
    }
    if (invalid.result_int() != 422 ||
        invalid[http::field::content_type] != "application/problem+json" ||
        invalid.body().find("invalid_integer") == std::string::npos) {
        return 3;
    }
    if (created.result() != http::status::ok ||
        created.body() != R"({"name":"widget","quantity":2})" ||
        created[http::field::content_type] != "application/json") {
        return 4;
    }
    if (invalid_body.result_int() != 422 ||
        invalid_body.body().find("constraint_failed") == std::string::npos) {
        return 5;
    }
    if (openapi.result() != http::status::ok ||
        openapi[http::field::content_type] != "application/json" ||
        openapi.body().find("\"openapi\":\"3.1.1\"") == std::string::npos ||
        openapi.body().find("\"/items\"") == std::string::npos) {
        return 6;
    }
    if (docs.result() != http::status::ok ||
        docs[http::field::content_type] != "text/html; charset=utf-8" ||
        docs.body().find("/openapi.json") == std::string::npos) {
        return 7;
    }
    if (erased.result() != http::status::no_content || !erased.body().empty()) {
        return 8;
    }
    if (state.result() != http::status::ok || state.body() != "42" ||
        services.uses != 1) {
        return 9;
    }
    return 0;
}
