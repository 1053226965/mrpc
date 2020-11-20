#include <functional>
#include "io/io_thread_pool.hpp"

namespace mrpc
{
  io_thread_pool_t::io_thread_pool_t(io_context_t &io_ctx, size_t max_thread_count) : _io_ctx(io_ctx),
                                                                                      _running(true),
                                                                                      _processing_count(0),
                                                                                      _max_thread_count(max_thread_count)
  {
    for (size_t i = 0; i < max_thread_count; i++)
    {
      _threads.emplace_back(std::thread(std::bind(&io_thread_pool_t::thread_func, this, i)));
    }
  }

  void io_thread_pool_t::thread_func(size_t thread_index)
  {
    while (_running.load(std::memory_order_acquire))
    {
      _processing_count.fetch_add(1, std::memory_order_relaxed);
      _io_ctx.process_one(milliseconds_t(-1));
      _processing_count.fetch_sub(1, std::memory_order_relaxed);
    }
  }

  void io_thread_pool_t::stop() noexcept
  {
    _running.exchange(false, std::memory_order_acq_rel);
    _io_ctx.wakeup(_processing_count.load(std::memory_order_relaxed));
  }

  void io_thread_pool_t::join() noexcept
  {
    stop();
    for (auto &th : _threads)
    {
      if(th.joinable())
        th.join();
    }
  }
} // namespace mrpc