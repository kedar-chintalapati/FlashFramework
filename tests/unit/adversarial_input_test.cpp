#include <flash/binding/request.hpp>
#include <flash/json/read.hpp>
#include <flash/json/write.hpp>
#include <flash/response.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct corpus_record {
    int value{};
    std::string name;
};

int main() {
    constexpr std::array malformed{
        std::string_view{""}, std::string_view{"{"},
        std::string_view{"["}, std::string_view{"null"},
        std::string_view{R"({"value":01,"name":"x"})"},
        std::string_view{R"({"value":1e999,"name":"x"})"},
        std::string_view{R"({"value":1,"value":2,"name":"x"})"},
        std::string_view{R"({"value":1,"name":"\uD800"})"},
        std::string_view{R"({"value":1,"name":"\uDC00"})"},
        std::string_view{R"({"value":1,"name":"\uD800\u0041"})"},
        std::string_view{R"({"value":1,"name":"x",})"},
        std::string_view{R"({"value":1,"name":"x"}true)"}};
    for (const auto input : malformed) {
        if (flash::json::read<corpus_record>(input)) {
            return 1;
        }
    }

    flash::json::read_limits limits;
    limits.maximum_input_bytes = 256;
    limits.maximum_depth = 4;
    limits.maximum_array_elements = 3;
    limits.maximum_object_members = 3;
    limits.maximum_string_bytes = 8;
    limits.reject_unknown_fields = false;
    if (flash::json::read<std::vector<int>>("[1,2,3,4]", limits) ||
        flash::json::read<std::string>(R"("123456789")", limits) ||
        flash::json::read<corpus_record>(
            R"({"value":1,"name":"x","extra":[[[[[0]]]]]})", limits) ||
        flash::json::read<corpus_record>(
            R"({"value":1,"name":"x","a":0,"b":0})", limits) ||
        flash::json::read<std::string>(std::string(257, ' '), limits)) {
        return 2;
    }
    if (!flash::json::read<std::vector<int>>("[1,2,3]", limits) ||
        !flash::json::read<std::string>(R"("12345678")", limits)) {
        return 3;
    }

    constexpr std::array escapes{
        std::string_view{"%"}, std::string_view{"%0"},
        std::string_view{"%GG"}, std::string_view{"abc%4Z"}};
    for (const auto input : escapes) {
        if (flash::binding::percent_decode(input)) {
            return 4;
        }
    }
    const flash::request_view duplicate{
        flash::http_method::get, "/?name=a&%6Eame=b", {}, {}, false};
    if (flash::binding::find_query_value(duplicate, "name")) {
        return 5;
    }
    if (flash::valid_header_value("ok\r\nInjected: yes") ||
        flash::valid_header_name("Bad Name")) {
        return 6;
    }

    std::uint32_t random = 0x915f07a3U;
    constexpr std::string_view seed = R"({"value":42,"name":"sample"})";
    for (std::size_t trial = 0; trial < 10000; ++trial) {
        std::string input{seed};
        random = random * 1664525U + 1013904223U;
        const auto index = static_cast<std::size_t>(random) % input.size();
        random = random * 1664525U + 1013904223U;
        input[index] = static_cast<char>(random & 255U);
        if (trial % 3 == 0) {
            input.resize(index);
        }
        const auto value = flash::json::read<corpus_record>(input, limits);
        if (value) {
            const auto roundtrip = flash::json::read<corpus_record>(
                flash::json::write(*value), limits);
            if (!roundtrip || roundtrip->value != value->value ||
                roundtrip->name != value->name) {
                return 7;
            }
        }
    }
}
