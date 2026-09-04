#include <flash/flash.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

struct [[=flash::schema_name("CreateWidget")]] CreateWidget {
    [[=flash::min_length(1), =flash::max_length(64)]]
    std::string name;
    [[=flash::minimum(1)]] std::uint32_t quantity{};
};

struct [[=flash::schema_name("Widget")]] Widget {
    std::uint64_t id{};
    std::string name;
    std::uint32_t quantity{};
};

namespace export_api {

[[=flash::get("/widgets/{id}"), =flash::description("Read one widget")]]
Widget get_widget(std::uint64_t id);

[[=flash::post("/widgets")]]
flash::created<Widget> create_widget(CreateWidget input);

} // namespace export_api

int main(int argument_count, char** arguments) {
    if (argument_count != 2) {
        std::cerr << "usage: flash_openapi_export <output-file>\n";
        return 2;
    }
    const std::filesystem::path output_path{arguments[1]};
    std::ofstream output{output_path, std::ios::binary | std::ios::trunc};
    if (!output) {
        std::cerr << "could not open output file\n";
        return 3;
    }
    output << flash::openapi::generate_document<^^export_api>(
        "Flash example API", "0.1.0") << '\n';
    if (!output) {
        std::cerr << "could not write output file\n";
        return 4;
    }
}
