#pragma once
#include <experimental/coroutine>

namespace mrpc
{
  namespace detail
  {
    template<typename T>
    class generator;

    template<typename T>
    class generator_promise_t
    {
    public:
      using value_type = std::remove_reference_t<T>;

      generator<T> get_return_object()
      {
        return generator<T>{std::experimental::coroutine_handle<
          generator_promise_t>::from_promise(*this)};
      }

      auto initial_suspend() noexcept
      {
        return std::experimental::suspend_always();
      }

      auto final_suspend() noexcept
      {
        return std::experimental::suspend_always();
      }

      auto yield_value(value_type&& v) noexcept
      {
        _value_pointer = std::addressof(v);
        return std::experimental::suspend_always{};
      }

      void unhandled_exception() noexcept
      {
        m_exception = std::current_exception();
      }

      T get() noexcept
      {
        return std::forward<T>(*_value_pointer);
      }

    private:
      value_type* _value_pointer;
      std::exception_ptr m_exception;
    };

    template<typename T>
    class generator
    {
    public:
      using promise_type = generator_promise_t<T>;
      using handle_type = std::experimental::coroutine_handle<promise_type>;

      generator(handle_type h) noexcept:
        _coroutine(h) {}

      ~generator() noexcept
      {
        if (_coroutine)
          _coroutine.destroy(0);
      }

      T next()
      {

      }

    private:
      std::experimental::coroutine_handle<> _coroutine;
    };
  }

  template<typename T>
  using generator = detail::generator<T>;
}