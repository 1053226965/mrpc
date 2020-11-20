#pragma once
#include "io/net/connection.hpp"
#include "io/net/detail/acceptor_base.hpp"

#ifdef OS_GNU_LINUX

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  class accept_task_t
  {
    using io_context_type = IO_CONTEXT;
    using acceptor_type = acceptor_base_t<io_context_type>;
    using connection_t = connection_base_t<IO_CONTEXT>;

    error_code safe_accept(io_state_t &io_state)
    {
      sockaddr_storage sock_storeage;
      socklen_t len = sizeof(sock_storeage);
      int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
      for (;;)
      {
        socket_handle_t fd = ::accept4(_acceptor.socket()->handle(),
                                       reinterpret_cast<sockaddr *>(&sock_storeage),
                                       &len, flags);
        if (fd < 0)
        {
          switch (errno)
          {
          case EINTR:
            continue;
            break;
#if EAGAIN != EWOULDBLOCK
          case EAGAIN:
          case EWOULDBLOCK:
#else
          case EAGAIN:
#endif
            return error_code::AGAIN;
          default:
            return error_code::SYSTEM_ERROR;
          }
        }
        else
        {
          _result_connection.own_socket(socket_t(fd, _acceptor.socket()->get_socket_detail()));
          _result_connection.socket()->set_remote_endpoint(*reinterpret_cast<sockaddr *>(&sock_storeage));
          return error_code::NONE_ERROR;
        }
      }
    }

  public:
    accept_task_t(io_context_type &io_ctx, acceptor_base_t<io_context_type> &acceptor) noexcept : _io_context(io_ctx),
                                                                                                  _acceptor(acceptor)
    {
    }

    bool await_ready() const noexcept
    {
      _acceptor.get_recv_io_state().set_error(error_code::INVLIAD);
      return false;
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &io_state = _acceptor.get_recv_io_state();
      io_state.set_error(error_code::IO_PENDING);
      io_state.set_coro_handle(handle);
      switch (safe_accept(io_state))
      {
      case error_code::AGAIN:
        return !_acceptor.socket()->notify_on_read(io_state);
      case error_code::NONE_ERROR:
        io_state.set_error(error_code::NONE_ERROR);
        return false;
      default:
        io_state.set_error(error_code::SYSTEM_ERROR);
        return false;
      }
    }

    connection_t await_resume()
    {
      auto &io_state = _acceptor.get_recv_io_state();
      if (io_state.get_error() != error_code::NONE_ERROR)
      {
        switch(safe_accept(io_state))
        {
        case error_code::NONE_ERROR:
          io_state.set_error(error_code::NONE_ERROR);
          break;
        case error_code::AGAIN:
          io_state.set_error(error_code::AGAIN);
          break;
        default:
          io_state.set_error(error_code::SYSTEM_ERROR);
        }
      }
      if (io_state.get_error() == error_code::NONE_ERROR)
      {
        sockaddr_storage localSockaddr;
        socklen_t nameLength = sizeof(localSockaddr);
        int result = ::getsockname(
            _result_connection.socket()->handle(),
            reinterpret_cast<sockaddr *>(&localSockaddr),
            &nameLength);
        if (result == 0)
        {
          _result_connection.socket()->set_local_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
        }

        sockaddr_storage remoteSockaddr;
        nameLength = sizeof(remoteSockaddr);
        result = ::getpeername(
            _result_connection.socket()->handle(),
            reinterpret_cast<sockaddr *>(&remoteSockaddr),
            &nameLength);
        if (result == 0)
        {
          _result_connection.socket()->set_remote_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
        }
        _result_connection.socket()->ready_to_read();
        _result_connection.socket()->ready_to_write();
        _result_connection.attach_to_io_ctx(&_io_context);
        DETAIL_LOG_INFO("[accept] new connection {}", _result_connection.socket()->handle());
        return std::move(_result_connection);
      }
      DETAIL_LOG_WARN("[accept] unknown error {}", _acceptor.get_recv_io_state().get_error());
      return connection_t();
    }

  private:
    io_context_type &_io_context;
    acceptor_type &_acceptor;
    connection_t _result_connection;
  };

} // namespace mrpc::net::detail

#endif