#pragma once

#include <flash/request.hpp>

#include <string>
#include <utility>

namespace flash {

struct request_context {
    request_view request;
    std::string request_id;
};

template <class Value>
class state {
public:
    explicit state(Value& value) noexcept : value_(&value) {}

    [[nodiscard]] Value* operator->() const noexcept { return value_; }
    [[nodiscard]] Value& operator*() const noexcept { return *value_; }

private:
    Value* value_;
};

} // namespace flash

