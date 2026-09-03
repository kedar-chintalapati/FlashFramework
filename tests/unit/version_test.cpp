#include <flash/version.hpp>

int main() {
    return flash::runtime_version() == flash::version ? 0 : 1;
}

