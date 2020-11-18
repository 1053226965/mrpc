#pragma once
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

    std::shared_ptr<socket_t> &socket() { return socket_; }
    void own_socket(socket_t &&s) noexcept { socket_ = std::make_shared<socket_t>(std::move(s)); }
    error_code attach_to_io_ctx(IO_CONTEXT *io_ctx);

    io_state_t &get_recv_io_state() { return recv_io_state_; }
    io_state_t &get_send_io_state() { return send_io_state_; }
    bool valid() noexcept { return socket_ && socket_->valid(); }
    void shutdown_wr() noexcept;
    void close() noexcept;

  private:
    IO_CONTEXT *io_ctx_ = nullptr;
    std::shared_ptr<socket_t> socket_;
    io_state_t recv_io_state_;
    io_state_t send_io_state_;
  };

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT>::connection_base_t() noexcept : io_ctx_(nullptr),
                                                                       recv_io_state_(io_state_t::type_t::recv),
                                                                       send_io_state_(io_state_t::type_t::send)
  {
  }

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT>::connection_base_t(socket_t &&s) noexcept : io_ctx_(nullptr),
                                                                                   socket_(std::make_unique<socket_t>(std::move(s))),
                                                                                   recv_io_state_(io_state_t::type_t::recv),
                                                                                   send_io_state_(io_state_t::type_t::send)
  {
  }

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT>::connection_base_t(connection_base_t &&c) noexcept : io_ctx_(c.io_ctx_),
                                                                                            socket_(std::move(c.socket_)),
                                                                                            recv_io_state_(std::move(c.recv_io_state_)),
                                                                                            send_io_state_(std::move(c.send_io_state_))
  {
  }

  template <typename IO_CONTEXT>
  inline connection_base_t<IO_CONTEXT> &connection_base_t<IO_CONTEXT>::operator=(connection_base_t<IO_CONTEXT> &&c) noexcept
  {
    io_ctx_ = c.io_ctx_;
    socket_ = std::move(c.socket_);
    recv_io_state_ = std::move(c.recv_io_state_);
    send_io_state_ = std::move(c.send_io_state_);
    return *this;
  }

  template <typename IO_CONTEXT>
  inline error_code connection_base_t<IO_CONTEXT>::attach_to_io_ctx(IO_CONTEXT *io_ctx)
  {
    io_ctx_ = io_ctx;
    return io_ctx_->add_socket(socket());
  }

  template <typename IO_CONTEXT>
  inline void connection_base_t<IO_CONTEXT>::shutdown_wr() noexcept
  {
    if (socket_)
    {
      socket_->shutdown_wr();
    }
  }

  template <typename IO_CONTEXT>
  inline void connection_base_t<IO_CONTEXT>::close() noexcept
  {
    if (socket_)
    {
      if (io_ctx_)
      {
        if (io_ctx_->rem_socket(socket()) != error_code::NONE_ERROR)
        {
          DETAIL_LOG_WARN("[connection] failed to detach fd {} from ioctx", socket()->handle());
        }
        else
        {
          DETAIL_LOG_INFO("[connection] {} detached from ioctx", socket()->handle());
        }
      }
      socket_->close();
#ifdef OS_GNU_LINUX
      socket_->ready_to_read(); // wakeup coroutine
      socket_->ready_to_write(); // wakeup coroutine
#endif
    }
  }
} // namespace mrpc::net::detail