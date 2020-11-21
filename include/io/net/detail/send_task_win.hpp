#pragma once
#include "io/net/detail/connection_base.hpp"
#include "io/buffer.hpp"

#ifdef OS_WIN

namespace mrpc::net::detail
{
  template <typename IO_CONTEXT>
  class send_task_t
  {
    using connection_t = connection_base_t<IO_CONTEXT>;

  public:
    template <typename BUFFER>
    send_task_t(connection_t &connection, BUFFER &&buffer) noexcept : _connection(connection),
                                                                      _buffer(std::forward<BUFFER>(buffer))
    {
    }

    bool await_ready() noexcept { return false; }

    bool await_suspend(std::experimental::coroutine_handle<> coroutine)
    {
      auto &state = _connection.get_send_io_state();

      DWORD bytes_transfer = 0;
      WSABUF bufs[max_chunk_count];
      size_t bufs_size = _buffer.append_remain_msg_to_array(bufs, max_chunk_count,
                                                            [](WSABUF *nbuf, byte *ibuf, size_t len) {
                                                              nbuf->buf = reinterpret_cast<char *>(ibuf);
                                                              nbuf->len = static_cast<ULONG>(len);
                                                            });
      state.set_coro_handle(coroutine);
      state.set_error(error_code::IO_PENDING);
      const bool skip_on_success = _connection.socket()->skip_compeletion_port_on_success();
      socket_handle_t sock_handle = _connection.socket()->handle();
      
      int result = ::WSASend(
          sock_handle,
          bufs,
          static_cast<DWORD>(bufs_size), // buffer count
          &bytes_transfer,
          0, // flags
          state.get_overlapped(),
          nullptr);

      /* 注意，不能在下面代码再使用this的成员变量。
         因为WSARecv投递异步操作后，也许，会在其他线程唤醒coroutine, this也许会被销毁 */
      if (result == SOCKET_ERROR)
      {
        int errorCode = ::WSAGetLastError();
        if (errorCode != WSA_IO_PENDING)
        {
          DETAIL_LOG_ERROR("[send] socket: {} error: {}", sock_handle,
                  get_sys_error_msg());
          state.set_error(error_code::SYSTEM_ERROR);
          return false;
        }
      }
      else if (skip_on_success)
      {
        // skip_on_success不生效（原因未知），所以iocp会通知
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
      _buffer.reader_goahead(_connection.get_send_io_state().bytes_transfer());
      return _connection.get_send_io_state().bytes_transfer();
    }

  private:
    connection_t &_connection;
    mrpc::buffer_t _buffer;
  };

} // namespace mrpc::net::detail

#endif