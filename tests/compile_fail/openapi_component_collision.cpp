#include <flash/openapi/schema.hpp>

struct [[=flash::schema_name("Payload")]] FirstPayload {
    int first{};
};

struct [[=flash::schema_name("Payload")]] SecondPayload {
    int second{};
};

namespace collision_api {

[[=flash::post("/first")]] int first(FirstPayload payload);
[[=flash::post("/second")]] int second(SecondPayload payload);

} // namespace collision_api

static_assert(flash::openapi::component_names_validated<^^collision_api>);
