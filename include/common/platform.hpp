#pragma once

#include <system_error>
#include "common/sys_exception.hpp"
#include "common/debug/log.hpp"

void platform_init();

#ifdef _WIN32

#define OS_WIN

#elif __linux__

#define OS_GNU_LINUX

#endif

#ifdef OS_WIN
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <Windows.h>
#endif

#ifdef OS_GNU_LINUX

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

#endif

namespace mrpc
{
  template <typename T, typename T2>
  auto min_v(T &&v1, T2 &&v2)
  {
    return v1 < v2 ? v1 : v2;
  }
} // namespace mrpc