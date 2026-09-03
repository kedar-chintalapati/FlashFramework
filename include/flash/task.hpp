#pragma once

#include <boost/asio/awaitable.hpp>

namespace flash {

template <class Value = void>
using task = boost::asio::awaitable<Value>;

} // namespace flash

