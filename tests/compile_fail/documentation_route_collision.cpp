#include <flash/openapi/docs.hpp>

namespace reserved_api {

[[=flash::get("/docs")]] int docs();

} // namespace reserved_api

int main() {
    flash::request_view request;
    (void)flash::openapi::documentation_response<^^reserved_api>(request);
}
