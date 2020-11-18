// #include "doctest/doctest.h"
// #include "io/io_context.hpp"
// #include "io/io_thread_pool.hpp"
// #include "io/timer.hpp"
// #include "io/net/acceptor.hpp"
// #include "io/net/connection.hpp"
// #include "common/coroutine/when_all.hpp"
// #include "common/coroutine/sync_wait.hpp"
// #include "common/coroutine/async_scope.hpp"
// #include "common/debug/log.hpp"
// #include "fmt/format.h"

// using namespace std;
// using namespace mrpc;
// using namespace mrpc::net;

// TEST_CASE("http benchmark")
// {
//   io_context_t io_ctx(8);
//   io_thread_pool_t pool(io_ctx, 8);

//   acceptor_t acceptor(io_ctx, std::string_view("0.0.0.0:80"));
//   REQUIRE(acceptor.valid());
  
//   auto http_task = [&](shared_ptr<connection_t> con) -> task_t<void> {
//     co_await io_ctx.schedule();

//     buffer_t rb(2048);
//     size_t rl = co_await recv_task_t(*con, rb);

//     buffer_t sb;
//     sb.append("random_string()");
//     size_t sl = co_await send_task_t(*con, sb);
//     co_return;
//   };

//   atomic_bool running = true;
//   async_scope_t as;
//   auto accept_task = [&]() -> task_t<void> {
//     while (running)
//     {
//       connection_t new_con = co_await accept_task_t(io_ctx, acceptor);
//       if (acceptor.get_recv_io_state().get_error() == mrpc::error_code::NONE_ERROR)
//       {
//         shared_ptr<connection_t> pc = make_shared<connection_t>(std::move(new_con));
//         as.spawn(http_task(pc));
//       }
//     }
//     co_return;
//   };

//   auto stop_task = [&]() -> task_t<void> {
//     co_await schedule_timer_t(io_ctx, 20000ms);
//     running = false;
//     acceptor.close();
//     co_return;
//   };

//   sync_wait(when_all(stop_task(), accept_task()));
//   while(1);
// }