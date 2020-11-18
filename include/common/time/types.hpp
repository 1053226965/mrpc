#pragma once

#include <chrono>

namespace mrpc
{
  typedef std::chrono::steady_clock steady_clock_t;
  typedef std::chrono::high_resolution_clock high_resolution_clock_t;
  typedef std::chrono::milliseconds milliseconds_t;
  typedef std::chrono::seconds seconds_t;
}