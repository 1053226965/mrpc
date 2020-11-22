#include "doctest/doctest.h"
#include "io/io_context.hpp"
#include "io/io_thread_pool.hpp"
#include "common/coroutine/coro_mutex.hpp"
#include "common/coroutine/sync_wait.hpp"
#include "common/coroutine/async_scope.hpp"
#include "common/coroutine/task.hpp"

using namespace std;
using namespace mrpc;
using namespace mrpc::net;

TEST_CASE("coro_mutex test")
{
  io_context_t io_ctx(10);
  io_thread_pool_t pool(io_ctx, 6);
  async_scope_t as;
  coro_mutex_t mtx;
  vector<int> vec;
  atomic_int value = 0;
  const int n = 100;
  bool is_lock = false;
  auto new_coro = [&]() -> task_t<void> {
    co_await io_ctx.schedule();

    {
      auto scope_mtx = mtx.scope_mutex();
      co_await scope_mtx;
      CHECK(is_lock == false);
      is_lock = true;
      for (int i = 0; i < n; i++)
        vec.push_back(value.fetch_add(1));
      is_lock = false;
    }
  };

  for (int i = 0; i < n; i++)
    as.spawn(new_coro());
  sync_wait(as.join());

  CHECK(vec.size() == n * n);
  for (int i = 0; i < value; i++)
  {
    CHECK(vec[i] == i);
  }
}