#include "io/timer.hpp"

namespace mrpc::detail
{
  thread_local std::unique_ptr<timers_type> local_timers = std::make_unique<timers_type>();

  task_t<void> gen_schedule_task(io_context_t &io_ctx, schedule_timer_t *timer)
  {
    co_await io_ctx.schedule();
    local_timers->add(timer);
    io_ctx.update_next_timeout_point(local_timers->top()->get_timeout_point());
  }

  schedule_timer_t::schedule_timer_t(io_context_t &io_ctx,
                                     milliseconds_t const &wait_time) noexcept : io_ctx_(io_ctx),
                                                                                 timeout_point_(high_resolution_clock_t::now() + wait_time),
                                                                                 schedule_task_(gen_schedule_task(io_ctx, this))
  {
  }

  schedule_timer_t::schedule_timer_t(io_context_t &io_ctx,
                                     milliseconds_t const &wait_time,
                                     timer_canceller_t &canceller) noexcept : io_ctx_(io_ctx),
                                                                              timeout_point_(high_resolution_clock_t::now() + wait_time),
                                                                              schedule_task_(gen_schedule_task(io_ctx, this)),
                                                                              cancelled_(std::make_shared<std::atomic_bool>(0))
  {
    canceller.set(cancelled_);
  }

  schedule_timer_t::schedule_timer_t(schedule_timer_t &&timer) noexcept : io_ctx_(timer.io_ctx_),
                                                                          timeout_point_(timer.timeout_point_),
                                                                          croutine_(std::move(timer.croutine_)),
                                                                          schedule_task_(std::move(timer.schedule_task_))
  {
  }

  schedule_timer_t::~schedule_timer_t() noexcept
  {
  }

  std::experimental::coroutine_handle<> schedule_timer_t::await_suspend(
      std::experimental::coroutine_handle<> handle) noexcept
  {
    croutine_ = handle;
    return schedule_task_.get_handle();
  }
} // namespace mrpc::detail