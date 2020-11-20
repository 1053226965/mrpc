#pragma once
#include "connection_base.hpp"

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  class acceptor_base_t
  {
    using io_context_type = IO_CONTEXT;
    acceptor_base_t(acceptor_base_t const &) = delete;
    acceptor_base_t &operator=(acceptor_base_t const &) = delete;

  public:
    acceptor_base_t(io_context_type &io_ctx, endpoint_t const &e);

    io_state_t &get_recv_io_state() { return _io_state; }
    std::shared_ptr<socket_t> &socket() { return _socket; }
    void own_socket(socket_t &&s) noexcept { _socket = std::make_shared<socket_t>(std::move(s)); }
    void close() noexcept;

#ifdef OS_WIN
  public:
    LPFN_ACCEPTEX get_accept_func() { return _accept_ex; }

  private:
    LPFN_ACCEPTEX _accept_ex = {nullptr};
#endif

  private:
    io_context_type &_io_ctx;
    std::shared_ptr<socket_t> _socket;
    io_state_t _io_state;
    endpoint_t _bound_info;
  };

  template <typename IO_CONTEXT>
  inline acceptor_base_t<IO_CONTEXT>::acceptor_base_t(
      io_context_type &io_ctx,
      endpoint_t const &e) : _io_ctx(io_ctx),
                             _io_state(io_state_t::type_t::accept),
                             _bound_info(e)
  {
    socket_t lsocket;
    if (e.is_v4())
    {
      lsocket = socket_t(socket_t::af_inet, socket_t::sock_stream, socket_t::default_protocol);
    }
    else if (e.is_v6())
    {
      lsocket = socket_t(socket_t::af_inet6, socket_t::sock_stream, socket_t::default_protocol);
    }
    else
    {
      DETAIL_LOG_WARN("[accept] unknown endpoint");
      return;
    }
    if (!lsocket.valid())
    {
      DETAIL_LOG_WARN("[accept] failed to create fd {}", get_sys_error_msg());
      _io_state.set_error(error_code::INVLIAD);
      return;
    }

    if (lsocket.bind_addr(e) == error_code::NONE_ERROR &&
        listen(lsocket.handle(), SOMAXCONN) != SOCKET_ERROR)
    {
      _io_state.set_error(error_code::NONE_ERROR);
      own_socket(std::move(lsocket));
      if (io_ctx.add_socket(socket()) != error_code::NONE_ERROR)
      {
         DETAIL_LOG_WARN("[accept] failed to attach fd {} to ioctx {}", socket()->handle(), get_sys_error_msg());
         return;
      }
      DETAIL_LOG_INFO("[accept] create: {}", socket()->handle());
    }
    else
    {
      DETAIL_LOG_WARN("[accept] failed to create fd {}", get_sys_error_msg());
    }

#ifdef OS_WIN
    int status;
    GUID guid = WSAID_ACCEPTEX;
    DWORD ioctl_num_bytes;
    _accept_ex = nullptr;
    status =
        WSAIoctl(socket()->handle(), SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &_accept_ex, sizeof(_accept_ex), &ioctl_num_bytes, NULL, NULL);
    if (status != 0)
    {
      DWORD errorCode = ::GetLastError();
      throw std::system_error{
          static_cast<int>(errorCode),
          std::system_category(),
          "Error get WSAID_ACCEPTEX: WSAIoctl"};
    }
#endif
  }

  template <typename IO_CONTEXT>
  inline void acceptor_base_t<IO_CONTEXT>::close() noexcept
  {
    if (_socket && _socket->valid())
    {
      if(_io_ctx.rem_socket(_socket) != error_code::NONE_ERROR)
      {
        DETAIL_LOG_WARN("[accept] failed to detach fd {} from ioctx", socket()->handle());
      }
      _socket->close();
#ifdef OS_GNU_LINUX
      _socket->ready_to_read(); // wakeup coroutine
#endif
    }
  }
} // namespace mrpc::net::detail