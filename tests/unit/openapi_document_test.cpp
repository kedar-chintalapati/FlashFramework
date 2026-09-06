#include <flash/openapi/document.hpp>

#include <cassert>
#include <cstdint>
#include <expected>
#include <string>

struct [[=flash::schema_name("Pet")]] Pet {
    std::uint64_t id{};
    [[=flash::min_length(1)]] std::string name;
};

struct [[=flash::schema_name("CreatePet")]] CreatePet {
    [[=flash::min_length(1)]] std::string name;
};

enum class pet_error {
    missing,
};

template <>
inline constexpr auto flash::error_map<pet_error> = flash::errors(
    flash::map<pet_error::missing>(
        flash::status::not_found, "pet_missing", "Pet not found"));

namespace document_api {

[[=flash::get("/pets/{id}"), =flash::description("Read one pet")]]
std::expected<Pet, pet_error> get_pet(std::uint64_t id);

[[=flash::post("/pets")]]
flash::created<Pet> create_pet(CreatePet input);

[[=flash::get("/search")]]
flash::text search(
    [[=flash::query("q"), =flash::min_length(2),
      =flash::description("Search text"), =flash::deprecated]]
    std::string query);

} // namespace document_api

int main() {
    const auto first = flash::openapi::generate_document<^^document_api>();
    const auto second = flash::openapi::generate_document<^^document_api>();
    assert(first == second);
    assert(first.starts_with(R"({"openapi":"3.1.1")"));
    assert(first.find(R"("/pets/{id}":{"get":{"operationId":"get_pet")") !=
           std::string::npos);
    assert(first.find(R"("description":"Read one pet")") != std::string::npos);
    assert(first.find(R"("name":"id","in":"path","required":true)") !=
           std::string::npos);
    assert(first.find(R"("post":{"operationId":"create_pet")") !=
           std::string::npos);
    assert(first.find(R"("requestBody":{"required":true)") != std::string::npos);
    assert(first.find(
               R"("name":"q","in":"query","description":"Search text","deprecated":true,"required":true,"schema":{"type":"string","minLength":2})") !=
           std::string::npos);
    assert(first.find(R"("201":{"description":"Successful response")") !=
           std::string::npos);
    assert(first.find(R"("404":{"$ref":"#/components/responses/Problem"})") !=
           std::string::npos);
    assert(first.find(R"("Pet":{"type":"object")") != std::string::npos);
    assert(&flash::openapi::document<^^document_api>() ==
           &flash::openapi::document<^^document_api>());
}
