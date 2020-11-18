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

    void set_coro_handle(std::experimental::coroutine_handle<> coro_handle) noexcept { coro_handle_ = coro_handle; }
    auto get_coro_handle() noexcept { return coro_handle_; }
    overlapped_t *get_overlapped() noexcept { return static_cast<overlapped_t *>(this); }

    void set_error(error_code err) noexcept { error_ = err; }
    error_code get_error() noexcept { return error_; }

    void io_completed(error_code ec, size_t bytes_transfer);
    size_t bytes_transfer() { return bytes_transfer_; }

  private:
    type_t type_;
    error_code error_;
    size_t bytes_transfer_;
    std::experimental::coroutine_handle<> coro_handle_;
  };

  inline io_state_t::io_state_t(type_t t) noexcept : overlapped_t{0},
                                                     type_(t),
                                                     error_(error_code::INVLIAD),
                                                     bytes_transfer_(0)
  {
  }

  inline io_state_t::io_state_t(io_state_t &&state) noexcept : overlapped_t{0},
                                                               type_(state.type_),
                                                               error_(state.error_),
                                                               bytes_transfer_(state.bytes_transfer_),
                                                               coro_handle_(state.coro_handle_)
  {
    state.error_ = error_code::INVLIAD;
    state.coro_handle_ = nullptr;
  }

  inline io_state_t &io_state_t::operator=(io_state_t &&state) noexcept
  {
    type_ = state.type_;
    error_ = state.error_;
    bytes_transfer_ = std::move(state.bytes_transfer_);
    coro_handle_ = std::move(state.coro_handle_);
    return *this;
  }

  inline void io_state_t::io_completed(error_code ec, size_t bytes_transfer)
  {
    error_ = ec;
    bytes_transfer_ = bytes_transfer;
  }
} // namespace mrpc