#pragma once
#include <atomic>
#include <memory>
#include "common/debug/helper.hpp"

namespace mrpc
{
  class ref_count_t
  {
  public:
    using count_type = std::atomic_uint32_t;

    ref_count_t():
      count_(new count_type(1u))
    {}

    ref_count_t(ref_count_t const& r) noexcept :
      count_(r.count_)
    {
      count_->fetch_add(1u, std::memory_order_relaxed);
    }

    ref_count_t(ref_count_t&& r) noexcept:
      count_(r.count_)
    {
      r.count_ = new count_type(1u);
    }

    ~ref_count_t() noexcept 
    {
      M_ASSERT(count_ != nullptr);
      if (count_->fetch_sub(1u, std::memory_order_relaxed) == 1u) {
        delete count_;
      }
    }

    ref_count_t& operator=(ref_count_t const& r) noexcept
    {
      this->~ref_count_t();
      count_ = r.count_;
      count_->fetch_add(1u, std::memory_order_relaxed);
      return *this;
    }

    ref_count_t& operator=(ref_count_t&& r) noexcept
    {
      this->~ref_count_t();
      count_ = r.count_;
      r.count_ = new count_type(1);
      return *this;
    }

    bool ready_release() { return *count_ == 1; }

  private:
    count_type* count_;
  };
}