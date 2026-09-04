#include <flash/server.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <string>
#include <thread>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

namespace {

template <class Predicate>
bool wait_until(Predicate predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

} // namespace

int main() {
    auto handler = [](
        flash::raw_request_view,
        flash::raw_response_writer& response) -> flash::task<void> {
        response.content_type("text/plain");
        co_await response.write("ok");
    };

    flash::server_config config;
    config.port = 0;
    config.workers = 1;
    config.header_timeout = 75ms;
    config.body_timeout = 75ms;
    config.idle_timeout = 75ms;
    config.write_timeout = 1s;
    config.graceful_shutdown_timeout = 1s;
    flash::raw_server server{config, handler};
    server.start();

    {
        asio::io_context context;
        beast::tcp_stream stream{context};
        stream.expires_after(2s);
        stream.connect(tcp::resolver{context}.resolve(
            "127.0.0.1", std::to_string(server.port())));
        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        boost::system::error_code error;
        http::read(stream, buffer, response, error);
        if (!error) {
            server.stop();
            server.wait();
            return 1;
        }
    }

    if (!wait_until([&] { return server.active_sessions() == 0; }, 1s)) {
        server.stop();
        server.wait();
        return 2;
    }

    {
        asio::io_context context;
        beast::tcp_stream stream{context};
        stream.expires_after(2s);
        stream.connect(tcp::resolver{context}.resolve(
            "127.0.0.1", std::to_string(server.port())));
        constexpr std::string_view partial_request =
            "POST /body HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n"
            "\r\n"
            "x";
        asio::write(stream.socket(), asio::buffer(partial_request));
        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        boost::system::error_code error;
        http::read(stream, buffer, response, error);
        if (!error) {
            server.stop();
            server.wait();
            return 3;
        }
    }

    if (!wait_until([&] { return server.active_sessions() == 0; }, 1s)) {
        server.stop();
        server.wait();
        return 4;
    }

    asio::io_context idle_context;
    beast::tcp_stream idle_stream{idle_context};
    idle_stream.expires_after(2s);
    idle_stream.connect(tcp::resolver{idle_context}.resolve(
        "127.0.0.1", std::to_string(server.port())));
    http::request<http::empty_body> request{http::verb::get, "/idle", 11};
    request.set(http::field::host, "127.0.0.1");
    request.keep_alive(true);
    http::write(idle_stream, request);
    beast::flat_buffer idle_buffer;
    http::response<http::string_body> idle_response;
    http::read(idle_stream, idle_buffer, idle_response);
    if (idle_response.result() != http::status::ok || idle_response.body() != "ok") {
        server.stop();
        server.wait();
        return 5;
    }

    if (!wait_until([&] { return server.active_sessions() == 0; }, 1s)) {
        server.stop();
        server.wait();
        return 6;
    }

    server.stop();
    server.wait();
    return 0;
}
