#include "doctest/doctest.h"
#include "io/io_context.hpp"
#include "io/timer.hpp"
#include "io/io_thread_pool.hpp"
#include "common/coroutine/when_all.hpp"
#include "common/coroutine/sync_wait.hpp"
#include "common/coroutine/async_scope.hpp"

TEST_CASE("timer test")
{
  using namespace std;
  using namespace mrpc;

  io_context_t io_ctx(10);
  io_thread_pool_t pool(io_ctx, 4);

  auto void_task = [&]() -> task_t<void> {
    co_return;
  };

  auto new_timer = [&](milliseconds_t dur) -> task_t<int> {
    auto now = high_resolution_clock_t::now();
    co_await schedule_timer_t(io_ctx, dur);
    co_return (int)chrono::duration_cast<milliseconds_t>(high_resolution_clock_t::now() - now).count();
  };

  async_scope_t scope;
  scope.spawn(new_timer(1500ms));
  scope.spawn(new_timer(300ms));
  this_thread::sleep_for(3000ms);
  sync_wait(scope.join());

  auto ret = sync_wait(when_all(void_task(), new_timer(1000ms), new_timer(2000ms), void_task()));
}