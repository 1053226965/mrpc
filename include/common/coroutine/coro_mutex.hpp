#pragma once
#include <atomic>
#include <experimental/coroutine>
#include "common/coroutine/coroutine_helper.hpp"

namespace mrpc
{
  class coro_mutex_t;
  class coro_scope_mutex_t
  {
    friend class coro_mutex_t;
  public:
    coro_scope_mutex_t(coro_scope_mutex_t &&mtx) noexcept;
    ~coro_scope_mutex_t() noexcept;

    bool await_ready() noexcept;
    std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept;
    void await_resume() noexcept;

  private:
    coro_scope_mutex_t(coro_scope_mutex_t const&) = delete;
    coro_scope_mutex_t& operator=(coro_scope_mutex_t const&) = delete;

    coro_scope_mutex_t(coro_mutex_t* owner) noexcept;

    coro_mutex_t* _owner;
  };

  class coro_mutex_t
  {
    friend class coro_scope_mutex_t;

    struct node_t
    {
      node_t(std::experimental::coroutine_handle<> coroutine) noexcept : _coroutine(coroutine), _next(nullptr) {}
      std::experimental::coroutine_handle<> _coroutine;
      node_t *_next;
    };

  public:
    coro_mutex_t() noexcept;
    coro_scope_mutex_t scope_mutex() noexcept;

  private:
    std::atomic<node_t *> _head;
  };

  inline coro_scope_mutex_t::coro_scope_mutex_t(coro_mutex_t* owner) noexcept : _owner(owner)
  {
  }

  inline coro_scope_mutex_t::coro_scope_mutex_t(coro_scope_mutex_t &&mtx) noexcept : _owner(mtx._owner)
  {
    mtx._owner = nullptr;
  }

  inline coro_scope_mutex_t::~coro_scope_mutex_t() noexcept
  {
    if(_owner == nullptr) return;

    coro_mutex_t::node_t *node = _owner->_head.load(std::memory_order_acquire);
    M_ASSERT(node != nullptr);
    while (!_owner->_head.compare_exchange_weak(node, node->_next,
                                                  std::memory_order_acquire,
                                                  std::memory_order_relaxed))
    {
      node = _owner->_head.load(std::memory_order_acquire);
      M_ASSERT(node != nullptr);
    }
    if (node->_next != nullptr)
    {
      node->_coroutine.resume();
    }
    delete node;
  }

  inline bool coro_scope_mutex_t::await_ready() noexcept
  {
    return false;
  }

  inline std::experimental::coroutine_handle<> coro_scope_mutex_t::await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept
  {
    coro_mutex_t::node_t *node = new coro_mutex_t::node_t(coroutine);
    coro_mutex_t::node_t *t_node = _owner->_head.load(std::memory_order_acquire);
    node->_next = t_node;
    while (!_owner->_head.compare_exchange_weak(t_node, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed))
    {
      t_node = _owner->_head.load(std::memory_order_acquire);
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

  inline void coro_scope_mutex_t::await_resume() noexcept
  {
  }


  coro_mutex_t::coro_mutex_t() noexcept : _head(nullptr)
  {
  }

  coro_scope_mutex_t coro_mutex_t::scope_mutex() noexcept
  {
    return { this };
  }
} // namespace mrpc