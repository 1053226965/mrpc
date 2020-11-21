#pragma once
#include "common/coroutine/coro_mutex.hpp"
#include "io/net/socket.hpp"
#include "io/io_state.hpp"

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  class connection_base_t
  {
    connection_base_t(connection_base_t const &) = delete;
    connection_base_t &operator=(connection_base_t const &) = delete;

  public:
    connection_base_t() noexcept;
    explicit connection_base_t(socket_t &&s) noexcept;
    connection_base_t(connection_base_t &&c) noexcept;
    ~connection_base_t() noexcept { close(); }
    connection_base_t &operator=(connection_base_t &&c) noexcept;

    std::shared_ptr<socket_t> &socket() { return _socket; }
    void own_socket(socket_t &&s) noexcept { _socket = std::make_shared<socket_t>(std::move(s)); }
    error_code attach_to_io_ctx(IO_CONTEXT *io_ctx);

    io_state_t &get_recv_io_state() noexcept { return _recv_io_state; }
    io_state_t &get_send_io_state() noexcept { return _send_io_state; }

    auto get_scope_mutex_for_sending() noexcept { return std::move(_send_strand_mtx.scope_mutex()); }

    bool valid() noexcept { return _socket && _socket->valid(); }
    bool shutdown_wr() noexcept;
    void close() noexcept;

  private:
    IO_CONTEXT *_io_ctx = nullptr;
    std::shared_ptr<socket_t> _socket;
    io_state_t _recv_io_state;
    io_state_t _send_io_state;
    coro_mutex_t _send_strand_mtx;
  };

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT>::connection_base_t() noexcept : _io_ctx(nullptr),
                                                                       _recv_io_state(io_state_t::type_t::recv),
                                                                       _send_io_state(io_state_t::type_t::send)
  {
  }

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT>::connection_base_t(socket_t &&s) noexcept : _io_ctx(nullptr),
                                                                                   _socket(std::make_unique<socket_t>(std::move(s))),
                                                                                   _recv_io_state(io_state_t::type_t::recv),
                                                                                   _send_io_state(io_state_t::type_t::send)
  {
  }

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT>::connection_base_t(connection_base_t &&c) noexcept : _io_ctx(c._io_ctx),
                                                                                            _socket(std::move(c._socket)),
                                                                                            _recv_io_state(std::move(c._recv_io_state)),
                                                                                            _send_io_state(std::move(c._send_io_state))
  {
  }

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT> &connection_base_t<IO_CONTEXT>::operator=(connection_base_t<IO_CONTEXT> &&c) noexcept
  {
    _io_ctx = c._io_ctx;
    _socket = std::move(c._socket);
    _recv_io_state = std::move(c._recv_io_state);
    _send_io_state = std::move(c._send_io_state);
    return *this;
  }

  template <typename IO_CONTEXT>
  inline error_code connection_base_t<IO_CONTEXT>::attach_to_io_ctx(IO_CONTEXT *io_ctx)
  {
    _io_ctx = io_ctx;
    return _io_ctx->add_socket(socket());
  }

  template <typename IO_CONTEXT>
  inline bool connection_base_t<IO_CONTEXT>::shutdown_wr() noexcept
  {
    if (_socket)
    {
      return _socket->shutdown_wr(); 
    }
    return false;
  }

  template <typename IO_CONTEXT>
  inline void connection_base_t<IO_CONTEXT>::close() noexcept
  {
    if (_socket)
    {
      if (_io_ctx)
      {
        if (_io_ctx->rem_socket(socket()) != error_code::NONE_ERROR)
        {
          DETAIL_LOG_WARN("[connection] failed to detach fd {} from ioctx", socket()->handle());
        }
        else
        {
          DETAIL_LOG_INFO("[connection] {} detached from ioctx", socket()->handle());
        }
      }
      _socket->close();
#ifdef OS_GNU_LINUX
      _socket->ready_to_read(); // wakeup coroutine
      _socket->ready_to_write(); // wakeup coroutine
#endif
    }
  }
} // namespace mrpc::net::detail