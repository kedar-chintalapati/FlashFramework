#include <flash/flash.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

struct BenchmarkItem {
    int value{};
    std::string name;
};

struct ValidatedValue {
    [[=flash::minimum(1)]] int value{};
};

namespace benchmark_api {

[[=flash::get("/health")]]
flash::text health() {
    return {"ok"};
}

[[=flash::get("/add/{left}/{right}")]]
int add(int left, int right) {
    return left + right;
}

[[=flash::get("/object")]]
BenchmarkItem object() {
    return {42, "widget"};
}

[[=flash::post("/echo")]]
BenchmarkItem echo(BenchmarkItem item) {
    return item;
}

[[=flash::post("/validate")]]
ValidatedValue validate(ValidatedValue value) {
    return value;
}

[[=flash::get("/wait")]]
flash::task<flash::text> wait(int delay) {
    boost::asio::steady_timer timer{co_await boost::asio::this_coro::executor};
    timer.expires_after(std::chrono::milliseconds{delay});
    co_await timer.async_wait(boost::asio::use_awaitable);
    co_return flash::text{"ok"};
}

} // namespace benchmark_api

namespace {

bool parse_size(std::string_view text, std::size_t& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

} // namespace

int main(int argument_count, char** arguments) {
    flash::server_config config;
    config.port = 8082;
    if (argument_count > 1) {
        std::size_t port = 0;
        if (!parse_size(arguments[1], port) || port > 65'535) {
            return 2;
        }
        config.port = static_cast<std::uint16_t>(port);
    }
    if (argument_count > 2 && !parse_size(arguments[2], config.workers)) {
        return 2;
    }
    return flash::serve<
        ^^benchmark_api,
        flash::openapi::documentation_mode::disabled>(config);
}
