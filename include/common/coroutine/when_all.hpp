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
    void set_notify(std::function<void()> const& notify) noexcept { _notify = notify; }
    void notify() { _notify(); }

    std::function<void()> _notify;
  };

  template<typename T>
  class when_all_promise_t : public when_all_promise_base_t
  {
    using coroutine_handle_type = std::experimental::coroutine_handle<when_all_promise_t<T>>;
  public:
    using value_type = std::remove_reference_t<T>;

    when_all_promise_t() noexcept:
      _type(result_type::empty),
      _excetion_ptr(nullptr)
    {}

    ~when_all_promise_t() noexcept {}

    auto get_return_object() noexcept { return coroutine_handle_type::from_promise(*this); }

    void unhandled_exception() noexcept
    {
      ::new (static_cast<void*>(std::addressof(_excetion_ptr))) std::exception_ptr(std::current_exception());
      _type = result_type::exception;
      notify();
    }

    template<typename VT, typename = std::enable_if_t<std::is_convertible_v<VT&&, T>>>
    void return_value(VT&& value) noexcept
    {
      ::new (static_cast<void*>(std::addressof(_value))) value_type(std::forward<VT>(value));
      _type = result_type::value;
      notify();
    }

    T result() noexcept
    {
      if (_type == result_type::exception) {
        std::rethrow_exception(_excetion_ptr);
      }
      M_ASSERT(_type == result_type::value);
      return std::move(_value);
    }

  private:
    enum class result_type { empty, value, exception };

    result_type _type;
    union
    {
      std::exception_ptr _excetion_ptr;
      value_type _value;
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
      _excetion_ptr = std::current_exception(); 
      notify();
    }

    void return_void() noexcept { notify(); }

    void result() noexcept
    {
      if (_excetion_ptr) {
        std::rethrow_exception(_excetion_ptr);
      }
    }

  private:
    std::exception_ptr _excetion_ptr;
  };

  template<typename T>
  class whell_all_task_t
  {
    whell_all_task_t(whell_all_task_t const&) = delete;
    whell_all_task_t& operator=(whell_all_task_t const&) = delete;
  public:
    using promise_type = when_all_promise_t<T>;

    whell_all_task_t(std::experimental::coroutine_handle<promise_type> handle) noexcept :
      _coroutine(handle)
    {}

    whell_all_task_t(whell_all_task_t&& t) noexcept :
      _coroutine(std::move(t._coroutine))
    {
      t._coroutine = nullptr;
    }

    whell_all_task_t& operator=(whell_all_task_t&& t) noexcept
    {
      _coroutine = t._coroutine;
      t._coroutine = nullptr;
      return *this;
    }

    void set_notify(std::function<void()> notify) noexcept
    {
      _coroutine.promise().set_notify(notify);
    }

    void start() 
    {
      _coroutine.resume();
    }

    T result()
    {
      if constexpr (std::is_void_v<T>)
        _coroutine.promise().result();
      else
        return _coroutine.promise().result();
    }

  private:
    std::experimental::coroutine_handle<promise_type> _coroutine;
  };

  template<typename TUPLE_TASK>
  class when_all_task_awaitable_t
  {
    template<size_t... INDICES>
    void awaitable_unfold(std::integer_sequence<size_t, INDICES...>)
    {
      auto notify = [this]() {
        if (_notifyed_count.fetch_add(1, std::memory_order_relaxed) == std::tuple_size_v<TUPLE_TASK> - 1)
          _coroutine.resume();
      };
      auto func = [&notify](auto&& when_all_task) {
        when_all_task.set_notify(notify);
        when_all_task.start();
        return 0;
      };
      int unused[] = { func(std::get<INDICES>(_tuple))... };
      (void)unused;
    }
  public:

    using tuple_task_type = TUPLE_TASK;

    when_all_task_awaitable_t(tuple_task_type&& c) noexcept :
      _notifyed_count(0),
      _tuple(std::move(c))
    {
    }

    bool await_ready() noexcept { return false; }

    void await_suspend(std::experimental::coroutine_handle<> handle)
    {
      _coroutine = handle;
      awaitable_unfold(std::make_integer_sequence<size_t, std::tuple_size<TUPLE_TASK>::value>{});
    }

    TUPLE_TASK await_resume() noexcept { return std::move(_tuple); }

  private:
    std::atomic_int _notifyed_count;
    std::experimental::coroutine_handle<> _coroutine;
    TUPLE_TASK _tuple;
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