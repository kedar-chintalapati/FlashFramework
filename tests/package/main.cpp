#include <flash/json/read.hpp>
#include <flash/json/write.hpp>
#include <flash/version.hpp>

struct record {
    int value{};
};

int main() {
    if (flash::runtime_version() != flash::version) {
        return 3;
    }
    const auto result = flash::json::read<record>("{\"value\":42}");
    if (!result || result->value != 42) {
        return 1;
    }
    return flash::json::write(*result) == "{\"value\":42}" ? 0 : 2;
}
