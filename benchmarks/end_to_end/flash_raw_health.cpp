#include <flash/server.hpp>

#include <charconv>
#include <cstdint>
#include <string_view>

int main(int argument_count, char** arguments) {
    flash::server_config config;
    config.port = 8080;
    if (argument_count > 1) {
        unsigned parsed_port = config.port;
        const std::string_view text{arguments[1]};
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), parsed_port);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            parsed_port > 65'535U) {
            return 2;
        }
        config.port = static_cast<std::uint16_t>(parsed_port);
    }

    return flash::serve_raw(config, [](flash::raw_request_view request,
                                       flash::raw_response_writer& response) -> flash::task<void> {
        if (request.method() == flash::http_method::get && request.path() == "/health") {
            response.content_type("text/plain");
            response.status(flash::status::ok);
            co_await response.write("ok");
            co_return;
        }
        response.status(flash::status::not_found);
        response.content_type("text/plain");
        co_await response.write("not found");
    });
}
