#pragma once

#include <atomic>
#include "io/io_state.hpp"

namespace mrpc
{
  class lf_notifier_t
  {
    enum state_t
    {
      addr = 1,
      not_ready = 2,
      ready = 4
    };

  public:
    lf_notifier_t() noexcept;
    ~lf_notifier_t();
    bool notify_on_ready(io_state_t &io_state);
    bool set_ready();

  private:
    std::atomic_uintptr_t state_;
  };

  inline lf_notifier_t::lf_notifier_t() noexcept : state_(state_t::not_ready)
  {
  }

  inline lf_notifier_t::~lf_notifier_t()
  {
  }

  inline bool lf_notifier_t::notify_on_ready(io_state_t &io_state)
  {
    uintptr_t state = state_.load(std::memory_order_acquire);
    while (true)
    {
      switch (state)
      {
      case state_t::not_ready:
      {
        uintptr_t sv = reinterpret_cast<uintptr_t>(&io_state) | state_t::addr;
        if (state_.compare_exchange_weak(state,
                                         sv,
                                         std::memory_order_release,
                                         std::memory_order_relaxed))
        {
          return false;
        }
        break;
      }
      case state_t::ready:
      {
        if (state_.compare_exchange_weak(state,
                                         state_t::not_ready,
                                         std::memory_order_relaxed,
                                         std::memory_order_relaxed))
        {
          M_ASSERT(io_state.get_coro_handle());
          M_ASSERT(!io_state.get_coro_handle().done());
          M_ASSERT(io_state.get_error() == error_code::IO_PENDING);
          return true;
        }
        break;
      }
      default:
        UNREACHABLE();
      }
    }
  }

  inline bool lf_notifier_t::set_ready()
  {
    uintptr_t state = state_.load(std::memory_order_acquire);
    while (true)
    {
      switch (state)
      {
      case state_t::not_ready:
      {
        if (state_.compare_exchange_weak(state,
                                         state_t::ready,
                                         std::memory_order_relaxed,
                                         std::memory_order_relaxed))
        {
          return false;
        }
        break;
      }
      case state_t::ready:
      {
        return false;
      }
      default:
      {
        if (state & state_t::addr)
        {
          if (state_.compare_exchange_weak(state,
                                           state_t::not_ready,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed))
          {
            io_state_t &io_state = *reinterpret_cast<io_state_t *>(state & (~state_t::addr));
            M_ASSERT(io_state.get_coro_handle() &&
                     !io_state.get_coro_handle().done() &&
                     io_state.get_error() == error_code::IO_PENDING);
            io_state.get_coro_handle().resume();
            return true;
          }
        }
        else
        {
          DETAIL_LOG_ERROR("error lf_notifier_t::set_ready this: {} state: {}", (void*)this, state);
          UNREACHABLE();
        }
      }
      }
    }
  }

} // namespace mrpc