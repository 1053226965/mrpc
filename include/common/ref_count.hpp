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
      _count(new count_type(1u))
    {}

    ref_count_t(ref_count_t const& r) noexcept :
      _count(r._count)
    {
      _count->fetch_add(1u, std::memory_order_relaxed);
    }

    ref_count_t(ref_count_t&& r) noexcept:
      _count(r._count)
    {
      r._count = new count_type(1u);
    }

    ~ref_count_t() noexcept 
    {
      M_ASSERT(_count != nullptr);
      if (_count->fetch_sub(1u, std::memory_order_relaxed) == 1u) {
        delete _count;
      }
    }

    ref_count_t& operator=(ref_count_t const& r) noexcept
    {
      this->~ref_count_t();
      _count = r._count;
      _count->fetch_add(1u, std::memory_order_relaxed);
      return *this;
    }

    ref_count_t& operator=(ref_count_t&& r) noexcept
    {
      this->~ref_count_t();
      _count = r._count;
      r._count = new count_type(1);
      return *this;
    }

    bool ready_release() { return *_count == 1; }

  private:
    count_type* _count;
  };
}