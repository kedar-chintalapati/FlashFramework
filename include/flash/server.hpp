#pragma once

#include <flash/config.hpp>
#include <flash/problem.hpp>
#include <flash/raw.hpp>
#include <flash/request.hpp>
#include <flash/response.hpp>
#include <flash/version.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <concepts>
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

enum class session_phase : std::uint8_t {
    reading,
    handling,
    writing,
    finished,
};

struct session_control {
    explicit session_control(tcp::socket socket) : stream{std::move(socket)} {}

    beast::tcp_stream stream;
    std::stop_source stop_source;
    asio::cancellation_signal cancellation;
    std::atomic<session_phase> phase{session_phase::reading};
};

inline void request_session_stop(
    const std::shared_ptr<session_control>& control, bool force) {
    control->stop_source.request_stop();
    asio::post(control->stream.get_executor(), [control, force] {
        if (force) {
            control->cancellation.emit(asio::cancellation_type::all);
        }
        if (force ||
            control->phase.load(std::memory_order_acquire) == session_phase::reading) {
            boost::system::error_code ignored;
            control->stream.socket().cancel(ignored);
            if (force) {
                control->stream.socket().close(ignored);
            }
        }
    });
}

[[nodiscard]] inline bool is_timeout_error(
    const boost::system::error_code& error) noexcept {
    return error == beast::error::timeout || error == asio::error::timed_out;
}

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
    } else if constexpr (is_task_v<result_type>) {
        if constexpr (std::is_void_v<task_value_t<result_type>>) {
            co_await std::invoke(handler, request, writer);
        } else {
            static_assert(std::same_as<task_value_t<result_type>, response_message>,
                          "FLASH-E100: a raw task must return void or response_message");
            co_return co_await std::invoke(handler, request, writer);
        }
    } else if constexpr (std::same_as<std::remove_cvref_t<result_type>, response_message>) {
        co_return std::invoke(handler, request, writer);
    } else {
        static_assert(std::is_void_v<result_type>,
                      "FLASH-E101: a raw handler must return void or flash::task<void>");
    }
    co_return response;
}

template <class Handler>
task<void> raw_session(std::shared_ptr<session_control> control,
                       const server_config& config,
                       Handler& handler) {
    auto& stream = control->stream;
    beast::flat_buffer buffer;
    bool first_request = true;

    for (;;) {
        if (control->stop_source.stop_requested()) {
            break;
        }
        http::request_parser<http::string_body> parser;
        parser.header_limit(static_cast<std::uint32_t>(std::min<std::size_t>(
            config.header_limit, std::numeric_limits<std::uint32_t>::max())));
        parser.body_limit(config.body_limit);

        boost::system::error_code error;
        control->phase.store(session_phase::reading, std::memory_order_release);
        stream.expires_after(
            first_request ? config.header_timeout : config.idle_timeout);
        co_await http::async_read_header(
            stream, buffer, parser, asio::redirect_error(asio::use_awaitable, error));
        first_request = false;

        if (error == http::error::end_of_stream || error == asio::error::eof ||
            error == asio::error::operation_aborted) {
            break;
        }

        if (error) {
            const bool too_large = error == http::error::body_limit;
            const bool timed_out = is_timeout_error(error);
            if (timed_out) {
                break;
            }
            auto response = transport_problem(
                too_large ? status::payload_too_large : status::bad_request,
                too_large ? "Request body too large" : "Malformed HTTP request",
                too_large ? "The request exceeds the configured body limit."
                          : "The request headers could not be parsed within the configured limits.");
            auto wire = to_beast_response(std::move(response), 11, false);
            control->phase.store(session_phase::writing, std::memory_order_release);
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
            control->phase.store(session_phase::writing, std::memory_order_release);
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
            const bool timed_out = is_timeout_error(error);
            if (timed_out) {
                break;
            }
            auto response = transport_problem(
                too_large ? status::payload_too_large : status::bad_request,
                too_large ? "Request body too large" : "Malformed HTTP request body",
                too_large ? "The request exceeds the configured body limit."
                          : "The request body could not be parsed.",
                std::string{parser.get().target()});
            auto wire = to_beast_response(std::move(response), parser.get().version(), false);
            control->phase.store(session_phase::writing, std::memory_order_release);
            stream.expires_after(config.write_timeout);
            co_await http::async_write(
                stream, wire, asio::redirect_error(asio::use_awaitable, error));
            break;
        }

        auto message = parser.release();
        const auto method = to_flash_method(message.method());
        response_message response;
        control->phase.store(session_phase::handling, std::memory_order_release);

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
            const raw_request_view request{
                *method, target, headers, body, message.keep_alive(),
                control->stop_source.get_token()};

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

        const bool request_keep_alive =
            message.keep_alive() && !control->stop_source.stop_requested();
        const bool keep_alive = request_keep_alive && response.keep_alive;
        auto wire = to_beast_response(
            std::move(response), message.version(), request_keep_alive);
        control->phase.store(session_phase::writing, std::memory_order_release);
        stream.expires_after(config.write_timeout);
        co_await http::async_write(
            stream, wire, asio::redirect_error(asio::use_awaitable, error));

        if (error || !keep_alive) {
            break;
        }
    }

    control->phase.store(session_phase::finished, std::memory_order_release);
    boost::system::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
    stream.socket().close(ignored);
}

} // namespace detail

template <class Handler>
class raw_server {
public:
    raw_server(server_config config, Handler handler)
        : config_(std::move(config)),
          context_(static_cast<int>(validated_worker_count(config_.workers))),
          control_strand_(detail::asio::make_strand(context_)),
          acceptor_(control_strand_),
          shutdown_timer_(control_strand_),
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

        detail::asio::co_spawn(
            control_strand_, accept_loop(), detail::asio::detached);
        workers_.reserve(config_.workers);
        for (std::size_t index = 0; index < config_.workers; ++index) {
            workers_.emplace_back([this] { context_.run(); });
        }
    }

    void stop() noexcept {
        if (!started_.load() || stopping_.exchange(true)) {
            return;
        }
        detail::asio::post(control_strand_, [this] { begin_stop(); });
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
    [[nodiscard]] std::size_t active_sessions() const noexcept {
        return active_sessions_.load(std::memory_order_acquire);
    }

private:
    static std::size_t validated_worker_count(std::size_t count) {
        if (count == 0 || count > 256) {
            throw std::invalid_argument{"Flash worker count must be between 1 and 256"};
        }
        return count;
    }

    [[nodiscard]] std::vector<std::shared_ptr<detail::session_control>>
    session_snapshot() {
        std::vector<std::shared_ptr<detail::session_control>> result;
        result.reserve(sessions_.size());
        for (const auto& weak : sessions_) {
            if (auto session = weak.lock()) {
                result.push_back(std::move(session));
            }
        }
        return result;
    }

    void begin_stop() {
        boost::system::error_code ignored;
        acceptor_.cancel(ignored);
        acceptor_.close(ignored);

        const auto sessions = session_snapshot();
        for (const auto& session : sessions) {
            detail::request_session_stop(session, false);
        }
        if (sessions.empty()) {
            return;
        }

        shutdown_timer_.expires_after(config_.graceful_shutdown_timeout);
        shutdown_timer_.async_wait([this](const boost::system::error_code& error) {
            if (!error) {
                force_stop();
            }
        });
    }

    void force_stop() {
        const auto sessions = session_snapshot();
        for (const auto& session : sessions) {
            detail::request_session_stop(session, true);
        }
        if (sessions.empty()) {
            return;
        }

        shutdown_timer_.expires_after(std::chrono::milliseconds{50});
        shutdown_timer_.async_wait([this](const boost::system::error_code& error) {
            if (!error) {
                context_.stop();
            }
        });
    }

    void session_complete(const std::shared_ptr<detail::session_control>& session) {
        std::erase_if(sessions_, [&session](const auto& weak) {
            const auto current = weak.lock();
            return !current || current == session;
        });
        active_sessions_.fetch_sub(1, std::memory_order_release);
        if (stopping_.load(std::memory_order_acquire) && sessions_.empty()) {
            try {
                static_cast<void>(shutdown_timer_.cancel());
            } catch (...) {
            }
        }
    }

    task<void> accept_loop() {
        for (;;) {
            boost::system::error_code error;
            detail::tcp::socket socket{detail::asio::make_strand(context_)};
            co_await acceptor_.async_accept(
                socket,
                detail::asio::redirect_error(detail::asio::use_awaitable, error));
            if (error == detail::asio::error::operation_aborted || !acceptor_.is_open()) {
                break;
            }
            if (error) {
                continue;
            }
            auto session =
                std::make_shared<detail::session_control>(std::move(socket));
            sessions_.push_back(session);
            active_sessions_.fetch_add(1, std::memory_order_release);
            detail::asio::co_spawn(
                session->stream.get_executor(),
                detail::raw_session(session, config_, handler_),
                detail::asio::bind_cancellation_slot(
                    session->cancellation.slot(),
                    [this, session](std::exception_ptr) {
                        detail::asio::post(
                            control_strand_,
                            [this, session] { session_complete(session); });
                    }));
        }
    }

    server_config config_;
    detail::asio::io_context context_;
    detail::asio::strand<detail::asio::io_context::executor_type> control_strand_;
    detail::tcp::acceptor acceptor_;
    detail::asio::steady_timer shutdown_timer_;
    Handler handler_;
    std::vector<std::weak_ptr<detail::session_control>> sessions_;
    std::vector<std::thread> workers_;
    std::atomic_bool started_{};
    std::atomic_bool stopping_{};
    std::atomic_size_t active_sessions_{};
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
