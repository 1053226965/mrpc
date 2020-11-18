#pragma once
#include <list>
#include <atomic>
#include <unordered_map>
#include <experimental/coroutine>
#include "io/io_state.hpp"
#include "io/net/socket.hpp"
#include "common/platform.hpp"
#include "common/time/types.hpp"
#include "common/concurrentqueue.h"
#include "buffer.hpp"

#ifdef OS_GNU_LINUX

namespace mrpc::detail
{
#define HANDLE int
#define MAX_EPOLL_EVENTS 100
#define DEFAULT_EPOLL_READ_BUFFER_SIZE 1024
#define DEFAULT_UPD_PACK_SIZE 1500

  class epoll_t
  {
    epoll_t(epoll_t const &) = delete;
    epoll_t &operator=(epoll_t const &) = delete;

    struct schedule_awaitable_t
    {
      schedule_awaitable_t(epoll_t& iocp) noexcept;
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::experimental::coroutine_handle<> handle) noexcept;
      void await_resume() const noexcept {}

      epoll_t& epoll_;
    };
    friend struct schedule_awaitable_t;

  public:
    epoll_t(size_t concurrencyHint);
    ~epoll_t();

    schedule_awaitable_t schedule() noexcept;
    void update_next_timeout_point(high_resolution_clock_t::time_point tp) noexcept;
    error_code process_one(milliseconds_t millisecs);
    error_code loop_process();
    error_code add_socket(std::shared_ptr<net::socket_t>& s);
    error_code rem_socket(std::shared_ptr<net::socket_t>& s);
    HANDLE get_handle() const noexcept { return epoll_fd_; }
    void wakeup(size_t wakeup_count);
    void stop();
    bool stoped() noexcept { return thread_state_.load(std::memory_order_relaxed) & 1; }
    void reset();

  private:
    void schedule_coroutine(std::experimental::coroutine_handle<> handle);
    bool try_enter_loop();
    void exit_loop();

    bool add_fd(socket_handle_t fd, epoll_event &ev) noexcept;
    bool del_fd(socket_handle_t fd) noexcept;
    void reset_event_fd();
    error_code wait_events(milliseconds_t millisecs);
    void handle_event(epoll_event const &ev);
    
    socket_handle_t epoll_fd_;
    //socket_handle_t wakeup_fds_[2];
    socket_handle_t event_fd_;

    moodycamel::ConcurrentQueue<std::experimental::coroutine_handle<>> coroutine_queue_;

    std::atomic_uint32_t thread_state_;
    static thread_local high_resolution_clock_t::time_point next_timeout_point_;

    std::mutex socket_refs_mtx_;
    std::unordered_map<socket_handle_t, std::shared_ptr<net::socket_t>> socket_refs_; // when a socket be added in epoll, we must hold the socekt and free it by epoll
  };

} // namespace mrpc::detail

#endif