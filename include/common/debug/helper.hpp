#pragma once
#include <assert.h>
#include <sstream>
#include <thread>
#include <string>
#include "common/platform.hpp"

#define M_ASSERT(exp) assert(exp)
#define UNREACHABLE() M_ASSERT(false)

#define mrpc_log(format, ...)                                                                      \
  do                                                                                               \
  {                                                                                                \
    std::stringstream ss;                                                                          \
    ss << std::this_thread::get_id();                                                              \
    printf("[log] [%s] %s:%d " #format "\n", ss.str().c_str(), __FILE__, __LINE__, ##__VA_ARGS__); \
  } while (0)

#ifdef OS_WIN
#define get_errno() GetLastError()

inline std::string get_sys_error_msg()
{
  DWORD err = ::GetLastError();
  LPSTR tmessage;
  DWORD status = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, (DWORD)err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
      (LPSTR)(&tmessage), 0, NULL);
  std::string ret(tmessage);
  LocalFree(tmessage);
  return ret;
}

#define sys_msg_log(err)                                              \
  do                                                                  \
  {                                                                   \
    LPSTR tmessage;                                                   \
    DWORD status = FormatMessageA(                                    \
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | \
            FORMAT_MESSAGE_IGNORE_INSERTS,                            \
        NULL, (DWORD)err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),  \
        (LPSTR)(&tmessage), 0, NULL);                                 \
    mrpc_log("%s", tmessage);                                         \
    LocalFree(tmessage);                                              \
  } while (0)

#define throw_system_error(msg)       \
  do                                  \
  {                                   \
    int errorCode = ::GetLastError(); \
    throw std::system_error{          \
        static_cast<int>(errorCode),  \
        std::system_category(),       \
        msg};                         \
  } while (0)

#endif

#ifdef OS_GNU_LINUX
#define get_errno() errno

inline std::string get_sys_error_msg()
{
  char *tmessage = strerror(errno);
  std::string ret(tmessage);
  return ret;
}

#define sys_msg_log(err)              \
  do                                  \
  {                                   \
    char *tmessage = strerror(errno); \
    printf("[log] %s\n", tmessage);   \
  } while (0)

#define throw_system_error(msg)      \
  do                                 \
  {                                  \
    int errorCode = errno;           \
    throw std::system_error{         \
        static_cast<int>(errorCode), \
        std::system_category(),      \
        msg};                        \
  } while (0)

#endif