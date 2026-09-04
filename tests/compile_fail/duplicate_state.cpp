#include <flash/flash.hpp>

struct Services {};

namespace invalid_api {

[[=flash::get("/health")]]
int health() {
    return 1;
}

} // namespace invalid_api

int main() {
    Services first;
    Services second;
    flash::reflected_handler<
        ^^invalid_api,
        flash::openapi::documentation_mode::development,
        Services,
        Services> handler{first, second};
    (void)handler;
}
