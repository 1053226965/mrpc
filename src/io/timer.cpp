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
                                     milliseconds_t const &wait_time) noexcept : _io_ctx(io_ctx),
                                                                                 _timeout_point(high_resolution_clock_t::now() + wait_time),
                                                                                 _schedule_task(gen_schedule_task(io_ctx, this))
  {
  }

  schedule_timer_t::schedule_timer_t(io_context_t &io_ctx,
                                     milliseconds_t const &wait_time,
                                     timer_canceller_t &canceller) noexcept : _io_ctx(io_ctx),
                                                                              _timeout_point(high_resolution_clock_t::now() + wait_time),
                                                                              _schedule_task(gen_schedule_task(io_ctx, this)),
                                                                              _cancelled(std::make_shared<std::atomic_bool>(0))
  {
    canceller.set(_cancelled);
  }

  schedule_timer_t::schedule_timer_t(schedule_timer_t &&timer) noexcept : _io_ctx(timer._io_ctx),
                                                                          _timeout_point(timer._timeout_point),
                                                                          _croutine(std::move(timer._croutine)),
                                                                          _schedule_task(std::move(timer._schedule_task))
  {
  }

  schedule_timer_t::~schedule_timer_t() noexcept
  {
  }

  std::experimental::coroutine_handle<> schedule_timer_t::await_suspend(
      std::experimental::coroutine_handle<> handle) noexcept
  {
    _croutine = handle;
    return _schedule_task.get_handle();
  }
} // namespace mrpc::detail