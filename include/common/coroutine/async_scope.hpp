#pragma once
#include <atomic>
#include <experimental/coroutine>
#include "common/scoped_func.hpp"
#include "common/debug/helper.hpp"

namespace mrpc
{
  class async_scope_t
  {
    struct no_block_task
    {
      struct promise_type
      {
        auto initial_suspend() noexcept { return std::experimental::suspend_never(); }
        auto final_suspend() noexcept { return std::experimental::suspend_never(); }
        void unhandled_exception() { std::terminate(); }
        auto get_return_object() noexcept { return std::experimental::coroutine_handle<promise_type>::from_promise(*this); }
        void return_void() noexcept {}
      };
      no_block_task(std::experimental::coroutine_handle<>) noexcept {}
    };

  public:
    async_scope_t() noexcept : _joinalbe(true),
                               _spawn_count(1u)
    {
    }

    template <typename AWAITABLE>
    void spawn(AWAITABLE &&awaitable)
    {
      [this](std::decay_t<AWAITABLE> awaitable) -> no_block_task {
        spawn_one();
        auto guard = exec_on_exit([this]() { one_done(); });
        (void)(guard);
        co_await awaitable;
      }(std::forward<AWAITABLE>(awaitable));
    }

    auto join()
    {
      struct awaitable
      {
        bool await_ready() noexcept
        {
          /* 如果等于零，则不存在未完成awaitable，不挂起coroutine */
          return (scope._spawn_count.load(std::memory_order_relaxed) & (~1u)) == 0;
        }

        bool await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept
        {
          M_ASSERT(!scope._join_coroutine);
          scope._join_coroutine = coroutine;
          /* 如果大于零，则存在未完成awaitable，挂起coroutine */
          return (scope._spawn_count.fetch_sub(1u, std::memory_order_release) & (~1u)) > 0;
        }

        void await_resume() noexcept {}

        async_scope_t &scope;
      };
      M_ASSERT(_joinalbe);
      _joinalbe = false;
      return awaitable{*this};
    }

    void reset()
    {
      M_ASSERT(_spawn_count.load(std::memory_order_relaxed) <= 1);
      _joinalbe = true;
      _spawn_count.store(1u, std::memory_order_relaxed);
    }

  private:
    void spawn_one() noexcept
    {
      _spawn_count.fetch_add(2, std::memory_order_relaxed);
    }

    void one_done() noexcept
    {
      /* 如果等于2,则：
       * 必定是最后一个awaitable结束并调用one_done，并且此处spawn_count_的减2操作必定发生
       * 在async_scope_t::join里的await_suspend的spawn_count_减1操作之后,_推导出join_coroutine
       * 必不为空，并且join_coroutine_必定被挂起了 */
      if (_spawn_count.fetch_sub(2u, std::memory_order_acquire) == 2)
      {
        _join_coroutine.resume();
      }
    }

  private:
    bool _joinalbe;
    std::atomic_uint _spawn_count;
    std::experimental::coroutine_handle<> _join_coroutine;
  };
} // namespace mrpc