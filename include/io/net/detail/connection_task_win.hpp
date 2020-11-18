#pragma once
#include "io/net/detail/connection_base.hpp"

#ifdef OS_WIN

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
      new_connection_.get_recv_io_state().set_error(error_code::INVLIAD);
      return false;
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &new_con_state = new_connection_.get_recv_io_state();
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

      LPFN_CONNECTEX connectExPtr;
      {
        GUID connectExGuid = WSAID_CONNECTEX;
        DWORD byteCount = 0;
        int result = ::WSAIoctl(
            new_connection_.socket()->handle(),
            SIO_GET_EXTENSION_FUNCTION_POINTER,
            static_cast<void *>(&connectExGuid),
            sizeof(connectExGuid),
            static_cast<void *>(&connectExPtr),
            sizeof(connectExPtr),
            &byteCount,
            nullptr,
            nullptr);
        if (result == SOCKET_ERROR)
        {
          new_con_state.set_error(error_code::SYSTEM_ERROR);
          //DWORD errorCode = ::GetLastError();
          return false;
        }
      }

      new_connection_.socket()->bind_addr(std::string_view("0.0.0.0:0"));
      new_connection_.attach_to_io_ctx(&io_ctx_);

      DWORD bytes_send;
      new_con_state.set_coro_handle(handle);
      new_con_state.set_error(error_code::IO_PENDING);
      const BOOL ok = connectExPtr(
          static_cast<SOCKET>(new_connection_.socket()->handle()),
          reinterpret_cast<const SOCKADDR *>(remote_endpoint_.get_sockaddr()),
          static_cast<int>(remote_endpoint_.addr_len()),
          nullptr, // send buffer
          0,       // size of send buffer
          &bytes_send,
          new_con_state.get_overlapped());
      if (!ok)
      {
        const int errorCode = ::WSAGetLastError();
        if (errorCode != ERROR_IO_PENDING)
        {
          DETAIL_LOG_ERROR("[connect] error {}", get_sys_error_msg());
          new_con_state.set_error(error_code::SYSTEM_ERROR);
        }
      }
      else if (new_connection_.socket()->skip_compeletion_port_on_success())
      {
        //new_con_state.set_error(error_code::NONE_ERROR);
        //return false;
        return true;
      }
      return true;
    }

    connection_t await_resume()
    {
      if (new_connection_.get_recv_io_state().get_error() == error_code::NONE_ERROR)
      {
        setsockopt(new_connection_.socket()->handle(),
                     SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        SOCKADDR_STORAGE localSockaddr;
        int nameLength = sizeof(localSockaddr);
        int result = ::getsockname(
            new_connection_.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&localSockaddr),
            &nameLength);
        if (result == 0)
        {
          new_connection_.socket()->set_local_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
           DETAIL_LOG_ERROR("[connect] error {}", get_sys_error_msg());
        }

        SOCKADDR_STORAGE remoteSockaddr;
        nameLength = sizeof(remoteSockaddr);
        result = ::getpeername(
            new_connection_.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&remoteSockaddr),
            &nameLength);
        if (result == 0)
        {
          new_connection_.socket()->set_remote_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
           DETAIL_LOG_ERROR("[connect] error {}", get_sys_error_msg());
        }
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