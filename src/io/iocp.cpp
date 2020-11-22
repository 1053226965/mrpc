#include "io/io_context.hpp"
#include "io/timer.hpp"
#include "common/platform.hpp"

#ifdef OS_WIN

namespace mrpc::detail
{
  struct winsock_init_t
  {
    winsock_init_t()
    {
      const WORD requestedVersion = MAKEWORD(2, 2);
      WSADATA winsockData;
      const int result = ::WSAStartup(requestedVersion, &winsockData);
      if (result == SOCKET_ERROR)
      {
        const int errorCode = ::WSAGetLastError();
        throw std::system_error(
            errorCode,
            std::system_category(),
            "Error initialsing winsock: WSAStartup");
      }
    }
    ~winsock_init_t()
    {
      WSACleanup();
    }
  };
  static winsock_init_t winsock_init;

  thread_local high_resolution_clock_t::time_point iocp_t::_next_timeout_point = high_resolution_clock_t::now();

  iocp_t::schedule_awaitable_t::schedule_awaitable_t(iocp_t &iocp) noexcept
      : _iocp_ctx(iocp) {}

  void iocp_t::schedule_awaitable_t::await_suspend(std::experimental::coroutine_handle<> handle) noexcept
  {
    _iocp_ctx.schedule_coroutine(handle);
  }

  iocp_t::iocp_t(size_t concurrencyHint) : _thread_state(0)
  {
    (void)winsock_init;
    _iocp_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0,
                                            static_cast<DWORD>(concurrencyHint));
    if (_iocp_handle == NULL)
    {
      DWORD errorCode = ::GetLastError();
      throw std::system_error{
          static_cast<int>(errorCode),
          std::system_category(),
          "Error creating io_service: CreateIoCompletionPort"};
    }
  }

  iocp_t::~iocp_t()
  {
    M_ASSERT(_iocp_handle);
    CloseHandle(_iocp_handle);
  }

  iocp_t::schedule_awaitable_t iocp_t::schedule() noexcept
  {
    return schedule_awaitable_t{*this};
  }

  error_code iocp_t::process_one(milliseconds_t millisecs)
  {
    using namespace std::literals::chrono_literals;
    if (!try_enter_loop())
      return error_code::STOPED;
    auto guard = exec_on_exit([this]() { exit_loop(); });
    (void)(guard);
    return iocp_wait(millisecs);
  }

  error_code iocp_t::loop_process()
  {
    using namespace std::literals::chrono_literals;
    if (!try_enter_loop())
      return error_code::STOPED;
    auto guard = exec_on_exit([this]() { exit_loop(); });
    (void)(guard);
    while (true)
    {
      if (iocp_wait(-1ms) == error_code::STOPED)
        return error_code::STOPED;
    }
  }

  error_code iocp_t::add_socket(std::shared_ptr<net::socket_t>& s)
  {
    const HANDLE result = ::CreateIoCompletionPort(
        (HANDLE)s->handle(),
        _iocp_handle,
        ULONG_PTR(0),
        DWORD(0));
    if (result == nullptr)
    {
      return error_code::SYSTEM_ERROR;
    }
    UCHAR completionModeFlags = FILE_SKIP_SET_EVENT_ON_HANDLE;
    // if (s->skip_compeletion_port_on_success())
    // {
    //   completionModeFlags |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    // }
    const BOOL ok = ::SetFileCompletionNotificationModes(
        (HANDLE)s->handle(),
        completionModeFlags);
    if (!ok)
    {
      // to do something. Maybe the socket shouldn't skip on success
    }
    return error_code::NONE_ERROR;
  }

  void iocp_t::wakeup(size_t wakeup_count)
  {
    for(size_t i = 0; i < wakeup_count; i++)
      ::PostQueuedCompletionStatus(_iocp_handle, 0, 0, nullptr);
  }

  void iocp_t::stop()
  {
    uint32_t state = _thread_state.fetch_or(1, std::memory_order_relaxed);
    if((state & 1) == 0)
    {
      wakeup(state / 2);
    }
  }

  void iocp_t::reset()
  {
    _thread_state.fetch_and((~static_cast<uint32_t>(1)), std::memory_order_relaxed);
  }

  bool iocp_t::try_enter_loop()
  {
    uint32_t expected = _thread_state.load(std::memory_order_acquire);
    do
    {
      if (expected & 1)
        return false;
    } while (_thread_state.compare_exchange_weak(expected,
                                                 expected + 2, std::memory_order_relaxed));
    return true;
  }

  void iocp_t::exit_loop()
  {
    _thread_state.fetch_sub(2, std::memory_order_relaxed);
  }

  error_code iocp_t::iocp_wait(milliseconds_t millisecs)
  {
    using namespace std::literals::chrono_literals;

    error_code ret = error_code::NONE_ERROR;
    DWORD numberOfBytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    auto timeout = std::chrono::duration_cast<milliseconds_t>(_next_timeout_point - high_resolution_clock_t::now());

    if (timeout < 0ms)
    {
      timeout = 0ms;
    }
    if (millisecs.count() > 0 && millisecs < timeout)
    {
      timeout = millisecs;
    }

    BOOL ok = ::GetQueuedCompletionStatus(
        _iocp_handle,
        &numberOfBytesTransferred,
        &completionKey,
        &overlapped,
        static_cast<DWORD>(timeout.count()));

    _next_timeout_point = high_resolution_clock_t::now() + milliseconds_t(3000);

    if (overlapped != NULL)
    {
      auto io_state = reinterpret_cast<io_state_t *>(overlapped);
      M_ASSERT(io_state->get_coro_handle());
      M_ASSERT(!io_state->get_coro_handle().done());
      error_code state_error;
      M_ASSERT((state_error = io_state->get_error()) == error_code::IO_PENDING);
      if (ok)
      {
        io_state->io_completed(error_code::NONE_ERROR,
                               static_cast<size_t>(numberOfBytesTransferred));
      }
      else
      {
        io_state->set_error(error_code::SYSTEM_ERROR);
      }
      io_state->get_coro_handle().resume();
    }
    else if (ok)
    {
      if (completionKey != 0)
      {
        std::experimental::coroutine_handle<>::
            from_address(reinterpret_cast<void *>(completionKey))
                .resume();
      }
    }
    else if (::GetLastError() == WAIT_TIMEOUT)
    {
      ret = error_code::TIMEOUT;
    }
    else
    {
      ret = error_code::SYSTEM_ERROR;
      UNREACHABLE();
    }
    schedule_timer_t::check_timer(*this);
    if (stoped())
    {
      ret = error_code::STOPED;
    }
    return ret;
  }

  void iocp_t::schedule_coroutine(std::experimental::coroutine_handle<> handle)
  {
    BOOL ok = ::PostQueuedCompletionStatus(
        _iocp_handle,
        0,
        reinterpret_cast<ULONG_PTR>(handle.address()),
        nullptr);
    if (!ok)
    {
      // todo:put in lockfree list
    }
  }

  void iocp_t::update_next_timeout_point(high_resolution_clock_t::time_point tp) noexcept
  {
    _next_timeout_point = tp;
  }
} // namespace mrpc::detail

#endif