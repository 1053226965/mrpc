#pragma once
#include <experimental/coroutine>
#include <future>
#include <optional>
#include "coroutine_helper.hpp"

namespace mrpc
{
  namespace detail
  {
    template<typename T>
    class sync_wait_task_t;

    template<typename T>
    class sync_promise_t
    {
    public:
      using value_type = std::optional<std::remove_reference_t<T>>;

      sync_promise_t() noexcept:
        promise_()
      {}

      auto initial_suspend() noexcept
      {
        return std::experimental::suspend_always();
      }

      auto final_suspend() noexcept
      {
        return std::experimental::suspend_always();
      }

      template<typename VT>
      void return_value(VT&& value) noexcept
      {
        promise_.set_value(std::forward<VT>(value));
      }

      void unhandled_exception() noexcept
      {
        promise_.set_exception(std::current_exception());
      }

      auto get_return_object() noexcept
      {
        return std::experimental::coroutine_handle<sync_promise_t<T>>::from_promise(*this);
      }

      std::future<value_type> get_future() noexcept 
      { 
        return promise_.get_future(); 
      }

    private:
      std::promise<value_type> promise_;
    };

    template<>
    class sync_promise_t<void>
    {
    public:
      sync_promise_t() noexcept :
        promise_()
      {}

      auto initial_suspend() noexcept
      {
        return std::experimental::suspend_always();
      }

      auto final_suspend() noexcept
      {
        return std::experimental::suspend_always();
      }

      void return_void() noexcept 
      { 
        promise_.set_value();
      }

      void unhandled_exception() noexcept
      {
        promise_.set_exception(std::current_exception());
      }

      auto get_return_object() noexcept
      {
        return std::experimental::coroutine_handle<sync_promise_t<void>>::from_promise(*this);
      }

      std::future<void> get_future() noexcept
      {
        return promise_.get_future();
      }

    private:
      std::promise<void> promise_;
    };

    template<typename T>
    class sync_wait_task_t
    {
    public:
      using value_type = std::conditional_t<std::is_void_v<T>, void, std::optional<std::remove_reference_t<T>>>;
      using promise_type = sync_promise_t<T> ;

      sync_wait_task_t(std::experimental::coroutine_handle<promise_type> coro) :
        coroutine_(coro),
        future_(coro.promise().get_future())
      {
      }

      void start()
      {
        coroutine_.resume();
      }

      auto wait_result()
      {
        if constexpr (std::is_void_v<value_type>)
        {
          future_.get();
        }
        else
        {
          return std::move(future_.get().value());
        }
      }

    private:
      std::experimental::coroutine_handle<> coroutine_;
      std::future<value_type> future_;
    };
  }

  template<typename AWAITABLE,
    typename RET = typename coroutine_trait<AWAITABLE>::result_type>
    auto sync_wait(AWAITABLE&& awaitable)
    -> RET
  {
    auto task_fun = [&awaitable]() -> detail::sync_wait_task_t<RET> {
      co_return co_await awaitable;
    };
    auto task = task_fun();
    task.start();
    if constexpr (std::is_void_v<RET>)
      task.wait_result();
    else
      return std::move(task.wait_result());
  }
}