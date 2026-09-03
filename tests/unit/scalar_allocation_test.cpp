#include <flash/binding/bind.hpp>
#include <flash/meta/reflection.hpp>
#include <flash/routing/matcher.hpp>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace allocation_api {

[[=flash::get("/add/{a}/{b}")]] int add(int a, int b);

} // namespace allocation_api

namespace {

std::atomic_size_t allocation_count{};
std::atomic_bool tracking{};

void count_allocation() noexcept {
    if (tracking.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace

void* operator new(std::size_t size) {
    count_allocation();
    if (void* pointer = std::malloc(size)) {
        return pointer;
    }
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

int main() {
    constexpr auto function = flash::meta::endpoint_at<^^allocation_api, 0>();
    const flash::request_view request{
        flash::http_method::get, "/add/20/22", {}, {}, false};

    auto warm_match = flash::routing::match_api<^^allocation_api>(
        request.method(), request.path());
    auto warm_a = flash::binding::bind_parameter<function, 0>(request, warm_match.route);
    auto warm_b = flash::binding::bind_parameter<function, 1>(request, warm_match.route);
    if (!warm_a || !warm_b) {
        return 1;
    }

    std::size_t checksum = 0;
    allocation_count.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_relaxed);
    for (std::size_t index = 0; index < 10'000; ++index) {
        auto matched = flash::routing::match_api<^^allocation_api>(
            request.method(), request.path());
        auto a = flash::binding::bind_parameter<function, 0>(request, matched.route);
        auto b = flash::binding::bind_parameter<function, 1>(request, matched.route);
        if (!a || !b) {
            tracking.store(false, std::memory_order_relaxed);
            return 2;
        }
        checksum += static_cast<std::size_t>(*a + *b);
    }
    tracking.store(false, std::memory_order_relaxed);

    // Measured boundary: generated route matching plus successful integer path
    // binding. Transport, coroutine, response, and user allocations are excluded.
    return allocation_count.load(std::memory_order_relaxed) == 0 &&
                   checksum == 420'000U
               ? 0
               : 3;
}

