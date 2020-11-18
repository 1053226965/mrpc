#pragma once
#include <tuple>
#include <atomic>
#include <functional>
#include <experimental/coroutine>
#include "task.hpp"
#include "coroutine_helper.hpp"

namespace mrpc::detail
{
  struct when_all_promise_base_t
  {
    auto initial_suspend() noexcept { return std::experimental::suspend_always(); }
    auto final_suspend() noexcept { return std::experimental::suspend_always(); }
    void set_notify(std::function<void()> const& notify) noexcept { notify_ = notify; }
    void notify() { notify_(); }

    std::function<void()> notify_;
  };

  template<typename T>
  class when_all_promise_t : public when_all_promise_base_t
  {
    using coroutine_handle_type = std::experimental::coroutine_handle<when_all_promise_t<T>>;
  public:
    using value_type = std::remove_reference_t<T>;

    when_all_promise_t() noexcept:
      type_(result_type::empty),
      excetion_ptr_(nullptr)
    {}

    ~when_all_promise_t() noexcept {}

    auto get_return_object() noexcept { return coroutine_handle_type::from_promise(*this); }

    void unhandled_exception() noexcept
    {
      ::new (static_cast<void*>(std::addressof(excetion_ptr_))) std::exception_ptr(std::current_exception());
      type_ = result_type::exception;
      notify();
    }

    template<typename VT, typename = std::enable_if_t<std::is_convertible_v<VT&&, T>>>
    void return_value(VT&& value) noexcept
    {
      ::new (static_cast<void*>(std::addressof(value_))) value_type(std::forward<VT>(value));
      type_ = result_type::value;
      notify();
    }

    T result() noexcept
    {
      if (type_ == result_type::exception) {
        std::rethrow_exception(excetion_ptr_);
      }
      M_ASSERT(type_ == result_type::value);
      return std::move(value_);
    }

  private:
    enum class result_type { empty, value, exception };

    result_type type_;
    union
    {
      std::exception_ptr excetion_ptr_;
      value_type value_;
    };
  };

  template<>
  class when_all_promise_t<void> : public when_all_promise_base_t
  {
    using coroutine_handle_type = std::experimental::coroutine_handle<when_all_promise_t<void>>;
  public:
    when_all_promise_t() noexcept
    {}

    ~when_all_promise_t() noexcept {}

    auto get_return_object() noexcept { return coroutine_handle_type::from_promise(*this); }

    void unhandled_exception() noexcept 
    { 
      excetion_ptr_ = std::current_exception(); 
      notify();
    }

    void return_void() noexcept { notify(); }

    void result() noexcept
    {
      if (excetion_ptr_) {
        std::rethrow_exception(excetion_ptr_);
      }
    }

  private:
    std::exception_ptr excetion_ptr_;
  };

  template<typename T>
  class whell_all_task_t
  {
    whell_all_task_t(whell_all_task_t const&) = delete;
    whell_all_task_t& operator=(whell_all_task_t const&) = delete;
  public:
    using promise_type = when_all_promise_t<T>;

    whell_all_task_t(std::experimental::coroutine_handle<promise_type> handle) noexcept :
      coroutine_(handle)
    {}

    whell_all_task_t(whell_all_task_t&& t) noexcept :
      coroutine_(std::move(t.coroutine_))
    {
      t.coroutine_ = nullptr;
    }

    whell_all_task_t& operator=(whell_all_task_t&& t) noexcept
    {
      coroutine_ = t.coroutine_;
      t.coroutine_ = nullptr;
      return *this;
    }

    void set_notify(std::function<void()> notify) noexcept
    {
      coroutine_.promise().set_notify(notify);
    }

    void start() 
    {
      coroutine_.resume();
    }

    T result()
    {
      if constexpr (std::is_void_v<T>)
        coroutine_.promise().result();
      else
        return coroutine_.promise().result();
    }

  private:
    std::experimental::coroutine_handle<promise_type> coroutine_;
  };

  template<typename TUPLE_TASK>
  class when_all_task_awaitable_t
  {
    template<size_t... INDICES>
    void awaitable_unfold(std::integer_sequence<size_t, INDICES...>)
    {
      auto notify = [this]() {
        if (notifyed_count_.fetch_add(1, std::memory_order_relaxed) == std::tuple_size_v<TUPLE_TASK> - 1)
          coroutine_.resume();
      };
      auto func = [&notify](auto&& when_all_task) {
        when_all_task.set_notify(notify);
        when_all_task.start();
        return 0;
      };
      int unused[] = { func(std::get<INDICES>(tuple_))... };
      (void)unused;
    }
  public:

    using tuple_task_type = TUPLE_TASK;

    when_all_task_awaitable_t(tuple_task_type&& c) noexcept :
      notifyed_count_(0),
      tuple_(std::move(c))
    {
    }

    bool await_ready() noexcept { return false; }

    void await_suspend(std::experimental::coroutine_handle<> handle)
    {
      coroutine_ = handle;
      awaitable_unfold(std::make_integer_sequence<size_t, std::tuple_size<TUPLE_TASK>::value>{});
    }

    TUPLE_TASK await_resume() noexcept { return std::move(tuple_); }

  private:
    std::atomic_int notifyed_count_;
    std::experimental::coroutine_handle<> coroutine_;
    TUPLE_TASK tuple_;
  };

  template<typename AWAITABLE>
  auto make_when_all_task(AWAITABLE&& awaitable)
    -> whell_all_task_t<typename coroutine_trait<AWAITABLE>::result_type>
  { 
    co_return co_await awaitable;
  }
}

namespace mrpc
{
  template<typename... AWAITABLES>
  auto when_all(AWAITABLES&&... awaitables)
  {
    using tuple_type = std::tuple<detail::whell_all_task_t<typename coroutine_trait<AWAITABLES>::result_type>...>;
    return detail::when_all_task_awaitable_t<tuple_type>
      (std::make_tuple<detail::whell_all_task_t<typename coroutine_trait<AWAITABLES>::result_type>...>(
        detail::make_when_all_task(std::forward<AWAITABLES>(awaitables))...));
  }
}