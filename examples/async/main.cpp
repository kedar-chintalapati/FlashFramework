#include <flash/flash.hpp>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>

namespace api {

[[=flash::get("/wait")]]
flash::task<flash::text> wait(
    [[=flash::minimum(0), =flash::maximum(5000)]] int milliseconds) {
    const auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::steady_timer timer{executor};
    timer.expires_after(std::chrono::milliseconds{milliseconds});
    co_await timer.async_wait(boost::asio::use_awaitable);
    co_return flash::text{"ready"};
}

} // namespace api

int main() {
    return flash::serve<^^api>({.port = 8080, .workers = 2});
}
