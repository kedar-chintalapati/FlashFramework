#include <flash/binding/meta.hpp>

struct FirstBody {
    int value{};
};

struct SecondBody {
    int value{};
};

namespace body_api {

[[=flash::post("/items")]]
int create(FirstBody first, SecondBody second);

} // namespace body_api

constexpr auto function = flash::meta::endpoint_at<^^body_api, 0>();
static_assert(flash::binding::endpoint_binding_validated<function>);
