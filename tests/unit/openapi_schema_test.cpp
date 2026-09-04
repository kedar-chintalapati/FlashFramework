#include <flash/openapi/schema.hpp>

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

struct [[=flash::schema_name("Address")]] ApiAddress {
    std::string city;
};

struct [[=flash::schema_name("CreateOrder")]] ApiOrder {
    [[=flash::name("product_name"), =flash::min_length(2)]]
    std::string product;
    [[=flash::minimum(1)]] std::uint32_t quantity{};
    std::optional<ApiAddress> address;
    std::vector<int> scores;
};

struct [[=flash::schema_name("Receipt")]] ApiReceipt {
    std::uint64_t id{};
};

struct [[=flash::schema_name("Tree")]] Tree {
    std::string name;
    std::vector<Tree> children;
};

namespace schema_api {

[[=flash::post("/orders")]]
ApiReceipt create(ApiOrder order);

} // namespace schema_api

using schemas = flash::openapi::api_schema_types<^^schema_api>;
static_assert(flash::openapi::type_list_size_v<schemas> == 3);
static_assert(flash::openapi::component_names_validated<^^schema_api>);

int main() {
    const auto schema = flash::openapi::component_schema<ApiOrder>();
    assert(schema.find(R"("product_name":{"type":"string","minLength":2})") !=
           std::string::npos);
    assert(schema.find(R"("quantity":{"type":"integer","format":"int32","minimum":1})") !=
           std::string::npos);
    assert(schema.find(R"("address":{"anyOf":[{"$ref":"#/components/schemas/Address"},{"type":"null"}]})") !=
           std::string::npos);
    assert(schema.find(R"("required":["product_name","quantity","scores"])") !=
           std::string::npos);
    const auto recursive = flash::openapi::component_schema<Tree>();
    assert(recursive.find(
               R"("children":{"type":"array","items":{"$ref":"#/components/schemas/Tree"}})") !=
           std::string::npos);
}
