#pragma once
#include <atomic>
#include <experimental/coroutine>
#include "common/coroutine/coroutine_helper.hpp"

namespace mrpc
{
  class strand_task_t
  {
    struct awaitable_t
    {
      bool await_ready() noexcept;
      std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept;
      void await_resume() noexcept;
      strand_task_t &_strand_task;
    };
    friend struct awaitable_t;

    struct node_t
    {
      node_t(std::experimental::coroutine_handle<> coroutine) noexcept : _coroutine(coroutine), _next(nullptr) {}
      std::experimental::coroutine_handle<> _coroutine;
      node_t *_next;
    };

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
    node_t *node = new node_t(coroutine);
    node_t *t_node = _strand_task._head.load(std::memory_order_acquire);
    node->_next = t_node;
    while (!_strand_task._head.compare_exchange_weak(t_node, node,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed))
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
      return noop_coroutine();
    }
  }

  void strand_task_t::awaitable_t::await_resume() noexcept
  {
    node_t *node = _strand_task._head.load(std::memory_order_acquire);
    M_ASSERT(node != nullptr);
    while (!_strand_task._head.compare_exchange_weak(node, node->_next,
                                                     std::memory_order_acquire,
                                                     std::memory_order_relaxed))
    {
      node = _strand_task._head.load(std::memory_order_acquire);
      M_ASSERT(node != nullptr);
    }
    if (node->_next != nullptr)
    {
      node->_coroutine.resume();
    }
    delete node;
  }

  strand_task_t::strand_task_t() noexcept : _head(nullptr)
  {
  }
} // namespace mrpc