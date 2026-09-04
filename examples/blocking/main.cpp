#include <flash/flash.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <thread>

struct Services {
    boost::asio::thread_pool blocking_pool{2};
};

int blocking_lookup(int value) {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    return value * 2;
}

namespace api {

[[=flash::get("/lookup/{value}")]]
flash::task<int> lookup(int value, flash::state<Services>& services) {
    co_return co_await boost::asio::co_spawn(
        services->blocking_pool,
        [value]() -> flash::task<int> {
            co_return blocking_lookup(value);
        },
        boost::asio::use_awaitable);
}

} // namespace api

int main() {
    flash::server_config config;
    Services services;
    const auto result = flash::serve<^^api>(config, services);
    services.blocking_pool.join();
    return result;
}
