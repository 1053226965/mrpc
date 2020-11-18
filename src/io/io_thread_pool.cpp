#include <functional>
#include "io/io_thread_pool.hpp"

namespace mrpc
{
  io_thread_pool_t::io_thread_pool_t(io_context_t &io_ctx, size_t max_thread_count) : io_ctx_(io_ctx),
                                                                                      running_(true),
                                                                                      processing_count_(0),
                                                                                      max_thread_count_(max_thread_count)
  {
    for (size_t i = 0; i < max_thread_count; i++)
    {
      threads_.emplace_back(std::thread(std::bind(&io_thread_pool_t::thread_func, this, i)));
    }
  }

  void io_thread_pool_t::thread_func(size_t thread_index)
  {
    while (running_.load(std::memory_order_acquire))
    {
      processing_count_.fetch_add(1, std::memory_order_relaxed);
      io_ctx_.process_one(milliseconds_t(-1));
      processing_count_.fetch_sub(1, std::memory_order_relaxed);
    }
  }

  void io_thread_pool_t::stop() noexcept
  {
    running_.exchange(false, std::memory_order_acq_rel);
    io_ctx_.wakeup(processing_count_.load(std::memory_order_relaxed));
  }

  void io_thread_pool_t::join() noexcept
  {
    stop();
    for (auto &th : threads_)
    {
      if(th.joinable())
        th.join();
    }
  }
} // namespace mrpc