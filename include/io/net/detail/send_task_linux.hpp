#pragma once
#include "io/net/detail/connection_base.hpp"
#include "io/buffer.hpp"

#ifdef OS_GNU_LINUX

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  class send_task_t
  {
    using connection_t = connection_base_t<IO_CONTEXT>;

    error_code try_send(size_t &total_size)
    {
      struct BUF
      {
        size_t buf_len;
        byte *buf = nullptr;
      };
      BUF bufs[1];

      auto get_buf = [](BUF *bufs, buffer_t &buffer) {
        buffer.append_remain_msg_to_array(bufs, 1, [](BUF *nbuf, byte *ibuf, size_t len) {
          nbuf->buf = ibuf;
          nbuf->buf_len = len;
        });
      };
      get_buf(bufs, _buffer);
      while (!done())
      {
        int bytes;
        do
        {
          bytes = ::send(_connection.socket()->handle(), bufs[0].buf, bufs[0].buf_len, 0);
        } while (bytes < 0 && errno == EINTR);

        if (bytes < 0)
        {
          switch (errno)
          {
#if EAGAIN == EWOULDBLOCK
          case EAGAIN:
#else
          case EAGAIN:
          case EWOULDBLOCK:
#endif
            if (!done())
            {
              return error_code::AGAIN;
            }
            return error_code::NONE_ERROR;
          default:
            return error_code::SYSTEM_ERROR;
          }
        }
        _buffer.reader_goahead(bytes);
        total_size += bytes;
        get_buf(bufs, _buffer);
      }
      return error_code::NONE_ERROR;
    }

  public:
    template <typename BUFFER>
    send_task_t(connection_t &connection, BUFFER &&buffer) noexcept : _connection(connection),
                                                                      _buffer(std::forward<BUFFER>(buffer))
    {
    }

    bool done() noexcept
    {
      _connection.get_send_io_state().set_error(error_code::INVLIAD);
      return _buffer.is_eof();
    }

    bool await_ready() noexcept { return _buffer.is_eof(); }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &io_state = _connection.get_send_io_state();
      size_t total_size = 0;
      switch (try_send(total_size))
      {
      case error_code::NONE_ERROR:
        io_state.io_completed(error_code::NONE_ERROR, total_size);
        return false;
      case error_code::AGAIN:
        io_state.set_coro_handle(handle);
        io_state.io_completed(error_code::IO_PENDING, total_size);
        return !_connection.socket()->notify_on_write(io_state);
      default:
        io_state.io_completed(error_code::SYSTEM_ERROR, total_size);
        return false;
      }
    }

    size_t await_resume() noexcept
    {
      auto &io_state = _connection.get_send_io_state();
      switch (io_state.get_error())
      {
      case error_code::NONE_ERROR:
      case error_code::SYSTEM_ERROR:
        return io_state.bytes_transfer();
      case error_code::IO_PENDING:
      {
        size_t total_size = io_state.bytes_transfer();
        switch (try_send(total_size))
        {
        case error_code::NONE_ERROR:
          io_state.io_completed(error_code::NONE_ERROR, total_size);
          return total_size;
        case error_code::AGAIN:
          io_state.io_completed(error_code::AGAIN, total_size);
          return total_size;
        default:
          io_state.io_completed(error_code::SYSTEM_ERROR, total_size);
          return total_size;
        }
      }
      default:
        UNREACHABLE();
      }
      UNREACHABLE();
      return 0;
    }

  private:
    connection_t &_connection;
    mrpc::buffer_t _buffer;
  };
} // namespace mrpc::net::detail

#endif