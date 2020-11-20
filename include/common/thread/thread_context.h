#pragma once
#include <queue>
#include <functional>

namespace mrpc
{
  typedef std::function<void()> ctx_call;

  class thread_ctx
  {
    thread_ctx(thread_ctx const&) = delete;
    thread_ctx& operator=(thread_ctx const&) = delete;

  public:
    thread_ctx();
    ~thread_ctx();
    static void dispatch(ctx_call const& call);
    static void flush();

  private:

    std::queue<ctx_call> _calls;
    thread_ctx* _pre_ctx;
  };
}