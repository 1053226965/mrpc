#pragma once
#include "io/io_state.hpp"
#include "io/buffer.hpp"

#ifdef OS_WIN
#include "io/net/detail/connection_task_win.hpp"
#include "io/net/detail/recv_task_win.hpp"
#include "io/net/detail/send_task_win.hpp"
#elif defined(OS_GNU_LINUX)
#include "io/net/detail/connection_task_linux.hpp"
#include "io/net/detail/recv_task_linux.hpp"
#include "io/net/detail/send_task_linux.hpp"
#endif

namespace mrpc
{
  namespace net
  {
    using connection_t = detail::connection_base_t<io_context_t>;
    using connect_task_t = detail::connect_task_t<io_context_t>;

#ifdef OS_WIN

    using recv_task_t = detail::recv_task_t<io_context_t>;
    using send_task_t = detail::send_task_t<io_context_t>;

#elif defined(OS_GNU_LINUX)

    inline task_t<size_t> recv_task_t(connection_t &connection, buffer_t & buffer)
    {
      auto task = detail::recv_task_t<io_context_t>(connection, buffer);
      size_t total = 0;
      do
      {
        total += co_await task;
      } while (connection.get_recv_io_state().get_error() == error_code::AGAIN);
      co_return total;
    }

    template <typename BUFFER>
    inline task_t<size_t> send_task_t(connection_t &connection, BUFFER &&buffer)
    {
      auto task = detail::send_task_t<io_context_t>(connection, std::forward<BUFFER>(buffer));
      size_t total = 0;
      while (!task.done())
      {
        total += co_await task;
      }
      co_return total;
    }
    
#endif
  } // namespace net
} // namespace mrpc