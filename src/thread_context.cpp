#include "common/thread/thread_context.h"
#include "common/debug/helper.hpp"

namespace mrpc
{
  thread_local thread_ctx* now_ctx = nullptr;

  thread_ctx::thread_ctx():
    _pre_ctx(nullptr)
  {
    if (now_ctx)
    {
      _pre_ctx = now_ctx;
    }
    now_ctx = this;
  }

  thread_ctx::~thread_ctx()
  {
    if (_pre_ctx)
    {
      now_ctx = _pre_ctx;
    }
    else
    {
      now_ctx = nullptr;
    }
  }

  void thread_ctx::dispatch(ctx_call const& call)
  {
    M_ASSERT(now_ctx);
    now_ctx->_calls.push(call);
  }

  void thread_ctx::flush()
  {
    M_ASSERT(now_ctx);
    while (!now_ctx->_calls.empty())
    {
      auto &call = now_ctx->_calls.front();
      call();
      now_ctx->_calls.pop();
    }
  }
}