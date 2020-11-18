#include "io/epoll.hpp"
#include "common/debug/helper.hpp"
#include "common/scoped_func.hpp"
#include "io/timer.hpp"

#ifdef OS_GNU_LINUX

namespace mrpc::detail
{
  static uintptr_t wakeup_event = 0;
  static uintptr_t corout_event = 1;

  thread_local high_resolution_clock_t::time_point epoll_t::next_timeout_point_ = high_resolution_clock_t::now();

  bool set_nonblocking(socket_handle_t fd, bool not_blocked)
  {
    int oldflags = fcntl(fd, F_GETFL, 0);
    if (oldflags < 0)
    {
      return false;
    }

    if (not_blocked)
    {
      oldflags |= O_NONBLOCK;
    }
    else
    {
      oldflags &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, oldflags) != 0)
    {
      return false;
    }
    return true;
  }

  epoll_t::schedule_awaitable_t::schedule_awaitable_t(epoll_t &epoll) noexcept
      : epoll_(epoll) {}

  void epoll_t::schedule_awaitable_t::await_suspend(std::experimental::coroutine_handle<> handle) noexcept
  {
    epoll_.schedule_coroutine(handle);
  }

  epoll_t::epoll_t(size_t concurrencyHint) : thread_state_(0), event_fd_(-1)
  {
    /* FD_CLOEXEC, close-on-exec */
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1)
    {
      throw_system_error("create epoll");
    }

    // if (pipe2(wakeup_fds_, O_CLOEXEC) == -1)
    // {
    //   throw_system_error("create pipe");
    // }
    // set_nonblocking(wakeup_fds_[0], true);
    // set_nonblocking(wakeup_fds_[1], true);
    // epoll_event ev;
    // ev.events = static_cast<uint32_t>(EPOLLIN | EPOLLET);
    // ev.data.ptr = &wakeup_fds_[0];
    // if (!add_fd(wakeup_fds_[0], ev))
    // {
    //   throw_system_error("add wakeup fd");
    // }

    reset_event_fd();
  }

  epoll_t::~epoll_t()
  {
    if (event_fd_ != -1)
    {
      close(event_fd_);
      event_fd_ = -1;
    }
    if (epoll_fd_ != -1)
    {
      close(epoll_fd_);
      epoll_fd_ = -1;
    }
  }

  epoll_t::schedule_awaitable_t epoll_t::schedule() noexcept
  {
    return schedule_awaitable_t{*this};
  }

  void epoll_t::update_next_timeout_point(high_resolution_clock_t::time_point tp) noexcept
  {
    next_timeout_point_ = tp;
  }

  error_code epoll_t::process_one(milliseconds_t millisecs)
  {
    using namespace std::literals::chrono_literals;
    if (!try_enter_loop())
      return error_code::STOPED;
    auto guard = exec_on_exit([this]() { exit_loop(); });
    (void)(guard);
    return wait_events(millisecs);
  }

  error_code epoll_t::loop_process()
  {
    using namespace std::literals::chrono_literals;
    if (!try_enter_loop())
      return error_code::STOPED;
    auto guard = exec_on_exit([this]() { exit_loop(); });
    (void)(guard);
    while (true)
    {
      if (wait_events(-1ms) == error_code::STOPED)
        return error_code::STOPED;
    }
  }

  error_code epoll_t::add_socket(std::shared_ptr<net::socket_t> &s)
  {
    epoll_event ev;
    ev.events = static_cast<uint32_t>(EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP);
    ev.data.ptr = (void *)s.get();

    socket_refs_mtx_.lock();
    socket_refs_[s->handle()] = s;
    socket_refs_mtx_.unlock();

    if (s->handle() == INVAILD_SOCKET || !add_fd(s->handle(), ev))
    {
      socket_refs_mtx_.lock();
      socket_refs_[s->handle()] = nullptr;
      socket_refs_mtx_.unlock();
      return error_code::SYSTEM_ERROR;
    }
    DETAIL_LOG_INFO("socket addr {} {}", (void *)s.get(), s->handle());
    return error_code::NONE_ERROR;
  }

  error_code epoll_t::rem_socket(std::shared_ptr<net::socket_t> &s)
  {
    if (del_fd(s->handle()))
      return error_code::NONE_ERROR;
    return error_code::SYSTEM_ERROR;
  }

  void epoll_t::wakeup(size_t wakeup_count)
  {
    int sc;
    uint64_t add_v = wakeup_count;
    do
    {
      sc = ::write(event_fd_, &add_v, sizeof(add_v));
    } while (sc < 0 && errno == EINTR);
    if(sc < 0 && errno == EAGAIN)
    {
      throw_system_error("wakeup count is too much");
    }
  }

  void epoll_t::stop()
  {
    uint32_t state = thread_state_.fetch_or(1, std::memory_order_relaxed);
    if((state & 1) == 0)
    {
      wakeup(state / 2);
    }
  }

  void epoll_t::reset()
  {
    thread_state_.fetch_and((~static_cast<uint32_t>(1)), std::memory_order_relaxed);
    reset_event_fd();
  }

  bool epoll_t::add_fd(socket_handle_t fd, epoll_event &ev) noexcept
  {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
      return false;
    }
    return true;
  }

  bool epoll_t::del_fd(socket_handle_t fd) noexcept
  {
    epoll_event ev_fd;
    memset(&ev_fd, 0, sizeof(ev_fd));
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev_fd) != 0)
    {
      return false;
    }
    return true;
  }

  void epoll_t::reset_event_fd()
  {
    if(event_fd_ != -1)
    {
      del_fd(event_fd_);
      close(event_fd_);
    }
    event_fd_ = eventfd(0, EFD_SEMAPHORE);
    if (event_fd_ == -1)
    {
       throw_system_error("can't create event_fd");
    }
    if(!set_nonblocking(event_fd_, true))
    {
      close(event_fd_);
      throw_system_error("can't set event_fd on noblocking way");
    }
    epoll_event ev;
    ev.events = static_cast<uint32_t>(EPOLLIN);
    ev.data.ptr = &event_fd_;
    if (!add_fd(event_fd_, ev))
    {
      throw_system_error("can't add event_fd to epoll");
    }
  }

  void epoll_t::schedule_coroutine(std::experimental::coroutine_handle<> handle)
  {
    coroutine_queue_.enqueue(handle);
    wakeup(1);
  }

  bool epoll_t::try_enter_loop()
  {
    uint32_t expected = thread_state_.load(std::memory_order_relaxed);
    do
    {
      if (expected & 1)
        return false;
    } while (thread_state_.compare_exchange_weak(expected,
                                                 expected + 2, std::memory_order_relaxed));
    return true;
  }

  void epoll_t::exit_loop()
  {
    thread_state_.fetch_sub(2, std::memory_order_relaxed);
  }

  error_code epoll_t::wait_events(milliseconds_t millisecs)
  {
    using namespace std::literals::chrono_literals;
    error_code ret = error_code::NONE_ERROR;
    auto timeout = std::chrono::duration_cast<milliseconds_t>(next_timeout_point_ - high_resolution_clock_t::now());

    if (timeout < 0ms)
    {
      timeout = 0ms;
    }
    if (millisecs.count() > 0 && millisecs < timeout)
    {
      timeout = millisecs;
    }

    epoll_event evs[MAX_EPOLL_EVENTS];
    int r;
    for (;;)
    {
      r = epoll_wait(epoll_fd_, evs, MAX_EPOLL_EVENTS, timeout.count());
      next_timeout_point_ = high_resolution_clock_t::now() + milliseconds_t(3000);
      if (r < 0)
      {
        if (errno == EINTR)
          continue;
      }
      break;
    }

    if (r > 0)
    {
      for (int i = 0; i < r; ++i)
      {
        if (evs[i].data.ptr == &event_fd_)
        {
          uint64_t semaphore_v;
          int re;
          do
          {
            re = ::read(event_fd_, &semaphore_v, sizeof(semaphore_v));
          } while (re < 0 && errno == EINTR);

          if (coroutine_queue_.size_approx() > 0)
          {
            std::experimental::coroutine_handle<> coroutine;
            while (coroutine_queue_.try_dequeue(coroutine))
            {
              coroutine.resume();
            }
          }
        }
        else
        {
          handle_event(evs[i]);
        }
      }
    }
    else if (r < 0)
    {
      throw_system_error("wait_events");
    }
    else if (r == 0)
    {
      ret = error_code::TIMEOUT;
    }
    schedule_timer_t::check_timer(*this);
    if (stoped())
    {
      ret = error_code::STOPED;
    }
    return ret;
  }

  void epoll_t::handle_event(epoll_event const &ev)
  {
    if (ev.data.ptr == nullptr)
    {
      mrpc_log("unexcepted handle_event");
      return;
    }
    net::socket_t *sock = reinterpret_cast<net::socket_t *>(ev.data.ptr);
    bool cancle = ev.events & (EPOLLHUP | EPOLLERR);
    if (ev.events & EPOLLIN || cancle)
    {
      sock->ready_to_read();
    }
    if (ev.events & EPOLLOUT || cancle)
    {
      sock->ready_to_write();
    }

    if (!sock->valid())
    {
      socket_refs_mtx_.lock();
      socket_refs_[sock->handle()] = nullptr;
      socket_refs_mtx_.unlock();
    }
  }
} // namespace mrpc::detail

#endif