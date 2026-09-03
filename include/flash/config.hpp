#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace flash {

struct server_config {
    std::string address{"127.0.0.1"};
    std::uint16_t port{8080};
    std::size_t workers{std::max<std::size_t>(1, std::thread::hardware_concurrency() / 2U)};
    std::size_t header_limit{16U * 1024U};
    std::size_t header_field_limit{100};
    std::size_t body_limit{1024U * 1024U};
    std::chrono::milliseconds header_timeout{std::chrono::seconds{5}};
    std::chrono::milliseconds body_timeout{std::chrono::seconds{15}};
    std::chrono::milliseconds write_timeout{std::chrono::seconds{15}};
    std::chrono::milliseconds idle_timeout{std::chrono::seconds{60}};
    std::chrono::milliseconds graceful_shutdown_timeout{std::chrono::seconds{10}};
};

} // namespace flash

