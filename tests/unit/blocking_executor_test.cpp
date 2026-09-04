#include <flash/task.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#include <future>
#include <thread>
#include <utility>

namespace {

flash::task<bool> run_blocking_work(boost::asio::thread_pool& pool) {
    const auto request_thread = std::this_thread::get_id();
    const auto result = co_await boost::asio::co_spawn(
        pool,
        []() -> flash::task<std::pair<int, std::thread::id>> {
            co_return std::pair{42, std::this_thread::get_id()};
        },
        boost::asio::use_awaitable);
    co_return result.first == 42 && result.second != request_thread;
}

} // namespace

int main() {
    boost::asio::io_context context;
    boost::asio::thread_pool pool{1};
    auto result = boost::asio::co_spawn(
        context, run_blocking_work(pool), boost::asio::use_future);
    context.run();
    pool.join();
    return result.get() ? 0 : 1;
}
