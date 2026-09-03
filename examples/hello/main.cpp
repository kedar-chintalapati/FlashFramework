#include <flash/flash.hpp>

namespace api {

[[=flash::get("/hello")]]
flash::text hello() {
    return {"Hello, world!"};
}

[[=flash::get("/add/{a}/{b}")]]
int add(int a, int b) {
    return a + b;
}

} // namespace api

int main() {
    return flash::serve<^^api>({
        .address = "127.0.0.1",
        .port = 8080,
    });
}

