#include <flash/flash.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using asio::awaitable;

struct BenchmarkItem {
    int value{};
    std::string name;
};

struct ValidatedValue {
    [[=flash::minimum(1)]] int value{};
};

namespace {

template <class Value>
bool parse_number(std::string_view text, Value& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

std::string_view path_of(std::string_view target) {
    return target.substr(0, target.find('?'));
}

std::optional<int> add_result(std::string_view path) {
    constexpr std::string_view prefix = "/add/";
    if (!path.starts_with(prefix)) {
        return std::nullopt;
    }
    path.remove_prefix(prefix.size());
    const auto separator = path.find('/');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    int left = 0;
    int right = 0;
    if (!parse_number(path.substr(0, separator), left) ||
        !parse_number(path.substr(separator + 1), right)) {
        return std::nullopt;
    }
    return left + right;
}

std::optional<int> wait_delay(std::string_view target) {
    constexpr std::string_view prefix = "/wait?delay=";
    if (!target.starts_with(prefix)) {
        return std::nullopt;
    }
    int delay = 0;
    if (!parse_number(target.substr(prefix.size()), delay) || delay < 0) {
        return std::nullopt;
    }
    return delay;
}

http::response<http::string_body> wire_response(
    flash::response_message response, unsigned version, bool keep_alive) {
    http::response<http::string_body> wire{
        static_cast<http::status>(static_cast<unsigned>(response.status_code)), version};
    wire.set(http::field::server, "manual-beast");
    wire.set(http::field::content_type, response.content_type);
    for (const auto& header : response.headers) {
        wire.set(header.name, header.value);
    }
    wire.keep_alive(keep_alive && response.keep_alive);
    wire.body() = std::move(response.body);
    wire.prepare_payload();
    return wire;
}

flash::response_message invalid_json(
    const flash::json::read_error& error, std::string_view target) {
    return flash::make_problem_response({
        .type = "https://flash.dev/problems/invalid-parameter",
        .title = "Invalid request parameter",
        .status_code = flash::status::unprocessable_content,
        .detail = "Request parameter could not be bound to the endpoint signature.",
        .instance = std::string{target},
        .errors = {{{"body", error.path}, error.code, error.message}},
        .request_id = {},
    });
}

template <class Body>
std::optional<flash::response_message> read_json_body(
    const http::request<http::string_body>& request, Body& body) {
    const auto content_type = request[http::field::content_type];
    if (content_type.empty() ||
        !flash::binding::is_json_media_type(
            {content_type.data(), content_type.size()})) {
        return flash::make_problem_response({
            .type = "https://flash.dev/problems/unsupported-media-type",
            .title = "Unsupported media type",
            .status_code = flash::status::unsupported_media_type,
            .detail = "The request body media type is not supported.",
            .instance = std::string{request.target()},
            .errors = {},
            .request_id = {},
        });
    }
    auto parsed = flash::json::read<Body>(request.body());
    if (!parsed) {
        return invalid_json(parsed.error(), request.target());
    }
    body = std::move(*parsed);
    return std::nullopt;
}

awaitable<void> session(tcp::socket socket) {
    beast::tcp_stream stream{std::move(socket)};
    beast::flat_buffer buffer;

    for (;;) {
        http::request_parser<http::string_body> parser;
        parser.header_limit(16U * 1024U);
        parser.body_limit(1024U * 1024U);
        boost::system::error_code error;
        stream.expires_after(std::chrono::seconds{5});
        co_await http::async_read_header(
            stream, buffer, parser, asio::redirect_error(asio::use_awaitable, error));
        if (error) {
            break;
        }
        stream.expires_after(std::chrono::seconds{15});
        co_await http::async_read(
            stream, buffer, parser, asio::redirect_error(asio::use_awaitable, error));
        if (error) {
            break;
        }

        auto request = parser.release();
        const std::string_view target{request.target().data(), request.target().size()};
        const auto path = path_of(target);
        flash::response_message response;

        if (request.method() == http::verb::get && path == "/health") {
            response.content_type = "text/plain; charset=utf-8";
            response.body = "ok";
        } else if (request.method() == http::verb::get && path.starts_with("/add/")) {
            const auto value = add_result(path);
            if (value) {
                response.body = flash::json::write(*value);
            } else {
                response = flash::make_problem_response({
                    .type = "https://flash.dev/problems/invalid-parameter",
                    .title = "Invalid request parameter",
                    .status_code = flash::status::unprocessable_content,
                    .detail = "Request parameter could not be bound to the endpoint signature.",
                    .instance = std::string{target},
                    .errors = {{{"path", "value"}, "invalid_integer",
                                "Expected an integer."}},
                    .request_id = {},
                });
            }
        } else if (request.method() == http::verb::get && path == "/object") {
            response.body = flash::json::write(BenchmarkItem{42, "widget"});
        } else if (request.method() == http::verb::post && path == "/echo") {
            BenchmarkItem item;
            if (auto failure = read_json_body(request, item)) {
                response = std::move(*failure);
            } else {
                response.body = flash::json::write(item);
            }
        } else if (request.method() == http::verb::post && path == "/validate") {
            ValidatedValue value;
            if (auto failure = read_json_body(request, value)) {
                response = std::move(*failure);
            } else {
                response.body = flash::json::write(value);
            }
        } else if (request.method() == http::verb::get && path == "/wait") {
            const auto delay = wait_delay(target);
            if (!delay) {
                response.status_code = flash::status::unprocessable_content;
            } else {
                asio::steady_timer timer{co_await asio::this_coro::executor};
                timer.expires_after(std::chrono::milliseconds{*delay});
                co_await timer.async_wait(asio::use_awaitable);
                response.content_type = "text/plain; charset=utf-8";
                response.body = "ok";
            }
        } else {
            response.status_code = flash::status::not_found;
            response.content_type = "text/plain";
            response.body = "not found";
        }

        const bool keep_alive = request.keep_alive() && response.keep_alive;
        auto wire = wire_response(
            std::move(response), request.version(), request.keep_alive());
        stream.expires_after(std::chrono::seconds{15});
        co_await http::async_write(
            stream, wire, asio::redirect_error(asio::use_awaitable, error));
        if (error || !keep_alive) {
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
        if (error) {
            break;
        }
        asio::co_spawn(
            acceptor.get_executor(), session(std::move(socket)), asio::detached);
    }
}

} // namespace

int main(int argument_count, char** arguments) {
    std::size_t port = 8083;
    std::size_t worker_count =
        std::max<std::size_t>(1, std::thread::hardware_concurrency() / 2U);
    if (argument_count > 1 && (!parse_number(
            std::string_view{arguments[1]}, port) || port > 65'535)) {
        return 2;
    }
    if (argument_count > 2 && (!parse_number(
            std::string_view{arguments[2]}, worker_count) || worker_count == 0 ||
            worker_count > 256)) {
        return 2;
    }

    asio::io_context context{static_cast<int>(worker_count)};
    tcp::acceptor acceptor{
        context,
        {asio::ip::make_address("127.0.0.1"), static_cast<std::uint16_t>(port)}};
    asio::co_spawn(context, listener(acceptor), asio::detached);

    std::vector<std::thread> workers;
    workers.reserve(worker_count - 1);
    for (std::size_t index = 1; index < worker_count; ++index) {
        workers.emplace_back([&context] { context.run(); });
    }
    context.run();
    for (auto& worker : workers) {
        worker.join();
    }
}
