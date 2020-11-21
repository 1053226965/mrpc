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
    connect_task_t(io_context_type &io_ctx, endpoint_t const &e) noexcept : _io_ctx(io_ctx),
                                                                            _remote_endpoint(e)
    {
    }

    bool await_ready() noexcept
    {
      _new_connection.get_recv_io_state().set_error(error_code::INVLIAD);
      return false;
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &new_con_state = _new_connection.get_recv_io_state();
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

      LPFN_CONNECTEX connectExPtr;
      {
        GUID connectExGuid = WSAID_CONNECTEX;
        DWORD byteCount = 0;
        int result = ::WSAIoctl(
            _new_connection.socket()->handle(),
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

      _new_connection.socket()->bind_addr(std::string_view("0.0.0.0:0"));
      _new_connection.attach_to_io_ctx(&_io_ctx);

      DWORD bytes_send;
      new_con_state.set_coro_handle(handle);
      new_con_state.set_error(error_code::IO_PENDING);
      const bool skip_on_success = _new_connection.socket()->skip_compeletion_port_on_success();

      const BOOL ok = connectExPtr(
          static_cast<SOCKET>(_new_connection.socket()->handle()),
          reinterpret_cast<const SOCKADDR *>(_remote_endpoint.get_sockaddr()),
          static_cast<int>(_remote_endpoint.addr_len()),
          nullptr, // send buffer
          0,       // size of send buffer
          &bytes_send,
          new_con_state.get_overlapped());

      /* 注意，不能在下面代码再使用this的成员变量。
         因为WSARecv投递异步操作后，也许，会在其他线程唤醒coroutine, this也许会被销毁 */
      if (!ok)
      {
        const int errorCode = ::WSAGetLastError();
        if (errorCode != ERROR_IO_PENDING)
        {
          DETAIL_LOG_ERROR("[connect] error {}", get_sys_error_msg());
          new_con_state.set_error(error_code::SYSTEM_ERROR);
          return false;
        }
      }
      else if (skip_on_success)
      {
        //new_con_state.set_error(error_code::NONE_ERROR);
        //return false;
        return true;
      }
      return true;
    }

    connection_t await_resume()
    {
      if (_new_connection.get_recv_io_state().get_error() == error_code::NONE_ERROR)
      {
        setsockopt(_new_connection.socket()->handle(),
                     SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        SOCKADDR_STORAGE localSockaddr;
        int nameLength = sizeof(localSockaddr);
        int result = ::getsockname(
            _new_connection.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&localSockaddr),
            &nameLength);
        if (result == 0)
        {
          _new_connection.socket()->set_local_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
           DETAIL_LOG_ERROR("[connect] error {}", get_sys_error_msg());
        }

        SOCKADDR_STORAGE remoteSockaddr;
        nameLength = sizeof(remoteSockaddr);
        result = ::getpeername(
            _new_connection.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&remoteSockaddr),
            &nameLength);
        if (result == 0)
        {
          _new_connection.socket()->set_remote_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
           DETAIL_LOG_ERROR("[connect] error {}", get_sys_error_msg());
        }
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