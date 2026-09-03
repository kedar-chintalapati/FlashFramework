#pragma once

#include <flash/request.hpp>
#include <flash/response.hpp>
#include <flash/task.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace flash {

class raw_response_writer {
public:
    explicit raw_response_writer(response_message& response) noexcept
        : response_(&response) {}

    void status(flash::status value) noexcept {
        response_->status_code = value;
    }

    void header(std::string name, std::string value) {
        response_->set_header(std::move(name), std::move(value));
    }

    void content_type(std::string value) {
        response_->content_type = std::move(value);
    }

    void close_connection() noexcept {
        response_->keep_alive = false;
    }

    task<void> write(std::string_view bytes) {
        response_->body.append(bytes);
        co_return;
    }

private:
    response_message* response_;
};

} // namespace flash

