#include <flash/json/schema.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

struct Product {
    [[=flash::name("product_name"), =flash::min_length(1)]]
    std::string name;

    [[=flash::minimum(1)]]
    std::uint32_t quantity{};
};

static_assert(flash::json::reflectable_object<Product>);
static_assert(flash::json::field_count<Product> == 2);
static_assert(flash::json::field_wire_name<Product, 0>() == "product_name");
static_assert(flash::json::field_wire_name<Product, 1>() == "quantity");
static_assert(std::same_as<flash::json::field_type_t<Product, 0>, std::string>);
static_assert(std::same_as<flash::json::field_type_t<Product, 1>, std::uint32_t>);
static_assert(flash::json::field_constraint_count<Product, 0> == 1);
static_assert(flash::json::field_constraint<Product, 1, 0>().kind ==
              flash::constraint_kind::minimum);
static_assert(flash::json::input_schema_validated<Product>);

int main() {
    Product product{"widget", 2};
    product.[:flash::json::field_reflection<Product, 1>:] = 3;
    return product.quantity == 3 ? 0 : 1;
}
