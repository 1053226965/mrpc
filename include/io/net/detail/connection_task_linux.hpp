#pragma once
#include "io/net/detail/connection_base.hpp"

#ifdef OS_GNU_LINUX

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  class connect_task_t
  {
    using io_context_type = IO_CONTEXT;
    using connection_t = connection_base_t<IO_CONTEXT>;
  public:
    connect_task_t(io_context_type &io_ctx, endpoint_t const &e) noexcept : _io_ctx(io_ctx),
                                                                            _remote_endpoint(e)
    {
    }

    bool await_ready() noexcept
    {
      _new_connection.get_send_io_state().set_error(error_code::INVLIAD);
      return false;
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &new_con_state = _new_connection.get_send_io_state();
      M_ASSERT(!_new_connection.valid());
      if (_remote_endpoint.is_v4())
      {
        _new_connection.own_socket(
            socket_t(socket_t::af_inet, socket_t::sock_stream, socket_t::default_protocol));
      }
      else if (_remote_endpoint.is_v6())
      {
        _new_connection.own_socket(
            socket_t(socket_t::af_inet6, socket_t::sock_stream, socket_t::default_protocol));
      }
      _new_connection.attach_to_io_ctx(&_io_ctx);
      new_con_state.set_coro_handle(handle);
      new_con_state.set_error(error_code::IO_PENDING);
      int err;
      do
      {
        err = ::connect(_new_connection.socket()->handle(),
                        _remote_endpoint.get_sockaddr(),
                        static_cast<socklen_t>(_remote_endpoint.addr_len()));
      } while (err < 0 && errno == EINTR);
      if (err >= 0)
      {
        new_con_state.set_error(error_code::NONE_ERROR);
        return false;
      }
      if (errno != EAGAIN && errno != EINPROGRESS)
      {
        new_con_state.set_error(error_code::SYSTEM_ERROR);
        return false;
      }
      return !_new_connection.socket()->notify_on_write(new_con_state);
    }

    connection_t await_resume()
    {
      auto &io_state = _new_connection.get_send_io_state();
      if (io_state.get_error() != error_code::NONE_ERROR)
      {
        int err;
        int so_error = 0;
        do
        {
          socklen_t so_error_size = sizeof(so_error);
          err = getsockopt(_new_connection.socket()->handle(), SOL_SOCKET, SO_ERROR, &so_error,
                           &so_error_size);
        } while (err < 0 && errno == EINTR);
        if (err == 0)
        {
          switch (so_error)
          {
          case 0:
            io_state.set_error(error_code::NONE_ERROR);
            break;
          case ENOBUFS:
            io_state.set_error(error_code::NOBUFS);
            break;
          case ECONNREFUSED:
            io_state.set_error(error_code::CONNREFUSED);
            break;
          default:
            io_state.set_error(error_code::SYSTEM_ERROR);
          }
        }
      }
      if (io_state.get_error() == error_code::NONE_ERROR)
      {
        sockaddr_storage localSockaddr;
        socklen_t nameLength = sizeof(localSockaddr);
        int result = ::getsockname(
            _new_connection.socket()->handle(),
            reinterpret_cast<sockaddr *>(&localSockaddr),
            &nameLength);
        if (result == 0)
        {
          _new_connection.socket()->set_local_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
        }

        sockaddr_storage remoteSockaddr;
        nameLength = sizeof(remoteSockaddr);
        result = ::getpeername(
            _new_connection.socket()->handle(),
            reinterpret_cast<sockaddr *>(&remoteSockaddr),
            &nameLength);
        if (result == 0)
        {
          _new_connection.socket()->set_remote_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
        }
        _new_connection.socket()->ready_to_read();
        _new_connection.socket()->ready_to_write();
        DETAIL_LOG_INFO("[connect] connected {}", _new_connection.socket()->handle());
        return std::move(_new_connection);
      }
      return connection_t();
    }

  private:
    io_context_type &_io_ctx;
    endpoint_t _remote_endpoint;
    connection_t _new_connection;
  };

} // namespace mrpc::net::detail

#endif