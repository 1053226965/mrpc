#pragma once

#include <experimental/coroutine>
#include "connection_base.hpp"
#include "io/buffer.hpp"

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  class recv_task_t
  {
    using connection_t = connection_base_t<IO_CONTEXT>;

  public:
    recv_task_t(connection_t &connection, buffer_t &buffer) noexcept : _connection(connection),
                                                                       _buffer(buffer)
    {
    }

    bool await_ready() noexcept
    {
      _connection.get_recv_io_state().set_error(error_code::INVLIAD);
      return false;
    }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &io_state = _connection.get_recv_io_state();
      io_state.set_error(error_code::IO_PENDING);
      io_state.set_coro_handle(handle);
      return !_connection.socket()->notify_on_read(io_state);
    }

    size_t await_resume() noexcept
    {
      auto &io_state = _connection.get_recv_io_state();
      size_t total_size = 0;

      struct BUF
      {
        size_t buf_len;
        byte *buf = nullptr;
      };
      BUF bufs[1];

      auto get_buf = [](BUF *bufs, buffer_t &buffer) {
        return buffer.get_remain_buf_for_append(bufs, 1, [](BUF *nbuf, byte *ibuf, size_t len) {
          nbuf->buf = ibuf;
          nbuf->buf_len = len;
        });
      };

      get_buf(bufs, _buffer);
      if (bufs[0].buf == nullptr)
      {
        io_state.set_error(error_code::NOBUFS);
        return 0;
      }

      for (;;)
      {
        int read_bytes = 0;
        do
        {
          read_bytes = ::recv(_connection.socket()->handle(), bufs[0].buf, bufs[0].buf_len, 0);
        } while (read_bytes < 0 && errno == EINTR);
        if (total_size > 0 && read_bytes <= 0)
        {
          io_state.io_completed(read_bytes != 0 ? error_code::NONE_ERROR : error_code::CLOSED, total_size);
          return total_size;
        }
        if (read_bytes < 0)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
          {
            io_state.io_completed(error_code::AGAIN, 0);
            return total_size;
          }
          io_state.io_completed(error_code::SYSTEM_ERROR, 0);
          return total_size;
        }
        if (read_bytes == 0)
        {
          io_state.io_completed(error_code::CLOSED, 0);
          return total_size;
        }
        _buffer.writer_goahead(read_bytes);
        bufs[0].buf_len -= read_bytes;
        bufs[0].buf += read_bytes;
        total_size += read_bytes;
        if (bufs[0].buf_len == 0)
        {
          get_buf(bufs, _buffer);
          if (bufs[0].buf == nullptr)
          {
            io_state.io_completed(error_code::NONE_ERROR, total_size);
            return total_size;
          }
        }
      }
      return total_size;
    }

  private:
    connection_t &_connection;
    buffer_t &_buffer;
  };

} // namespace mrpc::net::detail