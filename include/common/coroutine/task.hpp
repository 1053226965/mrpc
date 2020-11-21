#pragma once
#include <experimental/coroutine>
#include "coroutine_helper.hpp"

namespace mrpc
{
  template<typename T> class task_t;
  namespace detail
  {
    struct task_promise_base_t
    {
      struct final_waitable
      {
        bool await_ready() noexcept { return false; }

        template<typename promise_t>
        std::experimental::coroutine_handle<> await_suspend(
          std::experimental::coroutine_handle<promise_t> coroutine) noexcept
        {
          return coroutine.promise()._continue_coro ? coroutine.promise()._continue_coro : noop_coroutine();
        }

        void await_resume() noexcept {}
      };

    public:
      auto initial_suspend() noexcept 
      {
        return std::experimental::suspend_always();
      }

      auto final_suspend() noexcept 
      {
        return final_waitable{};
      }

      void set_continue(std::experimental::coroutine_handle<> continue_coro) { _continue_coro = continue_coro; }

      std::experimental::coroutine_handle<> _continue_coro;
    };

    template<typename T>
    class task_promose_t : public task_promise_base_t
    {
    public:
      using value_type = std::remove_reference_t<T>;

      task_promose_t() noexcept :
        _type(result_type::empty),
        _excetion_ptr(nullptr) {}

      ~task_promose_t() noexcept {}

      auto get_return_object() noexcept { return std::experimental::coroutine_handle<task_promose_t<T>>::from_promise(*this); }
      
      void unhandled_exception() noexcept 
      {
        ::new (static_cast<void*>(std::addressof(_excetion_ptr))) std::exception_ptr(std::current_exception());
        _type = result_type::exception;
      }

      template<typename VT, typename = std::enable_if_t<std::is_convertible_v<VT&&, T>>>
      void return_value(VT&& value) noexcept
      {
        ::new (static_cast<void*>(std::addressof(_value))) value_type(std::forward<VT>(value));
        _type = result_type::value;
      }

      auto result() noexcept
      {
        M_ASSERT(_type != result_type::empty);
        if (_type == result_type::exception) {
          std::rethrow_exception(_excetion_ptr);
        }
        return std::forward<T>(_value);
      }

    private:
      enum class result_type {empty, value, exception};

      result_type _type;
      union 
      {
        std::exception_ptr _excetion_ptr;
        value_type _value;
      };
    };

    template<>
    class task_promose_t<void> : public task_promise_base_t
    {
    public:
      task_promose_t() noexcept {}

      ~task_promose_t() noexcept {}

      auto get_return_object() noexcept { return std::experimental::coroutine_handle<task_promose_t<void>>::from_promise(*this); }

      void unhandled_exception() noexcept 
      {
        _excetion_ptr = std::current_exception();
      }

      void return_void() noexcept{}

      void result() noexcept 
      {
        if (_excetion_ptr) {
          std::rethrow_exception(_excetion_ptr);
        }
      }

    private:
      std::exception_ptr _excetion_ptr;
    };
  }

  template<typename RET>
  class task_t
  {
    task_t(task_t const&) = delete;
    task_t& operator=(task_t const&) = delete;
  public:
    using promise_type = detail::task_promose_t<RET>;
    using coroutine_handle_type = std::experimental::coroutine_handle<promise_type>;

    task_t(coroutine_handle_type coroutine) noexcept
      :_coroutine(coroutine) {}

    task_t(task_t&& t) noexcept:
      _coroutine(t._coroutine) 
    {
      t._coroutine = nullptr;
    }

    ~task_t() 
    {
      if (_coroutine)
        _coroutine.destroy();
    }

    task_t& operator=(task_t&& t) noexcept
    {
      _coroutine = t._coroutine;
      t._coroutine = nullptr;
    }

    bool is_done() noexcept { return _coroutine.done(); }
    coroutine_handle_type get_handle() noexcept { return _coroutine; }

    auto operator co_await() noexcept 
    {
      struct await_table
      {
        bool await_ready() noexcept { return false; }

        std::experimental::coroutine_handle<> await_suspend(
          std::experimental::coroutine_handle<> coroutine) noexcept
        {
          _task._coroutine.promise()._continue_coro = coroutine;
          return _task._coroutine;
        }

        RET await_resume() noexcept 
        { 
          return _task._coroutine.promise().result();
        }

        task_t& _task;
      };

      return await_table{ *this };
    }

  private:
    coroutine_handle_type _coroutine;
  };
}