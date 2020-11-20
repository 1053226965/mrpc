#pragma once
#include <type_traits>
#include <experimental/coroutine>

namespace mrpc
{
  namespace detail
  {
    template<typename T, typename = std::void_t<> >
    struct is_awaiter : std::false_type {};

    template<typename T>
    struct is_awaiter<T,
      std::void_t<decltype(std::declval<T>().await_ready()),
      decltype(std::declval<T>().await_suspend(std::declval<std::experimental::coroutine_handle<>>())),
      decltype(std::declval<T>().await_resume())>> : std::true_type {};

    template<typename T, typename = std::void_t<> >
    struct has_co_await_operator_1 :std::false_type {};

    template<typename T>
    struct has_co_await_operator_1<T, 
      std::void_t<decltype(std::declval<T>().operator co_await())>> : std::true_type {};

    template<typename T, typename = std::void_t<> >
    struct has_co_await_operator_2 :std::false_type {};

    template<typename T>
    struct has_co_await_operator_2<T,
      std::void_t<decltype(operator co_await(std::declval<T>()))>> : std::true_type {};

    template<typename T, typename = void>
    struct get_awaiter;

    template<typename T>
    struct get_awaiter < T, std::enable_if_t<is_awaiter<T>::value>>
    {
      using awaiter_type = T;
    };

    template<typename T>
    struct get_awaiter<T, std::enable_if_t<has_co_await_operator_1<T>::value>>
    {
      using awaiter_type = decltype(std::declval<T>().operator co_await());
    };

    template<typename T>
    struct get_awaiter<T, std::enable_if_t<has_co_await_operator_2<T>::value>>
    {
      using awaiter_type = decltype(operator co_await(std::declval<T>()));
    };

    struct null_task_t
    {
      struct promise_type
      {
        auto initial_suspend() noexcept
        {
          return std::experimental::suspend_never();
        }

        auto final_suspend() noexcept
        {
          return std::experimental::suspend_never();
        }

        null_task_t get_return_object()
        {
          return null_task_t{ std::experimental::coroutine_handle<promise_type>::from_promise(*this) };
        }

        void unhandled_exception()
        {
        }

        void return_void() noexcept {}
      };
      null_task_t(std::experimental::coroutine_handle<> handle) noexcept :
        _coroutine(handle)
      {}
      ~null_task_t() noexcept
      {
        if (_coroutine)
          _coroutine.destroy();
      }
      std::experimental::coroutine_handle<> _coroutine;
    };

    inline null_task_t new_null_task()
    {
      while(true) co_await std::experimental::suspend_always();
    }

    static const null_task_t null_task = new_null_task();
  }

  template<typename T>
  struct coroutine_trait
  {
    using awaiter_type = typename detail::get_awaiter<T>::awaiter_type;
    using result_type = decltype(std::declval<awaiter_type>().await_resume());
  };

  inline auto noop_coroutine()
  {
    return detail::null_task._coroutine;
  }
}