#pragma once
#include <experimental/coroutine>
#include "err_code.h"
#include "common/platform.hpp"
#include "io/buffer.hpp"

namespace mrpc
{
#ifdef OS_WIN
  typedef OVERLAPPED overlapped_t;
#elif defined(OS_GNU_LINUX)
  struct overlapped_t
  {
    uint8_t unused;
  };
#endif
  class io_state_t : overlapped_t
  {
    io_state_t(io_state_t &) = delete;
    io_state_t &operator=(io_state_t &) = delete;

  public:
    enum class type_t
    {
      invalid,
      accept,
      connect,
      send,
      recv
    };

    io_state_t(type_t t = type_t::invalid) noexcept;
    io_state_t(io_state_t &&state) noexcept;

    io_state_t &operator=(io_state_t &&state) noexcept;

    void set_coro_handle(std::experimental::coroutine_handle<> coro_handle) noexcept { _coro_handle = coro_handle; }
    auto get_coro_handle() noexcept { return _coro_handle; }
    overlapped_t *get_overlapped() noexcept { return static_cast<overlapped_t *>(this); }

    void set_error(error_code err) noexcept { _error = err; }
    error_code get_error() noexcept { return _error; }

    void io_completed(error_code ec, size_t bytes_transfer);
    size_t bytes_transfer() { return _bytes_transfer; }

  private:
    type_t _type;
    error_code _error;
    size_t _bytes_transfer;
    std::experimental::coroutine_handle<> _coro_handle;
  };

  inline io_state_t::io_state_t(type_t t) noexcept : overlapped_t{0},
                                                     _type(t),
                                                     _error(error_code::INVLIAD),
                                                     _bytes_transfer(0)
  {
  }

  inline io_state_t::io_state_t(io_state_t &&state) noexcept : overlapped_t{0},
                                                               _type(state._type),
                                                               _error(state._error),
                                                               _bytes_transfer(state._bytes_transfer),
                                                               _coro_handle(state._coro_handle)
  {
    state._error = error_code::INVLIAD;
    state._coro_handle = nullptr;
  }

  inline io_state_t &io_state_t::operator=(io_state_t &&state) noexcept
  {
    _type = state._type;
    _error = state._error;
    _bytes_transfer = std::move(state._bytes_transfer);
    _coro_handle = std::move(state._coro_handle);
    return *this;
  }

  inline void io_state_t::io_completed(error_code ec, size_t bytes_transfer)
  {
    _error = ec;
    _bytes_transfer = bytes_transfer;
  }
} // namespace mrpc