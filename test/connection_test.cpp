#include "doctest/doctest.h"
#include "io/io_context.hpp"
#include "io/io_thread_pool.hpp"
#include "io/net/acceptor.hpp"
#include "io/net/connection.hpp"
#include "common/coroutine/when_all.hpp"
#include "common/coroutine/sync_wait.hpp"
#include "common/coroutine/async_scope.hpp"
#include "common/debug/log.hpp"
#include "fmt/format.h"

using namespace std;
using namespace mrpc;
using namespace mrpc::net;

TEST_SUITE_BEGIN("connection test");

TEST_CASE("hello world")
{
  io_context_t io_ctx(10);
  io_thread_pool_t pool(io_ctx, 1);

  acceptor_t acceptor(io_ctx, std::string_view("0.0.0.0:1234"));
  REQUIRE(acceptor.valid());

  auto accept_task = [&]() -> task_t<void> {
    connection_t new_con = co_await accept_task_t(io_ctx, acceptor);
    REQUIRE(new_con.socket()->valid());
    CHECK(acceptor.get_recv_io_state().get_error() == mrpc::error_code::NONE_ERROR);
    buffer_t rbuf, sbuf;
    auto rs = co_await recv_task_t(new_con, rbuf);
    CHECK(rs == 5);
    CHECK(new_con.get_recv_io_state().get_error() == mrpc::error_code::NONE_ERROR);
    rbuf.go_to_start();
    CHECK(rbuf.to_string() == "hello");
    sbuf.append("world");
    auto ss = co_await send_task_t(new_con, sbuf);
    CHECK(ss == 5);
    co_return;
  };

  auto connect_task = [&]() -> task_t<void> {
    auto new_con = co_await connect_task_t(io_ctx, endpoint_t("127.0.0.1:1234"));
    REQUIRE(new_con.valid());
    buffer_t rbuf, sbuf;
    sbuf.append("hello");
    auto ss = co_await send_task_t(new_con, sbuf);
    CHECK(ss == 5);
    auto rs = co_await recv_task_t(new_con, rbuf);
    CHECK(rs == 5);
    rbuf.go_to_start();
    CHECK(rbuf.to_string() == "world");
    co_return;
  };

  sync_wait(when_all(accept_task(), connect_task()));
}

TEST_CASE("many connections")
{
  atomic_size_t total_send1 = 0, total_recv1 = 0;
  atomic_size_t total_send2 = 0, total_recv2 = 0;
  atomic_bool ready_to_connect = false;

  auto random_string = []() -> string {
    string ret;
    ret.resize((rand() % 2048) + 1024);
    for (int i = 0; i < ret.size(); i++)
    {
      ret[i] = (rand() % 26) + 'a';
    }
    return ret;
  };

  auto loop_client = [&](io_context_t *io_ctx, char const *addr) {
    thread([&, io_ctx, addr]() {
      while (!ready_to_connect)
        ;

      auto con = sync_wait([&]() -> task_t<connection_t> {
        co_return co_await connect_task_t(*io_ctx, std::string_view(addr));
      }());
      REQUIRE(con.valid());
      auto send_task = [&]() -> task_t<void> {
        while (true)
        {
          buffer_t sb;
          calloc(1024,1);
          sb.append(random_string());
          size_t sl = co_await send_task_t(con, sb);
          total_send2 += sl;
          if (total_send2 >= 1024 * 1024 * 100)
          {
            con.shutdown_wr();
            break;
          }
        }
        co_return;
      };

      auto recv_task = [&]() -> task_t<void> {
        while (true)
        {
          buffer_t rb(2048);
          size_t rl = co_await recv_task_t(con, rb);
          total_recv2 += rl;
          if (con.get_recv_io_state().get_error() == mrpc::error_code::CLOSED)
            break;
        }
        co_return;
      };

      sync_wait(when_all(send_task(), recv_task()));
    }).detach();
  };

  auto loop_send_recv = [&]() {
    io_context_t io_ctx(10);
    io_thread_pool_t pool(io_ctx, 4);

    acceptor_t acceptor(io_ctx, std::string_view("0.0.0.0:1235"));
    REQUIRE(acceptor.valid());

    auto send_task = [&](shared_ptr<connection_t> con) -> task_t<void> {
      co_await io_ctx.schedule();
      while (true)
      {
        buffer_t sb;
        sb.append(random_string());
        size_t sl = co_await send_task_t(*con, sb);
        total_send1 += sl;
        if (total_send1 >= 1024 * 1024 * 100)
        {
          con->shutdown_wr();
          break;
        }
      }
      co_return;
    };

    auto recv_task = [&](shared_ptr<connection_t> con) -> task_t<void> {
      co_await io_ctx.schedule();
      while (true)
      {
        buffer_t rb(2048);
        size_t rl = co_await recv_task_t(*con, rb);
        total_recv1 += rl;
        if (con->get_recv_io_state().get_error() == mrpc::error_code::CLOSED)
        {
          break;
        }
      }
      co_return;
    };

    async_scope_t as;
    const int connections_count = 3;
    auto accept_task = [&]() -> task_t<void> {
      ready_to_connect = true;

      int c = 0;
      while (true)
      {
        connection_t new_con = co_await accept_task_t(io_ctx, acceptor);
        if (acceptor.get_recv_io_state().get_error() == mrpc::error_code::NONE_ERROR)
        {
          shared_ptr<connection_t> pc = make_shared<connection_t>(std::move(new_con));
          as.spawn(send_task(pc));
          as.spawn(send_task(pc));
          as.spawn(recv_task(pc));
          if (++c >= connections_count)
            break;
        }
      }
      co_return;
    };
    for (int i = 0; i < connections_count; i++)
      loop_client(&io_ctx, "127.0.0.1:1235");

    sync_wait(accept_task());
    sync_wait(as.join());
  };

  loop_send_recv();
  CHECK(total_send1 != 0);
  CHECK(total_send2 != 0);
  CHECK(total_send1 == total_recv2);
  CHECK(total_send2 == total_recv1);
}

TEST_SUITE_END();