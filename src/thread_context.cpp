#include "common/thread/thread_context.h"
#include "common/debug/helper.hpp"

namespace mrpc
{
  thread_local thread_ctx* now_ctx = nullptr;

  thread_ctx::thread_ctx():
    pre_ctx_(nullptr)
  {
    if (now_ctx)
    {
      pre_ctx_ = now_ctx;
    }
    now_ctx = this;
  }

  thread_ctx::~thread_ctx()
  {
    if (pre_ctx_)
    {
      now_ctx = pre_ctx_;
    }
    else
    {
      now_ctx = nullptr;
    }
  }

  void thread_ctx::dispatch(ctx_call const& call)
  {
    M_ASSERT(now_ctx);
    now_ctx->calls_.push(call);
  }

  void thread_ctx::flush()
  {
    M_ASSERT(now_ctx);
    while (!now_ctx->calls_.empty())
    {
      auto &call = now_ctx->calls_.front();
      call();
      now_ctx->calls_.pop();
    }
  }
}