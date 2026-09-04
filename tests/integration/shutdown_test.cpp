#include <flash/server.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <atomic>
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
    std::atomic_bool slow_started{};
    std::atomic_bool slow_saw_stop{};
    std::atomic_bool slow_client_ok{};

    auto graceful_handler =
        [&slow_started, &slow_saw_stop](
            flash::raw_request_view request,
            flash::raw_response_writer& response) -> flash::task<void> {
        if (request.path() == "/slow") {
            slow_started.store(true, std::memory_order_release);
            asio::steady_timer timer{co_await asio::this_coro::executor};
            timer.expires_after(150ms);
            co_await timer.async_wait(asio::use_awaitable);
            slow_saw_stop.store(
                request.stop_token().stop_requested(), std::memory_order_release);
            response.content_type("text/plain");
            co_await response.write("done");
            co_return;
        }
        response.content_type("text/plain");
        co_await response.write("ok");
    };

    flash::server_config graceful_config;
    graceful_config.port = 0;
    graceful_config.workers = 2;
    graceful_config.graceful_shutdown_timeout = 1s;
    graceful_config.idle_timeout = 5s;
    flash::raw_server graceful_server{graceful_config, graceful_handler};
    graceful_server.start();

    asio::io_context idle_context;
    beast::tcp_stream idle_stream{idle_context};
    idle_stream.connect(tcp::resolver{idle_context}.resolve(
        "127.0.0.1", std::to_string(graceful_server.port())));
    http::request<http::empty_body> idle_request{http::verb::get, "/fast", 11};
    idle_request.set(http::field::host, "127.0.0.1");
    idle_request.keep_alive(true);
    http::write(idle_stream, idle_request);
    beast::flat_buffer idle_buffer;
    http::response<http::string_body> idle_response;
    http::read(idle_stream, idle_buffer, idle_response);
    if (idle_response.result() != http::status::ok || idle_response.body() != "ok") {
        graceful_server.stop();
        graceful_server.wait();
        return 1;
    }

    std::thread slow_client{[&graceful_server, &slow_client_ok] {
        try {
            asio::io_context context;
            beast::tcp_stream stream{context};
            stream.expires_after(2s);
            stream.connect(tcp::resolver{context}.resolve(
                "127.0.0.1", std::to_string(graceful_server.port())));
            http::request<http::empty_body> request{http::verb::get, "/slow", 11};
            request.set(http::field::host, "127.0.0.1");
            request.keep_alive(true);
            http::write(stream, request);
            beast::flat_buffer buffer;
            http::response<http::string_body> response;
            http::read(stream, buffer, response);
            slow_client_ok.store(
                response.result() == http::status::ok && response.body() == "done" &&
                    !response.keep_alive(),
                std::memory_order_release);
        } catch (...) {
        }
    }};

    if (!wait_until(
            [&] {
                return slow_started.load(std::memory_order_acquire) &&
                       graceful_server.active_sessions() == 2;
            },
            2s)) {
        graceful_server.stop();
        graceful_server.wait();
        slow_client.join();
        return 2;
    }

    const auto graceful_started = std::chrono::steady_clock::now();
    graceful_server.stop();
    graceful_server.wait();
    const auto graceful_duration =
        std::chrono::steady_clock::now() - graceful_started;
    slow_client.join();

    if (!slow_client_ok.load(std::memory_order_acquire) ||
        !slow_saw_stop.load(std::memory_order_acquire) ||
        graceful_server.active_sessions() != 0 || graceful_duration >= 750ms) {
        return 3;
    }

    std::atomic_bool forced_started{};
    std::atomic_bool forced_cancelled{};
    std::atomic_bool forced_saw_stop{};
    auto forced_handler =
        [&forced_started, &forced_cancelled, &forced_saw_stop](
            flash::raw_request_view request,
            flash::raw_response_writer& response) -> flash::task<void> {
        forced_started.store(true, std::memory_order_release);
        asio::steady_timer timer{co_await asio::this_coro::executor};
        timer.expires_after(5s);
        boost::system::error_code error;
        co_await timer.async_wait(
            asio::redirect_error(asio::use_awaitable, error));
        forced_cancelled.store(
            error == asio::error::operation_aborted, std::memory_order_release);
        forced_saw_stop.store(
            request.stop_token().stop_requested(), std::memory_order_release);
        co_await response.write("late");
    };

    flash::server_config forced_config;
    forced_config.port = 0;
    forced_config.workers = 2;
    forced_config.graceful_shutdown_timeout = 50ms;
    flash::raw_server forced_server{forced_config, forced_handler};
    forced_server.start();

    std::thread forced_client{[&forced_server] {
        try {
            asio::io_context context;
            beast::tcp_stream stream{context};
            stream.expires_after(2s);
            stream.connect(tcp::resolver{context}.resolve(
                "127.0.0.1", std::to_string(forced_server.port())));
            http::request<http::empty_body> request{http::verb::get, "/wait", 11};
            request.set(http::field::host, "127.0.0.1");
            http::write(stream, request);
            beast::flat_buffer buffer;
            http::response<http::string_body> response;
            http::read(stream, buffer, response);
        } catch (...) {
        }
    }};

    if (!wait_until(
            [&] { return forced_started.load(std::memory_order_acquire); }, 2s)) {
        forced_server.stop();
        forced_server.wait();
        forced_client.join();
        return 4;
    }

    const auto forced_stop_started = std::chrono::steady_clock::now();
    forced_server.stop();
    forced_server.wait();
    const auto forced_duration =
        std::chrono::steady_clock::now() - forced_stop_started;
    forced_client.join();

    return forced_cancelled.load(std::memory_order_acquire) &&
                   forced_saw_stop.load(std::memory_order_acquire) &&
                   forced_duration < 750ms
               ? 0
               : 5;
}
