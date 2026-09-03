#include <flash/server.hpp>

#include <string>

int main() {
    flash::server_config config;
    config.port = 8080;

    return flash::serve_raw(config, [](flash::raw_request_view request,
                                       flash::raw_response_writer& response) -> flash::task<void> {
        if (request.method() == flash::http_method::get && request.path() == "/health") {
            response.content_type("application/json");
            co_await response.write("{\"status\":\"ok\"}");
            co_return;
        }

        response.status(flash::status::not_found);
        response.content_type("application/problem+json");
        co_await response.write(
            "{\"type\":\"about:blank\",\"title\":\"Not Found\",\"status\":404}");
    });
}

