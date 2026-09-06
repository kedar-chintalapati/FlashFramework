#include <flash/server.hpp>

int main() {
    flash::server_config config;
    return flash::serve_raw(
        config,
        [](flash::raw_request_view, flash::raw_response_writer&) {
            return 1;
        });
}
