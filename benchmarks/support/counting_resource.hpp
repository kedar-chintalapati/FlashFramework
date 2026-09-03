#pragma once

#include <atomic>
#include <cstddef>
#include <memory_resource>

namespace flash::benchmark {

class counting_resource final : public std::pmr::memory_resource {
public:
    explicit counting_resource(
        std::pmr::memory_resource* upstream = std::pmr::get_default_resource()) noexcept
        : upstream_(upstream) {}

    [[nodiscard]] std::size_t allocation_count() const noexcept {
        return allocations_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t allocated_bytes() const noexcept {
        return bytes_.load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        allocations_.store(0, std::memory_order_relaxed);
        bytes_.store(0, std::memory_order_relaxed);
    }

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        allocations_.fetch_add(1, std::memory_order_relaxed);
        bytes_.fetch_add(bytes, std::memory_order_relaxed);
        return upstream_->allocate(bytes, alignment);
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        upstream_->deallocate(pointer, bytes, alignment);
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

    std::pmr::memory_resource* upstream_;
    std::atomic_size_t allocations_{};
    std::atomic_size_t bytes_{};
};

} // namespace flash::benchmark

