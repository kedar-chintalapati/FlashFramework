#include <flash/flash.hpp>

struct invalid_middleware {
    template <class Next>
    int operator()(flash::request_context&, Next) const {
        return 0;
    }
};

int main() {
    flash::application<invalid_middleware> application{invalid_middleware{}};
    flash::request_context context{
        .request = {flash::http_method::get, "/", {}, {}, false},
        .request_id = {},
        .stop_token = {},
    };
    auto final = []() -> flash::task<flash::response_message> {
        co_return flash::response_message{};
    };
    auto result = application(context, final);
    (void)result;
}
