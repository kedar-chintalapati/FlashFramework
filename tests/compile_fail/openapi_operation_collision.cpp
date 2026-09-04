#include <flash/openapi/document.hpp>

namespace operation_api {

[[=flash::get("/first")]] int lookup();
[[=flash::get("/second")]] int lookup(int value);

} // namespace operation_api

consteval bool validate() {
    flash::openapi::detail::validate_document<^^operation_api>();
    return true;
}

static_assert(validate());
