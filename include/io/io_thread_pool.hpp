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
    size_t get_max_count() noexcept { return max_thread_count_; }
    void stop() noexcept;
    void join() noexcept;

  private:
    void thread_func(size_t thread_index);

  private:
    io_context_t &io_ctx_;
    std::atomic_bool running_;
    std::atomic_int processing_count_;
    size_t max_thread_count_;
    std::vector<std::thread> threads_;
  };
} // namespace mrpc