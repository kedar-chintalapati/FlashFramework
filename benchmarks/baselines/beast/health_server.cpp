#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <charconv>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using asio::awaitable;

awaitable<void> session(tcp::socket socket) {
    beast::tcp_stream stream{std::move(socket)};
    beast::flat_buffer buffer;

    for (;;) {
        http::request_parser<http::empty_body> parser;
        parser.header_limit(16U * 1024U);
        parser.body_limit(1024U * 1024U);
        stream.expires_after(std::chrono::seconds{15});

        boost::system::error_code error;
        co_await http::async_read(
            stream, buffer, parser, asio::redirect_error(asio::use_awaitable, error));
        if (error == http::error::end_of_stream || error == asio::error::eof) {
            break;
        }
        if (error) {
            break;
        }

        auto request = parser.release();
        http::response<http::string_body> response{http::status::not_found, request.version()};
        response.set(http::field::server, "handwritten-beast");
        response.set(http::field::content_type, "text/plain");
        response.keep_alive(request.keep_alive());
        response.body() = "not found";

        if (request.method() == http::verb::get && request.target() == "/health") {
            response.result(http::status::ok);
            response.body() = "ok";
        }
        response.prepare_payload();

        stream.expires_after(std::chrono::seconds{15});
        co_await http::async_write(
            stream, response, asio::redirect_error(asio::use_awaitable, error));
        if (error || !request.keep_alive()) {
            break;
        }
    }

    boost::system::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
}

awaitable<void> listener(tcp::acceptor& acceptor) {
    for (;;) {
        boost::system::error_code error;
        auto socket = co_await acceptor.async_accept(
            asio::redirect_error(asio::use_awaitable, error));
        if (error == asio::error::operation_aborted) {
            break;
        }
        if (!error) {
            asio::co_spawn(acceptor.get_executor(), session(std::move(socket)), asio::detached);
        }
    }
}

int main(int argument_count, char** arguments) {
    std::uint16_t port = 8081;
    if (argument_count > 1) {
        unsigned parsed_port = port;
        const std::string_view text{arguments[1]};
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), parsed_port);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            parsed_port > 65'535U) {
            return 2;
        }
        port = static_cast<std::uint16_t>(parsed_port);
    }

    const auto worker_count = std::max(1U, std::thread::hardware_concurrency() / 2U);
    asio::io_context context{static_cast<int>(worker_count)};
    tcp::acceptor acceptor{context, {asio::ip::make_address("127.0.0.1"), port}};
    asio::signal_set signals{context, SIGINT, SIGTERM};
    signals.async_wait([&](const boost::system::error_code&, int) {
        boost::system::error_code ignored;
        acceptor.cancel(ignored);
        acceptor.close(ignored);
    });
    asio::co_spawn(context, listener(acceptor), asio::detached);

    std::vector<std::thread> workers;
    workers.reserve(worker_count - 1U);
    for (unsigned index = 1; index < worker_count; ++index) {
        workers.emplace_back([&context] { context.run(); });
    }
    context.run();
    for (auto& worker : workers) {
        worker.join();
    }
}
