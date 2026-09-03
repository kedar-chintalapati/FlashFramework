#include <flash/request.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string_view>

int main() {
    constexpr std::array headers{
        flash::header_view{"Host", "127.0.0.1"},
        flash::header_view{"X-Request-ID", "benchmark"},
        flash::header_view{"Accept", "application/json"},
    };
    constexpr std::size_t iterations = 5'000'000;
    std::size_t checksum = 0;

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        const flash::request_view request{
            flash::http_method::get,
            "/widgets/42?verbose=true",
            headers,
            {},
            true,
        };
        checksum += request.path().size();
        checksum += request.header("x-request-id")->size();
    }
    const auto finish = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double, std::nano>{finish - start}.count();
    std::cout << "request_view_ns_per_operation=" << elapsed / iterations
              << " iterations=" << iterations << " checksum=" << checksum << '\n';
}

