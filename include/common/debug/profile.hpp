#pragma once
#include <unordered_map>
#include <string_view>
#include <mutex>
#include "common/time/types.hpp"

#define USE_PROFILE

namespace mrpc
{
#ifdef USE_PROFILE
#define PROFILE_HACK() profile_hack_t __local_profile_hack__(__FUNCTION__)
#else
#define PROFILE_HACK()
#endif

  class profile_hack_t
  {
  public:
    profile_hack_t(char const* func_name) noexcept;
    ~profile_hack_t() noexcept;
  private:
    char const* fn_;
    high_resolution_clock_t::time_point start_;
  };
} // namespace mrpc