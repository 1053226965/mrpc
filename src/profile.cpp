#include "common/debug/profile.hpp"

namespace mrpc
{
  class profile_t
  {
    struct pdate_t
    {
      milliseconds_t min_{std::numeric_limits<milliseconds_t::rep>::min()};
      milliseconds_t max_{std::numeric_limits<milliseconds_t::rep>::max()};
      milliseconds_t sum_;
      size_t count_;
    };

  public:
    void input(char const *func_name, milliseconds_t times);

  private:
    std::unordered_map<std::string_view, pdate_t> data_;
    std::mutex data_mtx_;
  };

  void profile_t::input(char const *func_name, milliseconds_t times)
  {
    std::lock_guard lk(data_mtx_);
    auto &pd = data_[std::string_view(func_name)];
    pd.count_++;
    pd.sum_ += times;
    if (pd.min_ > times)
      pd.min_ = times;
    if (pd.max_ < times)
      pd.max_ = times;
  }

  static profile_t g_profile;

  profile_hack_t::profile_hack_t(char const *fn) noexcept : fn_(fn), start_(high_resolution_clock_t::now())
  {
  }

  profile_hack_t::~profile_hack_t()
  {
    g_profile.input(fn_, std::chrono::duration_cast<milliseconds_t>(high_resolution_clock_t::now() - start_));
  }
} // namespace mrpc