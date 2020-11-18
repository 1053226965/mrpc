#pragma once
#include "endpoint.hpp"
#include "io/err_code.h"
#include "io/lf_notifier.hpp"
#include "common/platform.hpp"

#ifdef OS_WIN
#define INVAILD_SOCKET -1
typedef SOCKET socket_handle_t;
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#elif defined(OS_GNU_LINUX)
#define INVAILD_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) ::close(s)
typedef int socket_handle_t;
#endif

namespace mrpc::net
{
  class socket_t final
  {
    socket_t(socket_t const &) = delete;
    socket_t &operator=(socket_t const &) = delete;

    struct socket_detail_t
    {
      int af_;
      int type_;
      int protocol_;
    };

  public:
    static constexpr int af_inet = AF_INET;
    static constexpr int af_inet6 = AF_INET6;
    static constexpr int sock_stream = SOCK_STREAM;
    static constexpr int sock_dgram = SOCK_DGRAM;
    static constexpr int default_protocol = 0;

    socket_t() : handle_(INVAILD_SOCKET), socket_detail_{0} {}
    socket_t(socket_handle_t socket, socket_detail_t const &detail) noexcept;
    socket_t(int af, int type, int protocol) noexcept;
    socket_t(socket_t &&socket) noexcept;
    socket_t(socket_detail_t const &detail) noexcept : socket_t(detail.af_, detail.type_, detail.protocol_){};
    ~socket_t() noexcept;

    socket_t &operator=(socket_t &&socket) noexcept;

    socket_handle_t handle() const noexcept { return handle_; }
    bool valid() noexcept { return handle_ != INVAILD_SOCKET; }
    size_t get_socket_addr_len() noexcept { return socket_detail_.af_ == af_inet ? sizeof(sockaddr_in) : sizeof(sockaddr_in6); }
    endpoint_t &get_local_endpoint() noexcept { return local_endpoint_; }
    endpoint_t &get_remote_endpoint() noexcept { return remote_endpoint_; }
    void set_local_endpoint(sockaddr const &s) noexcept { local_endpoint_ = endpoint_t(s); }
    void set_remote_endpoint(sockaddr const &s) noexcept { remote_endpoint_ = endpoint_t(s); }

    socket_detail_t get_socket_detail() const noexcept { return socket_detail_; }

    error_code bind_addr(endpoint_t const &e) noexcept;

    void shutdown_wr() noexcept;
    void shutdown_rd() noexcept;
    void close() noexcept;

#ifdef OS_WIN
    bool skip_compeletion_port_on_success() const
    {
      return skip_compeletion_port_on_success_;
    }
#endif
  private:
    error_code prepare() noexcept;

  private:
#ifdef OS_WIN
    bool skip_compeletion_port_on_success_ = {false};
#endif
#ifdef OS_GNU_LINUX
  public:
    bool notify_on_read(io_state_t &io_state) { return read_state_.notify_on_ready(io_state); }
    bool notify_on_write(io_state_t &io_state) { return write_state_.notify_on_ready(io_state); }
    void ready_to_read() { read_state_.set_ready(); }
    void ready_to_write() { write_state_.set_ready(); }

  private:
    lf_notifier_t read_state_;
    lf_notifier_t write_state_;
#endif
    socket_handle_t handle_;
    endpoint_t local_endpoint_;
    endpoint_t remote_endpoint_;
    socket_detail_t socket_detail_;
  };

  inline net::socket_t::socket_t(int af, int type, int protocol) noexcept : socket_detail_{af, type, protocol}
  {
    handle_ = ::socket(af, type, protocol);
    if (valid())
    {
      prepare();
    }
  }

  inline net::socket_t::socket_t(socket_handle_t socket, socket_detail_t const &detail) noexcept : socket_detail_{detail.af_, detail.type_, detail.protocol_}
  {
    handle_ = socket;
    prepare();
  }

  inline socket_t::socket_t(socket_t &&socket) noexcept : handle_(socket.handle_),
                                                          local_endpoint_(std::move(socket.local_endpoint_)),
                                                          remote_endpoint_(std::move(socket.remote_endpoint_)),
                                                          socket_detail_(socket.socket_detail_)
#ifdef OS_WIN
                                                          ,
                                                          skip_compeletion_port_on_success_(socket.skip_compeletion_port_on_success_)
#endif
  {
    socket.handle_ = INVAILD_SOCKET;
  }

  inline socket_t::~socket_t() noexcept
  {
    close();
  }

  inline socket_t &socket_t::operator=(socket_t &&socket) noexcept
  {
    if (valid())
    {
      closesocket(handle_);
    }
    handle_ = socket.handle_;
    local_endpoint_ = std::move(socket.local_endpoint_);
    remote_endpoint_ = std::move(socket.remote_endpoint_);
#ifdef OS_WIN
    skip_compeletion_port_on_success_ = socket.skip_compeletion_port_on_success_;
#endif
    socket_detail_ = std::move(socket.socket_detail_);
    socket.handle_ = INVAILD_SOCKET;
    return *this;
  }

  inline error_code socket_t::bind_addr(endpoint_t const &e) noexcept
  {
    if (::bind(handle_, e.get_sockaddr(), static_cast<int>(e.addr_len())) ==
        SOCKET_ERROR)
    {
      return error_code::BIND_ERROR;
    }
    set_local_endpoint(*e.get_sockaddr());
    return error_code::NONE_ERROR;
  }

  inline void socket_t::shutdown_wr() noexcept
  {
    if (valid())
    {
      shutdown(handle_, SHUT_WR);
    }
  }

  inline void socket_t::shutdown_rd() noexcept
  {
    if (valid())
    {
      shutdown(handle_, SHUT_RD);
    }
  }

  inline void socket_t::close() noexcept
  {
    if (valid())
    {
      DETAIL_LOG_INFO("[socket] socket {} be closed", handle_);
      closesocket(handle_);
      handle_ = INVAILD_SOCKET;
    }
    else
    {
      DETAIL_LOG_INFO("[socket] close invalid socket ", handle_);
    }
  }

  inline error_code socket_t::prepare() noexcept
  {
    M_ASSERT(valid());

#ifdef OS_WIN
    int status;
    uint32_t param = 1;
    DWORD ret;
    status = WSAIoctl(handle_, FIONBIO, &param, sizeof(param), NULL, 0, &ret,
                      NULL, NULL);
    if (status == 0)
    {
      BOOL param = TRUE;
      status = ::setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY,
                            reinterpret_cast<char *>(&param), sizeof(param));
    }

    WSAPROTOCOL_INFOW protocol_info;
    int opt_len = (int)sizeof protocol_info;
    if (getsockopt(handle_,
                   SOL_SOCKET,
                   SO_PROTOCOL_INFOW,
                   (char *)&protocol_info,
                   &opt_len) != SOCKET_ERROR)
    {
      if ((protocol_info.dwServiceFlags1 & XP1_IFS_HANDLES))
      {
        skip_compeletion_port_on_success_ = true;
      }
    }
#endif

#ifdef OS_GNU_LINUX

    auto set_flag = [](int fd, int flag) {
      int oldflags = fcntl(fd, F_GETFL, 0);
      if (oldflags < 0)
      {
        return error_code::SYSTEM_ERROR;
      }
      oldflags |= flag;
      if (fcntl(fd, F_SETFL, oldflags) != 0)
      {
        return error_code::SYSTEM_ERROR;
      }
      return error_code::NONE_ERROR;
    };
    set_flag(static_cast<int>(handle_), O_NONBLOCK);
    set_flag(static_cast<int>(handle_), FD_CLOEXEC);
    int val = 1;
    int newval;
    socklen_t intlen = sizeof(newval);
    if (0 != setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)))
    {
      return error_code::SYSTEM_ERROR;
    }
    if (0 != getsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, &newval, &intlen))
    {
      return error_code::SYSTEM_ERROR;
    }
    if ((newval != 0) != val)
    {
      return error_code::SYSTEM_ERROR;
    }

    val = 1;
    intlen = sizeof(newval);
    if (0 != setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)))
    {
      return error_code::SYSTEM_ERROR;
    }
    if (0 != getsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, &newval, &intlen))
    {
      return error_code::SYSTEM_ERROR;
    }
    if ((newval != 0) != val)
    {
      return error_code::SYSTEM_ERROR;
    }
#endif
    return error_code::NONE_ERROR;
  }
} // namespace mrpc::net