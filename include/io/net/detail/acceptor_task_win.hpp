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
    accept_task_t(io_context_type &io_ctx, acceptor_base_t<io_context_type> &acceptor) noexcept : io_context_(io_ctx),
                                                                                                  acceptor_(acceptor)
    {
    }

    bool await_ready() const noexcept
    {
      acceptor_.get_recv_io_state().set_error(error_code::INVLIAD);
      return false;
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      DWORD bytes_transfer = 0;
      auto const &lsockt = *acceptor_.socket();
      auto &new_con = result_connection_;
      auto &new_con_state = result_connection_.get_recv_io_state();
      new_con.own_socket(socket_t(lsockt.get_socket_detail()));
      memset(address_buffer_, 0, sizeof(address_buffer_));
      acceptor_.get_recv_io_state().set_coro_handle(handle);
      acceptor_.get_recv_io_state().set_error(error_code::IO_PENDING);
      BOOL ok = acceptor_.get_accept_func()(
          acceptor_.socket()->handle(),
          new_con.socket()->handle(),
          address_buffer_,
          0,
          sizeof(address_buffer_) / 2,
          sizeof(address_buffer_) / 2,
          &bytes_transfer,
          acceptor_.get_recv_io_state().get_overlapped());
      if (!ok)
      {
        int ec = ::WSAGetLastError();
        if (ec != ERROR_IO_PENDING)
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
          acceptor_.get_recv_io_state().set_error(error_code::ACCEPT_ERROR);
          return false;
        }
      }
      else if (acceptor_.socket()->skip_compeletion_port_on_success())
      {
        acceptor_.get_recv_io_state().set_error(error_code::NONE_ERROR);
        return false;
      }
      return true;
    }

    connection_t await_resume()
    {
      if (acceptor_.get_recv_io_state().get_error() == error_code::NONE_ERROR)
      {
        SOCKET ls = acceptor_.socket()->handle();
        int result = ::setsockopt(
            result_connection_.socket()->handle(),
            SOL_SOCKET,
            SO_UPDATE_ACCEPT_CONTEXT,
            (const char *)&ls,
            sizeof(SOCKET));

        SOCKADDR_STORAGE localSockaddr;
        int nameLength = sizeof(localSockaddr);
        result = ::getsockname(
            result_connection_.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&localSockaddr),
            &nameLength);
        if (result == 0)
        {
          result_connection_.socket()->set_local_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          DETAIL_LOG_ERROR("[accept] error {}", get_sys_error_msg());
        }

        SOCKADDR_STORAGE remoteSockaddr;
        nameLength = sizeof(remoteSockaddr);
        result = ::getpeername(
            result_connection_.socket()->handle(),
            reinterpret_cast<SOCKADDR *>(&remoteSockaddr),
            &nameLength);
        if (result == 0)
        {
          result_connection_.socket()->set_remote_endpoint(
              *reinterpret_cast<const sockaddr *>(&localSockaddr));
        }
        else
        {
          const int errorCode = ::WSAGetLastError();
          sys_msg_log(errorCode);
        }

        if (result_connection_.attach_to_io_ctx(&io_context_) != error_code::NONE_ERROR)
        {
          DETAIL_LOG_WARN("[accept] failed to attach socket to ioctx");
          return connection_t();
        }
        DETAIL_LOG_INFO("[accept] new connection {}", result_connection_.socket()->handle());
        return std::move(result_connection_);
      }
      DETAIL_LOG_WARN("[accept] unknown error {}", acceptor_.get_recv_io_state().get_error());
      return connection_t();
    }

  private:
    io_context_type &io_context_;
    acceptor_type &acceptor_;
    connection_t result_connection_;
    char address_buffer_[88] = {0};
  };

} // namespace mrpc::net::detail

#endif