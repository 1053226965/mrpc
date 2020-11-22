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

auto random_string = []() -> string {
  string ret;
  ret.resize((rand() % 2048) + 1024);
  for (int i = 0; i < ret.size(); i++)
  {
    ret[i] = (rand() % 26) + 'a';
  }
  return ret;
};

TEST_SUITE_BEGIN("connection test");

// TEST_CASE("connection hello world")
// {
//   io_context_t io_ctx(10);
//   io_thread_pool_t pool(io_ctx, 1);

//   acceptor_t acceptor(io_ctx, std::string_view("0.0.0.0:1234"));
//   REQUIRE(acceptor.valid());

//   auto accept_task = [&]() -> task_t<void> {
//     connection_t new_con = co_await accept_task_t(io_ctx, acceptor);
//     REQUIRE(new_con.socket()->valid());
//     CHECK(acceptor.get_recv_io_state().get_error() == mrpc::error_code::NONE_ERROR);
//     buffer_t rbuf, sbuf;
//     auto rs = co_await recv_task_t(new_con, rbuf);
//     CHECK(rs == 5);
//     CHECK(new_con.get_recv_io_state().get_error() == mrpc::error_code::NONE_ERROR);
//     rbuf.go_to_start();
//     CHECK(rbuf.to_string() == "hello");
//     sbuf.append("world");
//     auto ss = co_await send_task_t(new_con, sbuf);
//     CHECK(ss == 5);
//     co_return;
//   };

//   auto connect_task = [&]() -> task_t<void> {
//     auto new_con = co_await connect_task_t(io_ctx, endpoint_t("127.0.0.1:1234"));
//     REQUIRE(new_con.valid());
//     buffer_t rbuf, sbuf;
//     sbuf.append("hello");
//     auto ss = co_await send_task_t(new_con, sbuf);
//     CHECK(ss == 5);
//     auto rs = co_await recv_task_t(new_con, rbuf);
//     CHECK(rs == 5);
//     rbuf.go_to_start();
//     CHECK(rbuf.to_string() == "world");
//     co_return;
//   };

//   sync_wait(when_all(accept_task(), connect_task()));
// }

TEST_CASE("many connections")
{
  const int connections_count = 3;

  atomic_size_t total_send1 = 0, total_recv1 = 0;
  atomic_size_t total_send2 = 0, total_recv2 = 0;
  atomic_bool ready_to_connect = false;

  io_context_t io_ctx(10);
  io_thread_pool_t pool(io_ctx, 4);

  // 绑定并监听
  acceptor_t acceptor(io_ctx, std::string_view("0.0.0.0:1235"));
  REQUIRE(acceptor.valid());
  async_scope_t as;

  auto loop_client = [&](char const *addr) {
    while (!ready_to_connect)
    {
    }
    auto con = sync_wait([](io_context_t& io_ctx, char const *addr) -> task_t<connection_t> {
      co_return co_await connect_task_t(io_ctx, std::string_view(addr));
    }(io_ctx, addr));

    REQUIRE(con.valid());
    shared_ptr<connection_t> pc = make_shared<connection_t>(std::move(con));

    auto send_task = [](io_context_t &io_ctx, atomic_size_t& total_send2, shared_ptr<connection_t> pcon) -> task_t<void> {
      co_await io_ctx.schedule();
      while (true)
      {
        buffer_t sb;
        sb.append(random_string());
        size_t sl = co_await send_task_t(*pcon, sb); // 异步发送，发送成功或者失败会唤醒
        total_send2 += sl;
        if (total_send2 >= 1024 * 1024 * 100)
        {
          pcon->shutdown_wr(); // 客户端关闭写端， 服务端recv收到0，error_code为CLOSED
          break;
        }
      }
      DETAIL_LOG_INFO("[test] client stop write {}", pcon->socket()->handle());
      co_return;
    };

    auto recv_task = [&](io_context_t& io_ctx, atomic_size_t& total_recv2, shared_ptr<connection_t> pcon) -> task_t<void> {
      co_await io_ctx.schedule();
      while (true)
      {
        buffer_t rb(2048);
        size_t rl = co_await recv_task_t(*pcon, rb); // 异步接收，接收成功或者连接被关闭唤醒
        total_recv2 += rl;
        if (pcon->get_recv_io_state().get_error() != mrpc::error_code::NONE_ERROR)
          break;
      }
      DETAIL_LOG_INFO("[test] client stop read {}", pcon->socket()->handle());
      co_return;
    };

    as.spawn(send_task(io_ctx, total_send2, pc)); // 起一个协程写
    as.spawn(recv_task(io_ctx, total_recv2, pc)); // 起一个协程读
  };

  auto loop_send_recv = [&]() {
    auto send_task = [&](shared_ptr<connection_t> con) -> task_t<void> {
      co_await io_ctx.schedule();
      DETAIL_LOG_INFO("[test] server start write {}", con->socket()->handle());
      while (con->valid())
      {
        buffer_t sb;
        sb.append(random_string());
        size_t sl = co_await send_task_t(*con, sb);
        total_send1 += sl;
        if (total_send1 >= 1024 * 1024 * 100)
        {
          break;
        }
      }
      DETAIL_LOG_INFO("[test] server stop write {}", con->socket()->handle());
      co_return;
    };

    // 此处lambda引用捕获没问题，因为lambda生命周期跟协程一致的
    auto recv_task = [&](shared_ptr<connection_t> con) -> task_t<void> {
      co_await io_ctx.schedule();
      while (true)
      {
        buffer_t rb(2048);
        size_t rl = co_await recv_task_t(*con, rb);
        total_recv1 += rl;
        if (con->get_recv_io_state().get_error() != mrpc::error_code::NONE_ERROR)
        {
          break;
        }
      }
      DETAIL_LOG_INFO("[test] server stop read {}", con->socket()->handle());
      co_return;
    };

    // 此处lambda引用捕获没问题，因为lambda生命周期跟协程一致的
    auto accept_task = [&]() -> task_t<void> {
      ready_to_connect = true;

      int c = 0;
      while (true)
      {
        connection_t new_con = co_await accept_task_t(io_ctx, acceptor);
        if (acceptor.get_recv_io_state().get_error() == mrpc::error_code::NONE_ERROR)
        {
          shared_ptr<connection_t> pc = make_shared<connection_t>(std::move(new_con));
          as.spawn(send_task(pc)); // 起三个协程写。虽然协程间可能并发执行，但没问题，因为发送是原子执行的。
          as.spawn(send_task(pc)); // 不存在一个协程发一半，另一个协程接着发一半的情况。
          as.spawn(send_task(pc)); 
          as.spawn(recv_task(pc)); // 起一个协程读。不存在多个协程一起读，因为从逻辑上就讲不通。
          if (++c >= connections_count)
            break;
        }
      }
      co_return;
    };

    // 创建n个客户端
    thread([&loop_client]() {
      for (int i = 0; i < connections_count; i++)
        loop_client("127.0.0.1:1235");
    }).detach();

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