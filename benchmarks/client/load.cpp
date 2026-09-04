#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <latch>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

bool parse_size(std::string_view text, std::size_t& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

http::verb parse_method(std::string_view method) {
    if (method == "GET") {
        return http::verb::get;
    }
    if (method == "POST") {
        return http::verb::post;
    }
    return http::verb::unknown;
}

std::uint64_t percentile(
    const std::vector<std::uint64_t>& sorted, std::size_t numerator) {
    if (sorted.empty()) {
        return 0;
    }
    const auto index = (sorted.size() - 1) * numerator / 1000;
    return sorted[index];
}

} // namespace

int main(int argument_count, char** arguments) {
    if (argument_count < 6) {
        std::cerr << "usage: flash_benchmark_load port method target connections requests"
                     " [body] [content_type] [expected_status]\n";
        return 2;
    }

    std::size_t port_value = 0;
    std::size_t connection_count = 0;
    std::size_t requests_per_connection = 0;
    if (!parse_size(arguments[1], port_value) || port_value > 65'535 ||
        !parse_size(arguments[4], connection_count) || connection_count == 0 ||
        connection_count > 512 ||
        !parse_size(arguments[5], requests_per_connection) ||
        requests_per_connection == 0) {
        return 2;
    }
    const auto method = parse_method(arguments[2]);
    if (method == http::verb::unknown) {
        return 2;
    }
    const std::string target{arguments[3]};
    std::string body = argument_count > 6 ? arguments[6] : "";
    if (body.starts_with('@')) {
        std::ifstream input{body.substr(1), std::ios::binary};
        if (!input) {
            return 2;
        }
        body.assign(
            std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    }
    const std::string content_type = argument_count > 7 ? arguments[7] : "";
    std::size_t expected_status = 200;
    if (argument_count > 8 && !parse_size(arguments[8], expected_status)) {
        return 2;
    }

    std::vector<std::vector<std::uint64_t>> thread_latencies(connection_count);
    std::vector<std::thread> clients;
    clients.reserve(connection_count);
    std::latch ready{static_cast<std::ptrdiff_t>(connection_count)};
    std::atomic_bool start{};
    std::atomic_size_t errors{};

    for (std::size_t client_index = 0; client_index < connection_count; ++client_index) {
        clients.emplace_back([&, client_index] {
            bool announced = false;
            try {
                asio::io_context context;
                beast::tcp_stream stream{context};
                stream.expires_after(std::chrono::seconds{10});
                stream.connect(tcp::resolver{context}.resolve(
                    "127.0.0.1", std::to_string(port_value)));
                beast::flat_buffer buffer;

                http::request<http::string_body> warmup{method, target, 11};
                warmup.set(http::field::host, "127.0.0.1");
                if (!content_type.empty()) {
                    warmup.set(http::field::content_type, content_type);
                }
                warmup.body() = body;
                warmup.prepare_payload();
                warmup.keep_alive(true);
                stream.expires_after(std::chrono::seconds{10});
                http::write(stream, warmup);
                http::response<http::string_body> warmup_response;
                http::read(stream, buffer, warmup_response);
                if (warmup_response.result_int() != expected_status) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                }

                auto& latencies = thread_latencies[client_index];
                latencies.reserve(requests_per_connection);
                ready.count_down();
                announced = true;
                while (!start.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }

                for (std::size_t request_index = 0;
                     request_index < requests_per_connection;
                     ++request_index) {
                    http::request<http::string_body> request{method, target, 11};
                    request.set(http::field::host, "127.0.0.1");
                    if (!content_type.empty()) {
                        request.set(http::field::content_type, content_type);
                    }
                    request.body() = body;
                    request.prepare_payload();
                    request.keep_alive(request_index + 1 < requests_per_connection);

                    stream.expires_after(std::chrono::seconds{10});
                    const auto started = std::chrono::steady_clock::now();
                    http::write(stream, request);
                    http::response<http::string_body> response;
                    http::read(stream, buffer, response);
                    const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started);
                    latencies.push_back(static_cast<std::uint64_t>(duration.count()));
                    if (response.result_int() != expected_status) {
                        errors.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            } catch (...) {
                errors.fetch_add(requests_per_connection, std::memory_order_relaxed);
                if (!announced) {
                    ready.count_down();
                }
            }
        });
    }

    ready.wait();
    const auto started = std::chrono::steady_clock::now();
    start.store(true, std::memory_order_release);
    for (auto& client : clients) {
        client.join();
    }
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started);

    std::vector<std::uint64_t> latencies;
    const auto request_count = connection_count * requests_per_connection;
    latencies.reserve(request_count);
    for (auto& local : thread_latencies) {
        latencies.insert(latencies.end(), local.begin(), local.end());
    }
    std::sort(latencies.begin(), latencies.end());
    const auto throughput =
        elapsed.count() > 0.0 ? static_cast<double>(latencies.size()) / elapsed.count() : 0.0;

    std::cout << "{\"target\":\"" << target << "\",\"connections\":"
              << connection_count << ",\"requests\":" << latencies.size()
              << ",\"seconds\":" << elapsed.count() << ",\"requests_per_second\":"
              << throughput << ",\"p50_ns\":" << percentile(latencies, 500)
              << ",\"p90_ns\":" << percentile(latencies, 900)
              << ",\"p99_ns\":" << percentile(latencies, 990)
              << ",\"p999_ns\":" << percentile(latencies, 999)
              << ",\"errors\":" << errors.load(std::memory_order_relaxed) << "}\n";
    return errors.load(std::memory_order_relaxed) == 0 &&
                   latencies.size() == request_count
               ? 0
               : 1;
}
