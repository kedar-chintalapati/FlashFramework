#pragma once

#include <flash/config.hpp>
#include <flash/problem.hpp>
#include <flash/raw.hpp>
#include <flash/request.hpp>
#include <flash/response.hpp>
#include <flash/version.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace flash {
namespace detail {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

template <class>
struct awaitable_result;

template <class Value, class Executor>
struct awaitable_result<asio::awaitable<Value, Executor>> {
    using type = Value;
};

template <class Value>
concept asio_awaitable = requires {
    typename awaitable_result<std::remove_cvref_t<Value>>::type;
};

[[nodiscard]] inline std::optional<http_method> to_flash_method(http::verb method) noexcept {
    switch (method) {
    case http::verb::get: return http_method::get;
    case http::verb::post: return http_method::post;
    case http::verb::put: return http_method::put;
    case http::verb::patch: return http_method::patch;
    case http::verb::delete_: return http_method::delete_;
    case http::verb::head: return http_method::head;
    case http::verb::options: return http_method::options;
    default: return std::nullopt;
    }
}

[[nodiscard]] inline response_message transport_problem(status code,
                                                        std::string title,
                                                        std::string detail,
                                                        std::string instance = {}) {
    return make_problem_response({
        .type = "https://flash.dev/problems/http-transport",
        .title = std::move(title),
        .status_code = code,
        .detail = std::move(detail),
        .instance = std::move(instance),
        .errors = {},
        .request_id = {},
    });
}

inline http::response<http::string_body>
to_beast_response(response_message response, unsigned version_number, bool request_keep_alive) {
    const auto numeric_status = static_cast<unsigned>(response.status_code);
    http::response<http::string_body> wire{
        static_cast<http::status>(numeric_status), version_number};
    wire.set(http::field::server, std::string{"flash/"}.append(version));
    wire.set(http::field::content_type, response.content_type);
    for (const auto& header : response.headers) {
        wire.set(beast::string_view{header.name.data(), header.name.size()},
                 beast::string_view{header.value.data(), header.value.size()});
    }
    wire.keep_alive(request_keep_alive && response.keep_alive);
    wire.body() = std::move(response.body);
    wire.prepare_payload();
    return wire;
}

template <class Handler>
task<response_message> invoke_raw_handler(Handler& handler, raw_request_view request) {
    response_message response;
    raw_response_writer writer{response};
    using result_type = std::invoke_result_t<Handler&, raw_request_view, raw_response_writer&>;
    if constexpr (std::is_void_v<result_type>) {
        std::invoke(handler, request, writer);
    } else if constexpr (asio_awaitable<result_type>) {
        static_assert(std::is_void_v<typename awaitable_result<result_type>::type>,
                      "FLASH-E100: a raw awaitable handler must return task<void>");
        co_await std::invoke(handler, request, writer);
    } else {
        static_assert(std::is_void_v<result_type>,
                      "FLASH-E101: a raw handler must return void or flash::task<void>");
    }
    co_return response;
}

template <class Handler>
task<void> raw_session(tcp::socket socket, const server_config& config, Handler& handler) {
    beast::tcp_stream stream{std::move(socket)};
    beast::flat_buffer buffer;

    for (;;) {
        http::request_parser<http::string_body> parser;
        parser.header_limit(static_cast<std::uint32_t>(std::min<std::size_t>(
            config.header_limit, std::numeric_limits<std::uint32_t>::max())));
        parser.body_limit(config.body_limit);

        boost::system::error_code error;
        stream.expires_after(config.header_timeout);
        co_await http::async_read_header(
            stream, buffer, parser, asio::redirect_error(asio::use_awaitable, error));

        if (error == http::error::end_of_stream || error == asio::error::eof ||
            error == asio::error::operation_aborted) {
            break;
        }

        if (error) {
            const bool too_large = error == http::error::body_limit;
            auto response = transport_problem(
                too_large ? status::payload_too_large : status::bad_request,
                too_large ? "Request body too large" : "Malformed HTTP request",
                too_large ? "The request exceeds the configured body limit."
                          : "The request headers could not be parsed within the configured limits.");
            auto wire = to_beast_response(std::move(response), 11, false);
            stream.expires_after(config.write_timeout);
            co_await http::async_write(
                stream, wire, asio::redirect_error(asio::use_awaitable, error));
            break;
        }

        if (static_cast<std::size_t>(std::distance(parser.get().begin(), parser.get().end())) >
            config.header_field_limit) {
            auto response = transport_problem(
                status::bad_request, "Too many HTTP header fields",
                "The request exceeds the configured header field limit.",
                std::string{parser.get().target()});
            auto wire = to_beast_response(std::move(response), parser.get().version(), false);
            stream.expires_after(config.write_timeout);
            co_await http::async_write(
                stream, wire, asio::redirect_error(asio::use_awaitable, error));
            break;
        }

        stream.expires_after(config.body_timeout);
        co_await http::async_read(
            stream, buffer, parser, asio::redirect_error(asio::use_awaitable, error));

        if (error) {
            const bool too_large = error == http::error::body_limit;
            auto response = transport_problem(
                too_large ? status::payload_too_large : status::bad_request,
                too_large ? "Request body too large" : "Malformed HTTP request body",
                too_large ? "The request exceeds the configured body limit."
                          : "The request body could not be parsed.",
                std::string{parser.get().target()});
            auto wire = to_beast_response(std::move(response), parser.get().version(), false);
            stream.expires_after(config.write_timeout);
            co_await http::async_write(
                stream, wire, asio::redirect_error(asio::use_awaitable, error));
            break;
        }

        auto message = parser.release();
        const auto method = to_flash_method(message.method());
        response_message response;

        if (!method) {
            response = transport_problem(
                status::method_not_allowed, "Unsupported HTTP method",
                "Flash does not expose this HTTP method.", std::string{message.target()});
            response.set_header("Allow", "GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS");
        } else {
            std::vector<header_view> headers;
            headers.reserve(static_cast<std::size_t>(std::distance(message.begin(), message.end())));
            for (const auto& field : message) {
                headers.push_back({
                    {field.name_string().data(), field.name_string().size()},
                    {field.value().data(), field.value().size()},
                });
            }

            const std::string_view target{message.target().data(), message.target().size()};
            const std::string_view body{message.body()};
            const raw_request_view request{*method, target, headers, body, message.keep_alive()};

            try {
                response = co_await invoke_raw_handler(handler, request);
            } catch (const std::exception&) {
                response = transport_problem(
                    status::internal_server_error, "Internal server error",
                    "The request could not be completed.", std::string{target});
            } catch (...) {
                response = transport_problem(
                    status::internal_server_error, "Internal server error",
                    "The request could not be completed.", std::string{target});
            }
        }

        const bool keep_alive = message.keep_alive() && response.keep_alive;
        auto wire = to_beast_response(
            std::move(response), message.version(), message.keep_alive());
        stream.expires_after(config.write_timeout);
        co_await http::async_write(
            stream, wire, asio::redirect_error(asio::use_awaitable, error));

        if (error || !keep_alive) {
            break;
        }
    }

    boost::system::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
}

} // namespace detail

template <class Handler>
class raw_server {
public:
    raw_server(server_config config, Handler handler)
        : config_(std::move(config)),
          context_(static_cast<int>(validated_worker_count(config_.workers))),
          acceptor_(context_),
          handler_(std::move(handler)) {
        const auto address = detail::asio::ip::make_address(config_.address);
        const detail::tcp::endpoint endpoint{address, config_.port};
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(detail::asio::socket_base::reuse_address{true});
        acceptor_.bind(endpoint);
        acceptor_.listen(detail::asio::socket_base::max_listen_connections);
        bound_port_ = acceptor_.local_endpoint().port();
    }

    raw_server(const raw_server&) = delete;
    raw_server& operator=(const raw_server&) = delete;

    ~raw_server() {
        stop();
        wait();
    }

    void start() {
        if (started_.exchange(true)) {
            throw std::logic_error{"Flash server already started"};
        }

        detail::asio::co_spawn(context_, accept_loop(), detail::asio::detached);
        workers_.reserve(config_.workers);
        for (std::size_t index = 0; index < config_.workers; ++index) {
            workers_.emplace_back([this] { context_.run(); });
        }
    }

    void stop() noexcept {
        if (!started_.load() || stopping_.exchange(true)) {
            return;
        }
        detail::asio::post(context_, [this] {
            boost::system::error_code ignored;
            acceptor_.cancel(ignored);
            acceptor_.close(ignored);
            context_.stop();
        });
    }

    void wait() noexcept {
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    [[nodiscard]] std::uint16_t port() const noexcept { return bound_port_; }

private:
    static std::size_t validated_worker_count(std::size_t count) {
        if (count == 0 || count > 256) {
            throw std::invalid_argument{"Flash worker count must be between 1 and 256"};
        }
        return count;
    }

    task<void> accept_loop() {
        for (;;) {
            boost::system::error_code error;
            auto socket = co_await acceptor_.async_accept(
                detail::asio::redirect_error(detail::asio::use_awaitable, error));
            if (error == detail::asio::error::operation_aborted || !acceptor_.is_open()) {
                break;
            }
            if (error) {
                continue;
            }
            detail::asio::co_spawn(
                context_, detail::raw_session(std::move(socket), config_, handler_),
                detail::asio::detached);
        }
    }

    server_config config_;
    detail::asio::io_context context_;
    detail::tcp::acceptor acceptor_;
    Handler handler_;
    std::vector<std::thread> workers_;
    std::atomic_bool started_{};
    std::atomic_bool stopping_{};
    std::uint16_t bound_port_{};
};

template <class Handler>
int serve_raw(server_config config, Handler handler, std::stop_token stop_token = {}) {
    raw_server<Handler> server{std::move(config), std::move(handler)};
    std::stop_callback stop_callback{stop_token, [&server] { server.stop(); }};
    server.start();
    server.wait();
    return 0;
}

} // namespace flash
