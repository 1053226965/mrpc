#pragma once
#include "common/coroutine/task.hpp"
#include "common/time/types.hpp"
#include "common/heap_sort.hpp"
#include "io/io_context.hpp"

namespace mrpc::detail
{
  class timer_canceller_t
  {
    friend class schedule_timer_t;

  public:
    void cancel()
    {
      M_ASSERT(cancelled_);
      cancelled_->store(true, std::memory_order_relaxed);
    }

  private:
    void set(std::shared_ptr<std::atomic_bool> const &flag) { cancelled_ = flag; }
    std::shared_ptr<std::atomic_bool> cancelled_;
  };

  class schedule_timer_t
  {
    schedule_timer_t(schedule_timer_t const &) = delete;
    schedule_timer_t &operator=(schedule_timer_t const &) = delete;

  public:
    struct pointer_cmp_type;

    schedule_timer_t(io_context_t &io_ctx, milliseconds_t const &wait_time) noexcept;
    schedule_timer_t(io_context_t &io_ctx, milliseconds_t const &wait_time, timer_canceller_t &canceller) noexcept;
    schedule_timer_t(schedule_timer_t &&timer) noexcept;
    ~schedule_timer_t() noexcept;

    high_resolution_clock_t::time_point get_timeout_point() noexcept { return timeout_point_; }

    template <typename IO_CONTEXT_IMPL>
    static void check_timer(IO_CONTEXT_IMPL &&io_ctx);

    bool await_ready() const noexcept { return false; }
    std::experimental::coroutine_handle<> await_suspend(std::experimental::coroutine_handle<> handle) noexcept;
    void await_resume() const noexcept {}

    friend bool operator<(schedule_timer_t const &l, schedule_timer_t const &r);

  private:
    bool cancelled() noexcept { return cancelled_->load(std::memory_order_relaxed); }

  private:
    io_context_t &io_ctx_;
    high_resolution_clock_t::time_point timeout_point_;
    std::experimental::coroutine_handle<> croutine_;
    task_t<void> schedule_task_;
    std::shared_ptr<std::atomic_bool> cancelled_;
  };

  inline bool operator<(schedule_timer_t const &l, schedule_timer_t const &r)
  {
    return l.timeout_point_ < r.timeout_point_;
  }

  struct schedule_timer_t::pointer_cmp_type
  {
    using point_type = schedule_timer_t *;
    auto operator()(point_type const left, point_type const right) const
    {
      return (*left) < (*right);
    }
  };

  using timers_type = ds::heap_sort_t<schedule_timer_t *,
                                      typename schedule_timer_t::pointer_cmp_type>;
  extern thread_local std::unique_ptr<timers_type> local_timers;

  template <typename IO_CONTEXT_IMPL>
  void schedule_timer_t::check_timer(IO_CONTEXT_IMPL &&io_ctx)
  {
    auto now = high_resolution_clock_t::now();
    while (!local_timers->empty() && now >= local_timers->top()->timeout_point_)
    {
      auto const top = local_timers->pop();
      if (!top->cancelled_ || !top->cancelled())
        top->croutine_.resume();
    }
    if (!local_timers->empty())
    {
      io_ctx.update_next_timeout_point(local_timers->top()->timeout_point_);
    }
  }
} // namespace mrpc::detail

namespace mrpc
{
  using timer_canceller_t = detail::timer_canceller_t;
  using schedule_timer_t = detail::schedule_timer_t;
}