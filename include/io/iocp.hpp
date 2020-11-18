#pragma once
#include <atomic>
#include "common/platform.hpp"
#include "common/debug/helper.hpp"
#include "common/time/types.hpp"
#include "common/coroutine/task.hpp"
#include "common/scoped_func.hpp"
#include "net/socket.hpp"
#include "io_state.hpp"

#ifdef OS_WIN

namespace mrpc::detail
{
  class iocp_t final
  {
    iocp_t(iocp_t const&) = delete;
    iocp_t& operator=(iocp_t const&) = delete;

    struct schedule_awaitable_t
    {
      schedule_awaitable_t(iocp_t& iocp) noexcept;
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::experimental::coroutine_handle<> handle) noexcept;
      void await_resume() const noexcept {}

      iocp_t& iocp_ctx_;
    };
    friend struct schedule_awaitable_t;

  public:
    iocp_t(size_t concurrencyHint);
    ~iocp_t();

    schedule_awaitable_t schedule() noexcept;
    void update_next_timeout_point(high_resolution_clock_t::time_point tp) noexcept;
    error_code process_one(milliseconds_t millisecs);
    error_code loop_process();
    error_code add_socket(std::shared_ptr<net::socket_t>& s);
    error_code rem_socket(std::shared_ptr<net::socket_t>& s) { return error_code::NONE_ERROR; }
    HANDLE get_handle() const noexcept { return iocp_handle_; }
    void wakeup(size_t wakeup_count);
    void stop();
    bool stoped() noexcept { return thread_state_.load(std::memory_order_relaxed) & 1; }
    void reset();

  private:
    void schedule_coroutine(std::experimental::coroutine_handle<> handle);
    bool try_enter_loop();
    void exit_loop();
    error_code iocp_wait(milliseconds_t millis);

  private:
    std::atomic_uint32_t thread_state_;
    HANDLE iocp_handle_;
    static thread_local high_resolution_clock_t::time_point next_timeout_point_;
  };

  using io_context_impl = iocp_t;
}

#endif