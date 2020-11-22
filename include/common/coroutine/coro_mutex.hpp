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
    bool await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept;
    void await_resume() noexcept;

  private:
    coro_scope_mutex_t(coro_scope_mutex_t const &) = delete;
    coro_scope_mutex_t &operator=(coro_scope_mutex_t const &) = delete;

    coro_scope_mutex_t(coro_mutex_t *owner) noexcept;

    coro_mutex_t *_owner;
    coro_scope_mutex_t *_next;
    std::experimental::coroutine_handle<> _coroutine;
  };

  class coro_mutex_t
  {
    friend class coro_scope_mutex_t;

    constexpr static uintptr_t not_locked = 1;
    constexpr static uintptr_t waiter_running = 0;

  public:
    coro_mutex_t() noexcept;
    coro_scope_mutex_t scope_mutex() noexcept;

  private:
    std::atomic_uintptr_t _state;
    coro_scope_mutex_t *_waiters;
  };

  inline coro_scope_mutex_t::coro_scope_mutex_t(coro_mutex_t *owner) noexcept : _owner(owner),
                                                                                _next(nullptr),
                                                                                _coroutine(nullptr)
  {
  }

  inline coro_scope_mutex_t::coro_scope_mutex_t(coro_scope_mutex_t &&mtx) noexcept : _owner(mtx._owner),
                                                                                     _next(mtx._next),
                                                                                     _coroutine(mtx._coroutine)
  {
    mtx._owner = nullptr;
    mtx._next = nullptr;
    mtx._coroutine = nullptr;
  }

  inline coro_scope_mutex_t::~coro_scope_mutex_t() noexcept
  {
    // _coroutine为nullptr，说明没有co_await此对象,await_suspend没有被调用
    if (_owner == nullptr || _coroutine == nullptr)
    {
      return;
    }

    auto waiters = _owner->_waiters;
    if (waiters == nullptr)
    {
      uintptr_t old = coro_mutex_t::waiter_running;
      if (_owner->_state.compare_exchange_strong(old, coro_mutex_t::not_locked,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed))
      {
        return;
      }

      old = _owner->_state.exchange(coro_mutex_t::waiter_running,
                                    std::memory_order_acquire);
      coro_scope_mutex_t *pNode = reinterpret_cast<coro_scope_mutex_t *>(old);
      do
      {
        coro_scope_mutex_t *next = pNode->_next;
        pNode->_next = waiters;
        waiters = pNode;
        pNode = next;
      } while (pNode != nullptr);
    }
    _owner->_waiters = waiters->_next;
    waiters->_coroutine.resume();
  }

  inline bool coro_scope_mutex_t::await_ready() noexcept
  {
    return false;
  }

  inline bool coro_scope_mutex_t::await_suspend(std::experimental::coroutine_handle<> coroutine) noexcept
  {
    _coroutine = coroutine;
    uintptr_t old = _owner->_state.load(std::memory_order_acquire);
    while (true)
    {
      if (old == coro_mutex_t::not_locked)
      {
        if (_owner->_state.compare_exchange_weak(old, coro_mutex_t::waiter_running,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed))
        {
          return false;
        }
      }
      else
      {
        _next = reinterpret_cast<coro_scope_mutex_t *>(old);
        if (_owner->_state.compare_exchange_weak(old, reinterpret_cast<uintptr_t>(this),
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed))
        {
          return true;
        }
      }
    }
  }

  inline void coro_scope_mutex_t::await_resume() noexcept
  {
  }

  inline coro_mutex_t::coro_mutex_t() noexcept : _state(not_locked),
                                                 _waiters(nullptr)
  {
  }

  inline coro_scope_mutex_t coro_mutex_t::scope_mutex() noexcept
  {
    return {this};
  }
} // namespace mrpc