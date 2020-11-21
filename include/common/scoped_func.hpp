#pragma once
#include <memory>
#include <tuple>

namespace mrpc
{
  template<typename FUNC, typename...ARGS>
  class scoped_func_t
  {
    scoped_func_t(scoped_func_t const&) = delete;
    scoped_func_t& operator=(scoped_func_t const&) = delete;

    template<size_t... INDICES>
    void invoke(std::integer_sequence<size_t, INDICES...>)
    {
      _func(std::get<INDICES>(_params)...);
    }

  public:
  
    scoped_func_t(FUNC&& func, ARGS&...args) noexcept :
      _func(std::forward<FUNC>(func)),
      _params(std::make_tuple(std::forward<ARGS>(args)...))
    {}

    ~scoped_func_t() noexcept
    {
      invoke(std::make_integer_sequence<size_t, sizeof...(ARGS)>());
    }

  private:
    FUNC _func;
    std::tuple<ARGS...> _params;
  };

  template<typename FUNC, typename...ARGS>
  auto exec_on_exit(FUNC&& func, ARGS&&...args)
  {
    return scoped_func_t(std::forward<FUNC>(func), std::forward<ARGS>(args)...);
  }
}