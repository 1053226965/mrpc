#pragma once
#include <thread>
#include <vector>
#include "io_context.hpp"

namespace mrpc
{
  class io_thread_pool_t
  {
  public:
    io_thread_pool_t(io_context_t &io_ctx, size_t max_thread_count);
    ~io_thread_pool_t() { join(); }
    size_t get_max_count() noexcept { return _max_thread_count; }
    void stop() noexcept;
    void join() noexcept;

  private:
    void thread_func(size_t thread_index);

  private:
    io_context_t &_io_ctx;
    std::atomic_bool _running;
    std::atomic_int _processing_count;
    size_t _max_thread_count;
    std::vector<std::thread> _threads;
  };
} // namespace mrpc