#include <flash/binding/request.hpp>
#include <flash/binding/scalar.hpp>
#include <flash/json/read.hpp>
#include <flash/json/write.hpp>
#include <flash/problem.hpp>
#include <flash/request.hpp>
#include <flash/response.hpp>
#include <flash/routing/matcher.hpp>
#include <flash/routing/route.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#endif

namespace {

struct text_payload {
    int value{};
    std::string name;
};

struct constrained_payload {
    [[=flash::minimum(1)]] int value{};
};

template <class Value>
inline void keep(Value const& value) {
#if defined(__GNUC__)
    asm volatile("" : : "g"(&value) : "memory");
#else
    (void)value;
#endif
}

std::uint64_t read_cycles() noexcept {
#if defined(__i386__) || defined(__x86_64__)
    _mm_lfence();
    const auto value = __rdtsc();
    _mm_lfence();
    return value;
#else
    return 0;
#endif
}

template <class Operation>
void measure(std::string_view name,
             std::size_t iterations,
             std::size_t bytes_per_operation,
             Operation operation) {
    std::uint64_t checksum = 0;
    const auto warmup_count = std::min<std::size_t>(iterations, 1'000);
    for (std::size_t index = 0; index < warmup_count; ++index) {
        const auto value = static_cast<std::uint64_t>(operation(index));
        keep(value);
        checksum += value;
    }

    const auto cycle_start = read_cycles();
    const auto time_start = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        const auto value = static_cast<std::uint64_t>(operation(index));
        keep(value);
        checksum += value;
    }
    const auto time_finish = std::chrono::steady_clock::now();
    const auto cycle_finish = read_cycles();
    const auto elapsed_ns = std::chrono::duration<double, std::nano>{
        time_finish - time_start}.count();
    const auto divisor = static_cast<double>(iterations);

    std::cout << "{\"case\":\"" << name << "\",\"iterations\":" << iterations
              << ",\"bytes_per_operation\":" << bytes_per_operation
              << ",\"nanoseconds_per_operation\":" << elapsed_ns / divisor
              << ",\"cycles_per_operation\":"
              << static_cast<double>(cycle_finish - cycle_start) / divisor
              << ",\"checksum\":" << checksum << "}\n";
}

consteval flash::routing::route_pattern make_literal_route(std::size_t index) {
    flash::routing::route_pattern pattern;
    pattern.source.length = 6;
    pattern.source.characters[0] = '/';
    pattern.source.characters[1] = 'r';
    pattern.segments[0].kind = flash::routing::segment_kind::literal;
    pattern.segments[0].text.length = 5;
    pattern.segments[0].text.characters[0] = 'r';
    for (std::size_t digit = 0; digit < 4; ++digit) {
        const auto divisor = std::array<std::size_t, 4>{1'000, 100, 10, 1}[digit];
        const auto character = static_cast<char>('0' + (index / divisor) % 10);
        pattern.source.characters[digit + 2] = character;
        pattern.segments[0].text.characters[digit + 1] = character;
    }
    pattern.segment_count = 1;
    return pattern;
}

template <std::size_t Count>
consteval auto make_literal_routes() {
    std::array<flash::routing::route_pattern, Count> routes{};
    for (std::size_t index = 0; index < Count; ++index) {
        routes[index] = make_literal_route(index);
    }
    return routes;
}

template <std::size_t Count>
std::size_t scan_routes(const std::array<flash::routing::route_pattern, Count>& routes,
                        std::string_view path) noexcept {
    for (std::size_t index = 0; index < routes.size(); ++index) {
        if (flash::routing::match_route(routes[index], path).matched) {
            return index + 1;
        }
    }
    return 0;
}

template <std::size_t Count>
void measure_route_count(std::size_t base_iterations) {
    static constexpr auto routes = make_literal_routes<Count>();
    constexpr auto first_path = [] {
        if constexpr (Count == 1) {
            return std::string_view{"/r0000"};
        } else if constexpr (Count == 10) {
            return std::string_view{"/r0009"};
        } else if constexpr (Count == 100) {
            return std::string_view{"/r0099"};
        } else {
            return std::string_view{"/r0999"};
        }
    }();
    constexpr auto second_path = [] {
        if constexpr (Count == 1) {
            return std::string_view{"/r0000"};
        } else if constexpr (Count == 10) {
            return std::string_view{"/r0008"};
        } else if constexpr (Count == 100) {
            return std::string_view{"/r0098"};
        } else {
            return std::string_view{"/r0998"};
        }
    }();
    const auto iterations = std::max<std::size_t>(
        2'000, base_iterations / std::max<std::size_t>(Count, 1));
    const std::string name = "route_literal_" + std::to_string(Count);
    measure(name, iterations, first_path.size(), [&](std::size_t index) {
        const auto path = index % 2 == 0 ? first_path : second_path;
        keep(path);
        return scan_routes(routes, path);
    });
}

std::string make_json_body(std::size_t name_size, char fill) {
    return "{\"value\":42,\"name\":\"" + std::string(name_size, fill) + "\"}";
}

} // namespace

int main(int argument_count, char** arguments) {
    std::size_t base_iterations = 500'000;
    if (argument_count > 1) {
        base_iterations = std::stoull(arguments[1]);
        if (base_iterations == 0) {
            return 2;
        }
    }

    measure("method_mask", base_iterations * 4, 1, [](std::size_t index) {
        const auto method = static_cast<flash::http_method>(index % 7);
        return flash::routing::method_mask(method);
    });

    measure_route_count<1>(base_iterations);
    measure_route_count<10>(base_iterations);
    measure_route_count<100>(base_iterations);
    measure_route_count<1'000>(base_iterations);

    constexpr auto parameter_route = flash::routing::parse_route("/users/{id}");
    constexpr std::array parameter_paths{
        std::string_view{"/users/42"}, std::string_view{"/users/123456"}};
    measure("route_parameter", base_iterations, 12, [&](std::size_t index) {
        const auto matched = flash::routing::match_route(
            parameter_route, parameter_paths[index % parameter_paths.size()]);
        return matched.capture_count == 0 ? 0 : matched.captures[0].value.size();
    });

    constexpr auto catch_all_route = flash::routing::parse_route("/assets/{*path}");
    constexpr std::array catch_all_paths{
        std::string_view{"/assets/css/app.css"},
        std::string_view{"/assets/images/icon.svg"}};
    measure("route_catch_all", base_iterations, 22, [&](std::size_t index) {
        const auto matched = flash::routing::match_route(
            catch_all_route, catch_all_paths[index % catch_all_paths.size()]);
        return matched.capture_count == 0 ? 0 : matched.captures[0].value.size();
    });

    constexpr std::array integer_values{
        std::string_view{"42"}, std::string_view{"2147483647"}};
    measure("parse_integer", base_iterations, 6, [&](std::size_t index) {
        const auto value = flash::binding::parse_scalar<int>(
            integer_values[index % integer_values.size()]);
        return value ? static_cast<std::uint64_t>(*value) : 0;
    });

    constexpr std::array floating_values{
        std::string_view{"3.1415926"}, std::string_view{"-0.000125"}};
    measure("parse_float", base_iterations, 9, [&](std::size_t index) {
        const auto value = flash::binding::parse_scalar<double>(
            floating_values[index % floating_values.size()]);
        return value ? std::bit_cast<std::uint64_t>(*value) : 0;
    });

    constexpr std::array boolean_values{
        std::string_view{"true"}, std::string_view{"false"}};
    measure("parse_boolean", base_iterations, 5, [&](std::size_t index) {
        const auto value = flash::binding::parse_scalar<bool>(
            boolean_values[index % boolean_values.size()]);
        return value && *value ? 1 : 0;
    });

    constexpr std::array query_targets{
        std::string_view{"/items?target=42"},
        std::string_view{"/items?a=1&b=2&c=3&d=4&e=5&f=6&g=7&target=42"},
        std::string_view{
            "/items?k00=0&k01=1&k02=2&k03=3&k04=4&k05=5&k06=6&k07=7&"
            "k08=8&k09=9&k10=10&k11=11&k12=12&k13=13&k14=14&k15=15&"
            "k16=16&k17=17&k18=18&k19=19&k20=20&k21=21&k22=22&k23=23&"
            "k24=24&k25=25&k26=26&k27=27&k28=28&k29=29&k30=30&target=42"}};
    constexpr std::array query_names{
        std::string_view{"query_scan_1"},
        std::string_view{"query_scan_8"},
        std::string_view{"query_scan_32"}};
    for (std::size_t query_index = 0; query_index < query_targets.size(); ++query_index) {
        const flash::request_view request{
            flash::http_method::get, query_targets[query_index], {}, {}, true};
        measure(query_names[query_index],
                std::max<std::size_t>(1, base_iterations / (query_index + 1)),
                request.query().size(),
                [&](std::size_t) {
                    const auto value = flash::binding::find_query_value(request, "target");
                    return value && *value ? (**value).size() : 0;
                });
    }

    const std::array json_inputs{
        make_json_body(78, 'a'),
        make_json_body(1'002, 'b'),
        make_json_body(65'514, 'c')};
    const std::array json_names{
        std::string_view{"json_read_100"},
        std::string_view{"json_read_1k"},
        std::string_view{"json_read_64k"}};
    for (std::size_t json_index = 0; json_index < json_inputs.size(); ++json_index) {
        const auto iterations = std::max<std::size_t>(
            200, base_iterations / std::array<std::size_t, 3>{10, 50, 1'000}[json_index]);
        measure(json_names[json_index],
                iterations,
                json_inputs[json_index].size(),
                [&](std::size_t) {
                    const auto value = flash::json::read<text_payload>(json_inputs[json_index]);
                    return value ? value->name.size() + static_cast<std::size_t>(value->value) : 0;
                });
    }

    const std::array text_payloads{
        text_payload{42, std::string(78, 'a')},
        text_payload{42, std::string(1'002, 'b')},
        text_payload{42, std::string(65'514, 'c')}};
    const std::array write_names{
        std::string_view{"json_write_100"},
        std::string_view{"json_write_1k"},
        std::string_view{"json_write_64k"}};
    std::string output;
    output.reserve(70'000);
    for (std::size_t json_index = 0; json_index < text_payloads.size(); ++json_index) {
        const auto iterations = std::max<std::size_t>(
            200, base_iterations / std::array<std::size_t, 3>{10, 50, 1'000}[json_index]);
        measure(write_names[json_index],
                iterations,
                json_inputs[json_index].size(),
                [&](std::size_t) {
                    output.clear();
                    flash::json::append(output, text_payloads[json_index]);
                    return output.size();
                });
    }

    measure("validation_failure",
            std::max<std::size_t>(1, base_iterations / 5),
            11,
            [](std::size_t) {
                const auto value =
                    flash::json::read<constrained_payload>("{\"value\":0}");
                return value ? 0 : value.error().code.size();
            });

    measure("problem_response",
            std::max<std::size_t>(1, base_iterations / 20),
            0,
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
            std::max<std::size_t>(1, base_iterations / 20),
            0,
            [](std::size_t index) {
                flash::response_message response;
                response.set_header("Cache-Control", "no-store");
                response.set_header(
                    "X-Request-ID", index % 2 == 0 ? "request-a" : "request-b");
                response.set_header("Content-Language", "en-US");
                return response.headers.size();
            });
}
