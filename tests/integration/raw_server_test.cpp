#include <flash/server.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <string>

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

    auto handler = [](flash::raw_request_view request,
                      flash::raw_response_writer& response) -> flash::task<void> {
        response.header("X-Flash-Test", "raw");
        if (request.method() == flash::http_method::get && request.path() == "/health") {
            response.content_type("application/json");
            co_await response.write("{\"status\":\"ok\"}");
            co_return;
        }
        response.status(flash::status::not_found);
        response.content_type("text/plain");
        co_await response.write("not found");
    };

    flash::raw_server server{config, handler};
    server.start();

    asio::io_context client_context;
    tcp::resolver resolver{client_context};
    beast::tcp_stream stream{client_context};
    stream.connect(resolver.resolve("127.0.0.1", std::to_string(server.port())));

    beast::flat_buffer buffer;
    http::request<http::empty_body> first{http::verb::get, "/health", 11};
    first.set(http::field::host, "127.0.0.1");
    first.keep_alive(true);
    http::write(stream, first);

    http::response<http::string_body> first_response;
    http::read(stream, buffer, first_response);
    if (first_response.result() != http::status::ok ||
        first_response.body() != "{\"status\":\"ok\"}" ||
        first_response["X-Flash-Test"] != "raw" || !first_response.keep_alive()) {
        server.stop();
        server.wait();
        return 1;
    }

    http::request<http::empty_body> second{http::verb::get, "/missing", 11};
    second.set(http::field::host, "127.0.0.1");
    second.keep_alive(false);
    http::write(stream, second);

    http::response<http::string_body> second_response;
    http::read(stream, buffer, second_response);
    if (second_response.result() != http::status::not_found ||
        second_response.body() != "not found" || second_response.keep_alive()) {
        server.stop();
        server.wait();
        return 2;
    }

    server.stop();
    server.wait();
    return 0;
}

