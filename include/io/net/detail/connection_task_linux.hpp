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
    connect_task_t(io_context_type &io_ctx, endpoint_t const &e) noexcept : io_ctx_(io_ctx),
                                                                            remote_endpoint_(e)
    {
    }

    bool await_ready() noexcept
    {
      new_connection_.get_send_io_state().set_error(error_code::INVLIAD);
      return false;
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &new_con_state = new_connection_.get_send_io_state();
      M_ASSERT(!new_connection_.valid());
      if (remote_endpoint_.is_v4())
      {
        new_connection_.own_socket(
            socket_t(socket_t::af_inet, socket_t::sock_stream, socket_t::default_protocol));
      }
      else if (remote_endpoint_.is_v6())
      {
        new_connection_.own_socket(
            socket_t(socket_t::af_inet6, socket_t::sock_stream, socket_t::default_protocol));
      }
      new_connection_.attach_to_io_ctx(&io_ctx_);
      new_con_state.set_coro_handle(handle);
      new_con_state.set_error(error_code::IO_PENDING);
      int err;
      do
      {
        err = ::connect(new_connection_.socket()->handle(),
                        remote_endpoint_.get_sockaddr(),
                        static_cast<socklen_t>(remote_endpoint_.addr_len()));
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
      return !new_connection_.socket()->notify_on_write(new_con_state);
    }

    connection_t await_resume()
    {
      auto &io_state = new_connection_.get_send_io_state();
      if (io_state.get_error() != error_code::NONE_ERROR)
      {
        int err;
        int so_error = 0;
        do
        {
          socklen_t so_error_size = sizeof(so_error);
          err = getsockopt(new_connection_.socket()->handle(), SOL_SOCKET, SO_ERROR, &so_error,
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
            new_connection_.socket()->handle(),
            reinterpret_cast<sockaddr *>(&localSockaddr),
            &nameLength);
        if (result == 0)
        {
          new_connection_.socket()->set_local_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
        }

        sockaddr_storage remoteSockaddr;
        nameLength = sizeof(remoteSockaddr);
        result = ::getpeername(
            new_connection_.socket()->handle(),
            reinterpret_cast<sockaddr *>(&remoteSockaddr),
            &nameLength);
        if (result == 0)
        {
          new_connection_.socket()->set_remote_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
        }
        new_connection_.socket()->ready_to_read();
        new_connection_.socket()->ready_to_write();
        DETAIL_LOG_INFO("[connect] connected {}", new_connection_.socket()->handle());
        return std::move(new_connection_);
      }
      return connection_t();
    }

  private:
    io_context_type &io_ctx_;
    endpoint_t remote_endpoint_;
    connection_t new_connection_;
  };

} // namespace mrpc::net::detail

#endif