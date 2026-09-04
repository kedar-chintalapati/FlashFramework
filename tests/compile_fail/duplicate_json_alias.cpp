#include <flash/json/schema.hpp>

struct DuplicateAlias {
    [[=flash::name("value")]] int first{};
    [[=flash::name("value")]] int second{};
};

static_assert(flash::json::output_schema_validated<DuplicateAlias>);
