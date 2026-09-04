#include <flash/binding/bind.hpp>
#include <flash/binding/request.hpp>
#include <flash/json/read.hpp>
#include <flash/json/write.hpp>
#include <flash/meta/reflection.hpp>
#include <flash/problem.hpp>
#include <flash/response.hpp>
#include <flash/routing/matcher.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace allocation_api {

[[=flash::get("/add/{left}/{right}")]] int add(int left, int right);

} // namespace allocation_api

namespace {

std::atomic_size_t allocation_count{};
std::atomic_bool tracking{};

struct fixed_payload {
    int value{};
    std::array<int, 4> items{};
    bool active{};
};

struct owned_payload {
    std::string name;
    std::vector<int> items;
};

void record_allocation() noexcept {
    if (tracking.load(std::memory_order_relaxed)) {
        allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void* allocate(std::size_t size) {
    record_allocation();
    size = std::max<std::size_t>(size, 1);
    if (void* pointer = std::malloc(size)) {
        return pointer;
    }
    throw std::bad_alloc{};
}

void* allocate_aligned(std::size_t size, std::size_t alignment) {
    record_allocation();
    size = std::max<std::size_t>(size, 1);
#ifdef _WIN32
    if (void* pointer = _aligned_malloc(size, alignment)) {
        return pointer;
    }
#else
    const auto rounded = (size + alignment - 1) / alignment * alignment;
    if (void* pointer = std::aligned_alloc(alignment, rounded)) {
        return pointer;
    }
#endif
    throw std::bad_alloc{};
}

template <class Operation>
void measure(std::string_view name,
             std::string_view boundary,
             std::size_t iterations,
             Operation operation) {
    std::size_t checksum = 0;
    for (std::size_t index = 0; index < 100; ++index) {
        checksum += operation(index);
    }

    allocation_count.store(0, std::memory_order_relaxed);
    tracking.store(true, std::memory_order_relaxed);
    for (std::size_t index = 0; index < iterations; ++index) {
        checksum += operation(index);
    }
    tracking.store(false, std::memory_order_relaxed);
    const auto allocations = allocation_count.load(std::memory_order_relaxed);

    std::cout << "{\"case\":\"" << name << "\",\"boundary\":\"" << boundary
              << "\",\"iterations\":" << iterations
              << ",\"allocations\":" << allocations
              << ",\"allocations_per_operation\":"
              << static_cast<double>(allocations) / static_cast<double>(iterations)
              << ",\"checksum\":" << checksum << "}\n";
}

} // namespace

void* operator new(std::size_t size) {
    return allocate(size);
}

void* operator new[](std::size_t size) {
    return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate(size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return ::operator new(size, std::nothrow);
}

void* operator new(std::size_t size,
                   std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
    try {
        return allocate_aligned(size, static_cast<std::size_t>(alignment));
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t size,
                     std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
    return ::operator new(size, alignment, std::nothrow);
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

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer,
                     std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
    ::operator delete(pointer, alignment);
}

void operator delete[](void* pointer,
                       std::align_val_t alignment,
                       const std::nothrow_t&) noexcept {
    ::operator delete(pointer, alignment);
}

void operator delete(void* pointer, std::align_val_t) noexcept {
#ifdef _WIN32
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}

void operator delete[](void* pointer, std::align_val_t alignment) noexcept {
    ::operator delete(pointer, alignment);
}

void operator delete(void* pointer, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(pointer, alignment);
}

void operator delete[](void* pointer,
                       std::size_t,
                       std::align_val_t alignment) noexcept {
    ::operator delete(pointer, alignment);
}

int main() {
    constexpr auto function = flash::meta::endpoint_at<^^allocation_api, 0>();
    const flash::request_view route_request{
        flash::http_method::get, "/add/20/22", {}, {}, true};
    measure("route_and_integer_binding",
            "Flash route matcher and two integer path parameters",
            10'000,
            [&](std::size_t) {
                const auto matched = flash::routing::match_api<^^allocation_api>(
                    route_request.method(), route_request.path());
                const auto left = flash::binding::bind_parameter<function, 0>(
                    route_request, matched.route);
                const auto right = flash::binding::bind_parameter<function, 1>(
                    route_request, matched.route);
                return left && right ? static_cast<std::size_t>(*left + *right) : 0;
            });

    const flash::request_view query_request{
        flash::http_method::get,
        "/items?a=1&b=2&c=3&d=4&e=5&f=6&g=7&target=42",
        {},
        {},
        true};
    measure("query_scan",
            "Flash query scan with eight actual keys",
            10'000,
            [&](std::size_t) {
                const auto value = flash::binding::find_query_value(
                    query_request, "target");
                return value && *value ? (**value).size() : 0;
            });

    const std::string encoded(80, 'a');
    const std::string percent_input = encoded + "%20tail";
    measure("percent_decode_owned",
            "Flash decoded value ownership",
            10'000,
            [&](std::size_t) {
                const auto value = flash::binding::percent_decode(percent_input);
                return value ? value->view().size() : 0;
            });

    constexpr std::string_view fixed_json =
        "{\"value\":42,\"items\":[1,2,3,4],\"active\":true}";
    measure("json_read_fixed",
            "Flash parser and fixed size user object",
            10'000,
            [&](std::size_t) {
                const auto value = flash::json::read<fixed_payload>(fixed_json);
                return value ? static_cast<std::size_t>(value->value + value->items[3]) : 0;
            });

    const fixed_payload fixed_value{42, {1, 2, 3, 4}, true};
    std::string fixed_output;
    fixed_output.reserve(256);
    measure("json_write_reused",
            "Flash writer with a caller reserved output string",
            10'000,
            [&](std::size_t) {
                fixed_output.clear();
                flash::json::append(fixed_output, fixed_value);
                return fixed_output.size();
            });

    const std::string owned_json =
        "{\"name\":\"" + std::string(100, 'x') + "\",\"items\":[1,2,3,4]}";
    measure("json_read_owned",
            "Flash parser plus std string and std vector ownership",
            10'000,
            [&](std::size_t) {
                const auto value = flash::json::read<owned_payload>(owned_json);
                return value ? value->name.size() + value->items.size() : 0;
            });

    measure("problem_response",
            "Flash problem object serialization and response ownership",
            10'000,
            [](std::size_t index) {
                auto response = flash::make_problem_response({
                    .type = "https://flash.dev/problems/invalid-parameter",
                    .title = "Invalid request parameter",
                    .status_code = flash::status::unprocessable_content,
                    .detail = "A request value could not be parsed.",
                    .instance = "/items/42",
                    .errors = {},
                    .request_id = index % 2 == 0 ? "request-a" : "request-b"});
                return response.body.size();
            });

    measure("response_headers",
            "Flash response and three owned headers",
            10'000,
            [](std::size_t index) {
                flash::response_message response;
                response.set_header("Cache-Control", "no-store");
                response.set_header(
                    "X-Request-ID", index % 2 == 0 ? "request-a" : "request-b");
                response.set_header("Content-Language", "en-US");
                return response.headers.size();
            });
}
