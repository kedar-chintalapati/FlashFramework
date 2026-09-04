#pragma once

#include <flash/context.hpp>
#include <flash/problem.hpp>
#include <flash/response.hpp>
#include <flash/task.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace flash::middleware {

struct access_log_entry {
    http_method method{http_method::get};
    std::string_view target;
    status status_code{status::ok};
    std::chrono::nanoseconds duration{};
    std::string_view request_id;
};

class request_id {
public:
    template <class Next>
    task<response_message> operator()(request_context& context, Next next) const {
        context.request_id = incoming_id(context.request);
        if (context.request_id.empty()) {
            context.request_id = generate_id();
        }
        auto response = co_await next();
        response.set_header("X-Request-ID", context.request_id);
        co_return response;
    }

private:
    [[nodiscard]] static bool valid_id(std::string_view value) noexcept {
        if (value.empty() || value.size() > 128) {
            return false;
        }
        for (const char character : value) {
            const bool alpha_numeric =
                (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9');
            if (!alpha_numeric && character != '-' && character != '_' &&
                character != '.') {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static std::string incoming_id(const request_view& request) {
        std::string_view value;
        std::size_t count = 0;
        for (const auto& header : request.headers()) {
            if (ascii_iequals(header.name, "X-Request-ID")) {
                value = header.value;
                ++count;
            }
        }
        return count == 1 && valid_id(value) ? std::string{value} : std::string{};
    }

    [[nodiscard]] static std::string generate_id() {
        const auto sequence = next_sequence_.fetch_add(1, std::memory_order_relaxed);
        const auto clock = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        constexpr std::string_view digits{"0123456789abcdef"};
        std::array<char, 32> output{};
        for (std::size_t index = 0; index < 16; ++index) {
            const auto shift = static_cast<unsigned>((15 - index) * 4);
            output[index] = digits[(clock >> shift) & 0x0fU];
            output[index + 16] = digits[(sequence >> shift) & 0x0fU];
        }
        return {output.data(), output.size()};
    }

    inline static std::atomic<std::uint64_t> next_sequence_{};
};

class access_log {
public:
    using sink_type = std::function<void(const access_log_entry&)>;

    access_log() = default;
    explicit access_log(sink_type sink) : sink_(std::move(sink)) {}

    template <class Next>
    task<response_message> operator()(request_context& context, Next next) const {
        const auto started = std::chrono::steady_clock::now();
        try {
            auto response = co_await next();
            record(context, response.status_code, started);
            co_return response;
        } catch (...) {
            record(context, status::internal_server_error, started);
            throw;
        }
    }

private:
    void record(const request_context& context,
                status status_code,
                std::chrono::steady_clock::time_point started) const noexcept {
        if (!sink_) {
            return;
        }
        const access_log_entry entry{
            .method = context.request.method(),
            .target = context.request.target(),
            .status_code = status_code,
            .duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started),
            .request_id = context.request_id,
        };
        try {
            sink_(entry);
        } catch (...) {
        }
    }

    sink_type sink_;
};

struct recover_exceptions {
    template <class Next>
    task<response_message> operator()(request_context& context, Next next) const {
        try {
            co_return co_await next();
        } catch (...) {
            co_return make_problem_response({
                .type = "https://flash.dev/problems/internal-server-error",
                .title = "Internal server error",
                .status_code = status::internal_server_error,
                .detail = "The request could not be completed.",
                .instance = std::string{context.request.target()},
                .errors = {},
                .request_id = context.request_id,
            });
        }
    }
};

} // namespace flash::middleware

namespace flash {

template <class... Middleware>
class application {
public:
    application() requires(std::default_initializable<Middleware> && ...) = default;

    explicit application(Middleware... middleware)
        : middleware_{std::move(middleware)...} {}

    template <class Final>
    task<response_message> operator()(request_context& context, Final& final) {
        co_return co_await invoke<0>(context, final);
    }

private:
    template <std::size_t Index, class Final>
    task<response_message> invoke(request_context& context, Final& final) {
        if constexpr (Index == sizeof...(Middleware)) {
            co_return co_await std::invoke(final);
        } else {
            auto next = [this, &context, &final]() -> task<response_message> {
                co_return co_await invoke<Index + 1>(context, final);
            };
            using result_type = std::invoke_result_t<
                decltype(std::get<Index>(middleware_))&, request_context&, decltype(next)>;
            static_assert([] consteval {
                              if constexpr (is_task_v<result_type>) {
                                  return std::same_as<
                                      task_value_t<result_type>, response_message>;
                              }
                              return false;
                          }(),
                          "FLASH-E700: middleware must return flash::task<response_message>");
            co_return co_await std::invoke(
                std::get<Index>(middleware_), context, std::move(next));
        }
    }

    std::tuple<Middleware...> middleware_;
};

template <class>
inline constexpr bool is_application_v = false;

template <class... Middleware>
inline constexpr bool is_application_v<application<Middleware...>> = true;

} // namespace flash
