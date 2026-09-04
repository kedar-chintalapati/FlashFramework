#include <flash/json/read.hpp>
#include <flash/json/write.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class Priority { low, high };

struct Address {
    std::string city;
};

struct Order {
    [[=flash::name("product_name"), =flash::min_length(2)]]
    std::string product;

    [[=flash::minimum(1), =flash::maximum(10)]]
    std::uint32_t quantity{};

    std::optional<std::string> note;

    [[=flash::min_items(1), =flash::max_items(3)]]
    std::vector<int> scores;

    std::array<bool, 2> flags{};
    Priority priority{};
    Address address;
};

struct Defaulted {
    [[=flash::default_value(7)]] int value{};
    std::optional<int> optional;
};

int main() {
    constexpr std::string_view valid = R"({
        "product_name":"A\uD83D\uDE80",
        "quantity":3,
        "note":null,
        "scores":[1,-2,3],
        "flags":[true,false],
        "priority":"high",
        "address":{"city":"Seattle"}
    })";
    auto order = flash::json::read<Order>(valid);
    assert(order);
    assert(order->product == "A\xF0\x9F\x9A\x80");
    assert(order->quantity == 3);
    assert(!order->note);
    assert((order->scores == std::vector<int>{1, -2, 3}));
    assert((order->flags == std::array<bool, 2>{true, false}));
    assert(order->priority == Priority::high);
    assert(order->address.city == "Seattle");
    const std::string expected_json =
        std::string{R"({"product_name":"A)"} + "\xF0\x9F\x9A\x80" +
        R"(","quantity":3,"note":null,"scores":[1,-2,3],"flags":[true,false],"priority":"high","address":{"city":"Seattle"}})";
    assert(flash::json::write(*order) == expected_json);

    auto defaulted = flash::json::read<Defaulted>(R"({})");
    assert(defaulted && defaulted->value == 7 && !defaulted->optional);

    const std::string borrowed_input = R"("borrowed")";
    auto borrowed = flash::json::read<std::string_view>(borrowed_input);
    assert(borrowed && *borrowed == "borrowed");
    auto escaped_borrow = flash::json::read<std::string_view>(R"("not\tborrowed")");
    assert(!escaped_borrow && escaped_borrow.error().code == "escaped_borrowed_string");

    auto duplicate = flash::json::read<Order>(
        R"({"product_name":"ok","product_name":"again"})");
    assert(!duplicate && duplicate.error().code == "duplicate_field");

    auto unknown = flash::json::read<Defaulted>(R"({"extra":{"nested":[1,2]}})");
    assert(!unknown && unknown.error().code == "unknown_field");
    flash::json::read_limits permissive;
    permissive.reject_unknown_fields = false;
    auto ignored = flash::json::read<Defaulted>(
        R"({"extra":{"nested":[1,2]}})", permissive);
    assert(ignored && ignored->value == 7);

    auto missing = flash::json::read<Address>(R"({})");
    assert(!missing && missing.error().code == "missing_field" &&
           missing.error().path == "$.city");
    auto null_required = flash::json::read<Address>(R"({"city":null})");
    assert(!null_required && null_required.error().code == "null_not_allowed");
    auto constraint = flash::json::read<Order>(R"({"product_name":"x"})");
    assert(!constraint && constraint.error().code == "constraint_failed");
    auto overflow = flash::json::read<std::uint8_t>("256");
    assert(!overflow && overflow.error().code == "number_out_of_range");
    auto leading_zero = flash::json::read<int>("01");
    assert(!leading_zero && leading_zero.error().code == "invalid_number");
    auto short_array = flash::json::read<std::array<int, 2>>("[1]");
    assert(!short_array && short_array.error().code == "array_size");
    auto bad_surrogate = flash::json::read<std::string>(R"("\uD800x")");
    assert(!bad_surrogate && bad_surrogate.error().code == "invalid_unicode");
    const std::string invalid_utf8{"\xC0\xAF", 2};
    auto bad_utf8 = flash::json::read<std::string>('"' + invalid_utf8 + '"');
    assert(!bad_utf8 && bad_utf8.error().code == "invalid_utf8");
    auto trailing = flash::json::read<bool>("true false");
    assert(!trailing && trailing.error().code == "trailing_characters");
}
