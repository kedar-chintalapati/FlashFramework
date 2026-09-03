#include <flash/flash.hpp>

#include <cstdint>
#include <meta>
#include <string>
#include <string_view>

struct CreateWidget {
    [[=flash::min_length(1), =flash::max_length(64)]]
    std::string name;

    [[=flash::minimum(1), =flash::maximum(10'000)]]
    std::uint32_t quantity;
};

namespace reflection_api {

[[=flash::get("/widgets/{id}")]]
constexpr std::uint64_t find_widget(
    std::uint64_t id,
    [[=flash::query("include_details")]] bool details) {
    return id + (details ? 1U : 0U);
}

[[=flash::post("/widgets")]]
constexpr std::uint32_t create_widget(CreateWidget widget) {
    return widget.quantity;
}

constexpr int helper_not_an_endpoint() {
    return 7;
}

} // namespace reflection_api

constexpr auto find_widget_info = flash::meta::endpoint_at<^^reflection_api, 0>();
constexpr auto create_widget_info = flash::meta::endpoint_at<^^reflection_api, 1>();

static_assert(flash::meta::endpoint_count<^^reflection_api>() == 2);
static_assert(std::meta::identifier_of(find_widget_info) == "find_widget");
static_assert(std::meta::identifier_of(create_widget_info) == "create_widget");

static_assert(flash::meta::route_of<find_widget_info>().method == flash::http_method::get);
static_assert(flash::meta::route_of<find_widget_info>().path.view() == "/widgets/{id}");
static_assert(flash::meta::route_of<create_widget_info>().method == flash::http_method::post);
static_assert(flash::meta::parameter_count<find_widget_info>() == 2);
static_assert(flash::meta::parameter_name<find_widget_info, 0>() == "id");
static_assert(flash::meta::parameter_name<find_widget_info, 1>() == "details");

constexpr auto details_parameter = flash::meta::parameter_at<find_widget_info, 1>();
static_assert(std::meta::annotations_of_with_type(details_parameter, ^^flash::source_annotation).size() == 1);
static_assert(std::meta::extract<flash::source_annotation>(
                  std::meta::annotations_of_with_type(details_parameter, ^^flash::source_annotation)[0])
                  .wire_name.view() == "include_details");

static_assert(flash::meta::data_member_count<CreateWidget>() == 2);
constexpr auto quantity_member = flash::meta::data_member_at<CreateWidget, 1>();
static_assert(std::meta::identifier_of(quantity_member) == "quantity");
static_assert(std::meta::annotations_of_with_type(quantity_member, ^^flash::constraint_annotation).size() == 2);

static_assert(flash::meta::invoke<find_widget_info>(41U, true) == 42U);

int main() {}

