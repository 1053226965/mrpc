#pragma once
#include "common/platform.hpp"
#include "common/time/types.hpp"
#include "io/err_code.h"
#include "io/net/socket.hpp"
#ifdef OS_WIN
#include "iocp.hpp"
#elif defined(OS_GNU_LINUX)
#include "epoll.hpp"
#endif

namespace mrpc
{
  template<typename impl_t>
  class io_context_base_t
  {
  public:
    io_context_base_t(size_t concurrencyHint) :
      io_impl_(concurrencyHint)
    {}

    auto schedule() {
      return io_impl_.schedule();
    }

    void update_next_timeout_point(high_resolution_clock_t::time_point tp) noexcept {
      io_impl_.update_next_timeout_point(tp);
    }

    error_code process_one(milliseconds_t millisecs) {
      return io_impl_.process_one(millisecs);
    }

    error_code loop_process() {
      return io_impl_.loop_process();
    }

    error_code add_socket(std::shared_ptr<net::socket_t>& s) {
      return io_impl_.add_socket(s);
    }

    error_code rem_socket(std::shared_ptr<net::socket_t>& s) {
      return io_impl_.rem_socket(s);
    }

    HANDLE get_handle() const noexcept {
      return io_impl_.get_handle();
    }

    void wakeup(size_t wakeup_count) {
      io_impl_.wakeup(wakeup_count);
    }

    void stop() {
      io_impl_.stop();
    }

    bool stoped() noexcept {
      return io_impl_.stoped();
    }

    void reset() {
      io_impl_.reset();
    }

  private:
    impl_t io_impl_;
  };

#ifdef OS_WIN
  using io_context_t = io_context_base_t<detail::iocp_t> ;
#elif defined(OS_GNU_LINUX)
  using io_context_t = io_context_base_t<detail::epoll_t> ;
#endif
}
