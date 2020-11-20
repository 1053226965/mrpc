#pragma once
#include <atomic>
#include <experimental/coroutine>
#include "common/coroutine/coroutine_helper.hpp"

namespace mrpc::detail
{
  class strand_task_t
  {
    struct awaitable_t
    {
      bool await_ready() noexcept;
      std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<> handle) noexcept;
      void await_resume() noexcept;
      strand_task_t &_strand_task;
    };

    struct node_t
    {
      std::experimental::coroutine_handle<> _coroutine;
      node_t *_next{nullptr};
    };
    friend class awaitable_t;

  public:
    strand_task_t() noexcept;
    auto operator co_await() noexcept { return awaitable_t{*this}; }

  private:
    std::atomic<node_t *> _head;
  };

  bool strand_task_t::awaitable_t::await_ready() noexcept
  {
    return false;
  }

  std::experimental::coroutine_handle<> strand_task_t::awaitable_t::await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept
  {
    node_t *node = new node_t{coroutine};
    node_t *t_node = _strand_task._head.load(std::memory_order_acquire);
    node->_next = t_node;
    while (!_strand_task._head.compare_exchange_weak(t_node, node))
    {
      t_node = _strand_task._head.load(std::memory_order_acquire);
      node->_next = t_node;
    }
    if (t_node == nullptr)
    {
      return coroutine;
    }
    else
    {
      node->_next = t_node;
      return noop_coroutine();
    }
  }

  void strand_task_t::awaitable_t::await_resume() noexcept
  {
  }

  strand_task_t::strand_task_t() noexcept
  {
  }
} // namespace mrpc::detail