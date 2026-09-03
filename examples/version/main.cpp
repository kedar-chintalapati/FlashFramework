#include <flash/version.hpp>

#include <iostream>

int main() {
    std::cout << "Flash " << flash::runtime_version() << '\n';
}

