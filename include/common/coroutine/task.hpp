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
          return coroutine.promise().continue_coro_ ? coroutine.promise().continue_coro_ : noop_coroutine();
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

      void set_continue(std::experimental::coroutine_handle<> continue_coro) { continue_coro_ = continue_coro; }

      std::experimental::coroutine_handle<> continue_coro_;
    };

    template<typename T>
    class task_promose_t : public task_promise_base_t
    {
    public:
      using value_type = std::remove_reference_t<T>;

      task_promose_t() noexcept :
        type_(result_type::empty),
        excetion_ptr_(nullptr) {}

      ~task_promose_t() noexcept {}

      auto get_return_object() noexcept { return std::experimental::coroutine_handle<task_promose_t<T>>::from_promise(*this); }
      
      void unhandled_exception() noexcept 
      {
        ::new (static_cast<void*>(std::addressof(excetion_ptr_))) std::exception_ptr(std::current_exception());
        type_ = result_type::exception;
      }

      template<typename VT, typename = std::enable_if_t<std::is_convertible_v<VT&&, T>>>
      void return_value(VT&& value) noexcept
      {
        ::new (static_cast<void*>(std::addressof(value_))) value_type(std::forward<VT>(value));
        type_ = result_type::value;
      }

      auto result() noexcept
      {
        if (type_ == result_type::exception) {
          std::rethrow_exception(excetion_ptr_);
        }
        M_ASSERT(type_ == result_type::value);
        return std::forward<T>(value_);
      }

    private:
      enum class result_type {empty, value, exception};

      result_type type_;
      union 
      {
        std::exception_ptr excetion_ptr_;
        value_type value_;
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
        excetion_ptr_ = std::current_exception();
      }

      void return_void() noexcept{}

      void result() noexcept 
      {
        if (excetion_ptr_) {
          std::rethrow_exception(excetion_ptr_);
        }
      }

    private:
      std::exception_ptr excetion_ptr_;
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
      :coroutine_(coroutine) {}

    task_t(task_t&& t) noexcept:
      coroutine_(t.coroutine_) 
    {
      t.coroutine_ = nullptr;
    }

    ~task_t() 
    {
      if (coroutine_)
        coroutine_.destroy();
    }

    task_t& operator=(task_t&& t) noexcept
    {
      coroutine_ = t.coroutine_;
      t.coroutine_ = nullptr;
    }

    bool is_done() noexcept { return coroutine_.done(); }
    coroutine_handle_type get_handle() noexcept { return coroutine_; }

    auto operator co_await() noexcept 
    {
      struct await_table
      {
        bool await_ready() noexcept { return false; }

        std::experimental::coroutine_handle<> await_suspend(
          std::experimental::coroutine_handle<> coroutine) noexcept
        {
          task_.coroutine_.promise().continue_coro_ = coroutine;
          return task_.coroutine_;
        }

        RET await_resume() noexcept 
        { 
          return task_.coroutine_.promise().result();
        }

        task_t& task_;
      };

      return await_table{ *this };
    }

  private:
    coroutine_handle_type coroutine_;
  };
}