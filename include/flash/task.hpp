#pragma once

#include <boost/asio/awaitable.hpp>

#include <type_traits>

namespace flash {

template <class Value = void>
using task = boost::asio::awaitable<Value>;

template <class>
struct task_traits {
    static constexpr bool is_task = false;
};

template <class Value, class Executor>
struct task_traits<boost::asio::awaitable<Value, Executor>> {
    static constexpr bool is_task = true;
    using value_type = Value;
};

template <class Value>
inline constexpr bool is_task_v = task_traits<std::remove_cvref_t<Value>>::is_task;

template <class Value>
using task_value_t = typename task_traits<std::remove_cvref_t<Value>>::value_type;

} // namespace flash
