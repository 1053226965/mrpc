#pragma once
#include <memory>

namespace mrpc
{
  template<typename FUNC>
  class scoped_func_t
  {
    scoped_func_t(scoped_func_t const&) = delete;
    scoped_func_t& operator=(scoped_func_t const&) = delete;

  public:
    using function_type = FUNC;

    template<typename FUNC2>
    scoped_func_t(FUNC2&& func) noexcept :
      func_(std::forward<FUNC2>(func))
    {}

    ~scoped_func_t() noexcept
    {
      func_();
    }

  private:
    function_type func_;
  };

  template<typename FUNC>
  scoped_func_t<FUNC> exec_on_exit(FUNC&& func)
  {
    return scoped_func_t<FUNC>(std::forward<FUNC>(func));
  }
}