#pragma once

#include <experimental/coroutine>
#include "io/net/detail/connection_base.hpp"
#include "io/buffer.hpp"

#ifdef OS_WIN

namespace mrpc::net::detail
{
  template<typename IO_CONTEXT>
  class recv_task_t
  {
    using connection_t = connection_base_t<IO_CONTEXT>;
  public:
    recv_task_t(connection_t &connection, buffer_t &buffer) noexcept : connection_(connection),
                                                                            buffer_(buffer)
    {
    }

    bool await_ready() noexcept { return false; }

    bool await_suspend(std::experimental::coroutine_handle<> handle)
    {
      auto &state = connection_.get_recv_io_state();

      DWORD bytes_transfer = 0;
      WSABUF bufs[max_chunk_count];
      size_t bufs_size = buffer_.get_remain_buf_for_append(bufs, max_chunk_count,
                                        [](WSABUF *nbuf, byte *ibuf, size_t len) {
                                          nbuf->buf = reinterpret_cast<char *>(ibuf);
                                          nbuf->len = static_cast<ULONG>(len);
                                        });
      DWORD flags = 0;
      state.set_coro_handle(handle);
      state.set_error(error_code::IO_PENDING);
      int result = ::WSARecv(
          connection_.socket()->handle(),
          bufs,
          static_cast<DWORD>(bufs_size), // buffer count
          &bytes_transfer,
          &flags, // flags
          state.get_overlapped(),
          nullptr);
      if (result == SOCKET_ERROR)
      {
        int errorCode = ::WSAGetLastError();
        if (errorCode != WSA_IO_PENDING)
        {
          DETAIL_LOG_ERROR("[recv] socket: {} error: {}", connection_.socket()->handle(),
                  get_sys_error_msg());
          state.set_error(error_code::SYSTEM_ERROR);
          return false;
        }
      }
      else if (connection_.socket()->skip_compeletion_port_on_success())
      {
        M_ASSERT(result == 0);
        // state.io_completed(error_code::NONE_ERROR,
        //                    static_cast<size_t>(bytes_transfer));
        // return false;
        return true;
      }
      else
      {
        M_ASSERT(false);
      }
      return true;
    }

    size_t await_resume() noexcept
    {
      auto& state = connection_.get_recv_io_state();
      if(state.bytes_transfer() == 0)
      {
        state.set_error(error_code::CLOSED);
        return 0;
      }
      buffer_.writer_goahead(connection_.get_recv_io_state().bytes_transfer());
      return connection_.get_recv_io_state().bytes_transfer();
    }

  private:
    connection_t &connection_;
    buffer_t &buffer_;
  };
} // namespace mrpc::net::detail

#endif