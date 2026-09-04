#include <flash/flash.hpp>

struct Services {};

namespace invalid_api {

[[=flash::get("/state")]]
int endpoint(flash::state<Services>& services) {
    return services.operator->() != nullptr;
}

} // namespace invalid_api

int main() {
    flash::request_view request{
        flash::http_method::get, "/state", {}, {}, false};
    auto result = flash::dispatch<^^invalid_api>(request);
    (void)result;
}
