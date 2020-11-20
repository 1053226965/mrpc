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
      _io_impl(concurrencyHint)
    {}

    auto schedule() {
      return _io_impl.schedule();
    }

    void update_next_timeout_point(high_resolution_clock_t::time_point tp) noexcept {
      _io_impl.update_next_timeout_point(tp);
    }

    error_code process_one(milliseconds_t millisecs) {
      return _io_impl.process_one(millisecs);
    }

    error_code loop_process() {
      return _io_impl.loop_process();
    }

    error_code add_socket(std::shared_ptr<net::socket_t>& s) {
      return _io_impl.add_socket(s);
    }

    error_code rem_socket(std::shared_ptr<net::socket_t>& s) {
      return _io_impl.rem_socket(s);
    }

    HANDLE get_handle() const noexcept {
      return _io_impl.get_handle();
    }

    void wakeup(size_t wakeup_count) {
      _io_impl.wakeup(wakeup_count);
    }

    void stop() {
      _io_impl.stop();
    }

    bool stoped() noexcept {
      return _io_impl.stoped();
    }

    void reset() {
      _io_impl.reset();
    }

  private:
    impl_t _io_impl;
  };

#ifdef OS_WIN
  using io_context_t = io_context_base_t<detail::iocp_t> ;
#elif defined(OS_GNU_LINUX)
  using io_context_t = io_context_base_t<detail::epoll_t> ;
#endif
}
