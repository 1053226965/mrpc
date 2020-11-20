#include "common/debug/profile.hpp"

namespace mrpc
{
  class profile_t
  {
    struct pdate_t
    {
      milliseconds_t _min {std::numeric_limits<milliseconds_t::rep>::min()};
      milliseconds_t _max {std::numeric_limits<milliseconds_t::rep>::max()};
      milliseconds_t _sum;
      size_t _count;
    };

  public:
    void input(char const *func_name, milliseconds_t times);

  private:
    std::unordered_map<std::string_view, pdate_t> _data;
    std::mutex _data_mtx;
  };

  void profile_t::input(char const *func_name, milliseconds_t times)
  {
    std::lock_guard lk(_data_mtx);
    auto &pd = _data[std::string_view(func_name)];
    pd._count++;
    pd._sum += times;
    if (pd._min > times)
      pd._min = times;
    if (pd._max < times)
      pd._max = times;
  }

  static profile_t g_profile;

  profile_hack_t::profile_hack_t(char const *fn) noexcept : _fn(fn), _start(high_resolution_clock_t::now())
  {
  }

  profile_hack_t::~profile_hack_t()
  {
    g_profile.input(_fn, std::chrono::duration_cast<milliseconds_t>(high_resolution_clock_t::now() - _start));
  }
} // namespace mrpc