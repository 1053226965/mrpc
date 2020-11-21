#pragma once
#include "io/net/connection.hpp"
#include "io/net/detail/acceptor_base.hpp"

#ifdef OS_WIN

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  struct accept_task_t
  {
    using io_context_type = IO_CONTEXT;
    using acceptor_type = acceptor_base_t<io_context_type>;
    using connection_t = connection_base_t<IO_CONTEXT>;

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
      auto &acceptor_state = _acceptor.get_recv_io_state();
      DWORD bytes_transfer = 0;
      auto const &lsockt = *_acceptor.socket();
      auto &new_con = _result_connection;
      auto &new_con_state = _result_connection.get_recv_io_state();
      new_con.own_socket(socket_t(lsockt.get_socket_detail()));
      memset(_address_buffer, 0, sizeof(_address_buffer));
      acceptor_state.set_coro_handle(handle);
      acceptor_state.set_error(error_code::IO_PENDING);
      const bool skip_on_success = lsockt.skip_compeletion_port_on_success();

      const BOOL ok = _acceptor.get_accept_func()(
          _acceptor.socket()->handle(),
          new_con.socket()->handle(),
          _address_buffer,
          0,
          sizeof(_address_buffer) / 2,
          sizeof(_address_buffer) / 2,
          &bytes_transfer,
          acceptor_state.get_overlapped());

      /* 注意，不能在下面代码再使用this的成员变量。
         因为WSARecv投递异步操作后，也许，会在其他线程唤醒coroutine, this也许会被销毁 */
      if (!ok)
      {
        int ec = ::WSAGetLastError();
        if (ec != ERROR_IO_PENDING)
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
          acceptor_state.set_error(error_code::SYSTEM_ERROR);
          return false;
        }
      }
      else if (skip_on_success)
      {
        //acceptor_state.set_error(error_code::NONE_ERROR);
        //return false;
        return true;
      }
      return true;
    }

    connection_t await_resume()
    {
      if (_acceptor.get_recv_io_state().get_error() == error_code::NONE_ERROR)
      {
        SOCKET ls = _acceptor.socket()->handle();
        int result = ::setsockopt(
            _result_connection.socket()->handle(),
            SOL_SOCKET,
            SO_UPDATE_ACCEPT_CONTEXT,
            (const char *)&ls,
            sizeof(SOCKET));

        SOCKADDR_STORAGE localSockaddr;
        int nameLength = sizeof(localSockaddr);
        result = ::getsockname(
            _result_connection.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&localSockaddr),
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

        SOCKADDR_STORAGE remoteSockaddr;
        nameLength = sizeof(remoteSockaddr);
        result = ::getpeername(
            _result_connection.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&remoteSockaddr),
            &nameLength);
        if (result == 0)
        {
          _result_connection.socket()->set_remote_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          const int errorCode = ::WSAGetLastError();
          sys_msg_log(errorCode);
        }

        if (_result_connection.attach_to_io_ctx(&_io_context) != error_code::NONE_ERROR)
        {
          DETAIL_LOG_WARN("[accept] failed to attach socket to ioctx");
          return connection_t();
        }
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
    char _address_buffer[88] = {0};
  };

} // namespace mrpc::net::detail

#endif