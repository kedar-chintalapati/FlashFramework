#include <flash/server.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

int main() {
    constexpr std::size_t client_count = 8;
    constexpr std::size_t requests_per_client = 16;

    std::atomic_size_t handled{};
    std::atomic_size_t failures{};

    flash::server_config config;
    config.port = 0;
    config.workers = 2;
    config.header_timeout = std::chrono::seconds{3};
    config.body_timeout = std::chrono::seconds{3};
    config.write_timeout = std::chrono::seconds{3};

    auto handler = [&handled](flash::raw_request_view request,
                              flash::raw_response_writer& response) -> flash::task<void> {
        if (request.method() != flash::http_method::get || request.path() != "/health") {
            response.status(flash::status::not_found);
            co_await response.write("not found");
            co_return;
        }
        handled.fetch_add(1, std::memory_order_relaxed);
        response.content_type("text/plain");
        co_await response.write("ok");
    };

    flash::raw_server server{config, handler};
    server.start();

    std::vector<std::thread> clients;
    clients.reserve(client_count);
    for (std::size_t client_index = 0; client_index < client_count; ++client_index) {
        clients.emplace_back([&server, &failures] {
            try {
                asio::io_context context;
                tcp::resolver resolver{context};
                beast::tcp_stream stream{context};
                stream.connect(resolver.resolve("127.0.0.1", std::to_string(server.port())));
                beast::flat_buffer buffer;

                for (std::size_t request_index = 0;
                     request_index < requests_per_client;
                     ++request_index) {
                    http::request<http::empty_body> request{http::verb::get, "/health", 11};
                    request.set(http::field::host, "127.0.0.1");
                    request.keep_alive(request_index + 1 < requests_per_client);
                    http::write(stream, request);

                    http::response<http::string_body> response;
                    http::read(stream, buffer, response);
                    if (response.result() != http::status::ok || response.body() != "ok") {
                        failures.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            } catch (...) {
                failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& client : clients) {
        client.join();
    }

    server.stop();
    server.wait();

    const auto expected = client_count * requests_per_client;
    return failures.load(std::memory_order_relaxed) == 0 &&
                   handled.load(std::memory_order_relaxed) == expected
               ? 0
               : 1;
}

