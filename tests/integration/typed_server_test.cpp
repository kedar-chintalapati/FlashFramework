#include <flash/flash.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <cstdint>
#include <string>

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

    flash::raw_server server{
        config, flash::reflected_handler<^^typed_server_api>{}};
    server.start();

    asio::io_context client_context;
    tcp::resolver resolver{client_context};
    beast::tcp_stream stream{client_context};
    stream.connect(resolver.resolve("127.0.0.1", std::to_string(server.port())));
    beast::flat_buffer buffer;

    const auto exchange = [&stream, &buffer](http::verb method,
                                             std::string target,
                                             bool keep_alive) {
        http::request<http::empty_body> request{method, std::move(target), 11};
        request.set(http::field::host, "127.0.0.1");
        request.keep_alive(keep_alive);
        http::write(stream, request);
        http::response<http::string_body> response;
        http::read(stream, buffer, response);
        return response;
    };

    const auto health = exchange(http::verb::get, "/health", true);
    const auto added = exchange(http::verb::get, "/add/19/23", true);
    const auto invalid = exchange(http::verb::get, "/add/nope/1", true);
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
    if (erased.result() != http::status::no_content || !erased.body().empty()) {
        return 4;
    }
    return 0;
}

